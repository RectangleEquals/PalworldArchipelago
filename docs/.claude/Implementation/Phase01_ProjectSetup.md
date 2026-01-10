# Phase 01: Project Structure & Dependencies

**Status**: 🔴 Not Started

---

## Overview

Establish the foundational project structure, CMake build system, and integrate all third-party dependencies required for the Palworld Archipelago Framework.

**Goals**:
- Clean, maintainable project structure
- Properly configured CMake build system
- All third-party dependencies integrated and verified
- Successful build on Windows with MSVC (Visual Studio 2022)

---

## Prerequisites

### Development Environment
- **OS**: Windows
- **CMake**: 3.20 or higher
- **Compiler**: MSVC 2019+ (Visual Studio 2022 recommended)
- **Git**: For submodule management

### Required Third-Party Libraries
1. **apclientpp** - Archipelago WebSocket client
   - Location: `third_party/apclientpp` (git submodule)
   - Dependencies (all in `third_party/` root):
     - **wswrap** - `third_party/wswrap` (git submodule)
     - **asio** - `third_party/asio` (auto-fetched by CMake, v1.12)
     - **websocketpp** - `third_party/websocketpp` (git submodule)
     - **valijson** - `third_party/valijson` (git submodule)
2. **sol2** - Lua C++ bindings (header-only)
   - Location: `third_party/sol2/` (direct include, NOT submodule)
   - Files: `third_party/sol2/sol.hpp` and supporting headers
3. **nlohmann/json** - JSON parsing (header-only)
   - Location: `third_party/nlohmann/`
   - Files: `third_party/nlohmann/json.hpp`
4. **lua** - Lua 5.4.7 runtime
   - Location: `third_party/lua-5.4.7/`
   - Build as static library

---

## Project Structure

