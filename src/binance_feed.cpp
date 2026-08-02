#include <iostream>
#include <thread>
#include <chrono>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include "goldmine/live_ingestion.hpp"

using json = nlohmann::json;
using namespace goldmine;

int main() {
    ix::initNetSystem();

    // 1. Setup Shared Memory writer
    int fd = shm_open("/goldmine_tick_shm", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Failed to shm_open /dev/shm/goldmine_tick_shm" << std::endl;
        return 1;
    }
    if (ftruncate(fd, sizeof(SharedTick)) == -1) {
        std::cerr << "Failed to ftruncate SHM" << std::endl;
        return 1;
    }

    void* ptr = mmap(nullptr, sizeof(SharedTick), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to mmap SHM" << std::endl;
        return 1;
    }
    
    SharedTick* shm_tick = reinterpret_cast<SharedTick*>(ptr);
    
    // Initialize SHM
    shm_tick->magic_header = 0x474F4C44; // "GOLD"
    shm_tick->sequence_id.store(0, std::memory_order_relaxed);
    uint64_t seq_id = 0;

    // 2. Setup WebSocket connection
    ix::WebSocket webSocket;
    std::string url("wss://stream.binance.com:9443/ws/btcusdt@bookTicker");
    webSocket.setUrl(url);

    ix::SocketTLSOptions tlsOptions;
    tlsOptions.caFile = "/etc/ssl/certs/ca-certificates.crt";
    webSocket.setTLSOptions(tlsOptions);

    std::cout.setf(std::ios::unitbuf);
    std::cout << "[*] C++ Native Binance Feed Handler connecting..." << std::endl;
    std::cout << "[*] Bypassing Python WebSocket Bridge. Direct memory writes enabled." << std::endl;

    webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "[*] Binance WebSocket Connected." << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            std::cout << "[-] Binance WebSocket Closed. Reason: " << msg->errorInfo.reason << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "[!] Binance WebSocket Error: " << msg->errorInfo.reason << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto data = json::parse(msg->str);
                if (data.contains("b") && data.contains("a")) {
                    double bid = std::stod(data["b"].get<std::string>());
                    double ask = std::stod(data["a"].get<std::string>());
                    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    // Write payload to SHM
                    shm_tick->timestamp_ms = ts;
                    shm_tick->bid = bid;
                    shm_tick->ask = ask;
                    shm_tick->volume = 1;
                    shm_tick->checksum = 0; 
                    
                    // Memory barrier release for atomic sequence
                    seq_id++;
                    shm_tick->sequence_id.store(seq_id, std::memory_order_release);
                    
                    // Throttle logging to avoid spam
                    if (seq_id % 100 == 0 || seq_id < 10) {
                        std::cout << "[Binance -> C++ SHM] PAXG/USDT | Bid: " << bid 
                                  << " | Ask: " << ask << " | Seq: " << seq_id << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing: " << e.what() << " | Raw msg: " << msg->str << std::endl;
            }
        }
    });

    webSocket.start();

    // 3. Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    ix::uninitNetSystem();
    return 0;
}
