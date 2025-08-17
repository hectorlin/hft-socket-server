#include "../include/message.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace hft {

// Static member initialization
std::atomic<uint64_t> Message::global_sequence_counter_{0};

// Base Message implementation
Message::Message(MessageType type, MessagePriority priority)
    : type_(type), priority_(priority), sequence_number_(0), timestamp_(0), client_id_(0) {
    timestamp_ = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    sequence_number_ = global_sequence_counter_.fetch_add(1);
}

// OrderMessage implementation
OrderMessage::OrderMessage()
    : Message(MessageType::ORDER_NEW), order_id_(0), price_(0.0), quantity_(0), is_buy_(true) {
}

OrderMessage::OrderMessage(uint64_t order_id, const std::string& symbol, double price, uint32_t quantity, bool is_buy)
    : Message(MessageType::ORDER_NEW), order_id_(order_id), symbol_(symbol), 
      price_(price), quantity_(quantity), is_buy_(is_buy) {
}

std::vector<uint8_t> OrderMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(64); // Pre-allocate for efficiency
    
    // Header: type, priority, sequence, timestamp, client_id
    data.push_back(static_cast<uint8_t>(type_));
    data.push_back(static_cast<uint8_t>(priority_));
    
    // Sequence number (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back((sequence_number_ >> (i * 8)) & 0xFF);
    }
    
    // Timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp_ >> (i * 8)) & 0xFF);
    }
    
    // Client ID (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back((client_id_ >> (i * 8)) & 0xFF);
    }
    
    // Order ID (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back((order_id_ >> (i * 8)) & 0xFF);
    }
    
    // Symbol length and data
    uint8_t symbol_len = static_cast<uint8_t>(symbol_.length());
    data.push_back(symbol_len);
    data.insert(data.end(), symbol_.begin(), symbol_.end());
    
    // Price (8 bytes)
    uint64_t price_bits = *reinterpret_cast<const uint64_t*>(&price_);
    for (int i = 0; i < 8; ++i) {
        data.push_back((price_bits >> (i * 8)) & 0xFF);
    }
    
    // Quantity (4 bytes)
    for (int i = 0; i < 4; ++i) {
        data.push_back((quantity_ >> (i * 8)) & 0xFF);
    }
    
    // Buy/Sell flag (1 byte)
    data.push_back(is_buy_ ? 1 : 0);
    
    return data;
}

