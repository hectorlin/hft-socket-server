cmake_minimum_required(VERSION 3.10)
project(HFTSocketServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for low latency
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

# Find required packages
find_package(Threads REQUIRED)

# Add executables
add_executable(hft_server 
    src/main.cpp
    src/socket_server.cpp
    src/singleton.cpp
    src/service_manager.cpp
    src/interceptor.cpp
    src/message.cpp
)

add_executable(test_client
    src/test_client.cpp
    src/message.cpp
)

# Include directories
target_include_directories(hft_server PRIVATE include)

# Link libraries
target_link_libraries(hft_server PRIVATE Threads::Threads)
target_link_libraries(test_client PRIVATE Threads::Threads)

# Set compiler flags for low latency
target_compile_options(hft_server PRIVATE 
    -ffast-math 
    -funroll-loops 
    -fomit-frame-pointer
    -DNDEBUG
)

target_compile_options(test_client PRIVATE 
    -ffast-math 
    -funroll-loops 
    -fomit-frame-pointer
    -DNDEBUG
) 