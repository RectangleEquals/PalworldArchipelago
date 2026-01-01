# Palworld Archipelago - IPC Branch Project State

**Branch**: IPC Architecture Redesign
**Version**: 2.0.0 (Design Phase)
**Last Updated**: December 31, 2024
**Status**: 🔨 **IN DEVELOPMENT** - Architecture Design

---

## Table of Contents
1. [Branch Overview](#branch-overview)
2. [Design Goals](#design-goals)
3. [Current Status](#current-status)
4. [Architecture Summary](#architecture-summary)
5. [Implementation Roadmap](#implementation-roadmap)
6. [Differences from Main Branch](#differences-from-main-branch)

---

## Branch Overview

### What is This Branch?

This branch represents a **complete architectural redesign** of the Palworld Archipelago Framework. Instead of the submodule architecture (main branch), this uses **Inter-Process Communication (IPC)** to enable:

- **Standard UE4SS mods** (not forced submods)
- **Non-blocking operation** (no thread blocking)
- **Multiple mod types** (Lua, C++, BP Logic)
- **Clean separation** of framework and mod concerns

### Why the Redesign?

The main branch (submodule architecture) has fundamental limitations:

1. **Blocking Thread Issue**: Continuous polling blocks all mods loaded after APFramework
2. **Submod Requirement**: Mods must be placed inside APFramework folder structure
3. **Lua State Issues**: Threading approaches cause "Lua state changed" errors
4. **Limited Ecosystem**: Doesn't work well with existing UE4SS mods

The IPC branch solves all these issues.

### Relationship to Main Branch

- **Main Branch**: Working proof-of-concept, temporary blocking loop solution
- **IPC Branch**: Production-ready architecture, no blocking, standard mods
- **No Backward Compatibility**: Clean break, different design philosophy

---

## Design Goals

### Primary Objectives

✅ **Standard Mod Support**
- Mods are normal UE4SS mods in standard locations
- No special folder structure required
- Works with Lua mods, C++ mods, and BP Logic mods

✅ **Non-Blocking Operation**
- Framework doesn't block other mods during initialization
- Each mod runs independently
- No threading issues with Lua states

✅ **Fault Isolation**
- One mod's failure doesn't affect others
- Framework continues working if a mod crashes
- Clean error handling and reporting

✅ **Developer-Friendly**
- Simple API for mod developers
- Well-documented protocol
- Example mods for each type (Lua, C++, BP)

### Technical Goals

- Named Pipes for IPC (low latency, native Windows)
- C++ core for framework (native threading, no Lua limitations)
- JSON message protocol (human-readable, easy to debug)
- Per-mod message queues (scalability, isolation)
- Auto-discovery of AP-enabled mods (convenience)

---

## Current Status

### Phase: Architecture Design ✅

**Completed**:
- ✅ Architecture design document
- ✅ IPC protocol specification
- ✅ Threading model defined
- ✅ Mod integration patterns documented

**In Progress**:
- 🔨 Implementation plan
- 🔨 Technical specifications

**Not Started**:
- ❌ C++ framework core implementation
- ❌ C++ client library implementation
- ❌ Lua client wrapper implementation
- ❌ Example mods
- ❌ Testing

### Implementation Progress: 0%

This branch is in **design phase only**. No code has been written yet.

---

## Architecture Summary

### System Components

```
┌─────────────────────────────────────────────────────────┐
│ UE4SS Mod Ecosystem                                     │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │ APFramework (Lua mod + C++ core)               │    │
│  │  - Loads APFrameworkCore.dll via FFI           │    │
│  │  - Starts IPC server (Named Pipes)             │    │
│  │  - Auto-discovers AP mods                      │    │
│  │  - Connects to AP server                       │    │
│  │  - Routes messages between AP ↔ mods           │    │
│  └────────────────────────────────────────────────┘    │
│         ↕ IPC (Named Pipes, JSON messages)             │
│  ┌────────────────────────────────────────────────┐    │
│  │ AP-Enabled Mods (standard UE4SS mods)          │    │
│  │                                                 │    │
│  │  Lua Mod:                                       │    │
│  │  └─ Uses ap_client.lua (pure Lua IPC wrapper)  │    │
│  │                                                 │    │
│  │  C++ Mod:                                       │    │
│  │  └─ Links APClientLib.dll (IPC client)         │    │
│  │                                                 │    │
│  │  BP Logic Mod:                                  │    │
│  │  └─ Companion Lua mod (BP ↔ Lua ↔ IPC)         │    │
│  └────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

### Key Features

**APFrameworkCore.dll** (C++ Library):
- lua-apclientpp integration
- Background polling thread (no blocking)
- Named Pipes IPC server
- Per-mod message queues
- Message routing logic

**Client Libraries**:
- **ap_client.lua**: Pure Lua wrapper for Lua mods
- **APClientLib.dll**: C++ library for C++ mods

**IPC Protocol**:
- Named Pipes (Windows)
- JSON messages (newline-delimited)
- Bidirectional communication
- Non-blocking reads/writes

**Message Types**:
- `register`, `location_check`, `status_update`, `poll` (mod → framework)
- `item_received`, `location_checked`, `connection_status`, `framework_ready` (framework → mod)

### Threading Model

**Framework**:
- **Main Thread**: IPC server (mod connections, message dispatch)
- **Polling Thread**: AP client (continuous polling, message routing)

**Mods**:
- Poll at their own pace (typically once per frame)
- No forced synchronization
- Freedom to use their own threading

---

## Implementation Roadmap

### Phase 1: C++ Framework Core
**Goal**: Implement `APFrameworkCore.dll`

**Tasks**:
1. Set up C++ project (CMake)
2. Integrate lua-apclientpp
3. Implement Named Pipes IPC server
4. Implement background polling thread
5. Implement per-mod message queues
6. Implement message routing logic
7. Create Lua FFI bindings

**Deliverables**:
- `APFrameworkCore.dll` (Windows x64)
- FFI binding definitions
- Unit tests

### Phase 2: C++ Client Library
**Goal**: Implement `APClientLib.dll`

**Tasks**:
1. Implement Named Pipes IPC client
2. Message queue polling
3. C API for UE4SS C++ mods
4. Thread-safe operations
5. Error handling

**Deliverables**:
- `APClientLib.dll` (Windows x64)
- C header files
- Usage documentation

### Phase 3: Lua Client Wrapper
**Goal**: Implement `ap_client.lua`

**Tasks**:
1. Named Pipes wrapper (pure Lua)
2. JSON serialization/deserialization
3. Event callback system
4. Polling helpers
5. Error handling

**Deliverables**:
- `ap_client.lua` module
- API documentation
- Usage examples

### Phase 4: Framework Lua Mod
**Goal**: Implement APFramework UE4SS mod

**Tasks**:
1. Load `APFrameworkCore.dll` via FFI
2. Start IPC server
3. Auto-discovery (`ap_config.json` scanning)
4. Configuration management
5. Connect to AP server
6. Lifecycle management

**Deliverables**:
- `APFramework/Scripts/main.lua`
- `APFramework/Scripts/APFramework.lua`
- Configuration files
- Installation guide

### Phase 5: Example Mods
**Goal**: Create reference implementations

**Tasks**:
1. Example Lua mod (simple item/location integration)
2. Example C++ mod (demonstrates C++ API)
3. Example BP companion mod (BP ↔ Lua ↔ IPC)
4. Documentation for each

**Deliverables**:
- Three example mods
- README for each
- Integration guides

### Phase 6: Testing & Documentation
**Goal**: Validate and document

**Tasks**:
1. Unit tests for C++ components
2. Integration tests (framework + mods)
3. Performance testing (IPC latency, throughput)
4. API documentation
5. Developer guide
6. User installation guide

**Deliverables**:
- Test suite
- Complete documentation
- Migration guide (from main branch)

---

## Differences from Main Branch

### Main Branch (Submodule Architecture)

**Structure**:
```
APFramework/
├── Scripts/              # Framework core (Lua)
└── Mods/                 # Submodules (AP-enabled mods)
    └── APTest/
        └── ap_config.json
```

**Characteristics**:
- Mods must be inside APFramework folder
- Blocking while loop for polling (temporary solution)
- Direct function calls between framework and mods
- Same Lua state shared across framework and submods
- Blocks other mods during initialization

**Status**: Working proof-of-concept, not production-ready

### IPC Branch (IPC Architecture)

**Structure**:
```
ue4ss/mods/APFramework/        # Framework (Lua + C++)
ue4ss/mods/MyLuaMod/           # Standard Lua mod
ue4ss/mods/MyCppMod/           # Standard C++ mod
```

**Characteristics**:
- Mods are standard UE4SS mods in normal locations
- C++ background thread for polling (non-blocking)
- IPC communication (Named Pipes, JSON)
- Isolated environments (no Lua state crossing)
- No blocking, mods load independently

**Status**: Design phase, implementation in progress

### Migration Path

**Not supported**. IPC branch is a complete redesign.

Users/developers can choose:
- **Main Branch**: Quick testing, proof-of-concept, simple setups
- **IPC Branch**: Production use, complex mods, ecosystem integration

---

## Next Steps

### Immediate Priorities

1. **Complete Implementation Plan** (this document)
   - Detailed technical specifications
   - Build system setup
   - Development environment
   - Testing strategy

2. **Set Up Development Environment**
   - CMake project for C++ components
   - Build scripts
   - Development dependencies

3. **Implement APFrameworkCore.dll**
   - Named Pipes IPC server
   - lua-apclientpp integration
   - Background polling thread

4. **Implement Client Libraries**
   - APClientLib.dll (C++)
   - ap_client.lua (Lua)

5. **Create Example Mods**
   - Validate API design
   - Test integration patterns

### Future Milestones

- **Alpha Release**: Core framework + client libraries working
- **Beta Release**: Example mods, initial documentation
- **Release Candidate**: Full test coverage, complete documentation
- **v2.0.0 Release**: Production-ready IPC architecture

---

## Resources

### Documentation

- `ARCHITECTURE.md` - Detailed architecture design
- `.claude/IMPLEMENTATION_PLAN.md` - Technical implementation details
- Main branch docs - For comparison and context

### References

- UE4SS Documentation: https://docs.ue4ss.com/
- Named Pipes (Windows): https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipes
- lua-apclientpp: https://github.com/black-sliver/lua-apclientpp
- Archipelago Protocol: https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md

---

**This IPC branch represents the future of the Palworld Archipelago Framework - a production-ready architecture that integrates seamlessly with the UE4SS mod ecosystem.**