bool OrderMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 50) return false; // Minimum size check
    
    size_t pos = 0;
    
    // Header
    type_ = static_cast<MessageType>(data[pos++]);
    priority_ = static_cast<MessagePriority>(data[pos++]);
    
    // Sequence number
    sequence_number_ = 0;
    for (int i = 0; i < 8; ++i) {
        sequence_number_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Timestamp
    timestamp_ = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Client ID
    client_id_ = 0;
    for (int i = 0; i < 8; ++i) {
        client_id_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Order ID
    order_id_ = 0;
    for (int i = 0; i < 8; ++i) {
        order_id_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Symbol
    uint8_t symbol_len = data[pos++];
    if (pos + symbol_len > data.size()) return false;
    symbol_ = std::string(data.begin() + pos, data.begin() + pos + symbol_len);
    pos += symbol_len;
    
    // Price
    uint64_t price_bits = 0;
    for (int i = 0; i < 8; ++i) {
        price_bits |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    price_ = *reinterpret_cast<double*>(&price_bits);
    
    // Quantity
    quantity_ = 0;
    for (int i = 0; i < 4; ++i) {
        quantity_ |= static_cast<uint32_t>(data[pos++]) << (i * 8);
    }
    
    // Buy/Sell flag
    if (pos < data.size()) {
        is_buy_ = (data[pos] != 0);
    }
    
    return true;
}

// MarketDataMessage implementation
MarketDataMessage::MarketDataMessage()
    : Message(MessageType::MARKET_DATA), bid_(0.0), ask_(0.0), bid_size_(0), ask_size_(0) {
}

MarketDataMessage::MarketDataMessage(const std::string& symbol, double bid, double ask, uint32_t bid_size, uint32_t ask_size)
    : Message(MessageType::MARKET_DATA), symbol_(symbol), bid_(bid), ask_(ask), 
      bid_size_(bid_size), ask_size_(ask_size) {
}

std::vector<uint8_t> MarketDataMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(64);
    
    // Header
    data.push_back(static_cast<uint8_t>(type_));
    data.push_back(static_cast<uint8_t>(priority_));
    
    // Sequence, timestamp, client_id
    for (int i = 0; i < 8; ++i) {
        data.push_back((sequence_number_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((client_id_ >> (i * 8)) & 0xFF);
    }
    
    // Symbol
    uint8_t symbol_len = static_cast<uint8_t>(symbol_.length());
    data.push_back(symbol_len);
    data.insert(data.end(), symbol_.begin(), symbol_.end());
    
    // Bid, Ask (8 bytes each)
    uint64_t bid_bits = *reinterpret_cast<const uint64_t*>(&bid_);
    uint64_t ask_bits = *reinterpret_cast<const uint64_t*>(&ask_);
    for (int i = 0; i < 8; ++i) {
        data.push_back((bid_bits >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((ask_bits >> (i * 8)) & 0xFF);
    }
    
    // Sizes (4 bytes each)
    for (int i = 0; i < 4; ++i) {
        data.push_back((bid_size_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 4; ++i) {
        data.push_back((ask_size_ >> (i * 8)) & 0xFF);
    }
    
    return data;
}

bool MarketDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 50) return false;
    
    size_t pos = 0;
    
    // Header
    type_ = static_cast<MessageType>(data[pos++]);
    priority_ = static_cast<MessagePriority>(data[pos++]);
    
    // Sequence, timestamp, client_id
    sequence_number_ = 0;
    for (int i = 0; i < 8; ++i) {
        sequence_number_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    timestamp_ = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    client_id_ = 0;
    for (int i = 0; i < 8; ++i) {
        client_id_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Symbol
    uint8_t symbol_len = data[pos++];
    if (pos + symbol_len > data.size()) return false;
    symbol_ = std::string(data.begin() + pos, data.begin() + pos + symbol_len);
    pos += symbol_len;
    
    // Bid, Ask
    uint64_t bid_bits = 0, ask_bits = 0;
    for (int i = 0; i < 8; ++i) {
        bid_bits |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    for (int i = 0; i < 8; ++i) {
        ask_bits |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    bid_ = *reinterpret_cast<double*>(&bid_bits);
    ask_ = *reinterpret_cast<double*>(&ask_bits);
    
    // Sizes
    bid_size_ = 0;
    ask_size_ = 0;
    for (int i = 0; i < 4; ++i) {
        bid_size_ |= static_cast<uint32_t>(data[pos++]) << (i * 8);
    }
    for (int i = 0; i < 4; ++i) {
        ask_size_ |= static_cast<uint32_t>(data[pos++]) << (i * 8);
    }
    
    return true;
}

// HeartbeatMessage implementation
HeartbeatMessage::HeartbeatMessage()
    : Message(MessageType::HEARTBEAT) {
}

HeartbeatMessage::HeartbeatMessage(uint64_t client_id)
    : Message(MessageType::HEARTBEAT) {
    client_id_ = client_id;
}

std::vector<uint8_t> HeartbeatMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(32);
    
    // Header
    data.push_back(static_cast<uint8_t>(type_));
    data.push_back(static_cast<uint8_t>(priority_));
    
    // Sequence, timestamp, client_id
    for (int i = 0; i < 8; ++i) {
        data.push_back((sequence_number_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((client_id_ >> (i * 8)) & 0xFF);
    }
    
    return data;
}

bool HeartbeatMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 26) return false;
    
    size_t pos = 0;
    
    // Header
    type_ = static_cast<MessageType>(data[pos++]);
    priority_ = static_cast<MessagePriority>(data[pos++]);
    
    // Sequence, timestamp, client_id
    sequence_number_ = 0;
    for (int i = 0; i < 8; ++i) {
        sequence_number_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    timestamp_ = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    client_id_ = 0;
    for (int i = 0; i < 8; ++i) {
        client_id_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    return true;
}

// ErrorMessage implementation
ErrorMessage::ErrorMessage()
    : Message(MessageType::ERROR), error_code_(0) {
}

ErrorMessage::ErrorMessage(uint32_t error_code, const std::string& error_message)
    : Message(MessageType::ERROR), error_code_(error_code), error_message_(error_message) {
}

std::vector<uint8_t> ErrorMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(64);
    
    // Header
    data.push_back(static_cast<uint8_t>(type_));
    data.push_back(static_cast<uint8_t>(priority_));
    
    // Sequence, timestamp, client_id
    for (int i = 0; i < 8; ++i) {
        data.push_back((sequence_number_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp_ >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        data.push_back((client_id_ >> (i * 8)) & 0xFF);
    }
    
    // Error code
    for (int i = 0; i < 4; ++i) {
        data.push_back((error_code_ >> (i * 8)) & 0xFF);
    }
    
    // Error message
    uint8_t msg_len = static_cast<uint8_t>(error_message_.length());
    data.push_back(msg_len);
    data.insert(data.end(), error_message_.begin(), error_message_.end());
    
    return data;
}

bool ErrorMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 30) return false;
    
    size_t pos = 0;
    
    // Header
    type_ = static_cast<MessageType>(data[pos++]);
    priority_ = static_cast<MessagePriority>(data[pos++]);
    
    // Sequence, timestamp, client_id
    sequence_number_ = 0;
    for (int i = 0; i < 8; ++i) {
        sequence_number_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    timestamp_ = 0;
    for (int i = 0; i < 8; ++i) {
        timestamp_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    client_id_ = 0;
    for (int i = 0; i < 8; ++i) {
        client_id_ |= static_cast<uint64_t>(data[pos++]) << (i * 8);
    }
    
    // Error code
    error_code_ = 0;
    for (int i = 0; i < 4; ++i) {
        error_code_ |= static_cast<uint32_t>(data[pos++]) << (i * 8);
    }
    
    // Error message
    if (pos < data.size()) {
        uint8_t msg_len = data[pos++];
        if (pos + msg_len <= data.size()) {
            error_message_ = std::string(data.begin() + pos, data.begin() + pos + msg_len);
        }
    }
    
    return true;
}

// MessageFactory implementation
std::shared_ptr<Message> MessageFactory::createMessage(const std::vector<uint8_t>& data) {
    if (data.empty()) return nullptr;
    
    MessageType type = static_cast<MessageType>(data[0]);
    return createMessage(type);
}

std::shared_ptr<Message> MessageFactory::createMessage(MessageType type) {
    switch (type) {
        case MessageType::ORDER_NEW:
        case MessageType::ORDER_CANCEL:
        case MessageType::ORDER_REPLACE:
        case MessageType::ORDER_FILL:
            return std::make_shared<OrderMessage>();
        case MessageType::MARKET_DATA:
            return std::make_shared<MarketDataMessage>();
        case MessageType::HEARTBEAT:
            return std::make_shared<HeartbeatMessage>();
        case MessageType::ERROR:
            return std::make_shared<ErrorMessage>();
        default:
            return nullptr;
    }
}

} // namespace hft 