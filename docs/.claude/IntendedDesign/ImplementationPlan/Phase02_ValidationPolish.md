# Phase 2: Validation & Polish - Implementation Plan

**Document Version**: 1.0
**Date**: 2026-01-02
**Status**: Ready for Implementation
**Parent Document**: [REDESIGN_PLAN_OVERVIEW.md](../REDESIGN_PLAN_OVERVIEW.md)
**Previous Phase**: [Phase01_CriticalSafetyFeatures.md](Phase01_CriticalSafetyFeatures.md)

---

## Overview

Phase 2 implements **validation enforcement** and **ecosystem polish** to ensure the framework operates robustly and provides excellent developer experience. This phase builds on Phase 1's critical safety features by:
- Enforcing zero-tolerance for conflicts (no invalid capabilities)
- Verifying Python world compatibility (framework ↔ world alignment)
- Comprehensive testing (edge cases, stress tests)
- Documentation updates (migration guides, examples)

**Total Estimated Time**: 22-30 hours (3-4 days)

**Dependencies**: Requires Phase 1 completion (dependency/incompatibility systems)

---

## Feature Roadmap

### Implementation Order

1. **Zero-tolerance capabilities validation** (6-8 hours) → Block invalid capability generation
2. **Python world schema verification** (4-6 hours) → Ensure compatibility
3. **Comprehensive testing** (8-10 hours) → Edge cases, stress tests
4. **Documentation updates** (4-6 hours) → Migration guides, examples

---

## Feature 2.1: Zero-Tolerance Capabilities Validation

**Priority**: 1st (Critical for Data Integrity)
**Estimate**: 6-8 hours
**Dependencies**: Phase 1 (dependencies, incompatibilities)

### Problem Statement

**Current**: Validation exists but doesn't enforce blocking. The `validate()` method returns errors but capability generation proceeds anyway, allowing conflicting capabilities to reach the AP server.

**Issues**:
- Item ID collisions (two mods claim same ID) → server errors
- Location ID collisions → broken location checks
- Region name collisions → routing errors
- No explicit collision detection (relies on implicit map overwrites)
- Validation failures logged but not enforced

### Solution

Implement **ZERO TOLERANCE** for all conflicts:

**Validation Rules**:
1. Item ID collisions → BLOCK generation
2. Location ID collisions → BLOCK generation
3. Region name collisions → BLOCK generation
4. Dependency cycles → BLOCK generation
5. Missing dependencies → Already handled by Phase 1.2
6. Incompatibility conflicts → Already handled by Phase 1.3

**Behavior on Conflict**:
- **BLOCK** capability file generation
- **LOG** all conflicts with full details
- **BROADCAST** errors to all mods via IPC
- **DISPLAY** errors in UE4SS console (via framework mod)
- **REQUIRE** user/developer resolution before proceeding

### Implementation Steps

#### Step 1: Explicit Collision Detection

**File**: [src/framework_core/src/capabilities_generator.cpp](../../../../src/framework_core/src/capabilities_generator.cpp)

