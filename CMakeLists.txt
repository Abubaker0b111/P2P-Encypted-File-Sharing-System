cmake_minimum_required(VERSION 3.10)
project(EFTS_Project)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Tell CMake where to look for header files
include_directories(include)

# Find Libsodium on the system
find_library(SODIUM_LIB sodium REQUIRED)

# Build the Rendezvous Server
add_executable(bootstrap bootstrap_server.cpp)

# Build the Main Client
# Notice how we easily link all the .cpp files together
add_executable(client 
    src/main.cpp 
    src/rudp_socket.cpp 
    src/crypto_manager.cpp 
    src/nat_traversal.cpp 
    src/file_agent.cpp
)

# Link Libsodium to the client
target_link_libraries(client ${SODIUM_LIB})