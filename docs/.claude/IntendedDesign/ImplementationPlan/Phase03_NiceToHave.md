# Phase 3: Nice-to-Have Enhancements - Implementation Plan

**Phase**: 3 of 3
**Priority**: 🟢 Nice-to-Have
**Duration**: 3-4 days (18-25 hours)
**Status**: Ready after Phase 2 completion
**Dependencies**: Phase 1 (metadata schema), Phase 2 (validation foundation)

---

## Overview

Phase 3 implements advanced features that enhance system maturity and robustness. These features are not critical for initial release but provide significant long-term value.

### Goals

1. **Compatibility**: Runtime version validation and incompatibility checking
2. **Flexibility**: Static library build option for C++ mods
3. **Robustness**: Advanced error handling and recovery mechanisms

### Success Criteria

- [ ] Framework warns about version incompatibilities at startup
- [ ] Static .lib available for C++ mods (no DLL deployment required)
- [ ] Transient connection failures automatically recover
- [ ] Error messages include actionable suggestions
- [ ] All features thoroughly tested
- [ ] Complete documentation

---

## Feature 3.1: Semantic Version Validation

### Problem Statement

**Current State**: Version fields exist but not validated at runtime.

**Intended Design**: Runtime checking of:
- Mod version compatibility with game version
- Mod version compatibility with other mods (incompatible_mods)
- Framework version compatibility

**Impact**: Mods may break silently on game updates or conflict with each other.

### Solution Design

#### Semantic Versioning Specification

**Format**: `MAJOR.MINOR.PATCH` (e.g., `1.2.3`)

**Version Ranges**:
- `*` - Any version
- `1.2.3` - Exact version
- `>=1.0.0` - Greater than or equal
- `<2.0.0` - Less than
- `>=1.0.0 <2.0.0` - Range (combined)
- `^1.2.3` - Compatible with (>= 1.2.3, < 2.0.0)
- `~1.2.3` - Approximately (>= 1.2.3, < 1.3.0)

#### Validation Rules

1. **Mod Version**: Must be valid semver (`X.Y.Z`)
2. **Game Version**: Check against `supported_game_versions` range
3. **Incompatible Mods**: Check registered mods against `incompatible_mods` list
4. **Framework Version**: Optional, check minimum required framework version

#### Warning vs Error

- **WARNING**: Version mismatch (log, allow to proceed)
- **ERROR**: Invalid version format (reject mod)

### Implementation Steps

#### Step 1: Implement Semver Parser (4 hours)

**File**: [src/framework_core/include/semver.h](../../../src/framework_core/include/semver.h)

```cpp
#pragma once
#include <string>
#include <regex>

class SemanticVersion {
public:
    int major;
    int minor;
    int patch;

    SemanticVersion() : major(0), minor(0), patch(0) {}
    SemanticVersion(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    // Parse from string "1.2.3"
    static bool parse(const std::string& version_str, SemanticVersion& out);

    // Comparison operators
    bool operator==(const SemanticVersion& other) const;
    bool operator!=(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;

    // String representation
    std::string to_string() const;
};

class VersionRange {
public:
    // Parse range string ">=1.0.0 <2.0.0"
    static bool parse(const std::string& range_str, VersionRange& out);

    // Check if version satisfies range
    bool satisfies(const SemanticVersion& version) const;

    // String representation
    std::string to_string() const;

private:
    struct Constraint {
        enum Op { EQ, NE, LT, LE, GT, GE };
        Op op;
        SemanticVersion version;
    };

    std::vector<Constraint> constraints_;
};
```

**File**: [src/framework_core/src/semver.cpp](../../../src/framework_core/src/semver.cpp)