**Add collision detection**:
```cpp
struct ValidationError {
    enum Type {
        ITEM_ID_COLLISION,
        LOCATION_ID_COLLISION,
        REGION_NAME_COLLISION,
        DEPENDENCY_CYCLE,
        INVALID_DATA
    };

    Type type;
    std::string message;
    std::vector<std::string> affected_mods;
};

class CapabilitiesValidator {
public:
    std::vector<ValidationError> validate(const std::vector<ModMetadata>& mods) {
        std::vector<ValidationError> errors;

        check_item_id_collisions(mods, errors);
        check_location_id_collisions(mods, errors);
        check_region_name_collisions(mods, errors);
        check_dependency_cycles(mods, errors);

        return errors;
    }

private:
    void check_item_id_collisions(const std::vector<ModMetadata>& mods, std::vector<ValidationError>& errors) {
        std::map<int, std::vector<std::string>> item_id_map;

        // Build map of item IDs → mods that claim them
        for (const auto& mod : mods) {
            if (!mod.enabled) continue;

            const auto& capabilities = mod.capabilities;
            if (!capabilities.contains("items")) continue;

            for (const auto& item : capabilities["items"]) {
                int item_id = item["id"];
                item_id_map[item_id].push_back(mod.mod_id);
            }
        }

        // Detect collisions
        for (const auto& [item_id, mod_ids] : item_id_map) {
            if (mod_ids.size() > 1) {
                ValidationError error;
                error.type = ValidationError::ITEM_ID_COLLISION;
                error.message = "Item ID " + std::to_string(item_id) + " claimed by multiple mods";
                error.affected_mods = mod_ids;
                errors.push_back(error);
            }
        }
    }

    void check_location_id_collisions(const std::vector<ModMetadata>& mods, std::vector<ValidationError>& errors) {
        std::map<int, std::vector<std::string>> location_id_map;

        for (const auto& mod : mods) {
            if (!mod.enabled) continue;

            const auto& capabilities = mod.capabilities;
            if (!capabilities.contains("locations")) continue;

            for (const auto& location : capabilities["locations"]) {
                int location_id = location["id"];
                location_id_map[location_id].push_back(mod.mod_id);
            }
        }

        for (const auto& [location_id, mod_ids] : location_id_map) {
            if (mod_ids.size() > 1) {
                ValidationError error;
                error.type = ValidationError::LOCATION_ID_COLLISION;
                error.message = "Location ID " + std::to_string(location_id) + " claimed by multiple mods";
                error.affected_mods = mod_ids;
                errors.push_back(error);
            }
        }
    }

    void check_region_name_collisions(const std::vector<ModMetadata>& mods, std::vector<ValidationError>& errors) {
        std::map<std::string, std::vector<std::string>> region_name_map;

        for (const auto& mod : mods) {
            if (!mod.enabled) continue;

            const auto& capabilities = mod.capabilities;
            if (!capabilities.contains("regions")) continue;

            for (const auto& region : capabilities["regions"]) {
                std::string region_name = region["name"];
                region_name_map[region_name].push_back(mod.mod_id);
            }
        }

        for (const auto& [region_name, mod_ids] : region_name_map) {
            if (mod_ids.size() > 1) {
                ValidationError error;
                error.type = ValidationError::REGION_NAME_COLLISION;
                error.message = "Region name '" + region_name + "' claimed by multiple mods";
                error.affected_mods = mod_ids;
                errors.push_back(error);
            }
        }
    }

    void check_dependency_cycles(const std::vector<ModMetadata>& mods, std::vector<ValidationError>& errors) {
        // Build dependency graph
        std::map<std::string, std::vector<std::string>> graph;
        for (const auto& mod : mods) {
            if (!mod.enabled) continue;

            for (const auto& [dep_mod_id, _] : mod.dependencies) {
                graph[mod.mod_id].push_back(dep_mod_id);
            }
        }

        // Detect cycles using DFS
        std::set<std::string> visited;
        std::set<std::string> rec_stack;

        for (const auto& [mod_id, _] : graph) {
            if (has_cycle_dfs(mod_id, graph, visited, rec_stack)) {
                ValidationError error;
                error.type = ValidationError::DEPENDENCY_CYCLE;
                error.message = "Circular dependency detected involving mod: " + mod_id;
                error.affected_mods = {mod_id};  // Could trace full cycle
                errors.push_back(error);
            }
        }
    }

    bool has_cycle_dfs(const std::string& mod_id,
                      const std::map<std::string, std::vector<std::string>>& graph,
                      std::set<std::string>& visited,
                      std::set<std::string>& rec_stack) {
        visited.insert(mod_id);
        rec_stack.insert(mod_id);

        auto it = graph.find(mod_id);
        if (it != graph.end()) {
            for (const auto& neighbor : it->second) {
                if (visited.find(neighbor) == visited.end()) {
                    if (has_cycle_dfs(neighbor, graph, visited, rec_stack)) {
                        return true;
                    }
                } else if (rec_stack.find(neighbor) != rec_stack.end()) {
                    return true;  // Cycle detected
                }
            }
        }

        rec_stack.erase(mod_id);
        return false;
    }
};
```

**Estimate**: 3 hours

---

#### Step 2: Enforce Blocking on Validation Failure

**File**: [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp)

