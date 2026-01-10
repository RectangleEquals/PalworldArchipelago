#pragma once
#include <string>

namespace APClientLib {

// Result type for operations that may fail
struct VoidResult {
    bool success;
    std::string error_message;

    static VoidResult success_result() {
        return {true, ""};
    }

    static VoidResult failure(const std::string& msg) {
        return {false, msg};
    }

    bool is_success() const { return success; }
    bool is_failure() const { return !success; }
};

class APClientLibrary {
public:
    APClientLibrary();
    ~APClientLibrary();

    // TODO: Implement in Phase 09
};

} // namespace APClientLib