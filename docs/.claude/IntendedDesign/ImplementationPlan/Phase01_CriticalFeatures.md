# Phase 1: Critical Features - Implementation Plan

**Phase**: 1 of 3
**Priority**: 🔴 Critical
**Duration**: 2-3 days (12-17 hours)
**Status**: Ready to implement

---

## Overview

Phase 1 implements the critical features required for a user-friendly first release. These features address the most significant gaps between the current implementation and the intended design.

### Goals

1. **Visibility**: Users can see critical framework messages in UE4SS console
2. **Validation**: Rich mod metadata with versioning and compatibility
3. **Correctness**: Only enabled mods are discovered (no false timeouts)

### Success Criteria

- [ ] Framework errors/warnings appear in UE4SS console and `ue4ss/UE4SS.log`
- [ ] Mod metadata includes version, display_name, description, game_version, incompatible_mods
- [ ] Invalid metadata rejected with clear error messages
- [ ] Only enabled mods discovered (disabled mods skipped)
- [ ] No false registration timeouts from disabled mods
- [ ] All features documented and tested

---

## Feature 1.1: Framework→UE4SS Console Logging

### Problem Statement

**Current State**: Framework logs everything to `APFramework/Logs/framework.log`, but critical errors/warnings are invisible to users unless they check the log file.

**Intended Design**: Framework should "pump important messages" to the Lua mod so they appear in the UE4SS console and `ue4ss/UE4SS.log`.

**Impact**: Users can't debug issues without checking a separate log file.

### Solution Design

#### Message Pump Architecture

```
C++ Logger
    ↓ (important messages only)
MessageQueue<LogEntry>
    ↓ (Lua polls)
get_pending_log_messages()
    ↓
Lua main.lua
    ↓ (print to console)
UE4SS Console
    ↓ (automatic)
ue4ss/UE4SS.log
```

#### What Qualifies as "Important"?

- **ERROR**: All errors
- **WARNING**: All warnings
- **INFO**: Key lifecycle events only
  - "IPC server started"
  - "Discovered X mods"
  - "All mods registered"
  - "Connected to AP server"
  - "Generated APCapabilities.json"

#### LogEntry Structure

```cpp
struct LogEntry {
    std::string timestamp;  // "2026-01-02 14:23:45.123"
    std::string level;      // "ERROR", "WARNING", "INFO"
    std::string message;    // The log message
};
```

### Implementation Steps

#### Step 1: Extend Logger Class (2 hours)

**File**: [src/framework_core/include/logger.h](../../../src/framework_core/include/logger.h)

**Changes**:
```cpp
#include "message_queue.h"

class Logger {
public:
    struct LogEntry {
        std::string timestamp;
        std::string level;
        std::string message;
    };

    // Existing methods...

    // NEW: Get pending important messages
    std::vector<LogEntry> get_pending_log_messages();

private:
    MessageQueue<LogEntry> important_messages_;

    // NEW: Determine if message is important
    bool is_important_message(const std::string& level, const std::string& message);
};
```

**File**: [src/framework_core/src/logger.cpp](../../../src/framework_core/src/logger.cpp)

**Changes**:
```cpp
void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Get timestamp
    std::string timestamp = get_timestamp();

    // Write to file (existing behavior)
    if (log_file_.is_open()) {
        log_file_ << "[" << timestamp << "] [" << level << "] " << message << std::endl;
        log_file_.flush();
    }

    // NEW: Queue important messages
    if (is_important_message(level, message)) {
        important_messages_.push({timestamp, level, message});
    }
}

bool Logger::is_important_message(const std::string& level, const std::string& message) {
    // All errors and warnings are important
    if (level == "ERROR" || level == "WARNING") {
        return true;
    }

    // Only certain INFO messages are important
    if (level == "INFO") {
        // Check for key lifecycle events
        static const std::vector<std::string> important_keywords = {
            "IPC server started",
            "Discovered",
            "registered",
            "Connected to AP",
            "Generated APCapabilities",
            "Disconnected from AP"
        };

        for (const auto& keyword : important_keywords) {
            if (message.find(keyword) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

std::vector<Logger::LogEntry> Logger::get_pending_log_messages() {
    return important_messages_.pop_all();
}
```

