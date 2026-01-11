# Palworld Archipelago Framework - Implementation Plan

## Overview

This document provides a comprehensive implementation plan for the Palworld Archipelago Framework, split into discrete phases. Each phase focuses on specific components and builds upon previous phases to create a complete, working system.

**Architecture Reference**: [ARCHITECTURE.md](ARCHITECTURE.md)
**Architecture Review**: [ARCHITECTURE_REVIEW.md](ARCHITECTURE_REVIEW.md)
**Message Flow Reference**: [MessageFlowReference.md](MessageFlowReference.md)

---

## Implementation Status

| Phase | Status | Description |
|-------|--------|-------------|
| [Phase 01](Implementation/Phase01_ProjectSetup.md) | 🟢 Complete | Project Structure & Dependencies |
| [Phase 02](Implementation/Phase02_CoreInfrastructure.md) | 🟢 Complete | Core Infrastructure & Configuration |
| [Phase 03](Implementation/Phase03_IPCSystem.md) | 🟢 Complete | IPC Communication System |
| [Phase 04](Implementation/Phase04_APClientIntegration.md) | 🟢 Complete | AP Client Integration |
| [Phase 05](Implementation/Phase05_CapabilitiesSystem.md) | 🟢 Complete | Capabilities & Registry System |
| [Phase 06](Implementation/Phase06_MessageRouting.md) | 🟢 Complete | Message Routing & Polling |
| [Phase 07](Implementation/Phase07_LifecycleManagement.md) | 🟢 Complete | Lifecycle & State Management |
| [Phase 08](Implementation/Phase08_LuaBindings.md) | 🟢 Complete | Lua Bindings & APFrameworkMod |
| [Phase 09](Implementation/Phase09_ClientLibrary.md) | 🟢 Complete | APClientLib Implementation |
| [Phase 10](Implementation/Phase10_Testing.md) | 🔴 Not Started | Testing & Validation |

**Legend:**
- 🔴 Not Started
- 🟡 In Progress
- 🟢 Complete
- ⚠️ Blocked

---

## Phase Summaries

### Phase 01: Project Structure & Dependencies
**Goal**: Establish project structure, CMake build system, and integrate all third-party dependencies.

**Key Deliverables**:
- Root CMakeLists.txt with proper dependency management
- APFrameworkCore project structure (include/src separation)
- APClientLib project structure
- All third-party libraries integrated (apclientpp, sol2, nlohmann::json, lua)
- Build verification on Windows with MSVC (Visual Studio 2022)

**Duration Estimate**: Foundation phase
**Dependencies**: None

---

### Phase 02: Core Infrastructure & Configuration
**Goal**: Implement foundational classes and configuration management.

**Key Deliverables**:
- APConfig class with JSON parsing
- APLogger with file/console/IPC routing
- APDebugLog utility class
- framework_config.json schema
- Error handling infrastructure
- Basic unit tests for config/logging

**Duration Estimate**: Foundation phase
**Dependencies**: Phase 01

---

### Phase 03: IPC Communication System
**Goal**: Implement Named Pipes IPC system for framework-mod communication.

**Key Deliverables**:
- APIPCServer (server-side IPC handling)
- APIPCClient (client-side IPC handling)
- Windows Named Pipes implementation
- IPC message protocol (JSON-based)
- Connection management and error handling
- IPC system tests

**Duration Estimate**: Core system phase
**Dependencies**: Phase 02

---

### Phase 04: AP Client Integration
**Goal**: Integrate apclientpp and implement APClient wrapper.

**Key Deliverables**:
- APClient class wrapping apclientpp
- Connection management with async callbacks
- Authentication and slot synchronization
- Message queue system
- APPollingThread with lifecycle state checking
- WebSocket communication tests

**Duration Estimate**: Core system phase
**Dependencies**: Phase 02

---

### Phase 05: Capabilities & Registry System
**Goal**: Implement mod discovery, registration, and capabilities management.

**Key Deliverables**:
- APModRegistry (mod discovery and tracking)
- APCapabilitiesGenerator (JSON aggregation)
- AP_Config.json schema and validation
- Checksum generation (SHA256)
- Capability conflict detection
- Registry and capabilities tests

**Duration Estimate**: Core system phase
**Dependencies**: Phase 03

---

### Phase 06: Message Routing & Polling
**Goal**: Implement message routing between AP server and mods.

**Key Deliverables**:
- APMessageRouter (bidirectional routing)
- Message filtering by capability subscriptions
- Priority vs regular client message handling
- Console log routing to priority clients
- Integration with APPollingThread
- Message routing tests

**Duration Estimate**: Integration phase
**Dependencies**: Phase 03, Phase 04, Phase 05

---

### Phase 07: Lifecycle & State Management
**Goal**: Implement framework state machine and orchestrate all components.

**Key Deliverables**:
- APManager (main orchestration singleton)
- Full lifecycle state machine implementation
- State transition validation
- Priority vs regular client registration flow
- Command handling (CONNECT, GENERATE, DISCONNECT, etc.)
- Resync mechanism
- Comprehensive lifecycle tests

**Duration Estimate**: Integration phase
**Dependencies**: Phase 02, Phase 03, Phase 04, Phase 05, Phase 06

---

### Phase 08: Lua Bindings & APFrameworkMod
**Goal**: Create Lua bindings and implement the APFrameworkMod entry point.

**Key Deliverables**:
- sol2 bindings for APFrameworkCore
- APFramework.lua high-level wrapper
- main.lua entry point for UE4SS
- Lua error handling and logging
- Example Lua scripts
- Lua integration tests

