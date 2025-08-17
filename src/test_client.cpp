#include "../include/message.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <cstring>
#include <algorithm>
#include <signal.h>
#include <atomic>

namespace hft_test {

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

class TestClient {
public:
    TestClient(const std::string& server_ip, int port) 
        : server_ip_(server_ip), port_(port), connected_(false), client_fd_(-1) {
        // Setup signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
    }
    
    ~TestClient() {
        disconnect();
    }
    
    bool connect() {
        client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd_ < 0) {
            std::cerr << "[Client] Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Set socket options for low latency
        int flag = 1;
        setsockopt(client_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        // Set non-blocking for timeout handling
        int flags = fcntl(client_fd_, F_GETFL, 0);
        fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK);
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "[Client] Invalid server address" << std::endl;
            close(client_fd_);
            client_fd_ = -1;
            return false;
        }
        
        // Try to connect
        int result = ::connect(client_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (result < 0 && errno != EINPROGRESS) {
            std::cerr << "[Client] Failed to connect to server: " << strerror(errno) << std::endl;
            close(client_fd_);
            client_fd_ = -1;
            return false;
        }
        
        // Wait for connection to complete
        fd_set write_fds;
        struct timeval timeout;
        
        FD_ZERO(&write_fds);
        FD_SET(client_fd_, &write_fds);
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        result = select(client_fd_ + 1, nullptr, &write_fds, nullptr, &timeout);
        if (result <= 0) {
            std::cerr << "[Client] Connection timeout" << std::endl;
            close(client_fd_);
            client_fd_ = -1;
            return false;
        }
        
        // Check if connection was successful
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            std::cerr << "[Client] Connection failed: " << strerror(error) << std::endl;
            close(client_fd_);
            client_fd_ = -1;
            return false;
        }
        
        // Set back to blocking mode
        fcntl(client_fd_, F_SETFL, flags);
        
        connected_ = true;
        std::cout << "[Client] Connected to server " << server_ip_ << ":" << port_ << std::endl;
        return true;
    }
    
    void disconnect() {
        if (connected_ && client_fd_ >= 0) {
            close(client_fd_);
            client_fd_ = -1;
            connected_ = false;
            std::cout << "[Client] Disconnected from server" << std::endl;
        }
    }
    
