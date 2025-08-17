# HFT Socket Server

A high-frequency trading (HFT) socket server implemented in C++11, designed for ultra-low latency (< 10 microseconds) with a clean architecture using Singleton, Service, and Interceptor patterns.

## Architecture Overview

### 1. Singleton Pattern
- **SocketServer**: Manages network connections and I/O operations
- **ServiceManager**: Coordinates different trading services
- **PerformanceMonitor**: Tracks latency and throughput metrics

### 2. Service Layer
- **OrderMatchingService**: Handles order matching and execution
- **MarketDataService**: Manages market data distribution
- **RiskManagementService**: Performs risk checks and monitoring

### 3. Interceptor Pattern
- **ValidationInterceptor**: Validates message format and content
- **LoggingInterceptor**: Logs message processing details
- **PerformanceInterceptor**: Measures and tracks latency
- **ThrottlingInterceptor**: Implements rate limiting

## Key Features

- **Ultra-Low Latency**: Target < 10 microseconds average latency
- **High Performance**: epoll-based I/O, thread affinity, optimized buffers
- **Scalable**: Multi-threaded architecture with configurable worker threads
- **Reliable**: Comprehensive error handling and graceful shutdown
- **Configurable**: Command-line options for tuning performance

## Performance Optimizations

- **Compiler Flags**: `-O3`, `-march=native`, `-mtune=native`
- **Memory Management**: Pre-allocated buffer pools, zero-copy operations
- **Network**: TCP_NODELAY, optimized buffer sizes, non-blocking I/O
- **Threading**: CPU affinity, minimal sleep intervals (1 microsecond)
- **Data Structures**: Efficient serialization, minimal allocations

## Building

### Prerequisites
- C++11 compatible compiler (GCC 4.8+, Clang 3.3+)
- CMake 3.10+
- Linux kernel with epoll support

### Build Commands
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Usage

### Server
```bash
# Basic usage
./hft_server

# Custom configuration
./hft_server -p 9090 -t 8 -b 16384 -a

# Help
./hft_server -h
```

### Command Line Options
- `-p <port>`: Server port (default: 8080)
- `-t <threads>`: Worker thread count (default: 4)
- `-b <buffer_size>`: Buffer size in bytes (default: 8192)
- `-a`: Enable thread affinity (default: true)
- `-h`: Show help message

## Performance Testing

The server includes built-in performance tests:

1. **Interceptor Chain Test**: Tests message processing through the interceptor pipeline
2. **Latency Benchmark**: Measures processing latency over 100,000 iterations
3. **Real-time Monitoring**: Continuous latency and throughput tracking

## Message Types

- **Order Messages**: New, cancel, replace, fill orders
- **Market Data**: Bid/ask prices and sizes
- **Heartbeat**: Connection keep-alive
- **Error**: Error reporting and handling

## Network Protocol

Messages use a binary protocol optimized for speed:
- Fixed header (type, priority, sequence, timestamp, client_id)
- Variable payload based on message type
- Little-endian byte order for efficiency

## Monitoring and Metrics

- **Latency**: Average, P50, P95, P99 percentiles
- **Throughput**: Messages per second
- **Connections**: Active client connections
- **Services**: Service health and status

## Production Considerations

- **Hardware**: Use dedicated servers with high-frequency CPUs
- **Network**: Low-latency network cards, optimized kernel parameters
- **Monitoring**: External monitoring and alerting systems
- **Security**: Implement authentication and authorization
- **Logging**: Structured logging for production environments

## Development

### Project Structure
```
hftGw/
├── include/           # Header files
├── src/              # Source files
├── CMakeLists.txt    # Build configuration
└── README.md         # This file
```

### Adding New Services
1. Implement `IService` interface
2. Register with `ServiceManager`
3. Add message handling logic

### Adding New Interceptors
1. Implement `IInterceptor` interface
2. Add to interceptor chain
3. Configure processing order

## License

This project is provided as-is for educational and development purposes.

## Contributing

1. Follow the existing code style
2. Add performance tests for new features
3. Ensure latency targets are maintained
4. Update documentation as needed

## Performance Targets

- **Average Latency**: < 10 microseconds
- **P99 Latency**: < 50 microseconds
- **Throughput**: > 1,000,000 messages/second
- **Connection Capacity**: 10,000+ concurrent clients

## Troubleshooting

### High Latency
- Check CPU affinity settings
- Verify buffer sizes
- Monitor system load
- Check network configuration

### Connection Issues
- Verify port availability
- Check firewall settings
- Monitor connection limits
- Review error logs

### Performance Degradation
- Monitor system resources
- Check for memory leaks
- Verify thread counts
- Review interceptor chain 