#### Step 2: Expose to Lua (1 hour)

**File**: [src/framework_core/src/lua_bindings.cpp](../../../src/framework_core/src/lua_bindings.cpp)

**Add Method**:
```cpp
static int framework_get_log_messages(lua_State* L) {
    FrameworkCore* core = check_framework_core(L, 1);

    // Get pending log messages
    auto messages = Logger::instance().get_pending_log_messages();

    // Create Lua table
    lua_newtable(L);

    for (size_t i = 0; i < messages.size(); i++) {
        lua_newtable(L);

        lua_pushstring(L, messages[i].timestamp.c_str());
        lua_setfield(L, -2, "timestamp");

        lua_pushstring(L, messages[i].level.c_str());
        lua_setfield(L, -2, "level");

        lua_pushstring(L, messages[i].message.c_str());
        lua_setfield(L, -2, "message");

        lua_rawseti(L, -2, i + 1);  // Lua arrays are 1-indexed
    }

    return 1;  // Return table
}
```

**Register in Metatable**:
```cpp
static const luaL_Reg framework_methods[] = {
    // Existing methods...
    {"get_log_messages", framework_get_log_messages},
    {NULL, NULL}
};
```

#### Step 3: Poll and Print in Lua (1 hour)

**File**: [APFramework/Scripts/main.lua](../../../APFramework/Scripts/main.lua)

**Add to update_state_machine()**:
```lua
function update_state_machine()
    -- NEW: Poll and print framework log messages
    print_framework_logs()

    -- Existing state machine logic...
    if state == "INIT" then
        handle_init_state()
    -- ... etc
end

function print_framework_logs()
    local messages = framework:get_log_messages()

    for _, msg in ipairs(messages) do
        local prefix = "[APFramework]"
        local level_prefix = ""

        if msg.level == "ERROR" then
            level_prefix = "[ERROR]"
        elseif msg.level == "WARNING" then
            level_prefix = "[WARN]"
        else
            level_prefix = "[INFO]"
        end

        -- Print to UE4SS console (automatically goes to UE4SS.log)
        print(string.format("%s %s %s", prefix, level_prefix, msg.message))
    end
end
```

#### Step 4: Update FrameworkWrapper (30 min)

**File**: [APFramework/Scripts/framework_wrapper.lua](../../../APFramework/Scripts/framework_wrapper.lua)

**Add Method**:
```lua
function FrameworkWrapper:get_log_messages()
    return self.handle:get_log_messages()
end
```

### Testing Plan

**Unit Tests**:
1. Test `is_important_message()` with various messages
2. Test message queue doesn't overflow (check max size)
3. Test Lua binding returns correct table structure

**Integration Tests**:
1. Start framework → verify "IPC server started" appears in console
2. Cause registration timeout → verify WARNING appears in console
3. Cause connection error → verify ERROR appears in console
4. Check `ue4ss/UE4SS.log` contains framework messages

**Expected Output** (UE4SS Console):
```
[APFramework] [INFO] IPC server started on pipe: APFramework_default
[APFramework] [INFO] Discovered 2 mods: PalworldMod, AnotherMod
[APFramework] [WARN] Registration timeout! Only 1/2 mods registered
[APFramework] [INFO] Generated APCapabilities.json with 10 items, 15 locations
[APFramework] [INFO] Connected to AP server: archipelago.gg:38281
```

### Rollback Plan

If this feature causes issues:
1. Message queue could grow unbounded → Add max size (e.g., 100 messages)
2. Performance impact from frequent polling → Reduce poll frequency
3. Too much console spam → Make INFO filtering more aggressive

---

## Feature 1.2: Extended Mod Metadata Schema

### Problem Statement

**Current State**: Mod `ap_config.json` only contains mod_id and capabilities.

**Intended Design**: Rich metadata including version, display_name, description, supported_game_versions, incompatible_mods.

**Impact**: No versioning, no compatibility checking, poor error messages.

### Solution Design

#### New Schema (v2)