**Duration Estimate**: Interface phase
**Dependencies**: Phase 07

---

### Phase 09: APClientLib Implementation
**Goal**: Implement the lightweight client library for AP-enabled mods.

**Key Deliverables**:
- APClientLib C++ implementation
- sol2 bindings for APClientLib
- APClient.lua high-level wrapper
- Example mod demonstrating usage
- Client library tests
- Documentation and API reference

**Duration Estimate**: Interface phase
**Dependencies**: Phase 03, Phase 08

---

### Phase 10: Testing & Validation
**Goal**: Comprehensive end-to-end testing and validation.

**Key Deliverables**:
- Integration tests (full framework lifecycle)
- Mock AP server for testing
- Multiple test mods with various capabilities
- Performance testing (message throughput, polling latency)
- Error scenario testing (timeouts, disconnects, conflicts)
- Documentation review and updates
- Example mods and tutorials

**Duration Estimate**: Validation phase
**Dependencies**: All previous phases

---

## Implementation Principles

### 1. Incremental Development
- Each phase builds upon previous phases
- Components are independently testable
- No phase should be "skipped" without explicit justification

### 2. Test-Driven Approach
- Unit tests for individual components
- Integration tests for phase deliverables
- End-to-end tests in final phase

### 3. Documentation as Code
- Architecture documentation drives implementation
- Code comments reference architecture sections
- Implementation discoveries update architecture

### 4. Review & Validation Gates
- Each phase ends with a review checkpoint
- User validates deliverables before proceeding
- Issues discovered are documented and addressed

---

## Critical Implementation Notes

### From Architecture Review

**MUST Address**:
1. ✅ **Polling Thread Lifecycle Checking** (Issue 10) - Addressed in Phase 04/06
   - APPollingThread must check lifecycle state before polling
   - Only poll during CONNECTED_AND_SYNCING and RUNNING states

2. ✅ **Location Type Clarification** (Issue 17) - Addressed in Phase 05
   - "dynamic" means procedural positioning, NOT runtime registration
   - All locations must be declared in capabilities before generation

3. ⚠️ **Timeout Documentation** - Need to verify in Phase 02
   - Priority registration: 60s (not 5s)
   - Regular registration: 180s (not 10s)
   - AP connection: 30s

4. ⚠️ **Mod Hot-Reload** (Issue 8) - Optional feature
   - Detection mechanism TBD
   - Requires resync if mid-session
   - Low priority for initial implementation

5. ⚠️ **Checksum Mismatch** (Issue 6) - Addressed in Phase 05/07
   - Should enter ERROR_STATE on mismatch
   - Not just a warning

### UE4SS Lifecycle & Memory Safety

⚠️ **CRITICAL REQUIREMENTS**:

**Correct UE4SS API Usage (Phase 08)**:
- ❌ **DO NOT USE**: `RegisterInitGameStateHook` (hallucinated - does not exist)
- ❌ **DO NOT USE**: `RegisterUnrealEngineShutdownCallback` (hallucinated - does not exist)
- ✅ **USE**: `RegisterCustomEvent("Tick", callback)` for initialization (most reliable)
- See [ARCHITECTURE.md - UE4SS Lua Integration](ARCHITECTURE.md#ue4ss-lua-integration) for details

**Memory Safety Requirements (All Phases)**:
- **No Reliable Shutdown Hook**: UE4SS provides no guaranteed shutdown callback
- **Smart Pointers Required**: All heap allocations must use `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`
- **RAII for All Resources**: File handles, sockets, threads must clean up in destructors
- **Timeout-Based Thread Joining**: All threads must join with timeouts in destructors (2 seconds max)
- **shutdown() is Optional**: Framework must be memory-safe even if `APManager::shutdown()` is never called
- **Crash-Safe Design**: Game crashes, UE4SS termination, or mod unload may bypass all cleanup

**Implementation Checklist (Ongoing)**:
- [ ] All components use smart pointers (no raw `new`/`delete`)
- [ ] All threads join with timeout in destructors
- [ ] All file handles use RAII (ofstream, etc.)
- [ ] All IPC connections detect disconnection automatically
- [ ] No dangling pointers or memory leaks if shutdown() never called
- [ ] Valgrind/ASan testing shows no leaks (Phase 10)

See [ARCHITECTURE.md - Lifecycle Management & Memory Safety](ARCHITECTURE.md#ue4ss-lua-integration) for complete details.

### Platform Considerations

**Windows Focus**:
- Named Pipes for IPC (Windows-specific)
- MSVC (Visual Studio 2022) build toolchain
- UE4SS is Windows-only currently

**Future Cross-Platform** (out of scope for now):
- Unix domain sockets for Linux
- Cross-platform IPC abstraction layer

---

## Getting Started

1. **Review**: Read [ARCHITECTURE.md](ARCHITECTURE.md) thoroughly
2. **Setup**: Ensure development environment is ready (Visual Studio 2022, CMake, Git)
3. **Begin**: Start with [Phase 01: Project Setup](Implementation/Phase01_ProjectSetup.md)
4. **Iterate**: Complete each phase, review, and proceed to next

---

## Notes

- This is a living document and will be updated as implementation progresses
- Each phase document contains detailed implementation steps and acceptance criteria
- Blockers and issues discovered during implementation should be documented in phase docs
- Architecture updates based on implementation learnings should be tracked

---

**Last Updated**: 2026-01-10
**Current Phase**: Phase 10 - Testing & Validation
**Status**: Phase 01-09 Complete