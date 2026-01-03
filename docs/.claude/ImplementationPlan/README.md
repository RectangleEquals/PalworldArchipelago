# Implementation Plan - IPC Branch

This directory contains the split implementation plan for the IPC branch redesign of the APFramework.

## Document Structure

The implementation plan has been split into focused, manageable documents:

- **[OVERVIEW.md](OVERVIEW.md)** - High-level implementation overview and architecture
- **[PHASE_1.md](PHASE_1.md)** - ✅ C++ Framework Core (APFrameworkCore.dll) - **COMPLETE**
- **[PHASE_2.md](PHASE_2.md)** - ✅ Mod Client Library (APClientLib.dll & ap_client.lua) - **COMPLETE**
- **[PHASE_3.md](PHASE_3.md)** - Lua Framework Wrapper (main UE4SS mod)
- **[PHASE_4.md](PHASE_4.md)** - Example Mods and Testing
- **[DEPENDENCIES.md](DEPENDENCIES.md)** - External dependencies and setup

## Current Status

**Phase 2** ✅ **COMPLETE** - APClientLib.dll (10 KB) & ap_client.lua ready

See [PHASE_2.md](PHASE_2.md) for API documentation and usage examples.

## Phase Summary

### ✅ Phase 1: C++ Framework Core - COMPLETE
- APFrameworkCore.dll built and working (1023 KB)
- All 10 source files implemented
- ASIO 1.12.2 compatibility configured
- SSL and compression disabled (minimal dependencies)

### ✅ Phase 2: Mod Client Library - COMPLETE
- APClientLib.dll for C++ mods (10 KB)
- ap_client.lua for Lua mods (pure Lua)
- Complete C and Lua APIs with callbacks
- Example mods for both languages
- Full API documentation

### 🎮 Phase 3: Lua Framework Wrapper - REDESIGN
- Main APFramework UE4SS mod
- Loads APFrameworkCore.dll via native Lua C bindings
- Manages framework lifecycle
- Auto-discovery of mods
- **Update**: FFI not available in UE4SS (uses Lua 5.4, not LuaJIT)

### ✨ Phase 4: Example Mods & Testing
- Example Lua, C++, and BP Logic mods
- Integration testing
- Documentation

## Navigation

Start with [OVERVIEW.md](OVERVIEW.md) for the big picture, then dive into individual phases as needed.