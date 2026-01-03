# APFramework Redesign Plan - Overview

**Document Version**: 2.0
**Date**: 2026-01-02
**Status**: Planning (Revised with Corrected Design)

---

## Executive Summary

This document outlines the roadmap for evolving the current APFramework implementation to fully align with the **corrected intended design** as specified in [DESIGN_AND_FLOW.md](DESIGN_AND_FLOW.md). Based on the comprehensive gap analysis in [CURRENT_ANALYSIS.md](CURRENT_ANALYSIS.md), we have identified **12 key improvements** organized into **3 phases**.

### Current State

- **Architecture**: 70% aligned - library separation correct, but framework mod dual-role missing
- **Core Features**: Functional IPC, AP client, mod discovery, basic capabilities
- **Missing Features**: Critical safety systems (dependencies, incompatibilities, runtime enablement)
- **Quality**: Solid foundation but needs major additions

### Target State

- **100% design alignment** with all critical safety features implemented
- **Production-ready mod ecosystem** with dependency and incompatibility management
- **Multi-player support** via per-slot capability files
- **Enhanced user experience** with framework logs visible in UE4SS console
- **Zero-tolerance validation** blocking conflicting capabilities
- **Robust JSON handling** with lunajson integration

### High-Level Roadmap

- **Phase 1** (Critical Safety Features): Essential for production release
- **Phase 2** (Validation & Polish): Required for robust operation
- **Phase 3** (Nice-to-Have): Advanced features for ecosystem maturity

---

## Phase Overview

### Phase 1: Critical Safety Features (MUST HAVE)

**Goal**: Implement features absolutely required for a safe, production-ready release

**Features**:
1. Framework mod dual-role (server + priority client)
2. Dependency system (hard requirements + cascade disabling)
3. Incompatibility system (three types + auto-disable)
4. Runtime enablement (in-memory + file persistence)
5. Multi-slot APCapabilities (per-slot JSON files)
6. Log routing to framework mod (IPC → UE4SS console)
7. lunajson integration (replace custom JSON)

**Deliverables**:
- Framework mod registers as priority client with mod_id `archipelago.palworld.framework`
- Mods can declare dependencies (registration denied if missing)
- Mods can declare incompatibilities (auto-disable conflicts)
- Mods can be disabled at runtime (automatic conflict resolution)
- Capability files use slot-based naming: `APCapabilities_<slot>.json`
- Framework logs visible in UE4SS console (no more blind debugging)
- All Lua code uses lunajson for robust JSON handling

**Success Criteria**:
- Framework mod receives log messages and status updates via IPC
- Missing dependencies prevent mod registration
- Conflicting mods auto-disabled (mutual or one-way)
- `enabled` field in `ap_config.json` respected and editable at runtime
- Multiple players can generate distinct capability files
- Users see framework errors in UE4SS console
- Complex JSON structures (nested objects, unicode) handled correctly

**Why These Features Are Critical**:
- **Without dependencies**: Mods load without requirements → crashes
- **Without incompatibilities**: Conflicting mods run together → corruption/crashes
- **Without runtime enablement**: No conflict resolution mechanism
- **Without multi-slot**: Cannot support multiple Palworld players
- **Without log routing**: Users can't see errors → impossible to debug
- **Without lunajson**: Fragile JSON breaks on complex capabilities

---

### Phase 2: Validation & Polish (SHOULD HAVE)

**Goal**: Enforce safety guarantees and improve developer experience

**Features**:
8. Zero-tolerance capabilities validation (block generation on conflicts)
9. Python world schema verification (ensure compatibility)
10. Comprehensive testing suite (dependency cycles, conflicts, edge cases)
11. Documentation updates (examples, migration guides)

**Deliverables**:
- Capability generation BLOCKED if any ID conflicts detected
- Error messages broadcast to all mods via IPC
- Python world verified to handle new schema (dependencies, incompatibilities)
- Multi-slot capability loading tested in Python world
- Full test coverage for dependency/incompatibility logic
- Updated documentation with migration guide from v1 to v2 schema

**Success Criteria**:
- Conflicting item/location/region IDs prevent APCapabilities generation
- Detailed conflict reports displayed in UE4SS console
- Python world successfully loads multi-slot capability files
- All edge cases tested (circular dependencies, mutual conflicts, etc.)
- Mod developers have clear migration path

