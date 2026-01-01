cmake_minimum_required(VERSION 3.20)
project(APFrameworkCore VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows definitions for asio and apclientpp
add_compile_definitions(
    ASIO_STANDALONE                      # Use standalone asio (not boost::asio)
    WSWRAP_NO_SSL                        # Disable SSL in wswrap (no OpenSSL dependency)
    WSWRAP_NO_COMPRESSION                # Disable compression in wswrap (no zlib dependency)
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_    # Use C++11 random device
    _WEBSOCKETPP_CPP11_THREAD_           # Use C++11 threading
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_      # Use C++11 type traits
    _WEBSOCKETPP_CPP11_FUNCTIONAL_       # Use C++11 functional
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_     # Use C++11 system_error
    _WEBSOCKETPP_CPP11_MEMORY_           # Use C++11 smart pointers
    _WEBSOCKETPP_CPP11_CHRONO_           # Use C++11 chrono
    _WIN32_WINNT=0x0601                  # Windows 7+ for networking APIs
    WIN32_LEAN_AND_MEAN                  # Reduce Windows.h bloat
    ASIO_NO_WIN32_LEAN_AND_MEAN          # But allow asio to include what it needs
)

# Source files
set(SOURCES
    src/ap_client.cpp
    src/ipc_server.cpp
    src/message_router.cpp
    src/mod_registry.cpp
    src/polling_thread.cpp
    src/config_manager.cpp
    src/capabilities_generator.cpp
    src/framework_core.cpp
    src/ffi_bindings.cpp
    src/main.cpp
)

# Headers
set(HEADERS
    include/ap_client.h
    include/ipc_server.h
    include/message_queue.h
    include/message_router.h
    include/mod_registry.h
    include/polling_thread.h
    include/config_manager.h
    include/capabilities_generator.h
    include/framework_core.h
    include/ffi_bindings.h
)

# Build DLL
add_library(APFrameworkCore SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APFrameworkCore
    PUBLIC
        include
        include/nlohmann  # nlohmann/json in our include directory
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/apclientpp
        ${CMAKE_SOURCE_DIR}/third_party/wswrap/include
        ${CMAKE_SOURCE_DIR}/third_party/asio/asio/include
        ${CMAKE_SOURCE_DIR}/third_party/websocketpp
        ${CMAKE_SOURCE_DIR}/third_party/valijson/include
)

# Link libraries
target_link_libraries(APFrameworkCore
    PRIVATE apclientpp  # Header-only interface library
    PRIVATE ws2_32      # Windows sockets
    PRIVATE crypt32     # Windows crypto (for SSL)
)

# Set output directory
set_target_properties(APFrameworkCore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
