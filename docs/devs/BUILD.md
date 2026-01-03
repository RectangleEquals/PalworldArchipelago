# Build Instructions

This document describes how to build the APFramework IPC branch from source.

## Prerequisites

- **CMake** 3.20 or later
- **Visual Studio 2019 or 2022** with C++17 support
- **Windows 7+** (target platform)
- **Git** (for cloning dependencies)

## Directory Structure

```
ipc_branch/
├── src/
│   ├── framework_core/       # Core framework DLL
│   └── client_lib/           # Client library for mods
├── third_party/
│   ├── apclientpp/           # AP client library (header-only)
│   ├── lua-5.4.7/            # Lua 5.4 source
│   ├── asio/                 # Async I/O library
│   ├── websocketpp/          # WebSocket library
│   ├── wswrap/               # WebSocket wrapper
│   └── valijson/             # JSON validation
├── APFramework/              # Lua mod for UE4SS
│   └── Scripts/
│       ├── main.lua
│       ├── framework_wrapper.lua
│       └── config.lua
└── build/                    # Build output (generated)
```

## Build Steps

### 1. Clean Build Directory

If you're doing a fresh build, remove the existing build directory:

```bash
rm -rf build
mkdir build
cd build
```

### 2. Configure with CMake

```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

Or for Visual Studio 2019:

```bash
cmake .. -G "Visual Studio 16 2019" -A x64
```

### 3. Build

Build the Release configuration:

```bash
cmake --build . --config Release
```

Or build Debug configuration for development:

```bash
cmake --build . --config Debug
```

### 4. Build Output

After a successful build, you'll find:

- `build/bin/Release/APFrameworkCore.dll` - Main framework DLL
- `build/bin/Release/APClientLib.dll` - Client library for mods
- `build/lib/Release/lua.lib` - Lua static library (linked into APFrameworkCore.dll)

## Installation

### Install to Game

Copy the built DLL to your game's UE4SS mods folder:

```bash
cp build/bin/Release/APFrameworkCore.dll <game_path>/Pal/Binaries/Win64/ue4ss/Mods/APFramework/
```

Example for Palworld:

```bash
cp build/bin/Release/APFrameworkCore.dll E:/SteamLibrary/steamapps/common/Palworld/Pal/Binaries/Win64/ue4ss/Mods/APFramework/
```

### Copy Lua Scripts

Copy the Lua framework scripts:

```bash
cp -r APFramework/* <game_path>/Pal/Binaries/Win64/ue4ss/Mods/APFramework/
```

## Build Configuration

### Compiler Definitions

The build uses the following compile definitions (configured in `src/framework_core/CMakeLists.txt`):

- `ASIO_STANDALONE` - Use standalone ASIO (not Boost)
- `WSWRAP_NO_SSL` - Disable SSL (no OpenSSL dependency)
- `WSWRAP_NO_COMPRESSION` - Disable compression (no zlib dependency)
- `_WIN32_WINNT=0x0601` - Target Windows 7+
- `WIN32_LEAN_AND_MEAN` - Reduce Windows.h bloat

### C++ Standard

The project uses **C++17** as configured in the root `CMakeLists.txt`.

## Troubleshooting

### CMake can't find dependencies

Make sure all git submodules are initialized:

```bash
git submodule update --init --recursive
```

### Linker errors for Lua symbols

Make sure the `lua` target is being built and linked:

1. Check that `third_party/lua-5.4.7/CMakeLists.txt` exists
2. Check that root `CMakeLists.txt` includes `add_subdirectory(third_party/lua-5.4.7)`
3. Check that `src/framework_core/CMakeLists.txt` links against `lua` library

### Missing DLL when running

The APFrameworkCore.dll should be self-contained with no external dependencies except:

- `ws2_32.dll` (Windows Sockets - part of Windows)
- `crypt32.dll` (Windows Crypto - part of Windows)

Lua is statically linked, so no separate Lua DLL is needed.

## Development Build

For development, you may want to build in Debug mode for better debugging symbols:

```bash
cd build
cmake --build . --config Debug
```

Debug builds will be larger and slower but provide better error messages and debugging support.

## Clean Build

To completely rebuild:

```bash
rm -rf build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```
