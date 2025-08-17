#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include "../include/singleton.hpp"

namespace hft {

// Forward declarations
class Message;
class MessageHandler;
class PerformanceMonitor;

// High-performance socket server for HFT
class SocketServer : public Singleton<SocketServer> {
public:
    friend class Singleton<SocketServer>;
    
    ~SocketServer();
    
    bool initialize(int port, int max_connections = 10000);
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Performance settings
    void setBufferSize(size_t buffer_size);
    void setThreadCount(size_t thread_count);
    void setAffinity(bool enable);
    
    // Statistics
    size_t getConnectionCount() const;
    double getAverageLatency() const;
    size_t getMessagesProcessed() const;

protected:
    SocketServer() = default;
    
    void acceptLoop();
    void workerLoop(int worker_id);
    void handleConnection(int client_fd);
    void setSocketOptions(int sock_fd);
    void setThreadAffinity(int worker_id);
    
    // Socket management
    int server_fd_{-1};
    int epoll_fd_{-1};
    std::atomic<bool> running_{false};
    
    // Configuration
    int port_{8080};
    size_t max_connections_{10000};
    size_t buffer_size_{8192};
    size_t thread_count_{4};
    bool affinity_enabled_{true};
    
    // Threads
    std::thread accept_thread_;
    std::vector<std::thread> worker_threads_;
    
    // Performance monitoring
    std::shared_ptr<PerformanceMonitor> performance_monitor_;
    std::shared_ptr<MessageHandler> message_handler_;
    
    // Connection tracking
    std::atomic<size_t> connection_count_{0};
    std::atomic<size_t> messages_processed_{0};
    
    // Constants for optimization
    static constexpr size_t MAX_EVENTS = 1000;
    static constexpr size_t MAX_BUFFER_SIZE = 65536;
};

// Message handler for processing incoming messages
class MessageHandler {
public:
    MessageHandler();
    ~MessageHandler();
    
    void handleMessage(int client_fd, const char* data, size_t length);
    void setMessageCallback(std::function<void(std::shared_ptr<Message>)> callback);
    
    // Performance optimization
    void preallocateBuffers(size_t count);
    void setBatchSize(size_t batch_size);

private:
    std::function<void(std::shared_ptr<Message>)> message_callback_;
    size_t batch_size_{100};
    
    // Buffer pool for zero-copy operations
    std::vector<std::vector<char>> buffer_pool_;
    std::mutex buffer_pool_mutex_;
    size_t buffer_size_{8192}; // Default buffer size
    
    std::vector<char> getBuffer();
    void returnBuffer(std::vector<char> buffer);
};

// Performance monitoring for latency tracking
class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();
    
    void recordLatency(double latency_us);
    void recordThroughput(size_t messages_per_second);
    
    double getAverageLatency() const;
    double getP95Latency() const;
    double getP99Latency() const;
    size_t getThroughput() const;
    
    void reset();
    void printStats() const;

private:
    std::vector<double> latency_samples_;
    mutable std::mutex stats_mutex_;
    
    size_t throughput_{0};
    std::chrono::steady_clock::time_point last_throughput_update_;
    
    static constexpr size_t MAX_SAMPLES = 100000;
    static constexpr size_t THROUGHPUT_UPDATE_INTERVAL_MS = 1000;
};

} // namespace hft 