**Why These Features Are Important**:
- **Zero-tolerance validation**: Prevents invalid capabilities from reaching AP server
- **Python world verification**: Ensures framework and world are compatible
- **Testing**: Catches edge cases before users encounter them
- **Documentation**: Enables mod developers to adopt new features

---

### Phase 3: Nice-to-Have Enhancements (FUTURE)

**Goal**: Add advanced features for long-term ecosystem maturity

**Features**:
12. Semantic version validation (runtime version compatibility warnings)
13. Static .lib build option (for C++ mods preferring static linking)
14. Advanced error handling (retry logic, automatic recovery)

**Deliverables**:
- Framework warns about version mismatches at startup
- Both dynamic and static client library builds available
- Transient failures handled with automatic retry/recovery
- Enhanced error messages with actionable suggestions

**Success Criteria**:
- Version warnings logged for incompatible game/mod versions
- C++ mods can link statically (no DLL deployment)
- IPC and AP connection failures auto-retry with backoff
- All error messages include clear resolution steps

**Why These Features Are Nice-to-Have**:
- They improve user experience but aren't critical for core functionality
- Can be added after initial release based on community feedback
- Represent polish and long-term stability improvements

---

## Detailed Phase Plans

### Phase 1: Critical Safety Features

**See**: [ImplementationPlan/Phase01_CriticalSafetyFeatures.md](ImplementationPlan/Phase01_CriticalSafetyFeatures.md)

#### 1.1 Framework Mod Dual-Role

**Problem**: Framework mod only uses `APFrameworkCore.dll`, doesn't register as client, can't receive logs.

**Solution**: Framework mod should use BOTH libraries:
- Uses `APFrameworkCore.dll` (Lua C bindings) to run as IPC server
- Uses `ap_client.lua` (Lua wrapper) to register as priority client
- Mod ID: `archipelago.palworld.framework`
- Receives all important framework logs via IPC

**Changes Required**:
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua): Load `ap_client.lua`, register as client
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Special handling for framework mod_id (allow self-registration)
- [src/framework_core/src/message_router.cpp](../../src/framework_core/src/message_router.cpp): Route log messages to framework mod

**Estimate**: 4-6 hours

---

#### 1.2 Dependency System

**Problem**: No dependency enforcement. Mods can register without required dependencies.

**Solution**: Implement hard dependency requirements:

**Schema**:
```json
"dependencies": {
  "required.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"},
  "another.required.mod": true
}
```

**Behavior**:
- Parse dependencies during mod discovery
- Build dependency graph
- Check all dependencies exist and satisfy version constraints
- **DENY registration** if dependencies missing or wrong version
- **Cascade disable**: If Mod A disabled, disable all mods depending on A
- Send IPC error to mod explaining why registration denied

**Changes Required**:
- [src/framework_core/include/mod_registry.h](../../src/framework_core/include/mod_registry.h): Add `dependencies` field to `ModMetadata`
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Parse dependencies, build graph, validate
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Check dependencies before registration, send errors

**Estimate**: 8-10 hours

---

#### 1.3 Incompatibility System

**Problem**: No conflict detection. Incompatible mods can run together causing crashes.

**Solution**: Implement three types of incompatibility:

**Schema**:
```json
"incompatible_mods": {
  "mod.id": {"min_version": "1.0", "max_version": "2.0"},  // Version range
  "broken.mod": ["1.5.0", "1.5.1"],  // Specific versions
  "completely.bad.mod": true  // Complete ban
}
```

**Behavior**:
- Parse incompatibilities during mod discovery
- Build incompatibility matrix
- Check if any discovered mods match incompatibility criteria
- **Mutual incompatibility** (both mods list each other): Disable BOTH
- **One-way incompatibility** (only Mod A lists Mod B): Disable ONLY Mod A
- Send IPC warning to disabled mods

**Changes Required**:
- [src/framework_core/include/mod_registry.h](../../src/framework_core/include/mod_registry.h): Add `incompatible_mods` field
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Parse incompatibilities, build matrix, detect conflicts
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Auto-disable conflicting mods, send warnings

**Estimate**: 8-10 hours

---

#### 1.4 Runtime Enablement

**Problem**: Cannot disable mods at runtime. No conflict resolution mechanism.

