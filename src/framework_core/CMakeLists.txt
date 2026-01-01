cmake_minimum_required(VERSION 3.20)
project(APFrameworkCore VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(nlohmann_json REQUIRED)
find_package(apclientpp REQUIRED)

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

target_include_directories(APFrameworkCore PUBLIC include)
target_link_libraries(APFrameworkCore
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE apclientpp::apclientpp
    PRIVATE ws2_32  # Windows sockets for networking
)

# Set output directory
set_target_properties(APFrameworkCore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