### Target Directory Layout
```
ipc_design/
├── CMakeLists.txt                          # Root build configuration
├── README.md
├── LICENSE
├── .gitignore
├── .gitmodules
│
├── APFrameworkCore/                        # Main framework DLL
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── ap_manager.h                    # Main orchestrator
│   │   ├── ap_client.h                     # apclientpp wrapper
│   │   ├── ap_ipc_server.h                 # Named Pipes server
│   │   ├── ap_config.h                     # Configuration management
│   │   ├── ap_logger.h                     # Logging system
│   │   ├── ap_debug_log.h                  # Debug utilities
│   │   ├── ap_mod_registry.h               # Mod discovery/tracking
│   │   ├── ap_capabilities_generator.h     # Capability aggregation
│   │   ├── ap_message_router.h             # Message routing
│   │   ├── ap_polling_thread.h             # Polling thread
│   │   └── ap_types.h                      # Common types/enums
│   │
│   └── src/
│       ├── ap_manager.cpp
│       ├── ap_client.cpp
│       ├── ap_ipc_server.cpp
│       ├── ap_config.cpp
│       ├── ap_logger.cpp
│       ├── ap_debug_log.cpp
│       ├── ap_mod_registry.cpp
│       ├── ap_capabilities_generator.cpp
│       ├── ap_message_router.cpp
│       ├── ap_polling_thread.cpp
│       ├── lua_bindings.cpp                # sol2 bindings
│       └── main.cpp                        # DLL entry point
│
├── APClientLib/                            # Lightweight client library
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── ap_client_lib.h                 # Main API header
│   │   └── ap_ipc_client.h                 # Named Pipes client
│   │
│   └── src/
│       ├── ap_client_lib.cpp
│       ├── ap_ipc_client.cpp
│       └── lua_bindings.cpp                # sol2 bindings
│
├── third_party/                            # External dependencies
│   ├── apclientpp/                         # Git submodule
│   ├── wswrap/                             # Git submodule (apclientpp dep)
│   ├── websocketpp/                        # Git submodule (apclientpp dep)
│   ├── valijson/                           # Git submodule (apclientpp dep)
│   ├── asio/                               # Auto-fetched by CMake (apclientpp dep)
│   ├── sol2/                               # Header-only (direct include, NOT submodule)
│   │   ├── sol.hpp
│   │   └── (other sol2 headers)
│   ├── nlohmann/                           # Header-only (direct include)
│   │   ├── json.hpp
│   │   └── json_fwd.hpp
│   ├── lua-5.4.7/                          # Lua runtime (static lib)
│   │   ├── CMakeLists.txt
│   │   └── src/
│   └── lua/                                # Lua mod dependencies
│       ├── lunajson.lua
│       └── lunajson/
│           ├── decoder.lua
│           ├── encoder.lua
│           └── sax.lua
│
├── Mods/                                   # UE4SS mods
│   ├── APFrameworkMod/                     # Main framework Lua mod
│   │   ├── enabled.txt
│   │   ├── Scripts/
│   │   │   ├── main.lua                    # UE4SS entry point
│   │   │   ├── APFramework.lua             # High-level Lua wrapper
│   │   │   ├── APFrameworkCore.dll         # C++ lib with Lua bindings
│   │   │   ├── APClient.lua                # Lua wrapper for APClientLib
│   │   │   ├── APClientLib.dll             # C++ lib with Lua bindings
│   │   │   ├── lunajson.lua                # JSON for Lua (main module)
│   │   │   └── lunajson/                   # JSON for Lua (submodules)
│   │   │       ├── decoder.lua
│   │   │       ├── encoder.lua
│   │   │       └── sax.lua
│   │   └── dlls/                           # (empty - for UE4SS C++ mods only)
│   │
│   └── ExampleClientMod/                   # Example AP-enabled mod
│       ├── enabled.txt
│       ├── AP_Config.json                  # Mod capabilities
│       ├── Scripts/
│       │   ├── main.lua
│       │   ├── APClient.lua                # Lua wrapper for APClientLib
│       │   ├── APClientLib.dll             # C++ lib with Lua bindings
│       │   ├── lunajson.lua                # JSON for Lua (main module)
│       │   └── lunajson/                   # JSON for Lua (submodules)
│       │       ├── decoder.lua
│       │       ├── encoder.lua
│       │       └── sax.lua
│       └── dlls/                           # (empty - for UE4SS C++ mods only)
│
├── docs/
│   └── .claude/
│       ├── ARCHITECTURE.md
│       ├── ARCHITECTURE_REVIEW.md
│       ├── ImplementationPlan.md
│       ├── Implementation/
│       │   ├── Phase01_ProjectSetup.md     # This file
│       │   ├── Phase02_CoreInfrastructure.md
│       │   └── ...
│       └── Design.md
│
└── tests/                                  # Unit and integration tests
    ├── CMakeLists.txt
    ├── unit/
    │   ├── test_config.cpp
    │   ├── test_logger.cpp
    │   └── ...
    └── integration/
        ├── test_ipc.cpp
        ├── test_lifecycle.cpp
        └── ...
```

---

## Implementation Steps

### Step 1: Root CMakeLists.txt

Create the root build configuration with proper dependency management.

**File**: `CMakeLists.txt`

```cmake
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

# Installation rules (DLLs go in Scripts folder for Lua to load)
install(TARGETS APFrameworkCore APClientLib
    RUNTIME DESTINATION Mods/APFrameworkMod/Scripts
    LIBRARY DESTINATION Mods/APFrameworkMod/Scripts
)

# Copy lunajson files during install
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/lunajson.lua
    DESTINATION Mods/APFrameworkMod/Scripts
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/lunajson
    DESTINATION Mods/APFrameworkMod/Scripts
    FILES_MATCHING PATTERN "*.lua"
)
```

**Acceptance Criteria**:
- ✅ CMakeLists.txt exists at project root
- ✅ Sets C++17 standard
- ✅ Configures output directories
- ✅ Adds all third-party dependencies
- ✅ Checks for missing dependencies with clear error messages