```cpp
#include "semver.h"
#include <sstream>

bool SemanticVersion::parse(const std::string& version_str, SemanticVersion& out) {
    // Regex: X.Y.Z where X, Y, Z are integers
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;

    if (!std::regex_match(version_str, match, version_regex)) {
        return false;
    }

    out.major = std::stoi(match[1].str());
    out.minor = std::stoi(match[2].str());
    out.patch = std::stoi(match[3].str());

    return true;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

// Implement other operators...

std::string SemanticVersion::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool VersionRange::parse(const std::string& range_str, VersionRange& out) {
    // Handle special cases
    if (range_str == "*" || range_str.empty()) {
        // Match any version (no constraints)
        return true;
    }

    // Handle caret (^1.2.3 means >=1.2.3 <2.0.0)
    if (range_str[0] == '^') {
        SemanticVersion base;
        if (!SemanticVersion::parse(range_str.substr(1), base)) {
            return false;
        }

        // >=base
        out.constraints_.push_back({Constraint::GE, base});

        // <next_major.0.0
        SemanticVersion upper(base.major + 1, 0, 0);
        out.constraints_.push_back({Constraint::LT, upper});

        return true;
    }

    // Handle tilde (~1.2.3 means >=1.2.3 <1.3.0)
    if (range_str[0] == '~') {
        SemanticVersion base;
        if (!SemanticVersion::parse(range_str.substr(1), base)) {
            return false;
        }

        // >=base
        out.constraints_.push_back({Constraint::GE, base});

        // <next_minor.0
        SemanticVersion upper(base.major, base.minor + 1, 0);
        out.constraints_.push_back({Constraint::LT, upper});

        return true;
    }

    // Parse complex ranges: ">=1.0.0 <2.0.0"
    std::istringstream ss(range_str);
    std::string token;

    while (ss >> token) {
        Constraint constraint;

        // Parse operator
        if (token.substr(0, 2) == ">=") {
            constraint.op = Constraint::GE;
            token = token.substr(2);
        } else if (token.substr(0, 2) == "<=") {
            constraint.op = Constraint::LE;
            token = token.substr(2);
        } else if (token.substr(0, 2) == "!=") {
            constraint.op = Constraint::NE;
            token = token.substr(2);
        } else if (token[0] == '>') {
            constraint.op = Constraint::GT;
            token = token.substr(1);
        } else if (token[0] == '<') {
            constraint.op = Constraint::LT;
            token = token.substr(1);
        } else if (token[0] == '=') {
            constraint.op = Constraint::EQ;
            token = token.substr(1);
        } else {
            // No operator, assume exact match
            constraint.op = Constraint::EQ;
        }

        // Parse version
        if (!SemanticVersion::parse(token, constraint.version)) {
            return false;
        }

        out.constraints_.push_back(constraint);
    }

    return !out.constraints_.empty();
}

bool VersionRange::satisfies(const SemanticVersion& version) const {
    // No constraints means match any
    if (constraints_.empty()) {
        return true;
    }

    // All constraints must be satisfied
    for (const auto& constraint : constraints_) {
        bool satisfied = false;

        switch (constraint.op) {
            case Constraint::EQ: satisfied = (version == constraint.version); break;
            case Constraint::NE: satisfied = (version != constraint.version); break;
            case Constraint::LT: satisfied = (version < constraint.version); break;
            case Constraint::LE: satisfied = (version <= constraint.version); break;
            case Constraint::GT: satisfied = (version > constraint.version); break;
            case Constraint::GE: satisfied = (version >= constraint.version); break;
        }

        if (!satisfied) {
            return false;  // At least one constraint failed
        }
    }

    return true;  // All constraints satisfied
}

std::string VersionRange::to_string() const {
    if (constraints_.empty()) {
        return "*";
    }

    std::string result;
    for (size_t i = 0; i < constraints_.size(); i++) {
        if (i > 0) result += " ";

        const auto& c = constraints_[i];
        switch (c.op) {
            case Constraint::EQ: result += "="; break;
            case Constraint::NE: result += "!="; break;
            case Constraint::LT: result += "<"; break;
            case Constraint::LE: result += "<="; break;
            case Constraint::GT: result += ">"; break;
            case Constraint::GE: result += ">="; break;
        }

        result += c.version.to_string();
    }

    return result;
}
```

#### Step 2: Add Validation Logic (3 hours)

**File**: [src/framework_core/src/mod_registry.cpp](../../../src/framework_core/src/mod_registry.cpp)