```json
{
  "schema_version": 2,
  "mod_id": "author.game.mod",
  "version": "1.2.3",
  "display_name": "My Awesome Palworld Mod",
  "description": "Adds cool features to Palworld for Archipelago",
  "supported_game_versions": ">=0.3.0 <0.4.0",
  "incompatible_mods": [
    {
      "mod_id": "other.author.conflicting.mod",
      "reason": "Both mods modify the same game systems",
      "versions": ">=2.0.0"
    }
  ],
  "capabilities": {
    "items": [
      {"id": 100000, "name": "Super Pickaxe", "classification": "useful"}
    ],
    "locations": [
      {"id": 200000, "name": "Chest in Cave", "region": "Starting Area"}
    ],
    "regions": [
      {"name": "Starting Area", "connects_to": ["Mountain Path"]}
    ]
  }
}
```

#### Backward Compatibility (v1)

Old schema without `schema_version` field will be auto-upgraded:
```json
{
  "mod_id": "OldMod",
  "items": [...],
  "locations": [...],
  "regions": [...]
}
```

Converted to:
```json
{
  "schema_version": 1,
  "mod_id": "OldMod",
  "version": "0.0.0",
  "display_name": "OldMod",
  "description": "",
  "supported_game_versions": "*",
  "incompatible_mods": [],
  "capabilities": {
    "items": [...],
    "locations": [...],
    "regions": [...]
  }
}
```

### Implementation Steps

#### Step 1: Extend ModMetadata Struct (1 hour)

**File**: [src/framework_core/include/mod_registry.h](../../../src/framework_core/include/mod_registry.h)

**Changes**:
```cpp
struct IncompatibleMod {
    std::string mod_id;
    std::string reason;
    std::string versions;  // e.g., ">=2.0.0"
};

struct ModMetadata {
    int schema_version = 2;
    std::string mod_id;
    std::string version;                         // NEW: "1.2.3"
    std::string display_name;                    // NEW: Human-readable name
    std::string description;                     // NEW: Description
    std::string supported_game_versions;         // NEW: ">=0.3.0 <0.4.0"
    std::vector<IncompatibleMod> incompatible_mods;  // NEW

    // Existing capabilities
    std::vector<int64_t> items;
    std::vector<int64_t> locations;
    std::vector<std::string> regions;
};

class ModRegistry {
public:
    // NEW: Validate metadata
    bool validate_metadata(const ModMetadata& metadata, std::string& error_message);

private:
    // NEW: Auto-upgrade v1 to v2
    ModMetadata upgrade_v1_metadata(const nlohmann::json& v1_json);
};
```

