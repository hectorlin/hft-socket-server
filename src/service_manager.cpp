#include "../include/service_manager.hpp"
#include "../include/message.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace hft {

// ServiceManager implementation
ServiceManager::~ServiceManager() {
    stopAllServices();
    if (message_processor_thread_.joinable()) {
        message_processor_thread_.join();
    }
}

void ServiceManager::registerService(std::shared_ptr<IService> service) {
    if (!service) return;
    
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[service->getName()] = service;
    
    std::cout << "[ServiceManager] Registered service: " << service->getName() << std::endl;
}

void ServiceManager::unregisterService(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        if (it->second->isRunning()) {
            it->second->stop();
        }
        services_.erase(it);
        std::cout << "[ServiceManager] Unregistered service: " << service_name << std::endl;
    }
}

void ServiceManager::startAllServices() {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    for (auto& service_pair : services_) {
        if (!service_pair.second->isRunning()) {
            service_pair.second->start();
            std::cout << "[ServiceManager] Started service: " << service_pair.first << std::endl;
        }
    }
    
    running_ = true;
    
    // Start message processor thread
    if (!message_processor_thread_.joinable()) {
        message_processor_thread_ = std::thread(&ServiceManager::messageProcessorLoop, this);
    }
}

void ServiceManager::stopAllServices() {
    running_ = false;
    
    std::lock_guard<std::mutex> lock(services_mutex_);
    for (auto& service_pair : services_) {
        if (service_pair.second->isRunning()) {
            service_pair.second->stop();
            std::cout << "[ServiceManager] Stopped service: " << service_pair.first << std::endl;
        }
    }
}

void ServiceManager::sendMessage(const std::string& service_name, std::shared_ptr<Message> message) {
    if (!message) return;
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.emplace(service_name, message);
    queue_cv_.notify_one();
}

void ServiceManager::broadcastMessage(std::shared_ptr<Message> message) {
    if (!message) return;
    
    std::lock_guard<std::mutex> lock(services_mutex_);
    for (auto& service_pair : services_) {
        if (service_pair.second->isRunning()) {
            service_pair.second->processMessage(message);
        }
    }
}

std::shared_ptr<IService> ServiceManager::getService(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(service_name);
    return (it != services_.end()) ? it->second : nullptr;
}

size_t ServiceManager::getActiveServiceCount() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    size_t count = 0;
    for (const auto& service_pair : services_) {
        if (service_pair.second->isRunning()) count++;
    }
    return count;
}

double ServiceManager::getAverageLatency() const {
    // This would be implemented with actual latency tracking
    // For now, return a placeholder value
    return 5.0; // 5 microseconds average
}

void ServiceManager::messageProcessorLoop() {
    while (running_) {
        processMessageQueue();
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // 1 microsecond sleep for low latency
    }
}

void ServiceManager::processMessageQueue() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    if (message_queue_.empty()) {
        queue_cv_.wait_for(lock, std::chrono::microseconds(10)); // 10 microseconds timeout
        return;
    }
    
    // Process up to 100 messages per batch for efficiency
    size_t processed = 0;
    const size_t max_batch = 100;
    
    while (!message_queue_.empty() && processed < max_batch) {
        auto service_name = message_queue_.front().first;
        auto message = message_queue_.front().second;
        message_queue_.pop();
        lock.unlock();
        
        auto service = getService(service_name);
        if (service && service->isRunning()) {
            service->processMessage(message);
        }
        
        processed++;
        lock.lock();
    }
}

// OrderMatchingService implementation
OrderMatchingService::OrderMatchingService() {
    // Initialize order book and matching engine
}

OrderMatchingService::~OrderMatchingService() {
    stop();
}

void OrderMatchingService::start() {
    if (running_.load()) return;
    
    running_ = true;
    worker_thread_ = std::thread(&OrderMatchingService::workerLoop, this);
    std::cout << "[OrderMatching] Service started" << std::endl;
}

