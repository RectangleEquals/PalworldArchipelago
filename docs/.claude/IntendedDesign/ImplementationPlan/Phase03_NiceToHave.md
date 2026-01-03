# Phase 3: Nice-to-Have Enhancements - Implementation Plan

**Document Version**: 1.0
**Date**: 2026-01-02
**Status**: Future Work (Post-Release)
**Parent Document**: [REDESIGN_PLAN_OVERVIEW.md](../REDESIGN_PLAN_OVERVIEW.md)
**Previous Phase**: [Phase02_ValidationPolish.md](Phase02_ValidationPolish.md)

---

## Overview

Phase 3 implements **advanced features** for long-term ecosystem maturity. These features improve user experience and system robustness but are not critical for the initial production release. They can be implemented after beta testing based on community feedback.

**Total Estimated Time**: 18-25 hours (3-4 days)

**Status**: Optional - implement based on community needs and feedback

---

## Feature Roadmap

### Implementation Order

1. **Semantic version validation** (8-10 hours) → Runtime version warnings
2. **Static .lib build option** (2-3 hours) → C++ mod convenience
3. **Advanced error handling** (8-12 hours) → Retry logic, recovery

---

## Feature 3.1: Semantic Version Validation

**Priority**: 1st (Most Valuable)
**Estimate**: 8-10 hours
**Dependencies**: Phase 1 (mod metadata with versions)

### Problem Statement

**Current**: Version comparison uses simple string comparison, which is incorrect for semantic versions.

**Issues**:
- `"1.10.0" < "1.2.0"` (string comparison) → WRONG
- `"1.10.0" > "1.2.0"` (semantic comparison) → CORRECT
- Version ranges like `">=1.0.0 <2.0.0"` not parsed correctly
- Game version compatibility (`supported_game_versions`) not validated
- No warnings for version mismatches (silent failures)

### Solution

Implement **semantic versioning (semver)** parsing and validation:

**Semver Format**: `MAJOR.MINOR.PATCH` (e.g., `"1.2.3"`)

**Version Ranges**:
- `">=1.0.0"` - Greater than or equal to 1.0.0
- `"<2.0.0"` - Less than 2.0.0
- `">=1.0.0 <2.0.0"` - Range: 1.0.0 to 2.0.0 (exclusive)
- `"^1.2.3"` - Compatible with 1.2.3 (>= 1.2.3, < 2.0.0)
- `"~1.2.3"` - Approximately 1.2.3 (>= 1.2.3, < 1.3.0)

