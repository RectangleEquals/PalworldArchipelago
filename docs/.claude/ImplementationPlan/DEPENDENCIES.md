# External Dependencies

## Overview

This document lists all external dependencies required for the APFramework IPC branch, their purposes, and setup instructions.

## Git Submodules

All C++ dependencies are managed as git submodules in `third_party/`.

### Setup Command

```bash
# Initialize all submodules
git submodule update --init --recursive
```

### Individual Submodules

#### 1. apclientpp
- **Purpose**: C++ client library for Archipelago protocol
- **Type**: Header-only library
- **Repository**: https://github.com/black-sliver/apclientpp
- **Location**: `third_party/apclientpp/`
- **License**: MIT
- **Used in**: Phase 1 (APFrameworkCore.dll)
- **Notes**: Core dependency for AP communication

#### 2. ASIO (Standalone)
- **Purpose**: Asynchronous I/O library
- **Type**: Header-only library
- **Repository**: https://github.com/chriskohlhoff/asio
- **Location**: `third_party/asio/`
- **Version**: 1.12.2 (downgraded for websocketpp compatibility)
- **License**: Boost Software License
- **Used in**: Phase 1 (via websocketpp)
- **Notes**:
  - **CRITICAL**: Must use 1.12.2 or earlier
  - websocketpp requires `io_service` which was renamed to `io_context` in 1.13+
  - Setup: `cd third_party/asio && git checkout asio-1-12-2`

#### 3. websocketpp
- **Purpose**: WebSocket library for AP network protocol
- **Type**: Header-only library
- **Repository**: https://github.com/zaphoyd/websocketpp
- **Location**: `third_party/websocketpp/`
- **License**: BSD
- **Used in**: Phase 1 (via apclientpp)
- **Notes**: Requires ASIO for networking

#### 4. wswrap
- **Purpose**: WebSocket wrapper library used by apclientpp
- **Type**: Header-only library
- **Repository**: https://github.com/black-sliver/wswrap
- **Location**: `third_party/wswrap/`
- **License**: MIT
- **Used in**: Phase 1 (via apclientpp)
- **Build flags**:
  - `WSWRAP_NO_SSL` - Disable SSL/TLS
  - `WSWRAP_NO_COMPRESSION` - Disable WebSocket compression

#### 5. valijson
- **Purpose**: JSON schema validation (optional)
- **Type**: Header-only library
- **Repository**: https://github.com/tristanpenman/valijson
- **Location**: `third_party/valijson/`
- **License**: BSD
- **Used in**: Phase 1 (optional, for validating messages)
- **Notes**: Can be excluded if not using validation features

### Submodule Tree

```
third_party/
├── apclientpp/        (Main AP client)
│   └── [uses wswrap, valijson]
├── asio/              (Async I/O - MUST be 1.12.2)
├── websocketpp/       (WebSocket protocol)
├── wswrap/            (WebSocket wrapper)
└── valijson/          (JSON validation - optional)
```

## Manual Dependencies

### nlohmann/json
- **Purpose**: JSON serialization/deserialization
- **Type**: Header-only library
- **Source**: https://github.com/nlohmann/json
- **Location**: `src/framework_core/include/nlohmann/json.hpp`
- **Version**: Any recent version (3.x+)
- **License**: MIT
- **Used in**: All phases
- **Setup**:
  - Download single-header version: https://github.com/nlohmann/json/releases
  - Place `json.hpp` in `src/framework_core/include/nlohmann/`

**Why not a submodule?**
- Single header file easier to manage
- Avoid submodule overhead for simple dependency
- Most common approach for this library

### Lua 5.4
- **Purpose**: Lua C API for native bindings
- **Type**: Header files + library
- **Source**: https://www.lua.org/ftp/lua-5.4.7.tar.gz
- **Location**: `third_party/lua-5.4.7/`
- **License**: MIT
- **Used in**: Phase 1 (APFrameworkCore.dll Lua bindings)
- **Setup**:
  - Download Lua 5.4.7 source from lua.org
  - Build as static library or link to headers only
  - APFrameworkCore.dll will export Lua C bindings

**Required headers**:
- `lua.h` - Core Lua C API
- `lauxlib.h` - Auxiliary library functions
- `lualib.h` - Standard library loader

**Build options**:
1. **Static linking**: Compile Lua into APFrameworkCore.dll
2. **UE4SS headers**: Use Lua headers from UE4SS installation (if available)
3. **Standalone headers**: Include just headers, rely on UE4SS's Lua runtime

**Recommended: Option 1 (Static linking)** for maximum compatibility.

## Runtime Dependencies

### Windows System Libraries

Already included with Windows:
- **ws2_32.dll** - Windows Sockets 2 (for Named Pipes and networking)
- **crypt32.dll** - Windows Crypto API (minimal usage)
- **kernel32.dll** - Windows kernel (Named Pipes, file I/O)

### UE4SS
- **Purpose**: Unreal Engine 4 Scripting System
- **Version**: Latest stable (v2.5.2+)
- **Source**: https://github.com/UE4SS-RE/RE-UE4SS
- **Used in**: Phase 3 (Lua framework wrapper)
- **Installation**: User-side dependency
- **Provides**:
  - **Lua 5.4 runtime** (NOT LuaJIT - no FFI available)
  - Native C module loading via Lua C API
  - Hook system
  - UE4 API access