---

### Step 2: APFrameworkCore CMakeLists.txt

**File**: `APFrameworkCore/CMakeLists.txt`

```cmake
project(APFrameworkCore)

# Source files
set(SOURCES
    src/ap_manager.cpp
    src/ap_client.cpp
    src/ap_ipc_server.cpp
    src/ap_config.cpp
    src/ap_logger.cpp
    src/ap_debug_log.cpp
    src/ap_mod_registry.cpp
    src/ap_capabilities_generator.cpp
    src/ap_message_router.cpp
    src/ap_polling_thread.cpp
    src/lua_bindings.cpp
    src/main.cpp
)

# Header files (for IDE organization)
set(HEADERS
    include/ap_manager.h
    include/ap_client.h
    include/ap_ipc_server.h
    include/ap_config.h
    include/ap_logger.h
    include/ap_debug_log.h
    include/ap_mod_registry.h
    include/ap_capabilities_generator.h
    include/ap_message_router.h
    include/ap_polling_thread.h
    include/ap_types.h
)

# Create shared library (DLL)
add_library(APFrameworkCore SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APFrameworkCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include
)

# Link libraries
target_link_libraries(APFrameworkCore
    PRIVATE
        apclientpp
        lua_static
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(APFrameworkCore PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7+
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Export symbols for DLL
target_compile_definitions(APFrameworkCore PRIVATE APFRAMEWORK_EXPORTS)

# Set output name
set_target_properties(APFrameworkCore PROPERTIES
    OUTPUT_NAME "APFrameworkCore"
    PREFIX ""
)
```

**Acceptance Criteria**:
- ✅ APFrameworkCore/CMakeLists.txt exists
- ✅ Lists all source files (even if not implemented yet)
- ✅ Properly links apclientpp, lua, sol2, nlohmann/json
- ✅ Configures as shared library (DLL)
- ✅ Sets Windows-specific definitions

---

### Step 3: APClientLib CMakeLists.txt

**File**: `APClientLib/CMakeLists.txt`

```cmake
project(APClientLib)

# Source files
set(SOURCES
    src/ap_client_lib.cpp
    src/ap_ipc_client.cpp
    src/lua_bindings.cpp
)

# Header files
set(HEADERS
    include/ap_client_lib.h
    include/ap_ipc_client.h
)

# Create shared library (DLL)
add_library(APClientLib SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APClientLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include
)

# Link libraries
target_link_libraries(APClientLib
    PRIVATE
        lua_static
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(APClientLib PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Export symbols for DLL
target_compile_definitions(APClientLib PRIVATE APCLIENTLIB_EXPORTS)

# Set output name
set_target_properties(APClientLib PROPERTIES
    OUTPUT_NAME "APClientLib"
    PREFIX ""
)
```