#### Step 2: Parse New Fields (2 hours)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Update discover_mods()**:
```cpp
void ModRegistry::discover_mods(const std::string& directory) {
    // Scan for ap_config.json files...

    for (const auto& config_path : config_files) {
        try {
            std::ifstream file(config_path);
            nlohmann::json config_json;
            file >> config_json;

            // Detect schema version
            int schema_version = 1;  // Default to v1
            if (config_json.contains("schema_version")) {
                schema_version = config_json["schema_version"];
            }

            ModMetadata metadata;

            if (schema_version == 1) {
                // Auto-upgrade v1 to v2
                metadata = upgrade_v1_metadata(config_json);
                LOG_WARNING("Mod {} using deprecated schema v1, please upgrade",
                            metadata.mod_id);
            } else if (schema_version == 2) {
                // Parse v2 schema
                metadata = parse_v2_metadata(config_json);
            } else {
                LOG_ERROR("Unknown schema version {} in {}", schema_version, config_path);
                continue;
            }

            // Validate metadata
            std::string error;
            if (!validate_metadata(metadata, error)) {
                LOG_ERROR("Invalid metadata for {}: {}", metadata.mod_id, error);
                continue;
            }

            // Store metadata
            discovered_mods_metadata_[metadata.mod_id] = metadata;
            discovered_mods_.insert(metadata.mod_id);

            LOG_INFO("Discovered mod: {} v{} ({})",
                     metadata.display_name, metadata.version, metadata.mod_id);

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse {}: {}", config_path, e.what());
        }
    }
}

ModMetadata ModRegistry::upgrade_v1_metadata(const nlohmann::json& v1_json) {
    ModMetadata metadata;
    metadata.schema_version = 1;
    metadata.mod_id = v1_json["mod_id"];
    metadata.version = "0.0.0";
    metadata.display_name = metadata.mod_id;
    metadata.description = "";
    metadata.supported_game_versions = "*";

    // Parse capabilities
    if (v1_json.contains("items")) {
        for (const auto& item : v1_json["items"]) {
            metadata.items.push_back(item["id"]);
        }
    }
    // ... same for locations, regions

    return metadata;
}

ModMetadata ModRegistry::parse_v2_metadata(const nlohmann::json& json) {
    ModMetadata metadata;

    // Required fields
    metadata.schema_version = json["schema_version"];
    metadata.mod_id = json["mod_id"];
    metadata.version = json["version"];
    metadata.display_name = json["display_name"];

    // Optional fields
    metadata.description = json.value("description", "");
    metadata.supported_game_versions = json.value("supported_game_versions", "*");

    // Incompatible mods
    if (json.contains("incompatible_mods")) {
        for (const auto& incomp : json["incompatible_mods"]) {
            IncompatibleMod im;
            im.mod_id = incomp["mod_id"];
            im.reason = incomp.value("reason", "");
            im.versions = incomp.value("versions", "*");
            metadata.incompatible_mods.push_back(im);
        }
    }

    // Capabilities
    if (json.contains("capabilities")) {
        const auto& caps = json["capabilities"];

        if (caps.contains("items")) {
            for (const auto& item : caps["items"]) {
                metadata.items.push_back(item["id"]);
            }
        }
        // ... same for locations, regions
    }

    return metadata;
}
```

#### Step 3: Add Validation (2 hours)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Implement validate_metadata()**:
```cpp
bool ModRegistry::validate_metadata(const ModMetadata& metadata, std::string& error) {
    // Required fields
    if (metadata.mod_id.empty()) {
        error = "mod_id is required";
        return false;
    }

    if (metadata.version.empty()) {
        error = "version is required";
        return false;
    }

    if (metadata.display_name.empty()) {
        error = "display_name is required";
        return false;
    }

    // Validate version format (simple check, semver in Phase 3)
    if (!validate_version_format(metadata.version)) {
        error = "Invalid version format: " + metadata.version + " (expected X.Y.Z)";
        return false;
    }

    // Validate mod_id format (detailed check in Phase 2)
    if (metadata.mod_id.find('.') == std::string::npos) {
        error = "mod_id should follow 'author.game.mod' format";
        // WARNING only in Phase 1, will be ERROR in Phase 2
        LOG_WARNING("Mod {} has non-standard mod_id format", metadata.mod_id);
    }

    // Validate capabilities exist
    if (metadata.items.empty() && metadata.locations.empty() && metadata.regions.empty()) {
        error = "Mod must provide at least one capability (items, locations, or regions)";
        return false;
    }

    return true;
}

bool ModRegistry::validate_version_format(const std::string& version) {
    // Simple regex check for X.Y.Z
    std::regex version_regex(R"(^\d+\.\d+\.\d+$)");
    return std::regex_match(version, version_regex);
}
```

#### Step 4: Update APCapabilities.json (1 hour)

**File**: [src/framework_core/src/capabilities_generator.cpp](../../../src/framework_core/src/capabilities_generator.cpp)

**Add Metadata to Output**:
```cpp
nlohmann::json CapabilitiesGenerator::generate_json() {
    nlohmann::json output;

    // NEW: Add framework metadata
    output["framework_version"] = "2.0.0";
    output["generated_at"] = get_timestamp();

    // NEW: Add mod list with metadata
    nlohmann::json mods_array = nlohmann::json::array();
    for (const auto& [mod_id, metadata] : mod_metadata_) {
        mods_array.push_back({
            {"mod_id", metadata.mod_id},
            {"version", metadata.version},
            {"display_name", metadata.display_name},
            {"description", metadata.description}
        });
    }
    output["mods"] = mods_array;

    // Existing items, locations, regions...
    output["items"] = items_array;
    output["locations"] = locations_array;
    output["regions"] = regions_array;

    return output;
}
```