**Add Method**:
```cpp
void ModRegistry::validate_versions(const std::string& current_game_version) {
    SemanticVersion game_version;
    if (!SemanticVersion::parse(current_game_version, game_version)) {
        LOG_WARNING("Invalid game version format: {}", current_game_version);
        return;
    }

    LOG_INFO("Validating mod versions against game version {}", current_game_version);

    // Check each registered mod
    for (const auto& [mod_id, metadata] : discovered_mods_metadata_) {
        // Validate mod version format
        SemanticVersion mod_version;
        if (!SemanticVersion::parse(metadata.version, mod_version)) {
            LOG_WARNING("Mod {} has invalid version format: {}",
                        metadata.display_name, metadata.version);
            continue;
        }

        // Check game version compatibility
        VersionRange game_range;
        if (VersionRange::parse(metadata.supported_game_versions, game_range)) {
            if (!game_range.satisfies(game_version)) {
                LOG_WARNING("Mod {} v{} may not be compatible with game version {}",
                            metadata.display_name,
                            metadata.version,
                            current_game_version);
                LOG_WARNING("  Supported game versions: {}",
                            metadata.supported_game_versions);
            } else {
                LOG_DEBUG("Mod {} v{} compatible with game version {}",
                          metadata.display_name,
                          metadata.version,
                          current_game_version);
            }
        }

        // Check incompatible mods
        check_incompatibilities(mod_id, metadata);
    }
}

void ModRegistry::check_incompatibilities(
    const std::string& mod_id,
    const ModMetadata& metadata)
{
    for (const auto& incomp : metadata.incompatible_mods) {
        // Check if incompatible mod is registered
        if (discovered_mods_.count(incomp.mod_id) == 0) {
            continue;  // Not present, no conflict
        }

        // Get other mod's metadata
        const auto& other_metadata = discovered_mods_metadata_[incomp.mod_id];

        // Parse other mod's version
        SemanticVersion other_version;
        if (!SemanticVersion::parse(other_metadata.version, other_version)) {
            continue;  // Can't validate, skip
        }

        // Parse incompatibility version range
        VersionRange incomp_range;
        if (!VersionRange::parse(incomp.versions, incomp_range)) {
            continue;  // Invalid range, skip
        }

        // Check if conflict exists
        if (incomp_range.satisfies(other_version)) {
            LOG_WARNING("INCOMPATIBILITY DETECTED:");
            LOG_WARNING("  Mod: {} v{}",
                        metadata.display_name, metadata.version);
            LOG_WARNING("  Conflicts with: {} v{}",
                        other_metadata.display_name, other_metadata.version);
            if (!incomp.reason.empty()) {
                LOG_WARNING("  Reason: {}", incomp.reason);
            }
            LOG_WARNING("  This may cause crashes or unexpected behavior!");
        }
    }
}
```

#### Step 3: Call Validation at Startup (1 hour)

**File**: [src/framework_core/src/framework_core.cpp](../../../src/framework_core/src/framework_core.cpp)

**Add to State Machine** (after discovery):
```cpp
void FrameworkCore::on_all_mods_registered() {
    // Existing: Generate capabilities
    generate_capabilities();

    // NEW: Validate versions
    std::string game_version = get_game_version();  // Implement this
    mod_registry_->validate_versions(game_version);

    // Broadcast registration complete...
}

std::string FrameworkCore::get_game_version() {
    // TODO: Detect actual game version
    // For now, return from config or hardcoded
    return config_manager_->get_game_version();
}
```

**Add to Config**:
```json
{
  "game_version": "0.3.5",
  // ... other config
}
```

### Testing Plan

**Test Cases**:

| Mod Version | Game Version | Supported Range | Expected |
|-------------|--------------|-----------------|----------|
| `1.0.0` | `0.3.5` | `>=0.3.0 <0.4.0` | ✅ Compatible |
| `1.0.0` | `0.4.0` | `>=0.3.0 <0.4.0` | ⚠️ Warning |
| `1.0.0` | `0.2.0` | `>=0.3.0 <0.4.0` | ⚠️ Warning |
| `1.0.0` | `0.3.5` | `^0.3.0` | ✅ Compatible |
| `1.0.0` | `0.4.0` | `^0.3.0` | ⚠️ Warning |

**Incompatibility Test**:
```json
// Mod A
{
  "mod_id": "author.game.modA",
  "version": "1.0.0",
  "incompatible_mods": [
    {"mod_id": "author.game.modB", "versions": ">=2.0.0", "reason": "Both modify same systems"}
  ]
}

// Mod B
{
  "mod_id": "author.game.modB",
  "version": "2.1.0"
}
```