    bool sendMessage(const std::shared_ptr<hft::Message>& message) {
        if (!connected_) {
            std::cerr << "[Client] Not connected to server" << std::endl;
            return false;
        }
        
        auto data = message->serialize();
        ssize_t sent = send(client_fd_, data.data(), data.size(), MSG_NOSIGNAL);
        
        if (sent < 0) {
            std::cerr << "[Client] Failed to send message: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (sent != static_cast<ssize_t>(data.size())) {
            std::cerr << "[Client] Partial send: " << sent << "/" << data.size() << " bytes" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void runLatencyTest(size_t message_count) {
        if (!connected_) {
            std::cerr << "[Client] Not connected to server" << std::endl;
            return;
        }
        
        std::cout << "[Client] Running latency test with " << message_count << " messages..." << std::endl;
        
        std::vector<double> latencies;
        latencies.reserve(message_count);
        
        auto order_msg = std::make_shared<hft::OrderMessage>(12345, "AAPL", 150.50, 100, true);
        
        for (size_t i = 0; i < message_count && g_running; ++i) {
            // Update message for each iteration
            order_msg->setSequenceNumber(i + 1);
            order_msg->setTimestamp(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count());
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            if (!sendMessage(order_msg)) {
                std::cerr << "[Client] Failed to send message " << i << std::endl;
                continue;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            latencies.push_back(latency / 1000.0); // Convert to microseconds
            
            // Small delay to avoid overwhelming the server
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            
            // Progress indicator
            if ((i + 1) % 1000 == 0) {
                std::cout << "[Client] Sent " << (i + 1) << "/" << message_count << " messages" << std::endl;
            }
        }
        
        if (latencies.empty()) {
            std::cerr << "[Client] No messages were sent successfully" << std::endl;
            return;
        }
        
        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        
        double sum = 0.0;
        for (double latency : latencies) {
            sum += latency;
        }
        
        double avg_latency = sum / latencies.size();
        double p50_latency = latencies[latencies.size() * 0.5];
        double p95_latency = latencies[latencies.size() * 0.95];
        double p99_latency = latencies[latencies.size() * 0.99];
        double min_latency = latencies.front();
        double max_latency = latencies.back();
        
        std::cout << "\n[Client] Latency Test Results:" << std::endl;
        std::cout << "  Messages Sent: " << latencies.size() << std::endl;
        std::cout << "  Average Latency: " << avg_latency << " μs" << std::endl;
        std::cout << "  P50 Latency: " << p50_latency << " μs" << std::endl;
        std::cout << "  P95 Latency: " << p95_latency << " μs" << std::endl;
        std::cout << "  P99 Latency: " << p99_latency << " μs" << std::endl;
        std::cout << "  Min Latency: " << min_latency << " μs" << std::endl;
        std::cout << "  Max Latency: " << max_latency << " μs" << std::endl;
        
        // Check if we meet the 10 microsecond target
        if (avg_latency < 10.0) {
            std::cout << "  ✓ Target achieved: Average latency < 10 μs" << std::endl;
        } else {
            std::cout << "  ✗ Target missed: Average latency >= 10 μs" << std::endl;
        }
    }
    
    void runThroughputTest(size_t message_count, size_t duration_seconds) {
        if (!connected_) {
            std::cerr << "[Client] Not connected to server" << std::endl;
            return;
        }
        
        std::cout << "[Client] Running throughput test: " << message_count 
                  << " messages over " << duration_seconds << " seconds..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        size_t messages_sent = 0;
        
        auto order_msg = std::make_shared<hft::OrderMessage>(12345, "AAPL", 150.50, 100, true);
        auto md_msg = std::make_shared<hft::MarketDataMessage>("AAPL", 150.45, 150.55, 1000, 1000);
        
        std::vector<std::shared_ptr<hft::Message>> messages = {order_msg, md_msg};
        size_t msg_index = 0;
        
        while (messages_sent < message_count && g_running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            if (elapsed >= static_cast<long>(duration_seconds)) {
                break;
            }
            
            // Update message
            auto& msg = messages[msg_index % messages.size()];
            msg->setSequenceNumber(messages_sent + 1);
            msg->setTimestamp(std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
            
            if (sendMessage(msg)) {
                messages_sent++;
            }
            
            msg_index++;
            
            // Calculate target rate
            size_t target_messages = (message_count * elapsed) / duration_seconds;
            if (messages_sent < target_messages) {
                // Send faster to catch up
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            } else {
                // Slow down to match target rate
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            
            // Progress indicator
            if (messages_sent % 1000 == 0) {
                std::cout << "[Client] Sent " << messages_sent << "/" << message_count << " messages" << std::endl;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        double actual_rate = (messages_sent * 1000.0) / actual_duration;
        double target_rate = static_cast<double>(message_count) / duration_seconds;
        
        std::cout << "\n[Client] Throughput Test Results:" << std::endl;
        std::cout << "  Messages Sent: " << messages_sent << std::endl;
        std::cout << "  Actual Duration: " << (actual_duration / 1000.0) << " seconds" << std::endl;
        std::cout << "  Actual Rate: " << actual_rate << " msg/s" << std::endl;
        std::cout << "  Target Rate: " << target_rate << " msg/s" << std::endl;
        std::cout << "  Efficiency: " << (actual_rate / target_rate * 100.0) << "%" << std::endl;
    }
    
    void runStressTest(size_t message_count, size_t duration_seconds) {
        if (!connected_) {
            std::cerr << "[Client] Not connected to server" << std::endl;
            return;
        }
        
        std::cout << "[Client] Running stress test: " << message_count 
                  << " messages over " << duration_seconds << " seconds..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        size_t messages_sent = 0;
        size_t messages_failed = 0;
        
        auto order_msg = std::make_shared<hft::OrderMessage>(12345, "AAPL", 150.50, 100, true);
        
        while (messages_sent < message_count && g_running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            if (elapsed >= static_cast<long>(duration_seconds)) {
                break;
            }
            
            // Update message
            order_msg->setSequenceNumber(messages_sent + 1);
            order_msg->setTimestamp(std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count());
            
            if (sendMessage(order_msg)) {
                messages_sent++;
            } else {
                messages_failed++;
            }
            
            // Progress indicator
            if (messages_sent % 1000 == 0) {
                std::cout << "[Client] Sent " << messages_sent << "/" << message_count 
                          << " messages (Failed: " << messages_failed << ")" << std::endl;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        double actual_rate = (messages_sent * 1000.0) / actual_duration;
        double target_rate = static_cast<double>(message_count) / duration_seconds;
        double success_rate = (messages_sent * 100.0) / (messages_sent + messages_failed);
        
        std::cout << "\n[Client] Stress Test Results:" << std::endl;
        std::cout << "  Messages Sent: " << messages_sent << std::endl;
        std::cout << "  Messages Failed: " << messages_failed << std::endl;
        std::cout << "  Success Rate: " << success_rate << "%" << std::endl;
        std::cout << "  Actual Duration: " << (actual_duration / 1000.0) << " seconds" << std::endl;
        std::cout << "  Actual Rate: " << actual_rate << " msg/s" << std::endl;
        std::cout << "  Target Rate: " << target_rate << " msg/s" << std::endl;
        std::cout << "  Efficiency: " << (actual_rate / target_rate * 100.0) << "%" << std::endl;
    }
    
    bool isConnected() const { return connected_; }
    
    void waitForConnection(int max_retries = 10) {
        for (int i = 0; i < max_retries && !connected_; ++i) {
            std::cout << "[Client] Attempting to connect (attempt " << (i + 1) << "/" << max_retries << ")..." << std::endl;
            if (connect()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    std::string server_ip_;
    int port_;
    int client_fd_;
    bool connected_;
};

void printUsage() {
    std::cout << "HFT Test Client - Enhanced Version" << std::endl;
    std::cout << "Usage: ./test_client <server_ip> <port> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -l <count>           Latency test with <count> messages" << std::endl;
    std::cout << "  -t <count> <seconds> Throughput test" << std::endl;
    std::cout << "  -s <count> <seconds> Stress test" << std::endl;
    std::cout << "  -w                   Wait for server to be ready" << std::endl;
    std::cout << "  -h                   Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./test_client 127.0.0.1 8080 -l 10000" << std::endl;
    std::cout << "  ./test_client 127.0.0.1 8080 -t 100000 10" << std::endl;
    std::cout << "  ./test_client 127.0.0.1 8080 -s 50000 5" << std::endl;
    std::cout << "  ./test_client 127.0.0.1 8080 -w -l 1000" << std::endl;
}

} // namespace hft_test

int main(int argc, char* argv[]) {
    using namespace hft_test;
    
    if (argc < 3) {
        printUsage();
        return 1;
    }
    
    std::string server_ip = argv[1];
    int port = std::stoi(argv[2]);
    
    // Parse options
    size_t latency_count = 0;
    size_t throughput_count = 0;
    size_t throughput_duration = 0;
    size_t stress_count = 0;
    size_t stress_duration = 0;
    bool wait_for_server = false;
    
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else if (arg == "-l" && i + 1 < argc) {
            latency_count = std::stoul(argv[++i]);
        } else if (arg == "-t" && i + 2 < argc) {
            throughput_count = std::stoul(argv[++i]);
            throughput_duration = std::stoul(argv[++i]);
        } else if (arg == "-s" && i + 2 < argc) {
            stress_count = std::stoul(argv[++i]);
            stress_duration = std::stoul(argv[++i]);
        } else if (arg == "-w") {
            wait_for_server = true;
        }
    }
    
    std::cout << "=== HFT Test Client - Enhanced Version ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << port << std::endl;
    std::cout << "Latency Test: " << (latency_count > 0 ? std::to_string(latency_count) + " messages" : "disabled") << std::endl;
    std::cout << "Throughput Test: " << (throughput_count > 0 ? std::to_string(throughput_count) + " messages over " + std::to_string(throughput_duration) + "s" : "disabled") << std::endl;
    std::cout << "Stress Test: " << (stress_count > 0 ? std::to_string(stress_count) + " messages over " + std::to_string(stress_duration) + "s" : "disabled") << std::endl;
    std::cout << "Wait for Server: " << (wait_for_server ? "enabled" : "disabled") << std::endl;
    std::cout << "======================" << std::endl;
    
    TestClient client(server_ip, port);
    
    if (wait_for_server) {
        client.waitForConnection();
    }
    
    if (!client.connect()) {
        std::cerr << "[Main] Failed to connect to server" << std::endl;
        return 1;
    }
    
    try {
        // Run latency test
        if (latency_count > 0) {
            client.runLatencyTest(latency_count);
        }
        
        // Run throughput test
        if (throughput_count > 0 && throughput_duration > 0) {
            client.runThroughputTest(throughput_count, throughput_duration);
        }
        
        // Run stress test
        if (stress_count > 0 && stress_duration > 0) {
            client.runStressTest(stress_count, stress_duration);
        }
        
        // If no tests specified, run a simple demo
        if (latency_count == 0 && throughput_count == 0 && stress_count == 0) {
            std::cout << "\n[Main] Running demo mode..." << std::endl;
            
            // Send a few test messages
            auto order_msg = std::make_shared<hft::OrderMessage>(12345, "AAPL", 150.50, 100, true);
            auto md_msg = std::make_shared<hft::MarketDataMessage>("AAPL", 150.45, 150.55, 1000, 1000);
            
            std::cout << "[Main] Sending test messages..." << std::endl;
            
            if (client.sendMessage(order_msg)) {
                std::cout << "[Main] Order message sent successfully" << std::endl;
            }
            
            if (client.sendMessage(md_msg)) {
                std::cout << "[Main] Market data message sent successfully" << std::endl;
            }
            
            // Wait a bit
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n[Main] Test completed successfully" << std::endl;
    return 0;
} 