### Testing Plan

**Valid v2 Schema**:
```json
{
  "schema_version": 2,
  "mod_id": "myname.palworld.testmod",
  "version": "1.0.0",
  "display_name": "Test Mod",
  "description": "A test mod",
  "capabilities": {
    "items": [{"id": 100000, "name": "Test Item", "classification": "useful"}]
  }
}
```
Expected: Discovery succeeds, metadata stored

**Invalid v2 Schema (missing version)**:
```json
{
  "schema_version": 2,
  "mod_id": "test",
  "display_name": "Test"
}
```
Expected: Error logged, mod not discovered

**Old v1 Schema**:
```json
{
  "mod_id": "OldMod",
  "items": [{"id": 100000, "name": "Item", "classification": "useful"}]
}
```
Expected: Warning logged, auto-upgraded, discovery succeeds

### Documentation Updates

**Create**: `docs/MOD_METADATA_SCHEMA.md` with:
- Complete schema specification
- Field descriptions
- Examples
- Validation rules
- Migration guide from v1 to v2

---

## Feature 1.3: Mod Enablement Check

### Problem Statement

**Current State**: Framework discovers ALL mods with `ap_config.json`, even disabled ones.

**Intended Design**: Only discover mods that are "enabled" in UE4SS.

**Impact**: False registration timeouts waiting for disabled mods.

### Solution Design

#### UE4SS Enablement Mechanism

UE4SS uses `enabled.txt` file in mod folder:
- Present → Mod enabled
- Absent → Mod disabled

**Example**:
```
ue4ss/Mods/
  APFramework/
    enabled.txt  ← Present, mod enabled
    Scripts/
  DisabledMod/
    Scripts/
    (no enabled.txt)  ← Absent, mod disabled
```

### Implementation Steps

#### Step 1: Add Enablement Check (1 hour)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Add Helper Function**:
```cpp
bool ModRegistry::is_mod_enabled(const std::filesystem::path& mod_folder) {
    // Check for enabled.txt in mod folder
    auto enabled_file = mod_folder / "enabled.txt";
    return std::filesystem::exists(enabled_file);
}
```

**Update discover_mods()**:
```cpp
void ModRegistry::discover_mods(const std::string& directory) {
    LOG_INFO("Scanning for mods in: {}", directory);

    // Recursively find all ap_config.json files
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.path().filename() == "ap_config.json") {
            // Get mod folder (parent of ap_config.json)
            auto mod_folder = entry.path().parent_path();

            // NEW: Check if mod is enabled
            if (!is_mod_enabled(mod_folder)) {
                std::string mod_name = mod_folder.filename().string();
                LOG_DEBUG("Skipping disabled mod: {}", mod_name);
                continue;  // Skip this mod
            }

            // Parse and validate metadata...
            // (existing code)
        }
    }

    LOG_INFO("Discovered {} enabled mods", discovered_mods_.size());
}
```

#### Step 2: Update Logging (30 min)

**Add Summary**:
```cpp
void ModRegistry::discover_mods(const std::string& directory) {
    int total_found = 0;
    int enabled_count = 0;
    int disabled_count = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.path().filename() == "ap_config.json") {
            total_found++;

            auto mod_folder = entry.path().parent_path();

            if (!is_mod_enabled(mod_folder)) {
                disabled_count++;
                LOG_DEBUG("Skipping disabled mod: {}", mod_folder.filename().string());
                continue;
            }

            enabled_count++;
            // Parse metadata...
        }
    }

    LOG_INFO("Scanned {} mod(s): {} enabled, {} disabled",
             total_found, enabled_count, disabled_count);
}
```

### Testing Plan

**Test Case 1: Enabled Mod**:
1. Create `ue4ss/Mods/TestMod/enabled.txt`
2. Create `ue4ss/Mods/TestMod/ap_config.json`
3. Run discovery
4. Expected: Mod discovered

**Test Case 2: Disabled Mod**:
1. Create `ue4ss/Mods/DisabledMod/ap_config.json`
2. Do NOT create `enabled.txt`
3. Run discovery
4. Expected: Mod NOT discovered, DEBUG log says "Skipping disabled mod"

