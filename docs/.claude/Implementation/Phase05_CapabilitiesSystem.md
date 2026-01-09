# Phase 05: Capabilities & Registry System

**Status**: 🔴 Not Started

---

## Overview

Implement mod discovery, registration, capabilities management, and conflict detection.

**Goals**:
- Discover mods with AP_Config.json files
- Track mod registrations (priority vs regular)
- Generate aggregated capabilities JSON
- Compute SHA256 checksums
- Detect capability conflicts

---

## Prerequisites

- ✅ Phase 02 complete (APConfig, APLogger)
- ✅ Phase 03 complete (IPC system)

---

## Components

### 1. APModRegistry - Mod Discovery & Tracking
### 2. APCapabilitiesGenerator - JSON Aggregation & Checksum
### 3. AP_Config.json Schema
### 4. Conflict Detection

---

## Key Implementation Points

**APModRegistry**:
```cpp
class APModRegistry {
public:
    // Discover mods in Mods/ directory
    VoidResult discover_mods(const std::filesystem::path& mods_dir);

    // Register mod via IPC
    VoidResult register_mod(const std::string& mod_id,
                            const nlohmann::json& capabilities,
                            bool is_priority);

    // Get registered mods
    std::vector<ModInfo> get_registered_mods(bool priority_only = false) const;

    // Check if all priority mods registered
    bool all_priority_mods_registered() const;

    // Check if all discovered mods registered
    bool all_mods_registered() const;
};
```

**APCapabilitiesGenerator**:
```cpp
class APCapabilitiesGenerator {
public:
    // Generate aggregated capabilities from all registered mods
    Result<nlohmann::json> generate_capabilities(
        const std::vector<ModInfo>& mods,
        const std::string& game_name);

    // Compute SHA256 checksum of capabilities
    std::string compute_checksum(const nlohmann::json& capabilities);

    // Detect conflicts (duplicate item IDs, location IDs, etc.)
    std::vector<ConflictInfo> detect_conflicts(const nlohmann::json& capabilities);

    // Save capabilities to file
    VoidResult save_to_file(const nlohmann::json& capabilities,
                            const std::filesystem::path& output_path);
};
```

**AP_Config.json Schema**:
```json
{
  "mod_id": "author.palworld.modname",
  "mod_name": "My Palworld Mod",
  "version": "1.0.0",
  "capabilities": {
    "items": [
      {
        "id": 1000,
        "name": "TechUnlock_DoubleShotBow",
        "classification": "progression",
        "tags": ["technology"]
      }
    ],
    "locations": [
      {
        "id": 2000,
        "name": "Chest_OverworldBoss01",
        "type": "static",
        "classification": "default",
        "region": "Overworld"
      }
    ],
    "regions": [
      {"name": "Overworld", "entrances": []}
    ],
    "options": []
  }
}
```

**Conflict Detection**:
- Duplicate item IDs
- Duplicate location IDs
- Conflicting item names
- Invalid region references

**Checksum Validation** (from ARCHITECTURE_REVIEW.md):
- On mismatch with server's expected checksum → enter ERROR_STATE
- Not just a warning, this is a critical error

---

## Testing

- Test mod discovery in Mods/ directory
- Test capability aggregation with multiple mods
- Test conflict detection with intentionally conflicting configs
- Test checksum generation consistency

---

## Acceptance Criteria

- ✅ APModRegistry discovers mods from Mods/ directory
- ✅ APModRegistry tracks priority vs regular mods
- ✅ APCapabilitiesGenerator aggregates capabilities JSON
- ✅ SHA256 checksum computation working
- ✅ Conflict detection identifies duplicate IDs
- ✅ Aggregated capabilities saved to palworld_capabilities.json
- ✅ Unit tests pass

---

## Next Phase

[Phase 06: Message Routing & Polling](Phase06_MessageRouting.md)

---

**Last Updated**: 2026-01-09
**Status**: 🔴 Not Started
