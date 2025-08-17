#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic> // Added for atomic sequence counter

namespace hft {

// Message types for HFT system
enum class MessageType : uint8_t {
    ORDER_NEW = 1,
    ORDER_CANCEL = 2,
    ORDER_REPLACE = 3,
    ORDER_FILL = 4,
    MARKET_DATA = 5,
    HEARTBEAT = 6,
    LOGIN = 7,
    LOGOUT = 8,
    ERROR = 9
};

// Message priority levels
enum class MessagePriority : uint8_t {
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    CRITICAL = 4
};

// Base message class
class Message {
public:
    Message(MessageType type, MessagePriority priority = MessagePriority::NORMAL);
    virtual ~Message() = default;
    
    // Getters
    MessageType getType() const { return type_; }
    MessagePriority getPriority() const { return priority_; }
    uint64_t getSequenceNumber() const { return sequence_number_; }
    uint64_t getTimestamp() const { return timestamp_; }
    uint64_t getClientId() const { return client_id_; }
    
    // Setters
    void setSequenceNumber(uint64_t seq) { sequence_number_ = seq; }
    void setClientId(uint64_t client_id) { client_id_ = client_id; }
    void setTimestamp(uint64_t ts) { timestamp_ = ts; }
    
    // Serialization
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual bool deserialize(const std::vector<uint8_t>& data) = 0;
    
    // Performance tracking
    void setReceiveTime(std::chrono::high_resolution_clock::time_point time) { receive_time_ = time; }
    std::chrono::high_resolution_clock::time_point getReceiveTime() const { return receive_time_; }

protected:
    MessageType type_;
    MessagePriority priority_;
    uint64_t sequence_number_;
    uint64_t timestamp_;
    uint64_t client_id_;
    std::chrono::high_resolution_clock::time_point receive_time_;
    
    static std::atomic<uint64_t> global_sequence_counter_;
};

// Order message
class OrderMessage : public Message {
public:
    OrderMessage();
    OrderMessage(uint64_t order_id, const std::string& symbol, double price, uint32_t quantity, bool is_buy);
    
    // Getters
    uint64_t getOrderId() const { return order_id_; }
    std::string getSymbol() const { return symbol_; }
    double getPrice() const { return price_; }
    uint32_t getQuantity() const { return quantity_; }
    bool isBuy() const { return is_buy_; }
    
    // Serialization
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;

private:
    uint64_t order_id_;
    std::string symbol_;
    double price_;
    uint32_t quantity_;
    bool is_buy_;
};

// Market data message
class MarketDataMessage : public Message {
public:
    MarketDataMessage();
    MarketDataMessage(const std::string& symbol, double bid, double ask, uint32_t bid_size, uint32_t ask_size);
    
    // Getters
    std::string getSymbol() const { return symbol_; }
    double getBid() const { return bid_; }
    double getAsk() const { return ask_; }
    uint32_t getBidSize() const { return bid_size_; }
    uint32_t getAskSize() const { return ask_size_; }
    
    // Serialization
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;

private:
    std::string symbol_;
    double bid_;
    double ask_;
    uint32_t bid_size_;
    uint32_t ask_size_;
};

// Heartbeat message
class HeartbeatMessage : public Message {
public:
    HeartbeatMessage();
    HeartbeatMessage(uint64_t client_id);
    
    // Serialization
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
};

// Error message
class ErrorMessage : public Message {
public:
    ErrorMessage();
    ErrorMessage(uint32_t error_code, const std::string& error_message);
    
    // Getters
    uint32_t getErrorCode() const { return error_code_; }
    std::string getErrorMessage() const { return error_message_; }
    
    // Serialization
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;

private:
    uint32_t error_code_;
    std::string error_message_;
};

// Message factory for creating messages from serialized data
class MessageFactory {
public:
    static std::shared_ptr<Message> createMessage(const std::vector<uint8_t>& data);
    static std::shared_ptr<Message> createMessage(MessageType type);
    
private:
    static std::shared_ptr<Message> createOrderMessage(const std::vector<uint8_t>& data);
    static std::shared_ptr<Message> createMarketDataMessage(const std::vector<uint8_t>& data);
    static std::shared_ptr<Message> createHeartbeatMessage(const std::vector<uint8_t>& data);
    static std::shared_ptr<Message> createErrorMessage(const std::vector<uint8_t>& data);
};

} // namespace hft 