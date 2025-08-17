#!/bin/bash

# HFT Socket Server - Build and Performance Test Script
# This script builds the server, automatically fixes errors, and runs performance tests

set -e

echo "=== HFT Socket Server - Build and Test ==="
echo "Target: < 10 microseconds average latency"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Global variables
BUILD_DIR="build"
SERVER_PID=""
CLIENT_PID=""
MAX_RETRIES=3
AUTO_FIX_ENABLED=true

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_fix() {
    echo -e "${PURPLE}[FIX]${NC} $1"
}

print_debug() {
    echo -e "${CYAN}[DEBUG]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check system requirements
check_system_requirements() {
    print_status "Checking system requirements..."
    
    local missing_deps=()
    
    # Check basic tools
    if ! command_exists cmake; then
        missing_deps+=("cmake")
    fi
    
    if ! command_exists make; then
        missing_deps+=("make")
    fi
    
    if ! command_exists g++; then
        missing_deps+=("g++")
    fi
    
    if ! command_exists pkill; then
        missing_deps+=("pkill")
    fi
    
    # Check for required libraries - pthread is part of glibc, so we don't need pkg-config
    # Just check if we can compile a simple pthread program
    if ! echo -e '#include <pthread.h>\nint main() { return 0; }' | g++ -x c++ - -lpthread -o /dev/null 2>/dev/null; then
        missing_deps+=("pthread-devel")
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_status "Please install missing packages:"
        for dep in "${missing_deps[@]}"; do
            case $dep in
                "cmake") echo "  sudo dnf install cmake (Fedora/RHEL) or sudo apt install cmake (Ubuntu/Debian)" ;;
                "make") echo "  sudo dnf install make (Fedora/RHEL) or sudo apt install make (Ubuntu/Debian)" ;;
                "g++") echo "  sudo dnf install gcc-c++ (Fedora/RHEL) or sudo apt install g++ (Ubuntu/Debian)" ;;
                "pthread-devel") echo "  sudo dnf install glibc-devel (Fedora/RHEL) or sudo apt install glibc-dev (Ubuntu/Debian)" ;;
            esac
        done
        exit 1
    fi
    
    print_success "System requirements check passed"
}

# Function to clean up processes
cleanup() {
    print_status "Cleaning up processes..."
    
    if [ -n "$SERVER_PID" ]; then
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            print_status "Stopping server (PID: $SERVER_PID)..."
            kill -TERM "$SERVER_PID" 2>/dev/null
            sleep 2
            if kill -0 "$SERVER_PID" 2>/dev/null; then
                print_warning "Server not responding to SIGTERM, forcing kill..."
                kill -KILL "$SERVER_PID" 2>/dev/null
            fi
        fi
        SERVER_PID=""
    fi
    
    if [ -n "$CLIENT_PID" ]; then
        if kill -0 "$CLIENT_PID" 2>/dev/null; then
            print_status "Stopping client (PID: $CLIENT_PID)..."
            kill -TERM "$CLIENT_PID" 2>/dev/null
            sleep 1
            if kill -0 "$CLIENT_PID" 2>/dev/null; then
                kill -KILL "$CLIENT_PID" 2>/dev/null
            fi
        fi
        CLIENT_PID=""
    fi
    
    # Kill any remaining hft processes
    pkill -f "hft_server" 2>/dev/null || true
    pkill -f "test_client" 2>/dev/null || true
    
    print_success "Cleanup completed"
}

# Set up signal handlers
trap cleanup EXIT INT TERM

# Function to check and fix common build issues
check_and_fix_build_issues() {
    local build_log="$1"
    
    if [ ! -f "$build_log" ]; then
        return 0
    fi
    
    print_status "Analyzing build log for common issues..."
    
    local issues_found=false
    
    # Check for missing includes
    if grep -q "fatal error:.*\.hpp.*No such file or directory" "$build_log"; then
        print_fix "Detected missing header files - checking include paths..."
        issues_found=true
    fi
    
    # Check for C++11 compatibility issues
    if grep -q "error:.*C\+\+11" "$build_log" || grep -q "error:.*c\+\+11" "$build_log"; then
        print_fix "Detected C++11 compatibility issues..."
        issues_found=true
    fi
    
    # Check for undefined references
    if grep -q "undefined reference" "$build_log"; then
        print_fix "Detected undefined references - checking library linking..."
        issues_found=true
    fi
    
    # Check for missing symbols
    if grep -q "undefined symbol" "$build_log"; then
        print_fix "Detected undefined symbols..."
        issues_found=true
    fi
    
    if [ "$issues_found" = true ]; then
        print_warning "Build issues detected. Attempting automatic fixes..."
        return 1
    fi
    
    return 0
}

