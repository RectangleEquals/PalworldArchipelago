# APFramework Redesign Plan - Overview

**Document Version**: 1.0
**Date**: 2026-01-02
**Status**: Planning

---

## Executive Summary

This document outlines the roadmap for evolving the current APFramework implementation to fully align with the intended design as specified in [DESIGN_AND_FLOW.md](DESIGN_AND_FLOW.md). Based on the gap analysis in [CURRENT_ANALYSIS.md](CURRENT_ANALYSIS.md), we have identified 10 key improvements organized into 3 phases.

### Current State

- **Architecture**: 95% aligned with intended design
- **Core Features**: Fully functional (IPC, AP client, mod discovery, capabilities)
- **Missing Features**: Metadata schema, console logging, validation
- **Quality**: Production-ready core, needs polish

### Target State

- **100% design alignment** with all intended features implemented
- **Rich mod metadata** with versioning and compatibility checking
- **Enhanced user experience** with console logging and better error messages
- **Complete documentation** and examples for mod developers
- **Validated and tested** with real mods

### Timeline Estimate

- **Phase 1** (Critical): 2-3 days (12-17 hours)
- **Phase 2** (Important): 2-3 days (13-19 hours)
- **Phase 3** (Nice-to-Have): 3-4 days (18-25 hours)
- **Total**: 7-10 days (43-61 hours)

---

## Phase Overview

### Phase 1: Critical Features (Foundation)

**Goal**: Implement features required for a user-friendly first release

**Duration**: 2-3 days (12-17 hours)

**Features**:
1. Framework→UE4SS console logging (message pump system)
2. Extended mod metadata schema (versioning, compatibility)
3. Mod enablement checking (respect UE4SS enabled.txt)

**Deliverables**:
- Users can see critical framework messages in UE4SS console
- Mods can declare versions and incompatibilities
- Framework only discovers enabled mods
- Updated documentation

**Success Criteria**:
- Framework errors/warnings visible in UE4SS.log
- Mod metadata validation working
- No false registration timeouts from disabled mods

---

### Phase 2: Important Improvements (Polish)

**Goal**: Improve developer experience and robustness

**Duration**: 2-3 days (13-19 hours)

**Features**:
4. Mod ID format validation (`author.game.mod`)
5. Client library logging support
6. Load order documentation and validation
7. C++ mod example with delayed registration

**Deliverables**:
- Enforced mod ID namespacing
- Mods can integrate with framework logging
- Clear documentation on UE4SS configuration
- Working C++ mod example

**Success Criteria**:
- Invalid mod IDs rejected with clear error
- Client library logs to framework log file
- Developers can follow docs to set up load order
- C++ mod example compiles and works

---

### Phase 3: Nice-to-Have Enhancements (Future)

**Goal**: Add advanced features for ecosystem maturity

**Duration**: 3-4 days (18-25 hours)

**Features**:
8. Semantic version validation and compatibility checking
9. Static .lib build option for C++ mods
10. Advanced error handling and recovery

**Deliverables**:
- Runtime version compatibility validation
- Both static and dynamic library builds
- Robust error recovery mechanisms
- Enhanced error messages

**Success Criteria**:
- Framework warns about version mismatches
- C++ mods can link statically (no DLL deployment)
- Transient failures handled gracefully

---

## Detailed Phase Plans

### Phase 1: Critical Features

**See**: [ImplementationPlan/Phase01_CriticalFeatures.md](ImplementationPlan/Phase01_CriticalFeatures.md)

#### 1.1 Framework→UE4SS Console Logging

**Problem**: Critical framework errors are invisible to users (only in log file).

**Solution**: Implement message pump system:
- Add `MessageQueue<LogEntry>` to Logger for important messages
- Important = ERROR, WARNING, and key INFO events
- Expose `get_pending_log_messages()` to Lua
- Lua polls every tick and prints to UE4SS console

**Changes Required**:
- [logger.h](../../src/framework_core/include/logger.h): Add `important_messages_` queue
- [logger.cpp](../../src/framework_core/src/logger.cpp): Queue important messages
- [lua_bindings.cpp](../../src/framework_core/src/lua_bindings.cpp): Expose `get_log_messages()`
- [main.lua](../../APFramework/Scripts/main.lua): Poll and print messages

**Estimate**: 4-6 hours

#### 1.2 Extended Mod Metadata Schema

**Problem**: No versioning, compatibility checking, or descriptive metadata.

**Solution**: Extend `ap_config.json` schema:

```json
{
  "mod_id": "author.game.mod",
  "version": "1.2.3",
  "display_name": "My Awesome Mod",
  "description": "Does cool things with Palworld",
  "supported_game_versions": ">=0.3.0 <0.4.0",
  "incompatible_mods": [
    {
      "mod_id": "other.author.conflicting.mod",
      "versions": ">=2.0.0"
    }
  ],
  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
}
```

