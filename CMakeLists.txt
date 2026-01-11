cmake_minimum_required(VERSION 3.20)
project(APFramework VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform checks
if(NOT WIN32)
    message(WARNING "This project is currently Windows-only (UE4SS limitation)")
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Compile definitions for apclientpp dependencies
add_compile_definitions(
    ASIO_STANDALONE
    WSWRAP_NO_SSL
    WSWRAP_NO_COMPRESSION
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
    _WEBSOCKETPP_CPP11_MEMORY_
    _WEBSOCKETPP_CPP11_CHRONO_
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
    ASIO_NO_WIN32_LEAN_AND_MEAN
    SOL_ALL_SAFETIES_ON=1
)

# Third-party dependencies
add_subdirectory(third_party/lua-5.4.7)

# Find or add nlohmann_json
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/json.hpp)
    message(FATAL_ERROR "nlohmann/json not found in third_party/nlohmann/")
endif()

# Find or add sol2
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include/sol/sol.hpp)
    message(FATAL_ERROR "sol2 not found in third_party/sol2/")
endif()

# Add apclientpp
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/apclientpp/CMakeLists.txt)
    message(FATAL_ERROR "apclientpp submodule not initialized. Run: git submodule update --init --recursive")
endif()
add_subdirectory(third_party/apclientpp)

# Main projects
add_subdirectory(APFrameworkCore)
add_subdirectory(APClientLib)

# Testing
option(BUILD_TESTS "Build unit and integration tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Post-build packaging is handled by individual target CMakeLists.txt files
# See APFrameworkCore/CMakeLists.txt and APClientLib/CMakeLists.txt