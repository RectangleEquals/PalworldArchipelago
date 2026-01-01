cmake_minimum_required(VERSION 3.20)
project(APClientLib VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/ap_client_lib.cpp
    src/ipc_client.cpp
)

set(HEADERS
    include/ap_client_lib.h
    include/ipc_client.h
)

# Build shared library (DLL)
add_library(APClientLib SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APClientLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/framework_core/include
)

# Link libraries
target_link_libraries(APClientLib
    PRIVATE ws2_32  # Windows sockets for Named Pipes
)

# Windows-specific definitions
if(WIN32)
    target_compile_definitions(APClientLib PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601  # Windows 7+
    )
endif()

# Set output directory
set_target_properties(APClientLib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Export symbols for DLL
if(WIN32)
    target_compile_definitions(APClientLib PRIVATE
        APCLIENTLIB_EXPORTS
    )
endif()