**Update capability generation**:
```cpp
bool FrameworkCore::generate_capabilities(const std::string& output_filename) {
    LOG_INFO("Generating capabilities...");

    // Get all enabled mods
    auto enabled_mods = mod_registry_->get_enabled_mods();

    // Validate capabilities (ZERO TOLERANCE)
    CapabilitiesValidator validator;
    auto validation_errors = validator.validate(enabled_mods);

    if (!validation_errors.empty()) {
        LOG_ERROR("Capability validation FAILED - " + std::to_string(validation_errors.size()) + " errors");

        // Log all errors
        for (const auto& error : validation_errors) {
            log_validation_error(error);
        }

        // Broadcast errors to all mods
        broadcast_validation_errors(validation_errors);

        // BLOCK generation
        LOG_ERROR("APCapabilities file NOT generated due to validation errors");
        LOG_ERROR("Resolution required: Fix conflicts and try again");

        return false;  // Generation BLOCKED
    }

    // Validation passed, proceed with generation
    json capabilities_json;
    merge_capabilities(enabled_mods, capabilities_json);

    // Write to file
    std::ofstream file(output_filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: " + output_filename);
        return false;
    }

    file << capabilities_json.dump(2);  // Pretty-print with indent
    file.close();

    LOG_INFO("Capabilities generated successfully: " + output_filename);
    return true;
}

void FrameworkCore::log_validation_error(const ValidationError& error) {
    std::string error_type;
    switch (error.type) {
        case ValidationError::ITEM_ID_COLLISION:
            error_type = "ITEM_ID_COLLISION";
            break;
        case ValidationError::LOCATION_ID_COLLISION:
            error_type = "LOCATION_ID_COLLISION";
            break;
        case ValidationError::REGION_NAME_COLLISION:
            error_type = "REGION_NAME_COLLISION";
            break;
        case ValidationError::DEPENDENCY_CYCLE:
            error_type = "DEPENDENCY_CYCLE";
            break;
        default:
            error_type = "UNKNOWN";
    }

    LOG_ERROR("[VALIDATION] " + error_type + ": " + error.message);
    LOG_ERROR("[VALIDATION] Affected mods: " + join(error.affected_mods, ", "));
}

void FrameworkCore::broadcast_validation_errors(const std::vector<ValidationError>& errors) {
    for (const auto& error : errors) {
        json error_msg;
        error_msg["type"] = "validation_error";
        error_msg["error_type"] = error.type;
        error_msg["message"] = error.message;
        error_msg["affected_mods"] = error.affected_mods;

        // Send to all affected mods
        for (const auto& mod_id : error.affected_mods) {
            message_router_->send_to_mod(mod_id, error_msg.dump());
        }

        // Also send to framework mod for console display
        message_router_->send_to_mod("archipelago.palworld.framework", error_msg.dump());
    }
}
```

**Estimate**: 2 hours

---

#### Step 3: Display Validation Errors in UE4SS Console

**File**: [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua)

**Add validation error handling**:
```lua
-- In framework client callbacks
framework_client:on_message(function(message)
    if message.type == "validation_error" then
        print("========================================")
        print("[APFramework] [ERROR] CAPABILITY VALIDATION FAILED")
        print("[APFramework] [ERROR] Type: " .. message.error_type)
        print("[APFramework] [ERROR] " .. message.message)
        print("[APFramework] [ERROR] Affected mods:")
        for _, mod_id in ipairs(message.affected_mods) do
            print("[APFramework] [ERROR]   - " .. mod_id)
        end
        print("[APFramework] [ERROR] Resolution: Fix conflicts and regenerate")
        print("========================================")
    end
end)
```

**Estimate**: 1 hour

---

#### Step 4: Testing

**Test Cases**:

1. **Item ID collision**: Mod A and Mod B both claim item ID 100000 → generation blocked
2. **Location ID collision**: Mod A and Mod B both claim location ID 200000 → generation blocked
3. **Region name collision**: Mod A and Mod B both define region "StartingArea" → generation blocked
4. **Dependency cycle**: Mod A depends on B, Mod B depends on A → generation blocked
5. **Multiple collisions**: Multiple types of conflicts → all reported, generation blocked
6. **No collisions**: All IDs unique → generation succeeds

