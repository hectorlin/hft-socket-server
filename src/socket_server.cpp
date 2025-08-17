#include "../include/socket_server.hpp"
#include "../include/message.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <algorithm>
#include <numeric>

namespace hft {

// SocketServer implementation
SocketServer::~SocketServer() {
    stop();
    
    if (server_fd_ >= 0) {
        close(server_fd_);
    }
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

bool SocketServer::initialize(int port, int max_connections) {
    port_ = port;
    max_connections_ = max_connections;
    
    // Create server socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[SocketServer] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket options for low latency
    setSocketOptions(server_fd_);
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[SocketServer] Failed to bind socket: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Listen for connections
    if (listen(server_fd_, SOMAXCONN) < 0) {
        std::cerr << "[SocketServer] Failed to listen: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "[SocketServer] Failed to create epoll: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Add server socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = nullptr;
    event.data.fd = server_fd_;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
        std::cerr << "[SocketServer] Failed to add server socket to epoll: " << strerror(errno) << std::endl;
        close(epoll_fd_);
        close(server_fd_);
        epoll_fd_ = -1;
        server_fd_ = -1;
        return false;
    }
    
    // Initialize message handler and performance monitor
    message_handler_ = std::make_shared<MessageHandler>();
    performance_monitor_ = std::make_shared<PerformanceMonitor>();
    
    std::cout << "[SocketServer] Initialized on port " << port_ << std::endl;
    return true;
}

void SocketServer::start() {
    if (running_.load()) return;
    
    running_ = true;
    
    // Start accept thread
    accept_thread_ = std::thread(&SocketServer::acceptLoop, this);
    
    // Start worker threads
    for (size_t i = 0; i < thread_count_; ++i) {
        worker_threads_.emplace_back(&SocketServer::workerLoop, this, i);
    }
    
    std::cout << "[SocketServer] Started with " << thread_count_ << " worker threads" << std::endl;
}

void SocketServer::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    
    // Stop accept thread
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // Stop worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    std::cout << "[SocketServer] Stopped" << std::endl;
}

void SocketServer::setBufferSize(size_t buffer_size) {
    buffer_size_ = std::min(buffer_size, MAX_BUFFER_SIZE);
}

void SocketServer::setThreadCount(size_t thread_count) {
    if (running_.load()) {
        std::cerr << "[SocketServer] Cannot change thread count while running" << std::endl;
        return;
    }
    thread_count_ = thread_count;
}

void SocketServer::setAffinity(bool enable) {
    affinity_enabled_ = enable;
}

size_t SocketServer::getConnectionCount() const {
    return connection_count_.load();
}

double SocketServer::getAverageLatency() const {
    return performance_monitor_ ? performance_monitor_->getAverageLatency() : 0.0;
}

size_t SocketServer::getMessagesProcessed() const {
    return messages_processed_.load();
}

void SocketServer::acceptLoop() {
    while (running_.load()) {
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1); // 1ms timeout
        
        if (nfds < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[SocketServer] epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd_) {
                // New connection
                handleConnection(server_fd_);
            }
        }
    }
}

void SocketServer::workerLoop(int worker_id) {
    if (affinity_enabled_) {
        setThreadAffinity(worker_id);
    }
    
    while (running_.load()) {
        // Worker threads handle data processing
        // In a real implementation, this would process messages from a queue
        
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // 1 microsecond sleep
    }
}

void SocketServer::handleConnection(int client_fd) {
    if (connection_count_.load() >= max_connections_) {
        close(client_fd);
        return;
    }
    
    // Set client socket options
    setSocketOptions(client_fd);
    
    // Add to epoll for monitoring
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET; // Edge-triggered
    event.data.fd = client_fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event) < 0) {
        if (errno == EEXIST) {
            // Client already exists in epoll, remove and re-add
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                std::cerr << "[SocketServer] Failed to add client to epoll after retry: " << strerror(errno) << std::endl;
                close(client_fd);
                return;
            }
        } else {
            std::cerr << "[SocketServer] Failed to add client to epoll: " << strerror(errno) << std::endl;
            close(client_fd);
            return;
        }
    }
    
    connection_count_.fetch_add(1);
    std::cout << "[SocketServer] New connection accepted, total: " << connection_count_.load() << std::endl;
}

void SocketServer::setSocketOptions(int sock_fd) {
    // Set non-blocking
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Set TCP_NODELAY for low latency
    int flag = 1;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Set SO_REUSEADDR
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    // Set buffer sizes
    int send_buf_size = buffer_size_;
    int recv_buf_size = buffer_size_;
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
}