Expected: Warning logged about incompatibility.

---

## Feature 3.2: Static .lib Build Option

### Problem Statement

**Current State**: APClientLib is DLL-only, requires deployment with mods.

**Intended Design**: Provide static library option for simpler C++ mod deployment.

**Impact**: C++ mods must distribute APClientLib.dll alongside their own DLL.

### Solution Design

#### Build Options

- **Dynamic Library** (default): `APClientLib.dll` (shared runtime)
- **Static Library** (optional): `APClientLib.lib` (linked into mod DLL)

#### CMake Configuration

```bash
# Build dynamic (default)
cmake -B build

# Build static
cmake -B build -DBUILD_STATIC_CLIENT_LIB=ON

# Build both
cmake -B build -DBUILD_BOTH_CLIENT_LIBS=ON
```

### Implementation Steps

#### Step 1: Update CMakeLists.txt (2 hours)

**File**: [src/client_lib/CMakeLists.txt](../../../src/client_lib/src/client_lib/CMakeLists.txt)

```cmake
# Option to build static library
option(BUILD_STATIC_CLIENT_LIB "Build static library instead of DLL" OFF)
option(BUILD_BOTH_CLIENT_LIBS "Build both static and dynamic libraries" OFF)

# Source files
set(CLIENT_LIB_SOURCES
    src/ap_client_lib.cpp
    src/ipc_client.cpp
)

set(CLIENT_LIB_HEADERS
    include/ap_client_lib.h
    include/ipc_client.h
)

if(BUILD_BOTH_CLIENT_LIBS OR BUILD_STATIC_CLIENT_LIB)
    # Build static library
    add_library(APClientLib_static STATIC
        ${CLIENT_LIB_SOURCES}
        ${CLIENT_LIB_HEADERS}
    )

    target_include_directories(APClientLib_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann
    )

    target_link_libraries(APClientLib_static PUBLIC
        ws2_32
    )

    # Output name without _static suffix
    set_target_properties(APClientLib_static PROPERTIES
        OUTPUT_NAME APClientLib
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/Release
    )

    # Install
    install(TARGETS APClientLib_static
            ARCHIVE DESTINATION lib)
endif()

if(BUILD_BOTH_CLIENT_LIBS OR NOT BUILD_STATIC_CLIENT_LIB)
    # Build dynamic library (default)
    add_library(APClientLib SHARED
        ${CLIENT_LIB_SOURCES}
        ${CLIENT_LIB_HEADERS}
    )

    target_include_directories(APClientLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann
    )

    target_link_libraries(APClientLib PUBLIC
        ws2_32
    )

    target_compile_definitions(APClientLib PRIVATE
        APCLIENT_EXPORTS  # Export symbols
    )

    set_target_properties(APClientLib PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
    )

    # Install
    install(TARGETS APClientLib
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

# Always install headers
install(FILES ${CLIENT_LIB_HEADERS} DESTINATION include)
```

#### Step 2: Update Header for Static Build (30 min)

**File**: [src/client_lib/include/ap_client_lib.h](../../../src/client_lib/include/ap_client_lib.h)

```cpp
#pragma once

// DLL export/import macros
#ifdef _WIN32
    #ifdef APCLIENT_EXPORTS
        #define APCLIENT_API __declspec(dllexport)
    #elif defined(APCLIENT_STATIC)
        #define APCLIENT_API  // No export for static
    #else
        #define APCLIENT_API __declspec(dllimport)
    #endif
#else
    #define APCLIENT_API
#endif

// Rest of API...
```

#### Step 3: Document Usage (1 hour)

**File**: [docs/BUILDING_CPP_MODS.md](../../../docs/BUILDING_CPP_MODS.md)

