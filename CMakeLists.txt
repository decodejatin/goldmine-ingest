cmake_minimum_required(VERSION 3.20)
project(goldmine_ingest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch nlohmann_json
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2)
FetchContent_MakeAvailable(json)

# Fetch IXWebSocket
set(USE_TLS ON)
set(IXWEBSOCKET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.4
)
FetchContent_MakeAvailable(ixwebsocket)

add_executable(binance_feed src/binance_feed.cpp)
target_include_directories(binance_feed PRIVATE ../interfaces/cpp ../include)
target_link_libraries(binance_feed PRIVATE ixwebsocket nlohmann_json::nlohmann_json pthread rt dl)

# Output into ingest/build
set_target_properties(binance_feed PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
