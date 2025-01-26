cmake_minimum_required (VERSION 3.16)
project (websocket-client LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_subdirectory(external/simdjson)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(external/websocketpp)

file(GLOB_RECURSE ${PROJECT_NAME}_SRC FOLLOW_SYMLINKS "src/*.cpp")

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} Threads::Threads OpenSSL::SSL OpenSSL::Crypto simdjson)