```markdown
# Building C++ Mods with APClientLib

## Option 1: Dynamic Library (DLL)

**Pros**: Smaller mod DLL, shared runtime
**Cons**: Must distribute APClientLib.dll with mod

**CMakeLists.txt**:
```cmake
# Link against shared library
target_link_libraries(MyMod PRIVATE
    APClientLib
)
```

**Deployment**:
```
ue4ss/Mods/MyMod/
├── MyMod.dll
├── APClientLib.dll  ← Must include
└── ap_config.json
```

## Option 2: Static Library (.lib)

**Pros**: Single DLL, no extra files
**Cons**: Larger mod DLL

**Build APClientLib**:
```bash
cmake -B build -DBUILD_STATIC_CLIENT_LIB=ON
cmake --build build --config Release
```

**CMakeLists.txt**:
```cmake
# Link against static library
target_compile_definitions(MyMod PRIVATE APCLIENT_STATIC)
target_link_libraries(MyMod PRIVATE
    APClientLib_static  # Note: _static suffix
)
```

**Deployment**:
```
ue4ss/Mods/MyMod/
├── MyMod.dll  ← APClientLib code included
└── ap_config.json
```

## Recommended Approach

Use **static library** for simpler deployment. Users only need your mod DLL.
```

### Testing Plan

**Build Test**:
1. Build with `BUILD_STATIC_CLIENT_LIB=ON`
2. Verify `APClientLib.lib` created in `lib/Release/`
3. Build example mod linking static lib
4. Verify example mod DLL doesn't require APClientLib.dll

**Runtime Test**:
1. Deploy static-linked mod without APClientLib.dll
2. Launch game
3. Verify mod works correctly
4. Compare DLL sizes (static-linked should be larger)

---

## Feature 3.3: Advanced Error Handling

### Problem Statement

**Current State**: Many operations fail silently or just log errors.

**Intended Design**: Robust error handling with recovery:
- Automatic reconnection on IPC disconnect
- Automatic reconnection on AP disconnect
- Message queue persistence (optional)
- Better error messages with suggestions

**Impact**: Users must manually restart on transient failures.

### Solution Design

#### Error Handling Strategy

1. **Transient Errors**: Auto-retry with exponential backoff
2. **Permanent Errors**: Log clear message with solution
3. **Recoverable Errors**: Attempt recovery, fallback to restart prompt

#### Retry Logic

```
Attempt 1: Immediate
Attempt 2: Wait 1s
Attempt 3: Wait 2s
Attempt 4: Wait 4s
Attempt 5: Wait 8s
Give up: After 5 attempts (15s total)
```

### Implementation Steps

#### Step 1: IPC Client Reconnection (3 hours)

**File**: [src/client_lib/src/ipc_client.cpp](../../../src/client_lib/src/ipc_client.cpp)

**Add Retry Logic**:
```cpp
class IPCClient {
public:
    bool connect(const std::string& pipe_name);

private:
    bool connect_with_retry(int max_attempts = 5);
    void exponential_backoff(int attempt);

    int retry_attempt_ = 0;
    std::chrono::steady_clock::time_point last_retry_time_;
};

bool IPCClient::connect_with_retry(int max_attempts) {
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        retry_attempt_ = attempt;

        LOG_DEBUG("IPC connection attempt {} of {}", attempt, max_attempts);

        if (connect_immediate()) {
            LOG_INFO("IPC connected on attempt {}", attempt);
            retry_attempt_ = 0;
            return true;
        }

        if (attempt < max_attempts) {
            exponential_backoff(attempt);
        }
    }

    LOG_ERROR("IPC connection failed after {} attempts", max_attempts);
    last_error_ = "Failed to connect to framework. Ensure APFramework is loaded.";
    return false;
}

void IPCClient::exponential_backoff(int attempt) {
    int delay_ms = (1 << (attempt - 1)) * 1000;  // 1s, 2s, 4s, 8s...
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

bool IPCClient::connect_immediate() {
    // Wait for pipe availability (5s timeout)
    if (!WaitNamedPipeA(pipe_path_.c_str(), 5000)) {
        return false;
    }

    // Open pipe
    pipe_handle_ = CreateFileA(
        pipe_path_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Set message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe_handle_, &mode, nullptr, nullptr);

    connected_ = true;
    return true;
}

// Auto-reconnect on disconnect
bool IPCClient::send_message(const IPCMessage& msg) {
    if (!connected_) {
        // Attempt reconnection
        if (!connect_with_retry(3)) {  // Quick retry (3 attempts)
            return false;
        }
    }

    // Send message...
    if (!write_success) {
        // Pipe broken, mark disconnected
        connected_ = false;
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    return true;
}
```