**Test Mods**:

**Mod A** (item ID 100000):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.a",
  "version": "1.0.0",
  "capabilities": {
    "items": [{"id": 100000, "name": "ItemA", "classification": "useful"}]
  }
}
```

**Mod B** (SAME item ID 100000):
```json
{
  "schema_version": 2,
  "mod_id": "test.mod.b",
  "version": "1.0.0",
  "capabilities": {
    "items": [{"id": 100000, "name": "ItemB", "classification": "progression"}]
  }
}
```

**Expected Result**: Generation blocked, error message displayed in console.

**Estimate**: 2 hours

---

### Files Modified

- [src/framework_core/src/capabilities_generator.cpp](../../../../src/framework_core/src/capabilities_generator.cpp) - **MAJOR** (validation logic)
- [src/framework_core/src/framework_core.cpp](../../../../src/framework_core/src/framework_core.cpp) - **MODERATE** (enforce blocking)
- [APFramework/Scripts/main.lua](../../../../APFramework/Scripts/main.lua) - **MINOR** (error display)

**Estimated Lines Changed**: ~300-400 lines

---

### Success Criteria

- [ ] Item ID collisions detected and block generation
- [ ] Location ID collisions detected and block generation
- [ ] Region name collisions detected and block generation
- [ ] Dependency cycles detected and block generation
- [ ] Detailed error messages logged
- [ ] Errors broadcast to affected mods via IPC
- [ ] Errors displayed in UE4SS console
- [ ] Generation only succeeds when validation passes

---

## Feature 2.2: Python World Schema Verification

**Priority**: 2nd (Ensure Compatibility)
**Estimate**: 4-6 hours
**Dependencies**: Phase 1 (new schema v2)

### Problem Statement

**Current**: Python world ([worlds/palworld/](../../../../worlds/palworld/)) is MORE advanced than the framework. Need to verify compatibility and ensure both systems align.

**Known Python World Features** (from CURRENT_ANALYSIS.md):
- ✅ Dynamic capability loading
- ✅ Mod metadata parsing (id, name, version, author, description)
- ✅ Conflict declarations
- ✅ Extensible schema

**Uncertain Areas**:
- Multi-slot capability loading (does it support `APCapabilities_<slot>.json`?)
- Dependency schema (does it match C++ schema?)
- Incompatibility schema (does it match C++ schema?)
- Custom field handling (does it support extensible capabilities?)

### Solution

**Verify and update Python world** to fully support framework's schema v2:

1. **Multi-slot capability loading** - Support multiple files
2. **Dependency schema alignment** - Match C++ dependency spec
3. **Incompatibility schema alignment** - Match C++ incompatibility spec (three types)
4. **Custom field validation** - Ensure extensible capabilities work
5. **Testing** - Verify end-to-end with real framework output

### Implementation Steps

#### Step 1: Verify Current Python World Implementation

**Files to Review**:
- [worlds/palworld/__init__.py](../../../../worlds/palworld/__init__.py)
- [worlds/palworld/mod_interface.py](../../../../worlds/palworld/mod_interface.py)
- [worlds/palworld/options.py](../../../../worlds/palworld/options.py)

**Check**:
1. How does Python world load capability files?
2. What schema does it expect?
3. Does it support multiple capability files?
4. How does it handle mod metadata?

**Estimate**: 1 hour (code review)

---

#### Step 2: Update Multi-Slot Capability Loading

**File**: [worlds/palworld/options.py](../../../../worlds/palworld/options.py)

**Current** (likely):
```python
class PalworldOptions(PerGameCommonOptions):
    capability_manifest_path: str = "APCapabilities.json"
```

**Update to support multiple files**:
```python
class PalworldOptions(PerGameCommonOptions):
    # Option 1: Single file (backward compatible)
    capability_manifest_path: str = "APCapabilities.json"

    # Option 2: Multiple files (new for multi-slot)
    capability_manifest_paths: List[str] = []

    # Option 3: Auto-discovery (future)
    # auto_discover_capabilities: bool = True
