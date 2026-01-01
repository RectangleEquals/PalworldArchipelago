# Implementation Plan - Overview

## Design Philosophy

The IPC branch implements a decoupled architecture where:
- **APFramework** is a Lua mod with a C++ core (APFrameworkCore.dll) handling AP server communication
- **AP-enabled mods** are regular UE4SS mods (Lua, C++, or BP) that communicate with the framework via IPC
- **No parent-child relationship** - mods remain independent, framework coordinates them
- **Promise-based registration** - framework discovers mods and waits for all to register before connecting to AP

## Core Principles

### 1. Auto-Discovery & Registration
- Framework scans for `ap_config.json` files in mod directories
- Discovered mods must register via IPC within a timeout period
- Registration includes mod capabilities (items, locations, regions)
- All discovered mods must register before AP connection is allowed

### 2. Dynamic World Generation
- Mods provide capability data → framework generates `APCapabilities.json`
- AP Python server reads capabilities to know what's available for randomization
- Supports Palworld's dynamic nature (mods determine what can be randomized at runtime)

### 3. Controlled Connection Flow
1. Framework starts, auto-discovers mods
2. Waits for all discovered mods to register
3. Generates `APCapabilities.json` from registered mod data
4. Broadcasts "registration_complete" to all mods
5. Connects to AP server only when:
   - Autoconnect config enabled, OR
   - Explicit connection request via IPC

### 4. Framework as Special Mod
- Lua framework wrapper can register as a mod with special mod_id
- C++ framework recognizes this ID and pipes IPC logs to it
- Allows UE4SS to display framework activity in game logs

## Implementation Phases

### Phase 1: C++ Framework Core (APFrameworkCore.dll)
**Status**: In Progress
- Core C++ library handling AP protocol, IPC server, mod registry
- Compiles to `APFrameworkCore.dll`
- See [PHASE_1.md](PHASE_1.md)

### Phase 2: Mod Client Library
**Status**: Not Started
- Lightweight library for mods to communicate with framework
- Lua FFI wrapper + C interface
- See [PHASE_2.md](PHASE_2.md)

### Phase 3: Lua Framework Wrapper
**Status**: Not Started
- Main UE4SS Lua mod that loads APFrameworkCore.dll
- Provides configuration UI integration points
- See [PHASE_3.md](PHASE_3.md)

### Phase 4: Example Mods & Testing
**Status**: Not Started
- Sample AP-enabled mods demonstrating integration
- Test suite for IPC and registration flows
- See [PHASE_4.md](PHASE_4.md)

## Technical Stack

- **C++17** - Core framework implementation
- **apclientpp** - Archipelago protocol client
- **nlohmann/json** - JSON serialization
- **Windows Named Pipes** - IPC mechanism
- **LuaJIT FFI** - Lua-to-C++ bridge

## Key Design Decisions

### Why Named Pipes for IPC?
- Native Windows support, no external dependencies
- Bidirectional communication
- Per-mod message queues prevent blocking

### Why Split Framework (Lua + C++)?
- C++ handles complex AP protocol and threading
- Lua integrates with UE4SS ecosystem
- FFI bridge keeps them connected

### Why Promise-Based Registration?
- Framework needs to know what mods exist before generating APCapabilities.json
- Prevents race conditions (mod registering after AP connection)
- Provides clear error reporting (which mods failed to register)

## Next Steps

Current focus: Complete Phase 1 by implementing all C++ source files correctly according to headers.

See [PHASE_1.md](PHASE_1.md) for detailed implementation status.