**Changes Required**:
- [mod_registry.h](../../src/framework_core/include/mod_registry.h): Extend `ModMetadata` struct
- [mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Parse new fields
- Add validation: required fields, format checks
- Update APCapabilities.json to include metadata

**Estimate**: 6-8 hours

#### 1.3 Mod Enablement Check

**Problem**: Framework discovers disabled mods, waits for registration timeout.

**Solution**: Check UE4SS enablement before discovery:
- Look for `enabled.txt` in mod folder (UE4SS convention)
- If not present, skip mod during discovery
- Log skipped mods at DEBUG level

**Changes Required**:
- [mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Add `is_mod_enabled()` check
- Update `discover_mods()` to filter disabled mods

**Estimate**: 2-3 hours

---

### Phase 2: Important Improvements

**See**: [ImplementationPlan/Phase02_ImportantImprovements.md](ImplementationPlan/Phase02_ImportantImprovements.md)

#### 2.1 Mod ID Format Validation

**Problem**: No enforcement of namespaced mod IDs.

**Solution**: Validate format during registration:
- Regex: `^[a-z0-9_]+\.[a-z0-9_]+\.[a-z0-9_]+$`
- Reject invalid mod_ids with clear error message
- Suggest correct format in error

**Changes Required**:
- [mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Add `validate_mod_id()` function
- Call during registration, reject if invalid
- Return error via IPC to mod

**Estimate**: 2 hours

#### 2.2 Client Library Logging Support

**Problem**: Mods can't easily integrate with framework logging.

**Solution**: Add logging callbacks to client libraries:

**C++ Library**:
```cpp
void ap_client_set_log_callback(APClientHandle handle,
                                 void (*callback)(const char* level, const char* message, void* user_data),
                                 void* user_data);
```

**Lua Library**:
```lua
client.log = function(level, message)
    -- Forward to framework via special IPC message
end
```

**Changes Required**:
- [ap_client_lib.h](../../src/client_lib/include/ap_client_lib.h): Add log callback API
- [ap_client_lib.cpp](../../src/client_lib/src/ap_client_lib.cpp): Implement callback
- [ap_client.lua](../../src/lua_client/ap_client.lua): Add log method
- Framework: Handle "log" IPC message type

**Estimate**: 3-4 hours

#### 2.3 Load Order Documentation

**Problem**: Users don't know how to configure UE4SS load order.

**Solution**: Document configuration process:
- How to edit `ue4ss/Mods/mods.txt`
- Ensure APFramework loads first
- Example configurations
- Troubleshooting common issues

**Changes Required**:
- [README.md](../../README.md): Add "Installation" section
- Create `docs/LOAD_ORDER.md` with detailed instructions
- Update [BUILD.md](../../docs/BUILD.md) with deployment steps

**Estimate**: 2-3 hours

#### 2.4 C++ Mod Example

**Problem**: No reference for C++ mod developers.

**Solution**: Create complete C++ mod example:
- Uses `ap_client_lib`
- Shows delayed registration pattern
- Demonstrates item granting and location checking
- Includes CMakeLists.txt for building

**Changes Required**:
- Create `examples/cpp_mod_example/`
- Implement example mod
- Add README with build instructions

**Estimate**: 4-5 hours

---

### Phase 3: Nice-to-Have Enhancements

**See**: [ImplementationPlan/Phase03_NiceToHave.md](ImplementationPlan/Phase03_NiceToHave.md)

#### 3.1 Semantic Version Validation

**Problem**: No runtime version compatibility checking.

**Solution**: Implement semver parsing and validation:
- Parse `"1.2.3"` format
- Parse version ranges `">=1.0.0 <2.0.0"`
- Check game version compatibility
- Check incompatible mod versions
- Warn (don't block) on mismatches

**Changes Required**:
- Create `semver.h/cpp` utility
- [mod_registry.cpp](../../src/framework_core/src/mod_registry.cpp): Validate versions during registration
- Log warnings for version mismatches

**Estimate**: 8-10 hours

#### 3.2 Static .lib Build Option

**Problem**: C++ library is DLL-only, requires deployment.

**Solution**: Provide static library build:
- Add CMake option: `BUILD_STATIC_CLIENT_LIB`
- Build both `APClientLib.dll` and `APClientLib.lib`
- Developers choose which to use

**Changes Required**:
- [src/client_lib/CMakeLists.txt](../../src/client_lib/CMakeLists.txt): Add static library target
- Update documentation with linking instructions

**Estimate**: 2-3 hours

#### 3.3 Advanced Error Handling

**Problem**: Many error paths just log and return.

**Solution**: Add retry logic and recovery:
- IPC connection: retry with exponential backoff
- AP connection: automatic reconnection attempts
- Message send failures: queue and retry
- Better error messages with context

**Changes Required**:
- [ipc_client.cpp](../../src/client_lib/src/ipc_client.cpp): Retry logic
- [ap_client.cpp](../../src/framework_core/src/ap_client.cpp): Reconnection
- [ipc_server.cpp](../../src/framework_core/src/ipc_server.cpp): Handle disconnects gracefully

**Estimate**: 8-12 hours

---

## Implementation Strategy

### Approach

**Incremental Development**:
- Implement one feature at a time
- Test after each feature
- Commit working code frequently
- Maintain backward compatibility where possible

**Testing Strategy**:
- Unit tests for new validation logic
- Integration tests with mock mods
- Manual testing in UE4SS environment
- Regression testing after each phase

**Documentation Updates**:
- Update docs alongside code changes
- Keep CHANGES.md log of modifications
- Update examples to use new features

### Dependencies

**Phase 1 → Phase 2**:
- Phase 2 depends on Phase 1 metadata schema (for mod ID validation)
- Can start Phase 2 logging and docs in parallel with Phase 1

**Phase 2 → Phase 3**:
- Phase 3 semver depends on Phase 1 metadata schema
- Phase 3 static lib is independent
- Phase 3 error handling is independent

**Parallelization Opportunities**:
- Console logging (1.1) can be developed independently
- Documentation (2.3) can be written anytime
- Static lib (3.2) can be developed independently

---

## Risk Assessment

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking changes to IPC protocol | Low | High | Version IPC messages, maintain compatibility |
| Lua bindings memory leaks | Medium | Medium | Careful userdata management, testing |
| Performance impact from logging | Low | Low | Only log important messages, async queuing |
| Regex validation edge cases | Medium | Low | Comprehensive test cases |
| Semver parsing complexity | Medium | Low | Use well-tested library or limit scope |

### Schedule Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Scope creep in Phase 1 | Medium | Medium | Strict feature list, defer extras to Phase 3 |
| Integration testing delays | High | Medium | Plan testing time, have test mods ready |
| Documentation takes longer | Medium | Low | Start docs early, write as you code |

### Compatibility Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing mods | Low | High | Maintain backward compatibility, version schema |
| UE4SS API changes | Low | High | Pin UE4SS version in docs, test with specific versions |
| Windows API issues | Low | Medium | Use stable Windows APIs, test on Win10/11 |

---

## Success Criteria

### Phase 1 Success Criteria

- [ ] Framework errors appear in UE4SS console and `ue4ss/UE4SS.log`
- [ ] Mod metadata includes all required fields
- [ ] Invalid metadata rejected with clear error messages
- [ ] Only enabled mods are discovered (disabled mods ignored)
- [ ] No false registration timeouts
- [ ] All Phase 1 features documented

### Phase 2 Success Criteria

- [ ] Mod IDs follow `author.game.mod` format or are rejected
- [ ] Mods can log to framework via client library
- [ ] Load order documentation complete and tested
- [ ] C++ mod example compiles and runs
- [ ] Clear error messages for common mistakes
- [ ] All Phase 2 features documented

### Phase 3 Success Criteria

- [ ] Version compatibility warnings displayed at startup
- [ ] Static .lib available for C++ mods
- [ ] Transient connection failures auto-recover
- [ ] Error messages include actionable suggestions
- [ ] All features tested and working
- [ ] Complete documentation set

---

## Post-Implementation Plan

### Testing Phase

**Week 1-2**: Internal testing
- Test with example mods
- Test all message types
- Test error cases
- Test concurrent mods
- Performance testing

**Week 3-4**: Beta testing
- Invite modders to test
- Gather feedback
- Fix critical bugs
- Iterate on UX

### Release Preparation

- Finalize documentation
- Create installation guide
- Write migration guide (if needed)
- Prepare release notes
- Tag release version

### Community Engagement

- Announce to modding community
- Provide support for early adopters
- Collect feature requests
- Plan future enhancements

---

## Appendix A: Feature Priority Matrix

### Priority Scoring

**Impact**: 1-5 (1=Low, 5=Critical)
**Effort**: 1-5 (1=Low, 5=High)
**Priority**: Impact × (6 - Effort)

| Feature | Impact | Effort | Score | Phase |
|---------|--------|--------|-------|-------|
| Console logging | 5 | 2 | 20 | 1 |
| Metadata schema | 5 | 3 | 15 | 1 |
| Enablement check | 4 | 1 | 20 | 1 |
| Mod ID validation | 3 | 1 | 15 | 2 |
| Client logging | 3 | 2 | 12 | 2 |
| Load order docs | 4 | 1 | 20 | 2 |
| C++ example | 3 | 2 | 12 | 2 |
| Semver validation | 2 | 4 | 4 | 3 |
| Static .lib | 2 | 1 | 10 | 3 |
| Error handling | 3 | 4 | 6 | 3 |

### Recommended Sequence

Based on priority scores and dependencies:

1. **Console logging** (Impact=5, Effort=2, Score=20) - Critical UX improvement
2. **Enablement check** (Impact=4, Effort=1, Score=20) - Prevents timeout issues
3. **Load order docs** (Impact=4, Effort=1, Score=20) - Unblocks users
4. **Metadata schema** (Impact=5, Effort=3, Score=15) - Foundation for other features
5. **Mod ID validation** (Impact=3, Effort=1, Score=15) - Quick win after metadata
6. **Client logging** (Impact=3, Effort=2, Score=12) - Improves mod dev experience
7. **C++ example** (Impact=3, Effort=2, Score=12) - Helps C++ mod developers
8. **Static .lib** (Impact=2, Effort=1, Score=10) - Nice-to-have for C++ mods
9. **Error handling** (Impact=3, Effort=4, Score=6) - Long-term robustness
10. **Semver validation** (Impact=2, Effort=4, Score=4) - Advanced feature

---

## Appendix B: Backward Compatibility Plan

### Schema Versioning

**Current** (v1):
```json
{
  "mod_id": "SomeMod",
  "items": [...],
  "locations": [...],
  "regions": [...]
}
```

**Proposed** (v2):
```json
{
  "schema_version": 2,
  "mod_id": "author.game.mod",
  "version": "1.2.3",
  "display_name": "My Mod",
  // ... new fields ...
  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
}
```

### Compatibility Strategy

1. **Auto-detect schema version**:
   - If `schema_version` field present → use v2 parser
   - If not present → use v1 parser (backward compatible)

2. **Upgrade v1 to v2 internally**:
   - Fill in defaults for missing fields
   - Generate display_name from mod_id
   - Version defaults to "0.0.0"

3. **Deprecation timeline**:
   - v1 supported for 6 months
   - Warning logged for v1 schemas
   - Eventually require v2

### Migration Guide

Provide clear migration instructions:
- What changed
- How to update ap_config.json
- Example before/after
- Validation tool to check schema

---

## Appendix C: File Change Summary

### Phase 1 File Changes

**Modified Files**:
- `src/framework_core/include/logger.h`
- `src/framework_core/src/logger.cpp`
- `src/framework_core/include/mod_registry.h`
- `src/framework_core/src/mod_registry.cpp`
- `src/framework_core/src/lua_bindings.cpp`
- `APFramework/Scripts/main.lua`

**New Files**:
- None (all modifications)

**Estimated Lines Changed**: ~500-700 lines

---

### Phase 2 File Changes

**Modified Files**:
- `src/client_lib/include/ap_client_lib.h`
- `src/client_lib/src/ap_client_lib.cpp`
- `src/lua_client/ap_client.lua`
- `src/framework_core/src/mod_registry.cpp`
- `README.md`

**New Files**:
- `docs/LOAD_ORDER.md`
- `examples/cpp_mod_example/main.cpp`
- `examples/cpp_mod_example/CMakeLists.txt`
- `examples/cpp_mod_example/README.md`

**Estimated Lines Changed**: ~400-600 lines

---

### Phase 3 File Changes

**Modified Files**:
- `src/framework_core/src/mod_registry.cpp`
- `src/client_lib/CMakeLists.txt`
- `src/client_lib/src/ipc_client.cpp`
- `src/framework_core/src/ap_client.cpp`
- `src/framework_core/src/ipc_server.cpp`

**New Files**:
- `src/framework_core/include/semver.h`
- `src/framework_core/src/semver.cpp`

**Estimated Lines Changed**: ~800-1200 lines

---

## Conclusion

This redesign plan provides a clear, phased approach to evolving the APFramework from its current solid foundation to a complete, polished implementation that fully aligns with the intended design.

**Key Takeaways**:
- Current implementation is 85% complete
- 3 phases cover remaining 15% + polish
- Critical features (Phase 1) make framework user-friendly
- Important improvements (Phase 2) enhance developer experience
- Nice-to-have features (Phase 3) add maturity

**Next Steps**:
1. Review and approve this plan
2. Create detailed task breakdowns for Phase 1
3. Set up testing environment
4. Begin implementation of Phase 1 features

**Estimated Completion**: 7-10 days for all phases, ready for beta testing.

---

**End of Redesign Plan Overview**