**Solution**: Implement runtime enablement system:

**Schema**:
```json
"enabled": true
```

**Capabilities**:
- In-memory tracking: `discovered_mods_[mod_id].enabled`
- File persistence: Write `"enabled": false` to `ap_config.json` when disabled
- Framework can disable any mod (C++ library)
- Framework mod can disable any mod (special privilege as priority client)
- Regular mods cannot disable other mods

**Use Cases**:
1. Auto-disable on dependency missing
2. Auto-disable on incompatibility detected
3. Manual disable via framework command (future)
4. User edits config file manually

**Changes Required**:
- [src/framework_core/include/mod_registry.h](../../src/framework_core/include/mod_registry.h): Add `enabled` field to `ModMetadata`
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Parse `enabled`, respect at discovery
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Add `disable_mod()` API, write to file

**Estimate**: 6-8 hours

---

#### 1.5 Multi-Slot APCapabilities

**Problem**: Hardcoded `APCapabilities.json` prevents multiple Palworld players in multiworld.

**Solution**: Use slot-based filenames:

**Filename Format**:
```
APCapabilities_<slot_name>.json
```

**Source**:
```json
// framework_config.json
{
  "slot_name": "Player1"
}
```

**Generated Files**:
```
APCapabilities_Player1.json
APCapabilities_Player2.json
APCapabilities_P1.json
```

**Behavior**:
- Read `slot_name` from config
- Generate filename: `APCapabilities_<slot_name>.json`
- Log full absolute path when generating
- Never auto-delete old files (user manages)
- Overwrite same file if re-generated

**Changes Required**:
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua): Read `slot_name`, build dynamic filename
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp): Accept filename parameter
- [src/framework_core/src/lua_bindings.cpp](../../src/framework_core/src/lua_bindings.cpp): Update bindings to accept filename

**Estimate**: 3-4 hours

---

#### 1.6 Log Routing to Framework Mod

**Problem**: Users can't see framework errors. Must check log file for debugging.

**Solution**: Route important logs via IPC to framework mod:

**Architecture**:
```
C++ Logger
  ↓ (filter by log_mode)
IPC Message (type: "log")
  ↓
Framework Mod (priority client)
  ↓ (print to console)
UE4SS Console
  ↓ (automatic)
ue4ss/UE4SS.log
```

**Configuration**:
```json
// framework_config.json
{
  "log_mode": "framework_only",  // "minimal", "framework_only", "all"
  "log_verbosity": "info"  // "debug", "info", "warning", "error"
}
```

**Log Modes**:
- `"minimal"`: Errors only
- `"framework_only"`: Framework INFO/WARNING/ERROR (default)
- `"all"`: All logs from framework and mods

**Changes Required**:
- [APFramework/framework_config.json](../../APFramework/framework_config.json): Add `log_mode`, `log_verbosity`
- [src/framework_core/src/logger.cpp](../../src/framework_core/src/logger.cpp): Filter logs by mode, send IPC messages
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Route "log" IPC messages to framework mod
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua): Poll for log messages, print to console

**Estimate**: 6-8 hours

---

#### 1.7 lunajson Integration

**Problem**: Custom JSON implementations are fragile and break on complex structures.

