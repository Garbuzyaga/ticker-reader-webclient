---

# WebSocket Client Project

## Overview
This project implements a WebSocket client to connect to Binance's `bookTicker` stream and logs the received data along with calculated latencies. It uses `websocketpp` for WebSocket handling and `simdjson` for efficient JSON parsing. The project outputs latency metrics to `stdout` and stores the received data in a file.

---

## Prerequisites

Before compiling and running the project, ensure the following dependencies are installed on your system:

### **System Requirements**
- A Linux-based environment or any system with C++17-compatible compiler (GCC/Clang).

### **Dependencies**
1. **Boost Libraries**
   - Install Boost development libraries:
     ```bash
     sudo apt install libboost-all-dev -y      # Ubuntu/Debian
     ```
2. **OpenSSL**
   - Install OpenSSL development libraries:
     ```bash
     sudo apt install libssl-dev -y           # Ubuntu/Debian
     ```
3. **CMake** (if building with CMake):
   - Install CMake:
     ```bash
     sudo apt install cmake -y                # Ubuntu/Debian
     ```
4. **Simdjson and WebSocketpp (Submodules)**
   - These libraries are included as submodules in the `external` folder.

---

## Project Structure

```
project/
├── src/
│   └── main.cpp             # Main implementation
├── external/
│   ├── websocketpp/         # WebSocket++ submodule
│   └── simdjson/            # Simdjson submodule
├── CMakeLists.txt           # CMake build script (if applicable)
└── README.txt               # This file
```

---

## How to Compile

### **Step 1: Clone the Repository**
If you haven't already, clone the repository and initialize the submodules:
```bash
git clone <your-repo-url>
cd <your-repo-directory>
git submodule update --init --recursive
```

### **Step 2: Build the Project**
You can build the project using either CMake or `g++`.

#### **Using CMake**:
1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```
2. Configure the build:
   ```bash
   cmake ..
   ```
3. Compile the project:
   ```bash
   make
   ```

#### **Using g++**:
Alternatively, compile the project manually:
```bash
g++ -std=c++17 -Iexternal/websocketpp -Iexternal/simdjson src/main.cpp -o websocket_client -lboost_system -lssl -lcrypto -pthread
```

---

## How to Run

Run the compiled program with the following command:
```bash
./websocket_client <number_of_connections>
```

### **Arguments**
- `<number_of_connections>`: The number of WebSocket clients to launch simultaneously (e.g., `5`).

### **Output**
1. **Data File**:
   - Received data is stored in `aggregated_data.txt` in the current directory.
   - Each line includes the raw WebSocket message along with the calculated latency in milliseconds.
2. **Latency Metrics**:
   - Latency percentiles (P50, P90) are printed to `stdout`.

---

## Example

### **Compile and Run**
```bash
g++ -std=c++17 -Iexternal/websocketpp -Iexternal/simdjson src/main.cpp -o websocket_client -lboost_system -lssl -lcrypto -pthread
./websocket_client 3
```

### **Expected Output**
1. **stdout**:
   ```
   [Client 1] p50: 15 ms, p90: 25 ms
   [Client 2] p50: 16 ms, p90: 30 ms
   ```
2. **`aggregated_data.txt`**:
   ```json
   {"u":12345678,"T":1612345678901,"some_data":"example"}, "latency_ms":12
   {"u":12345679,"T":1612345678911,"some_data":"example"}, "latency_ms":15
   ```

---

## Notes

- **Submodules**:
  - Ensure the `websocketpp` and `simdjson` submodules are initialized and included in your build (`external/` directory).
- **Performance**:
  - The program calculates latency metrics in real-time and uses threading to handle multiple WebSocket connections simultaneously.
- **File Output**:
  - The data is written to `aggregated_data.txt`. Ensure you have write permissions in the current directory.

---