```

**File**: [worlds/palworld/__init__.py](../../../../worlds/palworld/__init__.py)

**Update loading logic**:
```python
def _load_capability_manifests(self):
    """Load one or more capability manifests."""
    manifests = []

    # Check if using multi-file mode
    if self.options.capability_manifest_paths:
        # Load multiple files
        for path in self.options.capability_manifest_paths:
            manifest = self.capability_manager.load_manifest(path)
            if manifest:
                manifests.append(manifest)
    else:
        # Load single file (backward compatible)
        path = self.options.capability_manifest_path.value or "APCapabilities.json"
        manifest = self.capability_manager.load_manifest(path)
        if manifest:
            manifests.append(manifest)

    # Merge all manifests
    for manifest in manifests:
        self._merge_mod_capabilities(manifest)
```

**Estimate**: 1.5 hours

---

#### Step 3: Align Dependency Schema

**File**: [worlds/palworld/mod_interface.py](../../../../worlds/palworld/mod_interface.py)

**Current** (likely):
```python
@dataclass
class ModInfo:
    id: str
    name: str
    version: str
    author: Optional[str]
    description: Optional[str]
```

**Update to match C++ schema**:
```python
from typing import Optional, Dict, List, Union

@dataclass
class VersionRange:
    min_version: Optional[str] = None
    max_version: Optional[str] = None
    any_version: bool = True

    def is_satisfied_by(self, version: str) -> bool:
        """Check if version satisfies this range."""
        if self.any_version:
            return True

        # Simple string comparison (Phase 3 will add semver)
        if self.min_version and version < self.min_version:
            return False
        if self.max_version and version > self.max_version:
            return False

        return True

@dataclass
class ModInfo:
    id: str
    name: str
    version: str
    author: Optional[str] = None
    description: Optional[str] = None
    enabled: bool = True

    # NEW: Dependencies (matching C++ schema)
    dependencies: Dict[str, Union[bool, VersionRange]] = field(default_factory=dict)

    # NEW: Incompatibilities (Feature 2.2 step 4)
    incompatible_mods: Dict[str, 'IncompatibilitySpec'] = field(default_factory=dict)
```

**Add parsing**:
```python
def parse_mod_metadata(metadata_json: dict) -> ModInfo:
    """Parse mod metadata from JSON."""
    mod_info = ModInfo(
        id=metadata_json["mod_id"],
        name=metadata_json.get("display_name", metadata_json["mod_id"]),
        version=metadata_json.get("version", "0.0.0"),
        author=metadata_json.get("author"),
        description=metadata_json.get("description"),
        enabled=metadata_json.get("enabled", True)
    )

    # Parse dependencies
    if "dependencies" in metadata_json:
        for dep_id, spec in metadata_json["dependencies"].items():
            if isinstance(spec, bool) and spec:
                mod_info.dependencies[dep_id] = VersionRange(any_version=True)
            elif isinstance(spec, dict):
                mod_info.dependencies[dep_id] = VersionRange(
                    min_version=spec.get("min_version"),
                    max_version=spec.get("max_version"),
                    any_version=False
                )

    return mod_info
```

**Estimate**: 1.5 hours

---

#### Step 4: Align Incompatibility Schema

**File**: [worlds/palworld/mod_interface.py](../../../../worlds/palworld/mod_interface.py)

**Add incompatibility spec**:
```python
from enum import Enum

class IncompatibilityType(Enum):
    COMPLETE = "complete"
    VERSION_RANGE = "version_range"
    SPECIFIC_VERSIONS = "specific_versions"

@dataclass
class IncompatibilitySpec:
    type: IncompatibilityType
    version_range: Optional[VersionRange] = None
    specific_versions: Optional[List[str]] = None

    def is_incompatible_with(self, version: str) -> bool:
        """Check if version is incompatible."""
        if self.type == IncompatibilityType.COMPLETE:
            return True
        elif self.type == IncompatibilityType.VERSION_RANGE:
            return self.version_range.is_satisfied_by(version)
        elif self.type == IncompatibilityType.SPECIFIC_VERSIONS:
            return version in self.specific_versions
        return False