void OrderMatchingService::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::cout << "[OrderMatching] Service stopped" << std::endl;
}

bool OrderMatchingService::isRunning() const {
    return running_.load();
}

void OrderMatchingService::processMessage(std::shared_ptr<Message> message) {
    if (!message || !running_.load()) return;
    
    // Process order messages with ultra-low latency
    auto start_time = std::chrono::high_resolution_clock::now();
    
    switch (message->getType()) {
        case MessageType::ORDER_NEW: {
            auto order_msg = std::dynamic_pointer_cast<OrderMessage>(message);
            if (order_msg) {
                // Process new order
                // In a real implementation, this would add to order book and attempt matching
            }
            break;
        }
        case MessageType::ORDER_CANCEL: {
            // Process order cancellation
            break;
        }
        case MessageType::ORDER_REPLACE: {
            // Process order replacement
            break;
        }
        default:
            break;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    
    // Track performance - should be under 10 microseconds
    if (latency > 10000) { // 10 microseconds in nanoseconds
        std::cout << "[OrderMatching] Warning: High latency detected: " 
                  << (latency / 1000.0) << " microseconds" << std::endl;
    }
}

void OrderMatchingService::workerLoop() {
    while (running_.load()) {
        // Process pending orders and perform matching
        // This would implement the actual matching engine logic
        
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // 1 microsecond sleep
    }
}

// MarketDataService implementation
MarketDataService::MarketDataService() {
    // Initialize market data structures
}

MarketDataService::~MarketDataService() {
    stop();
}

void MarketDataService::start() {
    if (running_.load()) return;
    
    running_ = true;
    worker_thread_ = std::thread(&MarketDataService::workerLoop, this);
    std::cout << "[MarketData] Service started" << std::endl;
}

void MarketDataService::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::cout << "[MarketData] Service stopped" << std::endl;
}

bool MarketDataService::isRunning() const {
    return running_.load();
}

void MarketDataService::processMessage(std::shared_ptr<Message> message) {
    if (!message || !running_.load()) return;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (message->getType() == MessageType::MARKET_DATA) {
        auto md_msg = std::dynamic_pointer_cast<MarketDataMessage>(message);
        if (md_msg) {
            // Process market data update
            // This would update internal market data structures
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    
    if (latency > 10000) {
        std::cout << "[MarketData] Warning: High latency detected: " 
                  << (latency / 1000.0) << " microseconds" << std::endl;
    }
}

void MarketDataService::workerLoop() {
    while (running_.load()) {
        // Process market data updates and disseminate to subscribers
        // This would implement market data distribution logic
        
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

// RiskManagementService implementation
RiskManagementService::RiskManagementService() {
    // Initialize risk management parameters
}

RiskManagementService::~RiskManagementService() {
    stop();
}

void RiskManagementService::start() {
    if (running_.load()) return;
    
    running_ = true;
    worker_thread_ = std::thread(&RiskManagementService::workerLoop, this);
    std::cout << "[RiskManagement] Service started" << std::endl;
}

void RiskManagementService::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::cout << "[RiskManagement] Service stopped" << std::endl;
}

bool RiskManagementService::isRunning() const {
    return running_.load();
}

void RiskManagementService::processMessage(std::shared_ptr<Message> message) {
    if (!message || !running_.load()) return;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Perform risk checks on orders
    if (message->getType() == MessageType::ORDER_NEW) {
        auto order_msg = std::dynamic_pointer_cast<OrderMessage>(message);
        if (order_msg) {
            // Perform risk validation
            // Check position limits, exposure limits, etc.
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    
    if (latency > 10000) {
        std::cout << "[RiskManagement] Warning: High latency detected: " 
                  << (latency / 1000.0) << " microseconds" << std::endl;
    }
}

void RiskManagementService::workerLoop() {
    while (running_.load()) {
        // Perform periodic risk checks and monitoring
        // This would implement risk monitoring and alerting logic
        
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

} // namespace hft 