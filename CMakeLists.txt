cmake_minimum_required(VERSION 3.20)
project(APFramework VERSION 2.0.0 LANGUAGES CXX)

# Set C++17 standard globally
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

# Add third-party dependencies
# apclientpp and its dependencies are header-only
set(APCLIENTPP_BUILD_TESTING OFF CACHE BOOL "Disable apclientpp tests" FORCE)
add_subdirectory(third_party/apclientpp)

# Add subdirectories
add_subdirectory(src/framework_core)
add_subdirectory(src/client_lib)

# Installation rules
install(TARGETS APFrameworkCore APClientLib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/framework_core/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY src/client_lib/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
