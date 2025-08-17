#!/bin/bash

# HFT Server Test Scenario Script
# This script tests the complete server-client interaction

set -e

echo "=== HFT Server Test Scenario ==="
echo "Testing server-client interaction and performance"
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

cd build

# Test 1: Basic connectivity test
echo "=== Test 1: Basic Connectivity ==="
print_status "Starting server in background (test mode)..."
./hft_server --test-mode -t 2 -b 4096 > server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    print_warning "Server failed to start"
    cat server.log
    exit 1
fi

print_success "Server started (PID: $SERVER_PID)"

# Wait a bit more for server to be ready
sleep 3

print_status "Testing basic connectivity..."
if ./test_client 127.0.0.1 8080 > client_test1.log 2>&1; then
    print_success "Basic connectivity test passed"
else
    print_warning "Basic connectivity test failed"
    cat client_test1.log
fi

echo ""

# Test 2: Latency test with small message count
echo "=== Test 2: Latency Test (100 messages) ==="
print_status "Running latency test..."
if ./test_client 127.0.0.1 8080 -l 100 > client_test2.log 2>&1; then
    print_success "Latency test completed"
    
    # Check results
    if grep -q "Target achieved: Average latency < 10 μs" client_test2.log; then
        print_success "Latency target achieved!"
    else
        print_warning "Latency target missed"
    fi
    
    # Show summary
    echo "Latency Test Summary:"
    grep -E "(Messages Sent|Average Latency|Target achieved|Target missed)" client_test2.log
else
    print_warning "Latency test failed"
    cat client_test2.log
fi

echo ""

# Test 3: Throughput test
echo "=== Test 3: Throughput Test (1000 messages over 2 seconds) ==="
print_status "Running throughput test..."
if ./test_client 127.0.0.1 8080 -t 1000 2 > client_test3.log 2>&1; then
    print_success "Throughput test completed"
    
    # Show summary
    echo "Throughput Test Summary:"
    grep -E "(Messages Sent|Actual Rate|Target Rate|Efficiency)" client_test3.log
else
    print_warning "Throughput test failed"
    cat client_test3.log
fi

echo ""

# Test 4: Stress test
echo "=== Test 4: Stress Test (5000 messages over 3 seconds) ==="
print_status "Running stress test..."
if ./test_client 127.0.0.1 8080 -s 5000 3 > client_test4.log 2>&1; then
    print_success "Stress test completed"
    
    # Show summary
    echo "Stress Test Summary:"
    grep -E "(Messages Sent|Messages Failed|Success Rate|Actual Rate)" client_test4.log
else
    print_warning "Stress test failed"
    cat client_test4.log
fi

echo ""

# Stop server
print_status "Stopping server..."
kill -TERM $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

# Show server logs
echo "=== Server Logs ==="
if [ -f "server.log" ]; then
    cat server.log
else
    echo "No server logs found"
fi

echo ""

# Summary
echo "=== Test Summary ==="
print_success "All tests completed"

# Check for any issues
if [ -f "client_test1.log" ] && grep -q "Failed\|ERROR" client_test1.log; then
    print_warning "Connectivity issues detected"
fi

if [ -f "client_test2.log" ] && grep -q "Target missed" client_test2.log; then
    print_warning "Latency target not achieved"
fi

if [ -f "client_test3.log" ] && grep -q "Failed\|ERROR" client_test3.log; then
    print_warning "Throughput test issues detected"
fi

if [ -f "client_test4.log" ] && grep -q "Failed\|ERROR" client_test4.log; then
    print_warning "Stress test issues detected"
fi

echo ""
echo "=== Performance Results ==="
echo "Check individual test logs for detailed results:"
echo "  - client_test1.log: Basic connectivity"
echo "  - client_test2.log: Latency test"
echo "  - client_test3.log: Throughput test"
echo "  - client_test4.log: Stress test"
echo "  - server.log: Server operation logs"
echo ""
echo "Target: < 10 microseconds average latency" 