**Current Issues**:
- [ap_client.lua](../../src/lua_client/ap_client.lua) lines 8-96: Custom regex-based JSON (can't handle nested structures, unicode)
- [config.lua](../../APFramework/Scripts/config.lua) lines 31-37: Hard-coded regex for each field (breaks on format changes)

**Solution**: Use lunajson throughout Lua codebase:

**Benefits**:
- ✅ Handles ALL valid JSON (nested objects, arrays, unicode, escapes)
- ✅ Battle-tested library (widely used)
- ✅ Cleaner code (remove hundreds of lines of regex)
- ✅ Better error messages (detailed parse errors)
- ✅ Easier maintenance (update library, not custom code)
- ✅ Complements `nlohmann/json` in C++ codebase

**Use Cases**:
1. **Client library** - Automatic encode/decode for mod capabilities
2. **Config management** - Parse `framework_config.json` cleanly
3. **Future Lua components** - All JSON operations use lunajson

**Changes Required**:
- Add `lunajson.lua` (or `lunajson/` directory) to project
- [src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua): Replace lines 8-96 with lunajson
- [APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua): Replace lines 31-37 with lunajson
- Update any other Lua files that parse/generate JSON

**Estimate**: 4-6 hours

---

### Phase 2: Validation & Polish

**See**: [ImplementationPlan/Phase02_ValidationPolish.md](ImplementationPlan/Phase02_ValidationPolish.md)

#### 2.1 Zero-Tolerance Capabilities Validation

**Problem**: Validation exists but doesn't enforce blocking. Invalid capabilities can be generated.

**Solution**: Enforce ZERO TOLERANCE for conflicts:

**Validation Rules**:
1. Item ID collisions → BLOCK
2. Location ID collisions → BLOCK
3. Region name collisions → BLOCK
4. Dependency cycles → BLOCK
5. Missing dependencies → BLOCK (already handled by dependency system)
6. Incompatibility conflicts → BLOCK (already handled by incompatibility system)

**Behavior on Conflict**:
- **BLOCK** generation (do not create file)
- **LOG** all conflicts with full details
- **BROADCAST** errors to all mods via IPC
- **DISPLAY** errors in UE4SS console (via framework mod)
- **REQUIRE** user/developer resolution

**Error Message Example**:
```
[APFramework] [ERROR] Capability generation FAILED
[APFramework] [ERROR] Item ID collision detected:
  - Mod: author1.palworld.mod1 claims ID 100042 (name: "Super Pickaxe")
  - Mod: author2.palworld.mod2 claims ID 100042 (name: "Magic Sword")
[APFramework] [ERROR] Resolution: Mods must use unique ID ranges
[APFramework] [ERROR] APCapabilities_<slot>.json NOT generated
```

**Changes Required**:
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp): Explicit collision checks, enforce blocking
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp): Check validation before generation, broadcast errors

**Estimate**: 6-8 hours

---

#### 2.2 Python World Schema Verification

**Problem**: Python world is more advanced than framework. Need to verify compatibility.

**Solution**: Test and update Python world for new schema:

**Tests Required**:
1. Multi-slot capability loading (`APCapabilities_<slot>.json`)
2. Dependency schema parsing
3. Incompatibility schema parsing
4. Custom field extensibility
5. Conflict detection

**Changes May Be Needed**:
- [worlds/palworld/options.py](../../worlds/palworld/options.py): Multi-file capability option
- [worlds/palworld/mod_interface.py](../../worlds/palworld/mod_interface.py): Verify schema matches C++ schema exactly
- [worlds/palworld/__init__.py](../../worlds/palworld/__init__.py): Handle multi-slot files

**Estimate**: 4-6 hours

---

#### 2.3 Comprehensive Testing

**Problem**: Complex edge cases (circular dependencies, mutual conflicts) not tested.

**Solution**: Create comprehensive test suite:

**Test Areas**:
1. Dependency validation (missing deps, version mismatches, cascade disabling)
2. Incompatibility detection (three types, mutual vs one-way)
3. Runtime enablement (auto-disable, file persistence)
4. Multi-slot capabilities (multiple files, naming)
5. Log routing (filtering, message delivery)
6. lunajson integration (complex JSON structures)
7. Zero-tolerance validation (all conflict types)

**Test Types**:
- Unit tests (C++ validation logic)
- Integration tests (full framework flow with mock mods)
- Manual tests (UE4SS environment with real mods)

**Estimate**: 8-10 hours

---

#### 2.4 Documentation Updates

**Problem**: Current docs don't reflect new features.

**Solution**: Update all documentation:

**Documents to Update**:
- [README.md](../../README.md): Installation, features overview, migration guide
- [ARCHITECTURE.md](../../docs/ARCHITECTURE.md): Dependency/incompatibility systems
- [BUILD.md](../../docs/BUILD.md): lunajson integration
- [CHANGES.md](../../CHANGES.md): Changelog for v2 schema
- New: `docs/SCHEMA_V2.md` - Complete schema reference
- New: `docs/MIGRATION_V1_TO_V2.md` - Migration guide

**Estimate**: 4-6 hours

---

### Phase 3: Nice-to-Have Enhancements

**See**: [ImplementationPlan/Phase03_NiceToHave.md](ImplementationPlan/Phase03_NiceToHave.md)

#### 3.1 Semantic Version Validation

