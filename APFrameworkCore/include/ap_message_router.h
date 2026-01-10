#pragma once
#include "ap_types.h"

namespace APFramework {

// Forward declaration
struct APMessage;

class APMessageRouter {
public:
    APMessageRouter();
    ~APMessageRouter();

    // Stub for Phase 04 - will be implemented in Phase 06
    void route_ap_message(const APMessage& message);

    // TODO: Implement remaining functionality in Phase 06
};

} // namespace APFramework