#### Step 2: AP Client Reconnection (2 hours)

**File**: [src/framework_core/src/ap_client.cpp](../../../src/framework_core/src/ap_client.cpp)

**Add Auto-Reconnect**:
```cpp
class APClientWrapper {
public:
    void poll();
    bool is_connected() const;
    void attempt_reconnect();

private:
    void handle_disconnect();

    bool auto_reconnect_ = true;
    int reconnect_attempt_ = 0;
    std::chrono::steady_clock::time_point last_reconnect_time_;

    // Cached connection params for reconnect
    std::string cached_server_;
    int cached_port_ = 0;
    std::string cached_slot_;
    std::string cached_password_;
};

void APClientWrapper::poll() {
    std::lock_guard<std::mutex> lock(client_mutex_);

    if (!impl_ || !impl_->client) {
        // Not connected, try reconnect if enabled
        if (auto_reconnect_ && !cached_server_.empty()) {
            attempt_reconnect();
        }
        return;
    }

    impl_->client->poll();

    // Check connection state
    if (impl_->client->get_state() == ::APClient::State::DISCONNECTED) {
        handle_disconnect();
    }
}

void APClientWrapper::handle_disconnect() {
    LOG_WARNING("Disconnected from AP server");

    // Queue disconnect message
    APMessage msg;
    msg.type = APMessage::Type::Disconnected;
    pending_messages_.push(msg);

    if (auto_reconnect_) {
        LOG_INFO("Auto-reconnect enabled, will retry...");
    }
}

void APClientWrapper::attempt_reconnect() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_reconnect_time_
    ).count();

    // Exponential backoff: 5s, 10s, 20s, 40s...
    int delay = (1 << std::min(reconnect_attempt_, 6)) * 5;

    if (elapsed < delay) {
        return;  // Too soon, wait longer
    }

    reconnect_attempt_++;
    last_reconnect_time_ = now;

    LOG_INFO("Reconnection attempt {} (waited {}s)", reconnect_attempt_, elapsed);

    if (connect(cached_server_, cached_port_, cached_slot_, cached_password_)) {
        LOG_INFO("Reconnected successfully!");
        reconnect_attempt_ = 0;
    } else {
        LOG_WARNING("Reconnection attempt {} failed", reconnect_attempt_);
    }
}

bool APClientWrapper::connect(
    const std::string& server,
    int port,
    const std::string& slot_name,
    const std::string& password)
{
    // Cache params for reconnect
    cached_server_ = server;
    cached_port_ = port;
    cached_slot_ = slot_name;
    cached_password_ = password;

    // Existing connection logic...
}
```

#### Step 3: Better Error Messages (3 hours)

**Create Error Message Formatter**:

**File**: [src/framework_core/include/error_messages.h](../../../src/framework_core/include/error_messages.h)

```cpp
#pragma once
#include <string>

class ErrorMessages {
public:
    // Format error with helpful context and suggestions
    static std::string format_error(
        const std::string& error_code,
        const std::string& context = ""
    );

    // Common errors
    static constexpr const char* INVALID_MOD_ID = "INVALID_MOD_ID";
    static constexpr const char* REGISTRATION_TIMEOUT = "REGISTRATION_TIMEOUT";
    static constexpr const char* IPC_CONNECTION_FAILED = "IPC_CONNECTION_FAILED";
    static constexpr const char* AP_CONNECTION_FAILED = "AP_CONNECTION_FAILED";
    static constexpr const char* VERSION_MISMATCH = "VERSION_MISMATCH";
};
```

**File**: [src/framework_core/src/error_messages.cpp](../../../src/framework_core/src/error_messages.cpp)

