#include "../include/interceptor.hpp"
#include "../include/message.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace hft {

// InterceptorContext implementation
InterceptorContext::InterceptorContext(std::shared_ptr<Message> msg)
    : message_(msg) {
    startTimer();
}

void InterceptorContext::startTimer() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

void InterceptorContext::endTimer() {
    end_time_ = std::chrono::high_resolution_clock::now();
}

double InterceptorContext::getLatencyUs() const {
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time_ - start_time_);
    return duration.count() / 1000.0; // Convert to microseconds
}

void InterceptorContext::setMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
}

std::string InterceptorContext::getMetadata(const std::string& key) const {
    auto it = metadata_.find(key);
    return (it != metadata_.end()) ? it->second : "";
}

// InterceptorChain implementation
void InterceptorChain::addInterceptor(std::shared_ptr<IInterceptor> interceptor) {
    interceptors_.push_back(interceptor);
}

bool InterceptorChain::process(InterceptorContext& context) {
    for (auto& interceptor : interceptors_) {
        if (!interceptor->intercept(context)) {
            return false; // Stop processing if interceptor returns false
        }
    }
    return true;
}

void InterceptorChain::clear() {
    interceptors_.clear();
}

// ValidationInterceptor implementation
bool ValidationInterceptor::intercept(InterceptorContext& context) {
    auto message = context.getMessage();
    if (!message) {
        context.setMetadata("error", "Null message");
        return false;
    }
    
    // Basic validation
    if (message->getSequenceNumber() == 0) {
        context.setMetadata("error", "Invalid sequence number");
        return false;
    }
    
    if (message->getTimestamp() == 0) {
        context.setMetadata("error", "Invalid timestamp");
        return false;
    }
    
    // Message-specific validation
    switch (message->getType()) {
        case MessageType::ORDER_NEW:
        case MessageType::ORDER_CANCEL:
        case MessageType::ORDER_REPLACE: {
            auto order_msg = std::dynamic_pointer_cast<OrderMessage>(message);
            if (order_msg) {
                if (order_msg->getOrderId() == 0) {
                    context.setMetadata("error", "Invalid order ID");
                    return false;
                }
                if (order_msg->getSymbol().empty()) {
                    context.setMetadata("error", "Empty symbol");
                    return false;
                }
                if (order_msg->getPrice() <= 0.0) {
                    context.setMetadata("error", "Invalid price");
                    return false;
                }
                if (order_msg->getQuantity() == 0) {
                    context.setMetadata("error", "Invalid quantity");
                    return false;
                }
            }
            break;
        }
        case MessageType::MARKET_DATA: {
            auto md_msg = std::dynamic_pointer_cast<MarketDataMessage>(message);
            if (md_msg) {
                if (md_msg->getSymbol().empty()) {
                    context.setMetadata("error", "Empty symbol");
                    return false;
                }
                if (md_msg->getBid() < 0.0 || md_msg->getAsk() < 0.0) {
                    context.setMetadata("error", "Invalid bid/ask");
                    return false;
                }
                if (md_msg->getBid() >= md_msg->getAsk()) {
                    context.setMetadata("error", "Bid >= Ask");
                    return false;
                }
            }
            break;
        }
        default:
            break;
    }
    
    context.setMetadata("validation", "passed");
    return true;
}

// LoggingInterceptor implementation
bool LoggingInterceptor::intercept(InterceptorContext& context) {
    auto message = context.getMessage();
    if (!message) return false;
    
    // Log message details (in production, use proper logging framework)
    std::stringstream ss;
    ss << "Processing message: Type=" << static_cast<int>(message->getType())
       << ", Seq=" << message->getSequenceNumber()
       << ", Client=" << message->getClientId()
       << ", Priority=" << static_cast<int>(message->getPriority());
    
    context.setMetadata("log", ss.str());
    
    // In production, this would write to a log file or syslog
    // std::cout << "[LOG] " << ss.str() << std::endl;
    
    return true;
}

// PerformanceInterceptor implementation
bool PerformanceInterceptor::intercept(InterceptorContext& context) {
    context.endTimer();
    double latency = context.getLatencyUs();
    
    // Record performance metrics
    context.setMetadata("latency_us", std::to_string(latency));
    
    // Check if latency exceeds threshold (10 microseconds target)
    if (latency > 10.0) {
        context.setMetadata("performance_warning", "Latency exceeds 10us threshold");
        // In production, this could trigger alerts or performance degradation handling
    }
    
    return true;
}

// ThrottlingInterceptor implementation
ThrottlingInterceptor::ThrottlingInterceptor(size_t max_messages_per_second)
    : max_messages_per_second_(max_messages_per_second), message_count_(0) {
    last_check_ = std::chrono::steady_clock::now();
}

bool ThrottlingInterceptor::intercept(InterceptorContext& context) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_).count();
    
    std::lock_guard<std::mutex> lock(throttle_mutex_);
    
    if (elapsed >= 1000) { // Reset counter every second
        message_count_ = 0;
        last_check_ = now;
    }
    
    if (message_count_ >= max_messages_per_second_) {
        context.setMetadata("throttled", "Rate limit exceeded");
        return false; // Reject message
    }
    
    message_count_++;
    context.setMetadata("throttle_status", "accepted");
    return true;
}

} // namespace hft 