**Problem**: No runtime version compatibility warnings.

**Solution**: Implement semver parsing and validation:

**Features**:
- Parse `"1.2.3"` format
- Parse version ranges `">=1.0.0 <2.0.0"`
- Check game version compatibility (`supported_game_versions`)
- Check incompatible mod versions
- Warn (don't block) on mismatches

**Changes Required**:
- Create `semver.h/cpp` utility
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Validate versions during registration, log warnings

**Estimate**: 8-10 hours

---

#### 3.2 Static .lib Build Option

**Problem**: C++ library is DLL-only, requires deployment.

**Solution**: Provide static library build option:

**Features**:
- CMake option: `BUILD_STATIC_CLIENT_LIB`
- Build both `APClientLib.dll` and `APClientLib.lib`
- Developers choose which to use

**Changes Required**:
- [src/client_lib/CMakeLists.txt](../../src/client_lib/CMakeLists.txt): Add static library target
- Documentation: Update with linking instructions

**Estimate**: 2-3 hours

---

#### 3.3 Advanced Error Handling

**Problem**: Many error paths just log and return. No retry logic.

**Solution**: Add retry logic and recovery:

**Features**:
- IPC connection: Retry with exponential backoff
- AP connection: Automatic reconnection attempts
- Message send failures: Queue and retry
- Better error messages with context and suggestions

**Changes Required**:
- [src/client_lib/src/ipc_client.cpp](../../src/client_lib/src/ipc_client.cpp): Retry logic
- [src/framework_core/src/ap_client.cpp](../../src/framework_core/src/ap_client.cpp): Reconnection logic
- [src/framework_core/src/ipc_server.cpp](../../src/framework_core/src/ipc_server.cpp): Handle disconnects gracefully

**Estimate**: 8-12 hours

---

## Implementation Strategy

### Approach

**Incremental Development**:
- Implement one feature at a time
- Test after each feature
- Commit working code frequently
- Maintain backward compatibility where possible (schema versioning)

**Testing Strategy**:
- Unit tests for validation logic (dependencies, incompatibilities)
- Integration tests with mock mods
- Manual testing in UE4SS environment
- Regression testing after each phase

**Documentation Updates**:
- Update docs alongside code changes
- Keep [CHANGES.md](../../CHANGES.md) log of modifications
- Update examples to use new features
- Write migration guide for mod developers

### Dependencies Between Phases

**Phase 1 Internal Dependencies**:
- **Dual-role framework mod** (1.1) must come first → enables log routing (1.6)
- **Dependency system** (1.2) and **Incompatibility system** (1.3) → enable **Runtime enablement** (1.4)
- **lunajson** (1.7) should be early → improves all Lua code

**Recommended Phase 1 Order**:
1. lunajson integration (1.7) - Improves all subsequent Lua work
2. Multi-slot capabilities (1.5) - Simple, enables multi-player
3. Framework mod dual-role (1.1) - Required for log routing
4. Dependency system (1.2) - Foundation for safety
5. Incompatibility system (1.3) - Foundation for safety
6. Runtime enablement (1.4) - Depends on 1.2 and 1.3
7. Log routing (1.6) - Depends on 1.1

**Phase 1 → Phase 2**:
- Phase 2 depends on Phase 1 schema (dependencies, incompatibilities) for validation
- Zero-tolerance validation (2.1) requires dependency/incompatibility systems (1.2, 1.3)

**Phase 2 → Phase 3**:
- Phase 3 is fully independent
- Can be implemented in any order
- Can be deferred to post-release

---

## Risk Assessment

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking changes to IPC protocol | Low | High | Version IPC messages, maintain compatibility layer |
| Lua bindings memory leaks | Medium | Medium | Careful userdata management, comprehensive testing |
| Dependency cycle detection bugs | Medium | High | Robust graph algorithms, extensive test cases |
| Incompatibility logic edge cases | Medium | Medium | Clear specification, comprehensive testing |
| lunajson integration issues | Low | Low | Well-tested library, simple integration |
| Multi-slot filename conflicts | Low | Medium | Log full paths, clear user documentation |

### Schedule Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Phase 1 scope creep | Medium | Medium | Strict feature list, defer extras to Phase 3 |
| Testing takes longer than expected | High | Medium | Plan testing time, prepare test mods early |
| Python world compatibility issues | Medium | High | Early verification, coordinate with Python devs |
| Documentation delays | Medium | Low | Start docs early, write as you code |

### Compatibility Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing mods | Medium | High | Schema versioning (v1 → v2), backward compatibility layer |
| UE4SS API changes | Low | High | Pin UE4SS version in docs, test with specific versions |
| Windows API issues | Low | Medium | Use stable Windows APIs, test on Win10/11 |
| Python world schema mismatch | Medium | High | Early verification (Phase 2.2), align schemas |

---

## Success Criteria

### Phase 1 Success Criteria

- [ ] Framework mod registers as priority client with mod_id `archipelago.palworld.framework`
- [ ] Framework mod receives log messages via IPC
- [ ] Framework errors/warnings visible in UE4SS console
- [ ] Mod with missing dependency denied registration
- [ ] Cascade disabling works (Mod A disabled → Mod B depending on A disabled)
- [ ] Conflicting mods auto-disabled (mutual and one-way)
- [ ] Mods can be disabled at runtime via `enabled` field
- [ ] Capability files use slot-based naming: `APCapabilities_<slot>.json`
- [ ] Multiple players can generate distinct capability files
- [ ] All Lua code uses lunajson for JSON operations
- [ ] Complex JSON structures (nested objects, unicode) handled correctly
- [ ] All Phase 1 features documented

### Phase 2 Success Criteria

- [ ] Capability generation BLOCKED if item/location/region ID conflicts
- [ ] Detailed conflict reports displayed in UE4SS console
- [ ] Python world successfully loads multi-slot capability files
- [ ] Python world correctly parses dependency/incompatibility schemas
- [ ] All edge cases tested (circular dependencies, mutual conflicts, etc.)
- [ ] Mod developers have clear migration guide from v1 to v2 schema
- [ ] All Phase 2 features documented

### Phase 3 Success Criteria

- [ ] Version compatibility warnings displayed at startup
- [ ] Static .lib available for C++ mods
- [ ] Transient IPC/AP connection failures auto-retry
- [ ] Error messages include actionable suggestions
- [ ] All features tested and working
- [ ] Complete documentation set

---

## Timeline Estimates

### Phase 1: Critical Safety Features

| Feature | Estimate | Priority |
|---------|----------|----------|
| 1.7 lunajson integration | 4-6 hours | Do first |
| 1.5 Multi-slot capabilities | 3-4 hours | Easy win |
| 1.1 Framework mod dual-role | 4-6 hours | Required for 1.6 |
| 1.2 Dependency system | 8-10 hours | Foundation |
| 1.3 Incompatibility system | 8-10 hours | Foundation |
| 1.4 Runtime enablement | 6-8 hours | Depends on 1.2, 1.3 |
| 1.6 Log routing | 6-8 hours | Depends on 1.1 |

**Total Phase 1**: 39-52 hours (5-7 days)

### Phase 2: Validation & Polish

| Feature | Estimate |
|---------|----------|
| 2.1 Zero-tolerance validation | 6-8 hours |
| 2.2 Python world verification | 4-6 hours |
| 2.3 Comprehensive testing | 8-10 hours |
| 2.4 Documentation updates | 4-6 hours |

**Total Phase 2**: 22-30 hours (3-4 days)

### Phase 3: Nice-to-Have Enhancements

| Feature | Estimate |
|---------|----------|
| 3.1 Semantic version validation | 8-10 hours |
| 3.2 Static .lib build | 2-3 hours |
| 3.3 Advanced error handling | 8-12 hours |

**Total Phase 3**: 18-25 hours (3-4 days)

**Grand Total**: 79-107 hours (11-15 days)

---

## Backward Compatibility Plan

### Schema Versioning

**Current (v1)** - Implicit schema:
```json
{
  "mod_id": "SomeMod",
  "items": [...],
  "locations": [...],
  "regions": [...]
}
```

**Proposed (v2)** - Explicit schema with safety features:
```json
{
  "schema_version": 2,
  "mod_id": "author.game.mod",
  "version": "1.2.3",
  "display_name": "My Mod",
  "description": "Does cool things",
  "supported_game_versions": ">=0.3.0 <0.4.0",

  "dependencies": {
    "required.mod.id": {"min_version": "1.0.0", "max_version": "2.0.0"}
  },

  "incompatible_mods": {
    "conflicting.mod.id": true
  },

  "enabled": true,

  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...],
    "custom_fields": {...}
  }
}
```

### Compatibility Strategy

1. **Auto-detect schema version**:
   - If `schema_version` field present → use v2 parser
   - If not present → use v1 parser (backward compatible)

2. **Upgrade v1 to v2 internally**:
   - Fill in defaults for missing fields
   - Generate `display_name` from `mod_id`
   - `version` defaults to `"0.0.0"`
   - `enabled` defaults to `true`
   - No dependencies or incompatibilities

3. **Deprecation timeline**:
   - v1 supported for 6 months
   - Warning logged for v1 schemas
   - Eventually require v2 for new features

### Migration Guide

**For Mod Developers**:
1. Add `"schema_version": 2` to `ap_config.json`
2. Rename `mod_id` to follow `author.game.mod` format
3. Add `version`, `display_name`, `description`
4. Wrap capabilities in `"capabilities": {}` object
5. Optionally add dependencies and incompatibilities
6. Optionally add `enabled` field (defaults to `true`)

**Example Migration**:
```json
// Before (v1)
{
  "mod_id": "ChestShuffle",
  "items": [...]
}

// After (v2)
{
  "schema_version": 2,
  "mod_id": "author.palworld.chestshuffle",
  "version": "1.0.0",
  "display_name": "Chest Shuffle",
  "description": "Shuffles chest contents into AP",
  "enabled": true,
  "capabilities": {
    "items": [...]
  }
}
```

---

## Post-Implementation Plan

### Phase 1 Release (Beta)

**After Phase 1 Complete**:
1. Internal testing with example mods
2. Beta release to modding community
3. Gather feedback on new features
4. Fix critical bugs
5. Iterate on UX

### Phase 2 Release (Stable)

**After Phase 2 Complete**:
1. Full validation testing
2. Python world integration testing
3. Stable release (v1.0)
4. Community announcement
5. Support for early adopters

### Phase 3 Release (Mature)

**After Phase 3 Complete**:
1. Advanced features available
2. Enhanced release (v1.1+)
3. Long-term stability
4. Collect feature requests for v2.0

---

## Critical Recommendation

### DO NOT RELEASE BEFORE PHASE 1 COMPLETE

**Why Phase 1 is Non-Negotiable**:

The features in Phase 1 are not "nice-to-have" - they are **critical for safety and usability**. Releasing without them will result in:

1. **User Frustration**: Can't see framework errors (blind debugging)
2. **Mod Conflicts**: Incompatible mods run together → crashes/corruption
3. **Missing Dependencies**: Mods load without requirements → crashes
4. **No Multi-Player**: Cannot support multiple Palworld players (hardcoded single file)
5. **Poor Developer Experience**: Fragile JSON breaks on complex capabilities
6. **No Conflict Resolution**: Cannot auto-disable problematic mods

**Release Criteria**:
- ✅ Complete Phase 1 (5-7 days)
- ✅ Basic testing with example mods
- ✅ Documentation for mod developers
- 🚀 Release for beta testing

**Post-Beta**:
- Complete Phase 2 (3-4 days) for production release
- Complete Phase 3 (3-4 days) for long-term maturity

---

## Appendix A: Feature Priority Matrix

**Priority Scoring**:
- **Impact**: 1-5 (1=Low, 5=Critical)
- **Effort**: 1-5 (1=Low, 5=High)
- **Priority**: Impact × (6 - Effort)

| Feature | Impact | Effort | Score | Phase |
|---------|--------|--------|-------|-------|
| Dependency system | 5 | 3 | 15 | 1 |
| Incompatibility system | 5 | 3 | 15 | 1 |
| Runtime enablement | 5 | 3 | 15 | 1 |
| Multi-slot capabilities | 5 | 1 | 25 | 1 |
| Log routing | 5 | 3 | 15 | 1 |
| Framework dual-role | 4 | 2 | 16 | 1 |
| lunajson integration | 4 | 2 | 16 | 1 |
| Zero-tolerance validation | 4 | 2 | 16 | 2 |
| Python world verification | 4 | 2 | 16 | 2 |
| Comprehensive testing | 4 | 3 | 12 | 2 |
| Documentation updates | 3 | 2 | 12 | 2 |
| Semantic version validation | 2 | 3 | 6 | 3 |
| Static .lib | 2 | 1 | 10 | 3 |
| Advanced error handling | 3 | 4 | 6 | 3 |

---

## Appendix B: File Change Summary

### Phase 1 Files Modified

**Framework Mod**:
- [APFramework/Scripts/main.lua](../../APFramework/Scripts/main.lua)
- [APFramework/Scripts/config.lua](../../APFramework/Scripts/config.lua)
- [APFramework/framework_config.json](../../APFramework/framework_config.json)

**Framework Core**:
- [src/framework_core/include/mod_registry.h](../../src/framework_core/include/mod_registry.h)
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp)
- [src/framework_core/src/framework_core.cpp](../../src/framework_core/src/framework_core.cpp)
- [src/framework_core/src/logger.cpp](../../src/framework_core/src/logger.cpp)
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp)
- [src/framework_core/src/lua_bindings.cpp](../../src/framework_core/src/lua_bindings.cpp)
- [src/framework_core/src/message_router.cpp](../../src/framework_core/src/message_router.cpp)

