#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <simdjson.h>

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_set>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>


typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

std::mutex data_mutex;
std::condition_variable data_cv;
std::queue<std::string> data_queue;
bool stop_worker = false;

std::mutex latency_mutex;
std::unordered_map<int, std::vector<int64_t>> latency_map;
const size_t LATENCY_WINDOW = 100;

// Global structures for message uniqueness and synchronization
std::unordered_set<uint64_t> global_unique_messages;
std::mutex global_unique_mutex; // Mutex for synchronizing access to unique message set

class WebSocketClient {
public:
    WebSocketClient(const std::string& uri, int id) : m_uri(uri), m_id(id) {
        m_client.init_asio();
        m_client.set_tls_init_handler([this](websocketpp::connection_hdl) {
            return on_tls_init();
        });
        m_client.set_message_handler(
            websocketpp::lib::bind(&WebSocketClient::on_message, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2)
        );
        m_client.set_open_handler(
            websocketpp::lib::bind(&WebSocketClient::on_open, this, websocketpp::lib::placeholders::_1)
        );
        m_client.set_close_handler(
            websocketpp::lib::bind(&WebSocketClient::on_close, this, websocketpp::lib::placeholders::_1)
        );
    }

    void connect() {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_client.get_connection(m_uri, ec);
        if (ec) {
            std::cerr << "[Client " << m_id << "] Connection error: " << ec.message() << std::endl;
            return;
        }
        m_client.connect(con);
        m_client.run();
    }

private:
    std::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::single_dh_use);
        return ctx;
    }

    void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
        auto receive_time = std::chrono::high_resolution_clock::now();
        try {
            simdjson::padded_string json(msg->get_payload());

            simdjson::ondemand::parser parser;
            auto doc = parser.iterate(json);

            uint64_t msg_id = doc["u"].get_uint64();
            {
                std::lock_guard<std::mutex> lock(global_unique_mutex);
                if (global_unique_messages.insert(msg_id).second) { // If it's a new message
                    int64_t server_timestamp = doc["T"].get_int64();
                    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(receive_time.time_since_epoch()).count();
                    int64_t latency = now_ms - server_timestamp;

                    {
                        std::lock_guard<std::mutex> lock(latency_mutex);
                        latency_map[m_id].push_back(latency);
                        if (latency_map[m_id].size() > LATENCY_WINDOW) {
                            latency_map[m_id].erase(latency_map[m_id].begin());
                        }

                        if (latency_map[m_id].size() == LATENCY_WINDOW) {
                            // Calculate and log percentiles
                            auto& latencies = latency_map[m_id];
                            std::vector<int64_t> sorted_latencies = latencies;
                            std::sort(sorted_latencies.begin(), sorted_latencies.end());

                            int64_t p50 = sorted_latencies[LATENCY_WINDOW / 2];
                            int64_t p90 = sorted_latencies[(LATENCY_WINDOW * 90) / 100];

                            std::cout << "[Client " << m_id << "] p50: " << p50 << " ms, p90: " << p90 << " ms" << std::endl;
                        }
                    }

                    std::lock_guard<std::mutex> queue_lock(data_mutex);
                    data_queue.push(msg->get_payload() + ", \"latency_ms\":" + std::to_string(latency));
                    data_cv.notify_one();
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Client " << m_id << "] Error parsing message: " << e.what() << std::endl;
        }
    }

    void on_open(websocketpp::connection_hdl hdl) {
        std::cout << "[Client " << m_id << "] Connection opened." << std::endl;
    }

    void on_close(websocketpp::connection_hdl) {
        std::cout << "[Client " << m_id << "] Connection closed." << std::endl;
    }

    client m_client;
    std::string m_uri;
    int m_id;
};

void data_worker(const std::string& output_file) {
    std::ofstream file(output_file, std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(data_mutex);
        data_cv.wait(lock, [] { return !data_queue.empty() || stop_worker; });

        while (!data_queue.empty()) {
            std::string data = data_queue.front();
            data_queue.pop();
            file << data << std::endl;
        }

        if (stop_worker) {
            break;
        }
    }

    file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_connections>" << std::endl;
        return 1;
    }

    int num_connections = std::stoi(argv[1]);
    if (num_connections <= 0) {
        std::cerr << "Number of connections must be a positive integer." << std::endl;
        return 1;
    }

    const std::string uri = "wss://fstream.binance.com/ws/btcusdt@bookTicker";
    const std::string output_file = "aggregated_data.txt";

    // Start the data worker thread
    std::thread worker(data_worker, output_file);

    // Launch multiple WebSocket clients
    std::vector<std::thread> threads;
    for (int i = 0; i < num_connections; ++i) {
        threads.emplace_back([uri, i]() {
            WebSocketClient ws_client(uri, i + 1);
            ws_client.connect();
        });
    }

    // Wait for all client threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Stop the worker thread
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        stop_worker = true;
    }
    data_cv.notify_one();
    worker.join();

    return 0;
}
