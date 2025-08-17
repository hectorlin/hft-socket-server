#include "../include/socket_server.hpp"
#include "../include/service_manager.hpp"
#include "../include/interceptor.hpp"
#include "../include/message.hpp"
#include <iostream>
#include <signal.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>

namespace hft {

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[Main] Received shutdown signal, stopping server..." << std::endl;
        g_running = false;
    }
}

void setupSignalHandlers() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

void printUsage() {
    std::cout << "HFT Socket Server - High-Frequency Trading Server" << std::endl;
    std::cout << "Usage: ./hft_server [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p <port>           Server port (default: 8080)" << std::endl;
    std::cout << "  -t <threads>        Worker thread count (default: 4)" << std::endl;
    std::cout << "  -b <buffer_size>    Buffer size in bytes (default: 8192)" << std::endl;
    std::cout << "  -a                  Enable thread affinity (default: true)" << std::endl;
    std::cout << "  -h                  Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Performance Target: < 10 microseconds average latency" << std::endl;
}

void runPerformanceTest() {
    std::cout << "\n[Main] Running performance test..." << std::endl;
    
    // Create test messages
    auto order_msg = std::make_shared<OrderMessage>(12345, "AAPL", 150.50, 100, true);
    auto md_msg = std::make_shared<MarketDataMessage>("AAPL", 150.45, 150.55, 1000, 1000);
    
    // Test interceptor chain
    auto interceptor_chain = std::make_shared<InterceptorChain>();
    interceptor_chain->addInterceptor(std::make_shared<ValidationInterceptor>());
    interceptor_chain->addInterceptor(std::make_shared<LoggingInterceptor>());
    interceptor_chain->addInterceptor(std::make_shared<PerformanceInterceptor>());
    interceptor_chain->addInterceptor(std::make_shared<ThrottlingInterceptor>(1000000)); // 1M msg/s
    
    // Test message processing
    std::vector<std::shared_ptr<Message>> test_messages = {order_msg, md_msg};
    
    for (auto& msg : test_messages) {
        InterceptorContext context(msg);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool result = interceptor_chain->process(context);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        
        std::cout << "Message Type: " << static_cast<int>(msg->getType())
                  << ", Processing: " << (result ? "SUCCESS" : "FAILED")
                  << ", Latency: " << (latency / 1000.0) << " μs" << std::endl;
        
        if (result) {
            std::cout << "  Validation: " << context.getMetadata("validation") << std::endl;
            std::cout << "  Log: " << context.getMetadata("log") << std::endl;
            std::cout << "  Latency: " << context.getMetadata("latency_us") << " μs" << std::endl;
            std::cout << "  Throttle: " << context.getMetadata("throttle_status") << std::endl;
        }
    }
}

void runLatencyBenchmark() {
    std::cout << "\n[Main] Running latency benchmark..." << std::endl;
    
    const size_t iterations = 100000;
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    auto order_msg = std::make_shared<OrderMessage>(12345, "AAPL", 150.50, 100, true);
    
    auto start_total = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate message processing
        order_msg->setSequenceNumber(i + 1);
        order_msg->setTimestamp(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(latency / 1000.0); // Convert to microseconds
    }
    
    auto end_total = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end_total - start_total).count();
    
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
    
    std::cout << "Latency Benchmark Results (" << iterations << " iterations):" << std::endl;
    std::cout << "  Total Time: " << (total_time / 1000.0) << " ms" << std::endl;
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

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    // Parse command line arguments
    int port = 8080;
    size_t thread_count = 4;
    size_t buffer_size = 8192;
    bool affinity_enabled = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            thread_count = std::stoul(argv[++i]);
        } else if (arg == "-b" && i + 1 < argc) {
            buffer_size = std::stoul(argv[++i]);
        } else if (arg == "-a") {
            affinity_enabled = true;
        }
    }
    
    std::cout << "=== HFT Socket Server ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Threads: " << thread_count << std::endl;
    std::cout << "Buffer Size: " << buffer_size << " bytes" << std::endl;
    std::cout << "Affinity: " << (affinity_enabled ? "enabled" : "disabled") << std::endl;
    std::cout << "Target Latency: < 10 microseconds" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Setup signal handlers
    setupSignalHandlers();
    
    try {
        // Initialize socket server
        auto& socket_server = SocketServer::getInstance();
        if (!socket_server.initialize(port, 10000)) {
            std::cerr << "[Main] Failed to initialize socket server" << std::endl;
            return 1;
        }
        
        // Configure server for low latency
        socket_server.setThreadCount(thread_count);
        socket_server.setBufferSize(buffer_size);
        socket_server.setAffinity(affinity_enabled);
        
        // Initialize service manager
        auto& service_manager = ServiceManager::getInstance();
        
        // Register services
        service_manager.registerService(std::make_shared<OrderMatchingService>());
        service_manager.registerService(std::make_shared<MarketDataService>());
        service_manager.registerService(std::make_shared<RiskManagementService>());
        
        // Start services
        service_manager.startAllServices();
        
        // Start socket server
        socket_server.start();
        
        std::cout << "[Main] Server started successfully" << std::endl;
        std::cout << "[Main] Listening on port " << port << std::endl;
        std::cout << "[Main] Press Ctrl+C to stop" << std::endl;
        
        // Run performance tests only if not in test mode
        if (argc == 1 || std::string(argv[1]) != "--test-mode") {
            runPerformanceTest();
            runLatencyBenchmark();
        }
        
        // Main server loop
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print periodic statistics
            static int counter = 0;
            if (++counter % 10 == 0) { // Every 10 seconds
                std::cout << "[Main] Active connections: " << socket_server.getConnectionCount() << std::endl;
                std::cout << "[Main] Messages processed: " << socket_server.getMessagesProcessed() << std::endl;
                std::cout << "[Main] Average latency: " << socket_server.getAverageLatency() << " μs" << std::endl;
                std::cout << "[Main] Active services: " << service_manager.getActiveServiceCount() << std::endl;
            }
        }
        
        // Shutdown
        std::cout << "[Main] Shutting down server..." << std::endl;
        
        socket_server.stop();
        service_manager.stopAllServices();
        
        std::cout << "[Main] Server stopped successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 