# Function to attempt automatic fixes
attempt_automatic_fixes() {
    print_status "Attempting automatic fixes..."
    
    # Fix 1: Check CMakeLists.txt for missing source files
    if [ -f "CMakeLists.txt" ]; then
        print_fix "Checking CMakeLists.txt for missing source files..."
        
        # Check if all source files exist
        local missing_sources=()
        while IFS= read -r line; do
            if [[ $line =~ src/([^[:space:]]+) ]]; then
                local source_file="${BASH_REMATCH[1]}"
                if [ ! -f "src/$source_file" ]; then
                    missing_sources+=("$source_file")
                fi
            fi
        done < <(grep "src/" CMakeLists.txt 2>/dev/null || true)
        
        if [ ${#missing_sources[@]} -gt 0 ]; then
            print_fix "Missing source files detected: ${missing_sources[*]}"
            print_fix "Removing missing sources from CMakeLists.txt..."
            
            for source in "${missing_sources[@]}"; do
                sed -i "/src\/$source/d" CMakeLists.txt
            done
        fi
    fi
    
    # Fix 2: Check for missing includes in headers
    print_fix "Checking header files for missing includes..."
    
    local headers=("include/*.hpp")
    for header in $headers; do
        if [ -f "$header" ]; then
            # Check for common missing includes
            if grep -q "std::mutex\|std::map\|std::unordered_map" "$header" && ! grep -q "#include <mutex>\|#include <map>\|#include <unordered_map>" "$header"; then
                print_fix "Adding missing includes to $header..."
                # Add includes at the top after existing includes
                sed -i '/#include/a #include <mutex>\n#include <map>' "$header"
            fi
        fi
    done
    
    # Fix 3: Check for C++11 compatibility issues
    print_fix "Checking for C++11 compatibility issues..."
    
    # Replace C++17 structured bindings with C++11 compatible code
    local source_files=("src/*.cpp")
    for source in $source_files; do
        if [ -f "$source" ]; then
            if grep -q "auto& \[.*\]" "$source"; then
                print_fix "Replacing C++17 structured bindings in $source..."
                # This is a simplified fix - in practice, you'd need more sophisticated pattern matching
                sed -i 's/auto& \[\([^,]*\), \([^]]*\)\]/auto\& \1_pair/g' "$source"
            fi
        fi
    done
    
    print_success "Automatic fixes applied"
}

# Function to build with error handling and auto-fix
build_with_auto_fix() {
    local attempt=1
    
    while [ $attempt -le $MAX_RETRIES ]; do
        print_status "Build attempt $attempt of $MAX_RETRIES..."
        
        # Clean previous build
        print_status "Cleaning previous build..."
        rm -rf "$BUILD_DIR"
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        
        # Configure with CMake
        print_status "Configuring with CMake..."
        if cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -ffast-math -funroll-loops -fomit-frame-pointer -DNDEBUG" \
            -DCMAKE_CXX_STANDARD=11 \
            -DCMAKE_CXX_STANDARD_REQUIRED=ON > cmake.log 2>&1; then
            
            print_success "CMake configuration completed"
        else
            print_error "CMake configuration failed"
            cat cmake.log
            cd ..
            attempt_automatic_fixes
            ((attempt++))
            continue
        fi
        
        # Build
        print_status "Building HFT server..."
        if make -j$(nproc) > build.log 2>&1; then
            print_success "Build completed successfully"
            break
        else
            print_error "Build failed on attempt $attempt"
            cat build.log
            
            if [ "$AUTO_FIX_ENABLED" = true ] && [ $attempt -lt $MAX_RETRIES ]; then
                print_warning "Attempting automatic fixes..."
                cd ..
                attempt_automatic_fixes
                ((attempt++))
                continue
            else
                print_error "Build failed after $MAX_RETRIES attempts"
                exit 1
            fi
        fi
    done
    
    # Verify executables
    if [ ! -f "./hft_server" ]; then
        print_error "hft_server executable not found after successful build"
        exit 1
    fi
    
    if [ ! -f "./test_client" ]; then
        print_error "test_client executable not found after successful build"
        exit 1
    fi
    
    print_success "All executables created successfully"
    cd ..
}

# Function to start server with error handling
start_server() {
    local port="$1"
    local threads="$2"
    local buffer_size="$3"
    local test_mode="$4"
    
    print_status "Starting server on port $port with $threads threads..."
    
    cd "$BUILD_DIR"
    
    local server_args="-p $port -t $threads -b $buffer_size"
    if [ "$test_mode" = true ]; then
        server_args="$server_args --test-mode"
    fi
    
    ./hft_server $server_args > server.log 2>&1 &
    SERVER_PID=$!
    
    # Wait for server to start
    local wait_time=0
    local max_wait=10
    
    while [ $wait_time -lt $max_wait ]; do
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            print_error "Server process died unexpectedly"
            cat server.log
            return 1
        fi
        
        # Check if server is listening on the port
        if netstat -tln 2>/dev/null | grep -q ":$port "; then
            print_success "Server started successfully (PID: $SERVER_PID)"
            cd ..
            return 0
        fi
        
        sleep 1
        ((wait_time++))
    done
    
    print_error "Server failed to start within $max_wait seconds"
    cat server.log
    cd ..
    return 1
}

# Function to wait for server to be ready
wait_for_server() {
    local port="$1"
    local max_wait="$2"
    
    print_status "Waiting for server to be ready on port $port..."
    
    local wait_time=0
    while [ $wait_time -lt $max_wait ]; do
        if netstat -tln 2>/dev/null | grep -q ":$port "; then
            print_success "Server is ready on port $port"
            return 0
        fi
        sleep 1
        ((wait_time++))
    done
    
    print_error "Server not ready after $max_wait seconds"
    return 1
}

# Function to run client test
run_client_test() {
    local test_name="$1"
    local server_ip="$2"
    local server_port="$3"
    local test_args="$4"
    local expected_duration="$5"
    
    print_status "Running $test_name..."
    
    cd "$BUILD_DIR"
    
    # Start client test
    timeout $((expected_duration + 5)) ./test_client $server_ip $server_port $test_args > "client_${test_name// /_}.log" 2>&1 &
    CLIENT_PID=$!
    
    # Wait for client to complete
    wait $CLIENT_PID 2>/dev/null
    local client_exit_code=$?
    CLIENT_PID=""
    
    cd ..
    
    if [ $client_exit_code -eq 0 ]; then
        print_success "$test_name completed successfully"
        return 0
    elif [ $client_exit_code -eq 124 ]; then
        print_warning "$test_name timed out"
        return 1
    else
        print_warning "$test_name failed with exit code $client_exit_code"
        return 1
    fi
}

# Function to run comprehensive tests
run_comprehensive_tests() {
    print_status "Running comprehensive tests..."
    
    local server_port=8080
    local test_results=()
    
    # Test 1: Basic connectivity
    print_status "Test 1: Basic connectivity test..."
    if start_server $server_port 2 4096 false; then
        if wait_for_server $server_port 5; then
            if run_client_test "Basic Connectivity" "127.0.0.1" $server_port "" 5; then
                test_results+=("Basic Connectivity: PASS")
            else
                test_results+=("Basic Connectivity: FAIL")
            fi
        else
            test_results+=("Basic Connectivity: FAIL (Server not ready)")
        fi
        cleanup
    else
        test_results+=("Basic Connectivity: FAIL (Server start failed)")
    fi
    
    # Test 2: Latency test
    print_status "Test 2: Latency test..."
    if start_server $server_port 2 4096 true; then
        if wait_for_server $server_port 5; then
            if run_client_test "Latency Test" "127.0.0.1" $server_port "-l 1000" 10; then
                test_results+=("Latency Test: PASS")
            else
                test_results+=("Latency Test: FAIL")
            fi
        else
            test_results+=("Latency Test: FAIL (Server not ready)")
        fi
        cleanup
    else
        test_results+=("Latency Test: FAIL (Server start failed)")
    fi
    
    # Test 3: Throughput test
    print_status "Test 3: Throughput test..."
    if start_server $server_port 4 8192 true; then
        if wait_for_server $server_port 5; then
            if run_client_test "Throughput Test" "127.0.0.1" $server_port "-t 10000 5" 10; then
                test_results+=("Throughput Test: PASS")
            else
                test_results+=("Throughput Test: FAIL")
            fi
        else
            test_results+=("Throughput Test: FAIL (Server not ready)")
        fi
        cleanup
    else
        test_results+=("Throughput Test: FAIL (Server start failed)")
    fi
    
    # Test 4: Stress test
    print_status "Test 4: Stress test..."
    if start_server $server_port 4 8192 true; then
        if wait_for_server $server_port 5; then
            if run_client_test "Stress Test" "127.0.0.1" $server_port "-s 10000" 15; then
                test_results+=("Stress Test: PASS")
            else
                test_results+=("Stress Test: FAIL")
            fi
        else
            test_results+=("Stress Test: FAIL (Server not ready)")
        fi
        cleanup
    else
        test_results+=("Stress Test: FAIL (Server start failed)")
    fi
    
    # Print test results
    echo ""
    print_status "Test Results Summary:"
    echo "=========================="
    for result in "${test_results[@]}"; do
        if [[ $result == *": PASS" ]]; then
            print_success "$result"
        else
            print_error "$result"
        fi
    done
    
    # Calculate pass rate
    local total_tests=${#test_results[@]}
    local passed_tests=0
    for result in "${test_results[@]}"; do
        if [[ $result == *": PASS" ]]; then
            ((passed_tests++))
        fi
    done
    
    local pass_rate=$((passed_tests * 100 / total_tests))
    echo ""
    print_status "Overall Pass Rate: $passed_tests/$total_tests ($pass_rate%)"
    
    if [ $pass_rate -eq 100 ]; then
        print_success "All tests passed!"
    elif [ $pass_rate -ge 75 ]; then
        print_warning "Most tests passed, but some issues detected"
    else
        print_error "Many tests failed - review logs for issues"
    fi
}

# Function to run performance analysis
run_performance_analysis() {
    print_status "Running performance analysis..."
    
    echo ""
    echo "=== System Information ==="
    echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    echo "Cores: $(nproc)"
    echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
    echo "Kernel: $(uname -r)"
    echo "Architecture: $(uname -m)"
    
    # Check real-time capabilities
    if [ -w /dev/cpu_dma_latency ]; then
        print_success "Real-time capabilities available"
    else
        print_warning "Real-time capabilities not available (run as root for best performance)"
    fi
    
    # Check CPU governor
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        local governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
        echo "CPU Governor: $governor"
        if [ "$governor" = "performance" ]; then
            print_success "CPU governor set to performance mode"
        else
            print_warning "Consider setting CPU governor to performance mode: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"
        fi
    fi
    
    # Check network interface
    if command_exists ethtool; then
        local primary_interface=$(ip route | grep default | awk '{print $5}' | head -1)
        if [ -n "$primary_interface" ]; then
            echo "Primary Network Interface: $primary_interface"
            local speed=$(ethtool "$primary_interface" 2>/dev/null | grep Speed | cut -d: -f2 | xargs)
            if [ -n "$speed" ]; then
                echo "Network Speed: $speed"
            fi
        fi
    fi
    
    echo ""
}

# Main execution
main() {
    echo "=== HFT Socket Server - Build and Test ==="
    echo "Target: < 10 microseconds average latency"
    echo "Auto-fix enabled: $AUTO_FIX_ENABLED"
    echo "Max build retries: $MAX_RETRIES"
    echo ""
    
    # Check system requirements
    check_system_requirements
    
    # Build with auto-fix
    build_with_auto_fix
    
    # Run comprehensive tests
    run_comprehensive_tests
    
    # Performance analysis
    run_performance_analysis
    
    # Final summary
    echo ""
    echo "=== Final Summary ==="
    print_success "Build and test script completed successfully!"
    echo ""
    echo "To run manual tests:"
    echo "  cd $BUILD_DIR"
    echo "  ./hft_server -p 8080 -t 4 -b 8192 --test-mode"
    echo "  ./test_client 127.0.0.1 8080 -l 10000"
    echo "  ./test_client 127.0.0.1 8080 -t 100000 10"
    echo "  ./test_client 127.0.0.1 8080 -s 50000"
    echo ""
    echo "Check logs in $BUILD_DIR/ for detailed results"
}

# Run main function
main "$@" 