**Client Library**:
- [src/lua_client/ap_client.lua](../../src/lua_client/ap_client.lua)

**New Files**:
- `lunajson.lua` (or `lunajson/` directory)

**Estimated Lines Changed**: ~1200-1500 lines

---

### Phase 2 Files Modified

**Framework Core**:
- [src/framework_core/src/capabilities_generator.cpp](../../src/framework_core/src/capabilities_generator.cpp)

**Python World**:
- [worlds/palworld/options.py](../../worlds/palworld/options.py)
- [worlds/palworld/mod_interface.py](../../worlds/palworld/mod_interface.py)
- [worlds/palworld/__init__.py](../../worlds/palworld/__init__.py)

**Documentation**:
- [README.md](../../README.md)
- [ARCHITECTURE.md](../../docs/ARCHITECTURE.md)
- [BUILD.md](../../docs/BUILD.md)
- [CHANGES.md](../../CHANGES.md)

**New Files**:
- `docs/SCHEMA_V2.md`
- `docs/MIGRATION_V1_TO_V2.md`
- Test files

**Estimated Lines Changed**: ~600-800 lines (excluding tests)

---

### Phase 3 Files Modified

**Framework Core**:
- [src/framework_core/src/mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp)
- [src/framework_core/src/ap_client.cpp](../../src/framework_core/src/ap_client.cpp)
- [src/framework_core/src/ipc_server.cpp](../../src/framework_core/src/ipc_server.cpp)

