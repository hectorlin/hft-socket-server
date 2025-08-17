#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "../include/singleton.hpp"

namespace hft {

// Forward declarations
class Message;
class IService;

// Service interface
class IService {
public:
    virtual ~IService() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void processMessage(std::shared_ptr<Message> message) = 0;
    virtual std::string getName() const = 0;
};

// Service manager for coordinating different services
class ServiceManager : public Singleton<ServiceManager> {
public:
    friend class Singleton<ServiceManager>;
    
    void registerService(std::shared_ptr<IService> service);
    void unregisterService(const std::string& service_name);
    
    void startAllServices();
    void stopAllServices();
    
    void sendMessage(const std::string& service_name, std::shared_ptr<Message> message);
    void broadcastMessage(std::shared_ptr<Message> message);
    
    std::shared_ptr<IService> getService(const std::string& service_name);
    
    // Performance monitoring
    size_t getActiveServiceCount() const;
    double getAverageLatency() const;

public:
    ~ServiceManager();
protected:
    ServiceManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<IService>> services_;
    std::atomic<bool> running_{false};
    mutable std::mutex services_mutex_;
    
    // Message queue for async processing
    std::queue<std::pair<std::string, std::shared_ptr<Message>>> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread message_processor_thread_;
    
    void messageProcessorLoop();
    void processMessageQueue();
};

// Concrete services
class OrderMatchingService : public IService {
public:
    OrderMatchingService();
    ~OrderMatchingService() override;
    
    void start() override;
    void stop() override;
    bool isRunning() const override;
    void processMessage(std::shared_ptr<Message> message) override;
    std::string getName() const override { return "OrderMatching"; }

private:
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    void workerLoop();
};

class MarketDataService : public IService {
public:
    MarketDataService();
    ~MarketDataService() override;
    
    void start() override;
    void stop() override;
    bool isRunning() const override;
    void processMessage(std::shared_ptr<Message> message) override;
    std::string getName() const override { return "MarketData"; }

private:
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    void workerLoop();
};

class RiskManagementService : public IService {
public:
    RiskManagementService();
    ~RiskManagementService() override;
    
    void start() override;
    void stop() override;
    bool isRunning() const override;
    void processMessage(std::shared_ptr<Message> message) override;
    std::string getName() const override { return "RiskManagement"; }

private:
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    void workerLoop();
};

} // namespace hft 