**Validation**:
- Parse all mod versions into semver format
- Check dependency version constraints
- Check incompatibility version constraints
- Check game version compatibility (`supported_game_versions`)
- **Warn** (don't block) on mismatches

### Implementation Steps

#### Step 1: Create Semver Utility

**File**: [src/framework_core/include/semver.h](../../../../src/framework_core/include/semver.h)

```cpp
#ifndef SEMVER_H
#define SEMVER_H

#include <string>
#include <regex>
#include <optional>

class SemanticVersion {
public:
    int major;
    int minor;
    int patch;

    SemanticVersion(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    // Parse version string "1.2.3"
    static std::optional<SemanticVersion> parse(const std::string& version_str);

    // Comparison operators
    bool operator<(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;
    bool operator==(const SemanticVersion& other) const;

    std::string to_string() const;
};

class VersionConstraint {
public:
    enum Operator {
        LESS_THAN,           // <
        LESS_THAN_EQUAL,     // <=
        GREATER_THAN,        // >
        GREATER_THAN_EQUAL,  // >=
        EQUAL,               // =
        CARET,               // ^ (compatible with)
        TILDE                // ~ (approximately)
    };

    struct Constraint {
        Operator op;
        SemanticVersion version;
    };

    std::vector<Constraint> constraints;

    // Parse constraint string ">=1.0.0 <2.0.0"
    static std::optional<VersionConstraint> parse(const std::string& constraint_str);

    // Check if version satisfies constraint
    bool is_satisfied_by(const SemanticVersion& version) const;
};

#endif // SEMVER_H
```

**File**: [src/framework_core/src/semver.cpp](../../../../src/framework_core/src/semver.cpp)

```cpp
#include "semver.h"
#include <sstream>

std::optional<SemanticVersion> SemanticVersion::parse(const std::string& version_str) {
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match;

    if (std::regex_match(version_str, match, version_regex)) {
        int major = std::stoi(match[1]);
        int minor = std::stoi(match[2]);
        int patch = std::stoi(match[3]);
        return SemanticVersion(major, minor, patch);
    }

    return std::nullopt;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
    return other < *this;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
    return !(other < *this);
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
    return !(*this < other);
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

std::string SemanticVersion::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::optional<VersionConstraint> VersionConstraint::parse(const std::string& constraint_str) {
    VersionConstraint result;

    // Parse constraints like ">=1.0.0 <2.0.0"
    std::regex constraint_regex(R"((>=|<=|>|<|=|\^|~)\s*(\d+\.\d+\.\d+))");
    std::smatch match;

    std::string str = constraint_str;
    while (std::regex_search(str, match, constraint_regex)) {
        std::string op_str = match[1];
        std::string version_str = match[2];

        auto version = SemanticVersion::parse(version_str);
        if (!version) return std::nullopt;

        Constraint constraint;
        constraint.version = *version;

        if (op_str == ">=") constraint.op = GREATER_THAN_EQUAL;
        else if (op_str == "<=") constraint.op = LESS_THAN_EQUAL;
        else if (op_str == ">") constraint.op = GREATER_THAN;
        else if (op_str == "<") constraint.op = LESS_THAN;
        else if (op_str == "=") constraint.op = EQUAL;
        else if (op_str == "^") constraint.op = CARET;
        else if (op_str == "~") constraint.op = TILDE;
        else return std::nullopt;

        result.constraints.push_back(constraint);

        str = match.suffix();
    }

    return result;
}

bool VersionConstraint::is_satisfied_by(const SemanticVersion& version) const {
    for (const auto& constraint : constraints) {
        bool satisfied = false;

        switch (constraint.op) {
            case LESS_THAN:
                satisfied = version < constraint.version;
                break;
            case LESS_THAN_EQUAL:
                satisfied = version <= constraint.version;
                break;
            case GREATER_THAN:
                satisfied = version > constraint.version;
                break;
            case GREATER_THAN_EQUAL:
                satisfied = version >= constraint.version;
                break;
            case EQUAL:
                satisfied = version == constraint.version;
                break;
            case CARET:  // ^1.2.3 → >=1.2.3 <2.0.0
                satisfied = version >= constraint.version &&
                           version.major == constraint.version.major;
                break;
            case TILDE:  // ~1.2.3 → >=1.2.3 <1.3.0
                satisfied = version >= constraint.version &&
                           version.major == constraint.version.major &&
                           version.minor == constraint.version.minor;
                break;
        }

        if (!satisfied) return false;
    }

    return true;
}
```

**Estimate**: 4 hours

---

#### Step 2: Update Version Validation

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Update VersionRange::is_satisfied_by()**:
```cpp
#include "semver.h"

bool VersionRange::is_satisfied_by(const std::string& version_str) const {
    if (any_version) {
        return true;
    }

    // Parse version
    auto version = SemanticVersion::parse(version_str);
    if (!version) {
        LOG_WARNING("Invalid version format: " + version_str);
        return false;  // Invalid version, fail validation
    }

    // Build constraint string
    std::string constraint_str = "";
    if (!min_version.empty()) {
        constraint_str += ">=" + min_version;
    }
    if (!max_version.empty()) {
        if (!constraint_str.empty()) constraint_str += " ";
        constraint_str += "<=" + max_version;
    }

    if (constraint_str.empty()) {
        return true;  // No constraints
    }

    // Parse and check constraint
    auto constraint = VersionConstraint::parse(constraint_str);
    if (!constraint) {
        LOG_WARNING("Invalid version constraint: " + constraint_str);
        return true;  // Invalid constraint, allow by default
    }

    return constraint->is_satisfied_by(*version);
}
```

**Estimate**: 1 hour

---

#### Step 3: Add Game Version Validation

**File**: [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp)

**Add validation**:
```cpp
void ModRegistry::validate_game_versions(const std::string& current_game_version) {
    LOG_INFO("Validating mod game version compatibility...");

    auto game_version = SemanticVersion::parse(current_game_version);
    if (!game_version) {
        LOG_WARNING("Invalid game version format: " + current_game_version);
        return;
    }

    for (const auto& [mod_id, metadata] : discovered_mods_) {
        if (!metadata.enabled) continue;

        if (metadata.supported_game_versions.empty()) {
            continue;  // No constraint specified
        }

        // Parse game version constraint
        auto constraint = VersionConstraint::parse(metadata.supported_game_versions);
        if (!constraint) {
            LOG_WARNING("Mod '" + mod_id + "' has invalid game version constraint: " +
                       metadata.supported_game_versions);
            continue;
        }

        // Check compatibility
        if (!constraint->is_satisfied_by(*game_version)) {
            LOG_WARNING("Mod '" + mod_id + "' may not be compatible with game version " +
                       current_game_version + " (expects: " + metadata.supported_game_versions + ")");
            // Don't disable, just warn
        }
    }
}
```

**Call in framework initialization**:
```cpp
void FrameworkCore::initialize(const std::string& game_version) {
    // ...
    mod_registry_->discover_mods("Mods");
    mod_registry_->validate_dependencies();
    mod_registry_->detect_incompatibilities();
    mod_registry_->validate_game_versions(game_version);  // NEW
    // ...
}
```

**Estimate**: 2 hours

---

#### Step 4: Testing

**Test Cases**:

1. **Valid semver**: `"1.2.3"` → parsed correctly
2. **Version comparison**: `"1.10.0" > "1.2.0"` → TRUE (correct)
3. **Range constraint**: `">=1.0.0 <2.0.0"` with version `"1.5.0"` → satisfied
4. **Caret constraint**: `"^1.2.3"` with version `"1.3.0"` → satisfied, with `"2.0.0"` → not satisfied
5. **Tilde constraint**: `"~1.2.3"` with version `"1.2.5"` → satisfied, with `"1.3.0"` → not satisfied
6. **Game version warning**: Mod requires game `">=0.3.0"`, game is `"0.2.5"` → warning logged

**Estimate**: 1-2 hours

---

### Files Modified

- [src/framework_core/include/semver.h](../../../../src/framework_core/include/semver.h) - **NEW**
- [src/framework_core/src/semver.cpp](../../../../src/framework_core/src/semver.cpp) - **NEW**
- [src/framework_core/src/mod_registry.cpp](../../../../src/framework_core/src/mod_registry.cpp) - **MODERATE**

**Estimated Lines Changed**: ~400-500 lines (mostly new semver utility)

---

### Success Criteria

- [ ] Semantic version parsing works correctly
- [ ] Version comparison uses semver (not string comparison)
- [ ] Version constraints parsed and validated
- [ ] Game version compatibility checked
- [ ] Warnings logged for version mismatches
- [ ] Caret (^) and tilde (~) operators work

---

## Feature 3.2: Static .lib Build Option

**Priority**: 2nd (Convenience)
**Estimate**: 2-3 hours
**Dependencies**: None

### Problem Statement

**Current**: Client library only builds as DLL (`APClientLib.dll`). C++ mods must:
1. Link against the DLL import library
2. Deploy the DLL alongside their mod
3. Ensure DLL is in correct location at runtime

**Inconvenience**: Extra deployment step, potential DLL versioning issues.

### Solution

Provide **static library build option** (`APClientLib.lib`):

**Benefits**:
- No DLL deployment required
- Mod binary is self-contained
- No runtime DLL loading issues
- Simpler deployment for C++ mod developers

**Trade-offs**:
- Larger binary size (library code embedded)
- Each mod has its own copy of library code

### Implementation Steps

#### Step 1: Add CMake Option

**File**: [src/client_lib/CMakeLists.txt](../../../../src/client_lib/CMakeLists.txt)

**Current**:
```cmake
# Build shared library (DLL)
add_library(APClientLib SHARED
    src/ap_client_lib.cpp
    src/ipc_client.cpp
    # ...
)
```

**Update**:
```cmake
# Option to build static library
option(BUILD_STATIC_CLIENT_LIB "Build static library in addition to shared library" ON)

# Build shared library (DLL)
add_library(APClientLib SHARED
    src/ap_client_lib.cpp
    src/ipc_client.cpp
    # ...
)

# Build static library (optional)
if(BUILD_STATIC_CLIENT_LIB)
    add_library(APClientLibStatic STATIC
        src/ap_client_lib.cpp
        src/ipc_client.cpp
        # ...
    )

    # Set output name to avoid confusion
    set_target_properties(APClientLibStatic PROPERTIES
        OUTPUT_NAME "APClientLib_static"
    )

    # Install static library
    install(TARGETS APClientLibStatic
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()
```

**Estimate**: 1 hour

---

#### Step 2: Update Documentation

**File**: [README.md](../../../../README.md)

**Add section**:
```markdown
## Building

### Client Library Build Options

The client library can be built as:
1. **Shared library** (DLL): `APClientLib.dll` (default)
2. **Static library**: `APClientLib_static.lib` (optional)

To build both:
```bash
cmake -DBUILD_STATIC_CLIENT_LIB=ON ..
cmake --build .
```

### Using the Client Library in Your Mod

**Option 1: Shared Library (DLL)**
- Link against `APClientLib.lib` (import library)
- Deploy `APClientLib.dll` alongside your mod

**Option 2: Static Library**
- Link against `APClientLib_static.lib`
- No DLL deployment needed (code embedded in your mod)

**Example CMakeLists.txt**:
```cmake
# Using shared library (DLL)
target_link_libraries(YourMod PRIVATE APClientLib)

# OR using static library
target_link_libraries(YourMod PRIVATE APClientLib_static)
```
```

**Estimate**: 30 minutes

---

#### Step 3: Testing

**Test**:
1. Build with `BUILD_STATIC_CLIENT_LIB=ON`
2. Verify `APClientLib_static.lib` generated
3. Create test C++ mod using static library
4. Verify mod works without DLL

**Estimate**: 30 minutes

---

### Files Modified

- [src/client_lib/CMakeLists.txt](../../../../src/client_lib/CMakeLists.txt) - **MINOR**
- [README.md](../../../../README.md) - **MINOR**

**Estimated Lines Changed**: ~20-30 lines

---

### Success Criteria

- [ ] Static library build option available
- [ ] Both DLL and static .lib build successfully
- [ ] Documentation updated with usage instructions
- [ ] Test mod using static library works

---

## Feature 3.3: Advanced Error Handling

**Priority**: 3rd (Robustness)
**Estimate**: 8-12 hours
**Dependencies**: None

### Problem Statement

**Current**: Many error paths just log and return. No retry logic or recovery mechanisms.

**Issues**:
- IPC connection failure → mod cannot register (no retry)
- AP connection failure → framework offline (no reconnection)
- Message send failure → message lost (no retry)
- Transient network issues cause permanent failures

### Solution

Implement **retry logic and recovery**:

**IPC Client**:
- Retry connection with exponential backoff
- Queue messages if not connected
- Flush queue when connection established

**AP Client**:
- Automatic reconnection on disconnect
- Exponential backoff for reconnection attempts
- State preservation across reconnections

**Message Router**:
- Retry failed message sends
- Queue messages for offline mods
- Handle mod disconnections gracefully

### Implementation Steps

#### Step 1: IPC Client Retry Logic

**File**: [src/client_lib/src/ipc_client.cpp](../../../../src/client_lib/src/ipc_client.cpp)

**Add retry logic**:
```cpp
class IPCClient {
private:
    std::queue<std::string> message_queue_;
    int retry_count_ = 0;
    int max_retries_ = 5;
    int base_delay_ms_ = 100;

public:
    bool connect_with_retry() {
        for (int attempt = 0; attempt < max_retries_; ++attempt) {
            if (connect()) {
                LOG_INFO("IPC connection established");
                flush_message_queue();
                return true;
            }

            int delay = base_delay_ms_ * (1 << attempt);  // Exponential backoff
            LOG_WARNING("IPC connection failed, retrying in " + std::to_string(delay) + "ms (attempt " +
                       std::to_string(attempt + 1) + "/" + std::to_string(max_retries_) + ")");

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        LOG_ERROR("Failed to connect to IPC server after " + std::to_string(max_retries_) + " attempts");
        return false;
    }

    void send_or_queue(const std::string& message) {
        if (is_connected_) {
            if (!send(message)) {
                LOG_WARNING("Send failed, queueing message");
                message_queue_.push(message);
            }
        } else {
            LOG_WARNING("Not connected, queueing message");
            message_queue_.push(message);
        }
    }

    void flush_message_queue() {
        while (!message_queue_.empty()) {
            auto& message = message_queue_.front();
            if (send(message)) {
                message_queue_.pop();
            } else {
                LOG_WARNING("Failed to flush message queue");
                break;
            }
        }
    }
};
```

**Estimate**: 3 hours

---

#### Step 2: AP Client Reconnection Logic

**File**: [src/framework_core/src/ap_client.cpp](../../../../src/framework_core/src/ap_client.cpp)

**Add reconnection**:
```cpp
class APClient {
private:
    bool auto_reconnect_ = true;
    int reconnect_attempt_ = 0;
    int max_reconnect_attempts_ = 10;

public:
    void on_disconnect() {
        LOG_WARNING("AP connection lost");

        if (auto_reconnect_) {
            reconnect_with_backoff();
        }
    }

    void reconnect_with_backoff() {
        for (int attempt = 0; attempt < max_reconnect_attempts_; ++attempt) {
            int delay = 1000 * (1 << attempt);  // 1s, 2s, 4s, 8s, ...
            LOG_INFO("Reconnecting to AP server in " + std::to_string(delay / 1000) + "s (attempt " +
                    std::to_string(attempt + 1) + "/" + std::to_string(max_reconnect_attempts_) + ")");

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (connect(server_, port_, slot_name_, password_)) {
                LOG_INFO("Reconnected to AP server successfully");
                return;
            }
        }

        LOG_ERROR("Failed to reconnect to AP server after " + std::to_string(max_reconnect_attempts_) + " attempts");
    }
};
```

**Estimate**: 3 hours

---

#### Step 3: Enhanced Error Messages

**Add contextual error messages**:
```cpp
void FrameworkCore::send_error_with_context(const std::string& mod_id, const std::string& error, const std::string& suggestion) {
    json error_msg;
    error_msg["type"] = "error";
    error_msg["mod_id"] = mod_id;
    error_msg["error"] = error;
    error_msg["suggestion"] = suggestion;

    message_router_->send_to_mod(mod_id, error_msg.dump());

    LOG_ERROR("[" + mod_id + "] " + error);
    LOG_INFO("[" + mod_id + "] Suggestion: " + suggestion);
}

// Example usage
send_error_with_context(
    "some.mod.id",
    "Registration denied: Missing dependency 'required.mod.id'",
    "Install 'required.mod.id' or disable this mod"
);
```

**Estimate**: 2 hours

---

#### Step 4: Testing

**Test Cases**:
1. IPC server offline → client retries with backoff
2. AP server offline → framework retries with backoff
3. Transient network failure → automatic recovery
4. Message queue → messages sent when connection restored

**Estimate**: 2-4 hours

---

### Files Modified

- [src/client_lib/src/ipc_client.cpp](../../../../src/client_lib/src/ipc_client.cpp) - **MAJOR**
- [src/framework_core/src/ap_client.cpp](../../../../src/framework_core/src/ap_client.cpp) - **MAJOR**
- [src/framework_core/src/ipc_server.cpp](../../../../src/framework_core/src/ipc_server.cpp) - **MODERATE**

**Estimated Lines Changed**: ~300-400 lines

---

### Success Criteria

- [ ] IPC connection retries with exponential backoff
- [ ] AP connection auto-reconnects on disconnect
- [ ] Message queue prevents message loss
- [ ] Enhanced error messages with actionable suggestions
- [ ] Transient failures recovered gracefully

---

## Phase 3 Completion Checklist

### Implementation Order

- [ ] 3.1 Semantic version validation (8-10 hours)
- [ ] 3.2 Static .lib build (2-3 hours)
- [ ] 3.3 Advanced error handling (8-12 hours)

### Success Criteria

- [ ] Semantic versioning implemented and tested
- [ ] Static library build option available
- [ ] Retry/recovery logic working
- [ ] All features documented
- [ ] Ready for enhanced release (v1.1+)

---

## Recommendation

**Phase 3 is OPTIONAL for initial release**. These features improve polish and long-term stability but are not critical for core functionality.

**Recommended Approach**:
1. Complete Phase 1 + Phase 2
2. Release v1.0 (beta or stable)
3. Gather community feedback
4. Implement Phase 3 features based on actual user needs
5. Release v1.1+ with enhancements

**Community-Driven Priorities**:
- If users report version confusion → prioritize 3.1
- If C++ mod developers request static linking → prioritize 3.2
- If users experience connection issues → prioritize 3.3

---

**End of Phase 3 Implementation Plan**

**All Phases Complete** - Ready for review and implementation!