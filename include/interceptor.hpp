#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <map>
#include <mutex>

namespace hft {

// Forward declarations
class Message;
class InterceptorContext;

// Base interceptor interface
class IInterceptor {
public:
    virtual ~IInterceptor() = default;
    virtual bool intercept(InterceptorContext& context) = 0;
};

// Interceptor context for passing data through the chain
class InterceptorContext {
public:
    explicit InterceptorContext(std::shared_ptr<Message> msg);
    
    std::shared_ptr<Message> getMessage() const { return message_; }
    void setMessage(std::shared_ptr<Message> msg) { message_ = msg; }
    
    // Performance tracking
    void startTimer();
    void endTimer();
    double getLatencyUs() const;
    
    // Metadata
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key) const;

private:
    std::shared_ptr<Message> message_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    std::map<std::string, std::string> metadata_;
};

// Interceptor chain for processing messages
class InterceptorChain {
public:
    void addInterceptor(std::shared_ptr<IInterceptor> interceptor);
    bool process(InterceptorContext& context);
    void clear();

private:
    std::vector<std::shared_ptr<IInterceptor>> interceptors_;
};

// Concrete interceptors
class ValidationInterceptor : public IInterceptor {
public:
    bool intercept(InterceptorContext& context) override;
};

class LoggingInterceptor : public IInterceptor {
public:
    bool intercept(InterceptorContext& context) override;
};

class PerformanceInterceptor : public IInterceptor {
public:
    bool intercept(InterceptorContext& context) override;
};

class ThrottlingInterceptor : public IInterceptor {
public:
    explicit ThrottlingInterceptor(size_t max_messages_per_second);
    bool intercept(InterceptorContext& context) override;

private:
    size_t max_messages_per_second_;
    std::chrono::steady_clock::time_point last_check_;
    size_t message_count_;
    mutable std::mutex throttle_mutex_;
};

} // namespace hft 