**Important**: UE4SS uses standard Lua 5.4, not LuaJIT. This means:
- No FFI (`ffi.cdef`, `ffi.load`) available
- Must use Lua C API for native bindings
- DLLs must export `luaopen_<modulename>()` function

## Build Tools

### Required
- **CMake**: 3.20 or higher
- **C++ Compiler**:
  - Windows: MSVC 14.44+ (Visual Studio 2022)
  - C++17 standard required

### Optional
- **Git**: For submodule management
- **Visual Studio**: IDE (can use Build Tools only)

## Preprocessor Definitions

### Required for Build

Phase 1 (APFrameworkCore.dll) requires these preprocessor definitions:

```cmake
# ASIO Configuration
ASIO_STANDALONE          # Use standalone ASIO, not boost::asio
ASIO_NO_WIN32_LEAN_AND_MEAN  # Allow ASIO to include what it needs

# wswrap Configuration
WSWRAP_NO_SSL            # Disable SSL/TLS (no OpenSSL dependency)
WSWRAP_NO_COMPRESSION    # Disable compression (no zlib dependency)

# websocketpp C++11 Compatibility
_WEBSOCKETPP_CPP11_RANDOM_DEVICE_
_WEBSOCKETPP_CPP11_THREAD_
_WEBSOCKETPP_CPP11_TYPE_TRAITS_
_WEBSOCKETPP_CPP11_FUNCTIONAL_
_WEBSOCKETPP_CPP11_SYSTEM_ERROR_
_WEBSOCKETPP_CPP11_MEMORY_
_WEBSOCKETPP_CPP11_CHRONO_

# Windows API Version
_WIN32_WINNT=0x0601      # Windows 7+
WIN32_LEAN_AND_MEAN      # Reduce Windows.h bloat
```

## Dependency Setup Checklist

### Initial Setup

```bash
# 1. Clone repository
git clone <repo-url>
cd ipc_branch

# 2. Initialize all submodules
git submodule update --init --recursive

# 3. Downgrade ASIO to 1.12.2 (CRITICAL!)
cd third_party/asio
git checkout asio-1-12-2
cd ../..

# 4. Download nlohmann/json
# Download from: https://github.com/nlohmann/json/releases
# Place json.hpp in: src/framework_core/include/nlohmann/

# 5. Verify directory structure
ls third_party/  # Should see: apclientpp, asio, websocketpp, wswrap, valijson
ls src/framework_core/include/nlohmann/  # Should see: json.hpp

# 6. Configure build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 7. Build
cmake --build build --config Release
```

### Troubleshooting

#### "io_context is not a member of asio"
- **Cause**: ASIO version too new (1.13+)
- **Fix**: `cd third_party/asio && git checkout asio-1-12-2`

#### "Cannot open include file: 'openssl/conf.h'"
- **Cause**: SSL not disabled in wswrap
- **Fix**: Ensure `WSWRAP_NO_SSL` is defined in CMakeLists.txt

#### "Cannot open include file: 'zlib.h'"
- **Cause**: Compression not disabled in wswrap
- **Fix**: Ensure `WSWRAP_NO_COMPRESSION` is defined in CMakeLists.txt

#### "Cannot open include file: 'nlohmann/json.hpp'"
- **Cause**: nlohmann/json not downloaded
- **Fix**: Download and place in `src/framework_core/include/nlohmann/`

#### "No such file or directory: 'apclient.hpp'"
- **Cause**: Submodules not initialized
- **Fix**: `git submodule update --init --recursive`

## Updating Dependencies

### Updating Submodules

```bash
# Update all submodules to latest
git submodule update --remote

# Update specific submodule
cd third_party/apclientpp
git pull origin master
cd ../..

# IMPORTANT: After updating, re-checkout ASIO 1.12.2!
cd third_party/asio
git checkout asio-1-12-2
cd ../..
```

### Updating nlohmann/json

1. Download new version from releases
2. Replace `src/framework_core/include/nlohmann/json.hpp`
3. Rebuild and test

## Dependency Licenses

All dependencies use permissive licenses compatible with project distribution:

- **apclientpp**: MIT
- **ASIO**: Boost Software License
- **websocketpp**: BSD
- **wswrap**: MIT
- **valijson**: BSD
- **nlohmann/json**: MIT

**Result**: Project can be distributed under any license without conflicts.

## Future Considerations

### Potential Additions

#### zlib (for compression)
- **Status**: Currently disabled via `WSWRAP_NO_COMPRESSION`
- **Reason**: Reduces external dependencies
- **Future**: May be required by Archipelago protocol
- **Action**: Add zlib when compression becomes mandatory

#### OpenSSL (for SSL/TLS)
- **Status**: Currently disabled via `WSWRAP_NO_SSL`
- **Reason**: Local AP connections use plain WebSocket
- **Future**: Might need for remote AP servers with wss://
- **Action**: Add OpenSSL if SSL support needed

#### Testing Libraries
- **Catch2** or **Google Test**: For unit testing (Phase 6)
- **Status**: Not yet added
- **Action**: Add when implementing test suite

## Notes

- All C++ dependencies are header-only (except Windows system libraries)
- No runtime DLL dependencies beyond Windows system DLLs
- Minimal footprint: ~1 MB for APFrameworkCore.dll
- Easy distribution: Just include the DLL
- Version pinning: ASIO 1.12.2 is critical for compatibility