void SocketServer::setThreadAffinity(int worker_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker_id % 8, &cpuset); // Assuming 8 cores, adjust as needed
    
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[SocketServer] Failed to set thread affinity for worker " << worker_id << std::endl;
    }
}

// MessageHandler implementation
MessageHandler::MessageHandler() {
    // Pre-allocate buffers for zero-copy operations
    preallocateBuffers(1000);
    buffer_size_ = 8192; // Default buffer size
}

MessageHandler::~MessageHandler() {
    // Cleanup
}

void MessageHandler::handleMessage(int client_fd, const char* data, size_t length) {
    if (!data || length == 0) return;
    
    // Convert data to vector for message processing
    std::vector<uint8_t> message_data(data, data + length);
    
    // Create message from data
    auto message = MessageFactory::createMessage(message_data);
    if (!message) {
        std::cerr << "[MessageHandler] Failed to create message from data" << std::endl;
        return;
    }
    
    // Set receive time for latency tracking
    message->setReceiveTime(std::chrono::high_resolution_clock::now());
    
    // Process message through callback if set
    if (message_callback_) {
        message_callback_(message);
    }
}

void MessageHandler::setMessageCallback(std::function<void(std::shared_ptr<Message>)> callback) {
    message_callback_ = callback;
}

void MessageHandler::preallocateBuffers(size_t count) {
    std::lock_guard<std::mutex> lock(buffer_pool_mutex_);
    buffer_pool_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        buffer_pool_.emplace_back(buffer_size_);
    }
}

void MessageHandler::setBatchSize(size_t batch_size) {
    batch_size_ = batch_size;
}

std::vector<char> MessageHandler::getBuffer() {
    std::lock_guard<std::mutex> lock(buffer_pool_mutex_);
    if (buffer_pool_.empty()) {
        return std::vector<char>(buffer_size_);
    }
    
    auto buffer = std::move(buffer_pool_.back());
    buffer_pool_.pop_back();
    return buffer;
}

void MessageHandler::returnBuffer(std::vector<char> buffer) {
    std::lock_guard<std::mutex> lock(buffer_pool_mutex_);
    if (buffer_pool_.size() < 1000) { // Limit pool size
        buffer_pool_.push_back(std::move(buffer));
    }
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor() {
    latency_samples_.reserve(MAX_SAMPLES);
}

PerformanceMonitor::~PerformanceMonitor() {
    // Cleanup
}

void PerformanceMonitor::recordLatency(double latency_us) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (latency_samples_.size() >= MAX_SAMPLES) {
        latency_samples_.erase(latency_samples_.begin());
    }
    latency_samples_.push_back(latency_us);
}

void PerformanceMonitor::recordThroughput(size_t messages_per_second) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_throughput_update_).count();
    
    if (elapsed >= THROUGHPUT_UPDATE_INTERVAL_MS) {
        throughput_ = messages_per_second;
        last_throughput_update_ = now;
    }
}

double PerformanceMonitor::getAverageLatency() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (latency_samples_.empty()) return 0.0;
    
    double sum = std::accumulate(latency_samples_.begin(), latency_samples_.end(), 0.0);
    return sum / latency_samples_.size();
}

double PerformanceMonitor::getP95Latency() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (latency_samples_.size() < 20) return getAverageLatency();
    
    std::vector<double> sorted_samples = latency_samples_;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    size_t index = static_cast<size_t>(sorted_samples.size() * 0.95);
    return sorted_samples[index];
}

double PerformanceMonitor::getP99Latency() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (latency_samples_.size() < 100) return getAverageLatency();
    
    std::vector<double> sorted_samples = latency_samples_;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    size_t index = static_cast<size_t>(sorted_samples.size() * 0.99);
    return sorted_samples[index];
}

size_t PerformanceMonitor::getThroughput() const {
    return throughput_;
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    latency_samples_.clear();
    throughput_ = 0;
}

void PerformanceMonitor::printStats() const {
    std::cout << "\n=== Performance Statistics ===" << std::endl;
    std::cout << "Average Latency: " << getAverageLatency() << " μs" << std::endl;
    std::cout << "P95 Latency: " << getP95Latency() << " μs" << std::endl;
    std::cout << "P99 Latency: " << getP99Latency() << " μs" << std::endl;
    std::cout << "Throughput: " << getThroughput() << " msg/s" << std::endl;
    std::cout << "Sample Count: " << latency_samples_.size() << std::endl;
    std::cout << "=============================" << std::endl;
}

} // namespace hft 