# Add parsing in parse_mod_metadata
def parse_mod_metadata(metadata_json: dict) -> ModInfo:
    # ... (previous code)

    # Parse incompatibilities
    if "incompatible_mods" in metadata_json:
        for incompat_id, spec in metadata_json["incompatible_mods"].items():
            if isinstance(spec, bool) and spec:
                mod_info.incompatible_mods[incompat_id] = IncompatibilitySpec(
                    type=IncompatibilityType.COMPLETE
                )
            elif isinstance(spec, dict):
                mod_info.incompatible_mods[incompat_id] = IncompatibilitySpec(
                    type=IncompatibilityType.VERSION_RANGE,
                    version_range=VersionRange(
                        min_version=spec.get("min_version"),
                        max_version=spec.get("max_version"),
                        any_version=False
                    )
                )
            elif isinstance(spec, list):
                mod_info.incompatible_mods[incompat_id] = IncompatibilitySpec(
                    type=IncompatibilityType.SPECIFIC_VERSIONS,
                    specific_versions=spec
                )

    return mod_info
```

**Estimate**: 1 hour

---

#### Step 5: End-to-End Testing

**Test Scenario**:
1. Generate capability files from framework (multi-slot)
2. Provide to Python world generator
3. Verify world generation succeeds
4. Verify all mod capabilities included
5. Verify dependencies and incompatibilities respected

**Test Files**:
```
APCapabilities_Player1.json  (2 mods with dependencies)
APCapabilities_Player2.json  (1 mod with incompatibility)
```

**Estimate**: 1-2 hours

---

### Files Modified

- [worlds/palworld/options.py](../../../../worlds/palworld/options.py) - **MINOR** (multi-file option)
- [worlds/palworld/__init__.py](../../../../worlds/palworld/__init__.py) - **MODERATE** (multi-file loading)
- [worlds/palworld/mod_interface.py](../../../../worlds/palworld/mod_interface.py) - **MAJOR** (schema alignment)

**Estimated Lines Changed**: ~150-200 lines

---

### Success Criteria

- [ ] Python world loads multi-slot capability files
- [ ] Dependency schema matches C++ implementation
- [ ] Incompatibility schema matches C++ implementation (three types)
- [ ] Custom field extensibility works
- [ ] End-to-end test passes (framework → world → multiworld)
- [ ] Documentation updated with multi-file usage

---

## Features 2.3 and 2.4 (Testing & Documentation)

### Feature 2.3: Comprehensive Testing

**Estimate**: 8-10 hours

**Test Areas**:
1. Dependency validation (missing, version mismatches, cascade)
2. Incompatibility detection (three types, mutual vs one-way)
3. Runtime enablement (auto-disable, file persistence)
4. Multi-slot capabilities (multiple files, naming)
5. Log routing (filtering, message delivery)
6. lunajson integration (complex JSON)
7. Zero-tolerance validation (all conflict types)

**Test Types**:
- Unit tests (C++ validation logic)
- Integration tests (full framework flow)
- Manual tests (UE4SS environment)

### Feature 2.4: Documentation Updates

**Estimate**: 4-6 hours

**Documents to Update**:
- [README.md](../../../../README.md) - Installation, features, migration
- [ARCHITECTURE.md](../../../../docs/ARCHITECTURE.md) - Dependency/incompatibility systems
- [BUILD.md](../../../../docs/BUILD.md) - lunajson integration
- [CHANGES.md](../../../../CHANGES.md) - Changelog for v2 schema

**New Documents**:
- `docs/SCHEMA_V2.md` - Complete schema reference
- `docs/MIGRATION_V1_TO_V2.md` - Migration guide

---

## Phase 2 Completion Checklist

### Implementation Order

- [ ] 2.1 Zero-tolerance validation (6-8 hours)
- [ ] 2.2 Python world verification (4-6 hours)
- [ ] 2.3 Comprehensive testing (8-10 hours)
- [ ] 2.4 Documentation updates (4-6 hours)

### Success Criteria

- [ ] All validation enforced (zero-tolerance)
- [ ] Python world compatible with framework
- [ ] All edge cases tested
- [ ] Documentation complete and accurate
- [ ] Ready for stable release (v1.0)

---

**End of Phase 2 Implementation Plan**

**Next**: [Phase03_NiceToHave.md](Phase03_NiceToHave.md)