**Test Case 3: Mixed**:
1. Create 2 enabled mods, 1 disabled mod
2. Run discovery
3. Expected: "Scanned 3 mod(s): 2 enabled, 1 disabled"

### Edge Cases

**What if `enabled.txt` is created/deleted while game is running?**
- Not an issue: Discovery only runs at startup
- Mods can't be dynamically enabled/disabled mid-session

**What if a mod creates `enabled.txt` but doesn't register?**
- Existing timeout mechanism handles this (180s)
- Timeout warning will correctly identify the mod

---

## Integration and Testing

### Integration Testing Sequence

**Test 1: Clean Slate**
1. Delete all mods
2. Start framework
3. Verify: "Discovered 0 enabled mods" message in console

**Test 2: Single Mod Registration**
1. Create valid v2 mod with `enabled.txt`
2. Start framework
3. Verify: Discovery message in console, registration succeeds
4. Verify: APCapabilities.json generated with mod metadata

**Test 3: Multiple Mods**
1. Create 2 enabled v2 mods, 1 disabled v2 mod, 1 enabled v1 mod
2. Start framework
3. Verify: Only 3 mods discovered (2 v2 + 1 v1)
4. Verify: Warning for v1 mod in console
5. Verify: All 3 register successfully

**Test 4: Invalid Metadata**
1. Create mod with missing `version` field
2. Start framework
3. Verify: Error in console: "Invalid metadata for ModX: version is required"
4. Verify: Mod NOT discovered

**Test 5: Registration Timeout**
1. Create enabled mod that never registers
2. Start framework
3. Wait 180s
4. Verify: Timeout warning in console with mod name

### Performance Testing

**Large Mod Set**:
- Create 50 mods (25 enabled, 25 disabled)
- Measure discovery time
- Expected: <1 second

**Message Queue Spam**:
- Generate 1000 log messages rapidly
- Verify: No memory leak
- Verify: Oldest messages dropped if queue exceeds limit

---

## Documentation Requirements

### User Documentation

**File**: `README.md`

**Add Section**: "Mod Metadata Schema"
- Link to detailed schema doc
- Example `ap_config.json`
- Required vs optional fields

**File**: `docs/MOD_METADATA_SCHEMA.md` (new)

**Contents**:
- Schema version history (v1 vs v2)
- Complete field reference
- Validation rules
- Examples
- Migration guide

### Developer Documentation

**File**: `docs/ARCHITECTURE.md`

**Update**:
- Document message pump architecture
- Document metadata validation flow
- Update diagrams

---

## Rollout Plan

### Phase 1.1: Console Logging Only

**Week 1, Days 1-2**:
- Implement message pump
- Test thoroughly
- Deploy to internal testing

**Risk**: Minimal, non-breaking change

### Phase 1.2: Metadata Schema

**Week 1, Days 3-4**:
- Implement v2 schema parsing
- Implement v1→v2 upgrade
- Implement validation
- Test with sample mods

**Risk**: Medium - could break existing mods if not backward compatible

**Mitigation**: Auto-upgrade ensures v1 mods continue working

### Phase 1.3: Enablement Check

**Week 1, Day 5**:
- Implement enablement check
- Test with mixed enabled/disabled mods

**Risk**: Low - only affects discovery, well-tested UE4SS convention

### Integration Testing

**Week 2, Days 1-2**:
- Full integration testing
- Fix any issues found
- Update documentation

---

## Success Metrics

After Phase 1 completion:

- [ ] 100% of critical errors visible in UE4SS console
- [ ] 100% of mods use v2 schema (or auto-upgraded v1)
- [ ] 0% false registration timeouts from disabled mods
- [ ] User satisfaction: Can debug issues without checking log file
- [ ] Mod developer satisfaction: Clear error messages for invalid metadata

---

## Next Steps

Upon completion of Phase 1:

1. **Tag Release**: `v2.0.0-beta1`
2. **Internal Testing**: 1 week with example mods
3. **Gather Feedback**: Document any issues
4. **Proceed to Phase 2**: If no critical issues found

---

**End of Phase 1 Plan**