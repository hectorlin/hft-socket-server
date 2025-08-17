#!/bin/bash

# HFT Socket Server Build Script
# This script builds the HFT server with optimized settings for low latency

set -e

echo "=== Building HFT Socket Server ==="

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -ffast-math -funroll-loops -fomit-frame-pointer -DNDEBUG"

# Build
echo "Building..."
make -j$(nproc)

echo "=== Build completed successfully ==="
echo ""
echo "Executables created:"
echo "  - hft_server: Main HFT server"
echo "  - test_client: Test client for performance testing"
echo ""
echo "To run the server:"
echo "  ./hft_server"
echo ""
echo "To run performance tests:"
echo "  ./test_client 127.0.0.1 8080 -l 10000"
echo "  ./test_client 127.0.0.1 8080 -t 100000 10"
echo ""
echo "Target: < 10 microseconds average latency" 