**Client Library**:
- [src/client_lib/CMakeLists.txt](../../src/client_lib/CMakeLists.txt)
- [src/client_lib/src/ipc_client.cpp](../../src/client_lib/src/ipc_client.cpp)

**New Files**:
- `src/framework_core/include/semver.h`
- `src/framework_core/src/semver.cpp`

**Estimated Lines Changed**: ~800-1200 lines

---

## Conclusion

This redesign plan provides a clear, phased approach to evolving the APFramework from its current solid foundation (70% aligned, 45% complete) to a **production-ready, safety-first implementation** (100% aligned) that fully realizes the intended design.

**Key Takeaways**:

1. **Phase 1 is CRITICAL**: Do not release without it. These features prevent crashes, enable multi-player, and make debugging possible.

2. **Clear Priorities**: Multi-slot (easiest) → lunajson → dual-role → dependencies → incompatibilities → enablement → log routing

3. **Solid Foundation**: Current implementation is well-architected. Missing features are additive, not requiring major refactoring.

4. **Realistic Timeline**: 11-15 days for complete implementation across all 3 phases.

5. **Backward Compatibility**: Schema versioning ensures existing mods continue working during transition.

**Next Steps**:

1. ✅ Review and approve this plan
2. ✅ Review detailed phase documents (Phase01, Phase02, Phase03)
3. ⏭️ Begin Phase 1 implementation
4. ⏭️ Beta release after Phase 1
5. ⏭️ Stable release after Phase 2
6. ⏭️ Enhanced release after Phase 3

**Estimated Completion**: 11-15 days for all phases, ready for beta testing after Phase 1 (5-7 days).

---

**End of Redesign Plan Overview**