```cpp
#include "error_messages.h"
#include <map>

static const std::map<std::string, std::string> ERROR_TEMPLATES = {
    {"INVALID_MOD_ID",
     "Invalid mod_id format.\n"
     "Expected: author.game.mod\n"
     "Example: john.palworld.chest_shuffle\n"
     "Fix: Update 'mod_id' in your ap_config.json"},

    {"REGISTRATION_TIMEOUT",
     "Mod registration timeout.\n"
     "Possible causes:\n"
     "  1. Mod loaded before framework (check load order)\n"
     "  2. Mod crashed during startup\n"
     "  3. Mod's ap_config.json has incorrect mod_id\n"
     "Fix: Check UE4SS.log for mod errors, verify load order"},

    {"IPC_CONNECTION_FAILED",
     "Failed to connect to framework IPC server.\n"
     "Possible causes:\n"
     "  1. APFramework not loaded\n"
     "  2. Framework crashed during startup\n"
     "  3. Incorrect pipe name\n"
     "Fix: Ensure APFramework loads before your mod, check framework.log"},

    {"AP_CONNECTION_FAILED",
     "Failed to connect to Archipelago server.\n"
     "Possible causes:\n"
     "  1. Server is offline\n"
     "  2. Incorrect server address or port\n"
     "  3. Network firewall blocking connection\n"
     "Fix: Verify server is running, check connection settings"},

    {"VERSION_MISMATCH",
     "Mod version incompatible with game version.\n"
     "This may cause crashes or unexpected behavior.\n"
     "Fix: Update mod to compatible version or ignore warning"}
};

std::string ErrorMessages::format_error(
    const std::string& error_code,
    const std::string& context)
{
    auto it = ERROR_TEMPLATES.find(error_code);
    if (it == ERROR_TEMPLATES.end()) {
        return "Unknown error: " + error_code;
    }

    std::string msg = it->second;

    if (!context.empty()) {
        msg += "\n\nContext: " + context;
    }

    return msg;
}
```

**Usage**:
```cpp
LOG_ERROR("{}", ErrorMessages::format_error(
    ErrorMessages::INVALID_MOD_ID,
    "mod_id: 'MyMod'"
));
```

### Testing Plan

**IPC Reconnection Test**:
1. Start mod before framework
2. Verify retry attempts logged
3. Start framework mid-retry
4. Verify connection succeeds

**AP Reconnection Test**:
1. Connect to AP server
2. Stop AP server
3. Verify disconnect logged
4. Restart AP server
5. Verify auto-reconnect succeeds

**Error Message Test**:
1. Trigger each error condition
2. Verify helpful error message logged
3. Verify message includes fix suggestions

---

## Integration and Testing

### Full System Test

**Test Environment**:
- Clean UE4SS installation
- 3 test mods (1 Lua, 1 C++ static, 1 C++ dynamic)
- Mock AP server
- Various game versions

**Test Scenarios**:

1. **Clean Install**: All mods compatible
   - Expected: No warnings, all features work

2. **Version Mismatch**: Mod incompatible with game version
   - Expected: Warning logged, mod still loads

3. **Mod Conflict**: Two incompatible mods
   - Expected: Warning logged, both mods load

4. **Transient IPC Failure**: Framework starts late
   - Expected: Mods retry and connect

5. **Transient AP Failure**: Server goes down mid-session
   - Expected: Auto-reconnect when server returns

---

## Documentation Requirements

**New Documents**:
- `docs/VERSION_COMPATIBILITY.md` - Semver guide
- `docs/ERROR_RECOVERY.md` - Error handling guide
- `docs/BUILDING_CPP_MODS.md` - Static vs dynamic linking

**Updated Documents**:
- `README.md` - Add version compatibility section
- `docs/TROUBLESHOOTING.md` - Add error message reference

---

## Success Metrics

After Phase 3 completion:

- [ ] Version warnings displayed for all mismatches
- [ ] Static library build available and tested
- [ ] Transient failures recover automatically
- [ ] Error messages rated "helpful" by testers
- [ ] Framework achieves 99% uptime in stress tests
- [ ] All documentation complete and accurate

---

## Rollout Strategy

### Phase 3.1: Semver (Week 1)
- Implement semver parser
- Add validation logic
- Test with various version scenarios
- Deploy to beta

### Phase 3.2: Static Lib (Week 2)
- Add CMake build options
- Test both static and dynamic builds
- Update documentation
- Provide example

### Phase 3.3: Error Handling (Week 2-3)
- Implement retry logic
- Add error message templates
- Test failure scenarios
- Gather user feedback on messages

---

**End of Phase 3 Plan**