**Acceptance Criteria**:
- ✅ APClientLib/CMakeLists.txt exists
- ✅ Lists all source files
- ✅ Properly links lua, sol2, nlohmann/json
- ✅ Does NOT link apclientpp (client lib doesn't need it)
- ✅ Configures as shared library (DLL)

---

### Step 4: Third-Party Dependency Setup

#### 4.1: Verify Lua Build

**Check**: `third_party/lua-5.4.7/CMakeLists.txt` exists and builds `lua_static` target.

If missing, create:

```cmake
project(lua C)

set(LUA_CORE_SRC
    src/lapi.c src/lcode.c src/lctype.c src/ldebug.c src/ldo.c
    src/ldump.c src/lfunc.c src/lgc.c src/llex.c src/lmem.c
    src/lobject.c src/lopcodes.c src/lparser.c src/lstate.c
    src/lstring.c src/ltable.c src/ltm.c src/lundump.c src/lvm.c
    src/lzio.c src/lauxlib.c src/lbaselib.c src/lcorolib.c
    src/ldblib.c src/liolib.c src/lmathlib.c src/loadlib.c
    src/loslib.c src/lstrlib.c src/ltablib.c src/lutf8lib.c
    src/linit.c
)

add_library(lua_static STATIC ${LUA_CORE_SRC})

target_include_directories(lua_static
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(lua_static PROPERTIES
    OUTPUT_NAME "lua"
    POSITION_INDEPENDENT_CODE ON
)
```

**Acceptance Criteria**:
- ✅ Lua builds as static library
- ✅ `lua_static` target available
- ✅ Headers exposed via target_include_directories

#### 4.2: Verify nlohmann/json

**Files Required**:
- `third_party/nlohmann/json.hpp`
- `third_party/nlohmann/json_fwd.hpp`

**Action**: If missing, download from [nlohmann/json releases](https://github.com/nlohmann/json/releases) (single include version)

**Acceptance Criteria**:
- ✅ Header files present
- ✅ Can `#include <nlohmann/json.hpp>` from projects

#### 4.3: Verify sol2

**Files Required**:
- `third_party/sol2/include/sol/sol.hpp`
- `third_party/sol2/include/sol/*` (entire header-only library)

**Action**: If missing, initialize git submodule or download from [sol2 releases](https://github.com/ThePhD/sol2/releases)

**Acceptance Criteria**:
- ✅ Header files present
- ✅ Can `#include <sol/sol.hpp>` from projects

#### 4.4: Verify apclientpp and Dependencies

**apclientpp** and its dependencies must be added as git submodules in `third_party/`:

**Action**: Initialize git submodules if missing:
```bash
# Add apclientpp
git submodule add https://github.com/black-sliver/apclientpp.git third_party/apclientpp

# Add apclientpp dependencies to third_party root
git submodule add https://github.com/black-sliver/wswrap.git third_party/wswrap
git submodule add https://github.com/zaphoyd/websocketpp.git third_party/websocketpp
git submodule add https://github.com/tristanpenman/valijson.git third_party/valijson

# Update all submodules
git submodule update --init --recursive
```

**Note**: asio (v1.12) will be auto-fetched by CMake using FetchContent (see Design.md for reference).

**Acceptance Criteria**:
- ✅ apclientpp submodule initialized
- ✅ wswrap submodule initialized
- ✅ websocketpp submodule initialized
- ✅ valijson submodule initialized
- ✅ All submodules in `third_party/` root directory
- ✅ apclientpp CMakeLists.txt present and references dependencies

---

### Step 5: Create Placeholder Headers and Sources

Create stub files for all components to verify build system works.

#### APFrameworkCore Headers

**File**: `APFrameworkCore/include/ap_types.h`
```cpp
#pragma once
#include <string>
#include <cstdint>

namespace APFramework {

// Forward declarations
class APManager;
class APClient;
class APIPCServer;

// Common enums
enum class LifecyclePhase {
    UNINITIALIZED,
    DISCOVERING_MODS,
    AWAITING_PRIORITY_REGISTRATION,
    AWAITING_REGULAR_REGISTRATION,
    VALIDATING_CAPABILITIES,
    READY_FOR_CONNECTION,
    CONNECTING,
    CONNECTED_AND_SYNCING,
    GENERATING_CAPABILITIES,
    RUNNING,
    ERROR_STATE
};

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

} // namespace APFramework
```

**Files**: Create stub headers for all components:
- `ap_manager.h`
- `ap_client.h`
- `ap_ipc_server.h`
- `ap_config.h`
- `ap_logger.h`
- `ap_debug_log.h`
- `ap_mod_registry.h`
- `ap_capabilities_generator.h`
- `ap_message_router.h`
- `ap_polling_thread.h`

**Template for stub headers**:
```cpp
#pragma once
#include "ap_types.h"

namespace APFramework {

class APClassName {
public:
    APClassName();
    ~APClassName();

    // TODO: Implement in Phase XX
};

} // namespace APFramework
```

#### APFrameworkCore Sources

**File**: `APFrameworkCore/src/main.cpp`
```cpp
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // TODO: Initialize framework
            break;
        case DLL_PROCESS_DETACH:
            // TODO: Cleanup framework
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
```

**Files**: Create stub sources for all components (matching headers)

**Template for stub sources**:
```cpp
#include "ap_class_name.h"

namespace APFramework {

APClassName::APClassName() {
    // TODO: Implement in Phase XX
}

APClassName::~APClassName() {
    // TODO: Implement in Phase XX
}

} // namespace APFramework
```

#### APClientLib Stubs

Create similar stub headers and sources for:
- `APClientLib/include/ap_client_lib.h`
- `APClientLib/include/ap_ipc_client.h`
- `APClientLib/src/ap_client_lib.cpp`
- `APClientLib/src/ap_ipc_client.cpp`

**Acceptance Criteria**:
- ✅ All header files created with proper include guards
- ✅ All source files created with matching implementations
- ✅ Namespace `APFramework` used consistently
- ✅ Files compile without errors

---

### Step 6: Build Verification

**Commands** (for Visual Studio 2022):
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Expected Output**:
- `build/bin/Release/APFrameworkCore.dll`
- `build/bin/Release/APClientLib.dll`
- `build/lib/Release/lua.lib` (static lib)
- No compilation errors
- Warnings acceptable at this stage

**Note**: If using a different Visual Studio version, adjust the generator accordingly:
- VS 2019: `-G "Visual Studio 16 2019"`
- VS 2022: `-G "Visual Studio 17 2022"`

**Acceptance Criteria**:
- ✅ CMake configuration succeeds
- ✅ All targets build successfully
- ✅ DLLs generated in expected locations
- ✅ No linker errors

---

### Step 7: .gitignore Update

Ensure build artifacts are ignored:

**File**: `.gitignore`
```
# Build directories
build/
cmake-build-*/
out/

# Compiled binaries
*.dll
*.exe
*.a
*.lib
*.so
*.dylib

# IDE files
.vscode/
.vs/
*.suo
*.user

# CMake cache
CMakeCache.txt
CMakeFiles/
cmake_install.cmake

# Test outputs
Testing/
tests/output/

# Logs
*.log
APFramework.log
```

**Acceptance Criteria**:
- ✅ Build artifacts not tracked by git
- ✅ IDE-specific files ignored

---

## Testing & Validation

### Build Test
```bash
cd build
cmake --build . --clean-first
```
**Expected**: Clean build with no errors

### Dependency Check
```bash
# On Windows with MinGW
objdump -p APFrameworkCore.dll | grep "DLL Name"
```
**Expected**: Should show dependencies on lua, system DLLs, but NOT expose apclientpp internals to client mods

### Size Check
```bash
ls -lh *.dll
```
**Expected**: Reasonable file sizes (< 10MB for framework, < 1MB for client lib)

---

## Acceptance Criteria

**Phase 01 is complete when**:
- ✅ Project structure matches specification
- ✅ Root CMakeLists.txt configures all dependencies
- ✅ APFrameworkCore/CMakeLists.txt builds DLL
- ✅ APClientLib/CMakeLists.txt builds DLL
- ✅ All third-party dependencies integrated (apclientpp, sol2, nlohmann/json, lua)
- ✅ Stub headers/sources for all components created
- ✅ Project builds successfully on Windows
- ✅ No linker errors
- ✅ .gitignore properly configured
- ✅ Build artifacts in correct output directories

---

## Known Issues / Blockers

**None currently identified**

---

## Next Phase

[Phase 02: Core Infrastructure & Configuration](Phase02_CoreInfrastructure.md)

**Prerequisites from Phase 01**:
- Working build system
- All dependencies available
- Stub files ready for implementation

---

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started