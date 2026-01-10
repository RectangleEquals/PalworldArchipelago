#pragma once
#include "ap_types.h"

namespace APFramework {

class APManager {
public:
    APManager();
    ~APManager();

    // Stub for Phase 04 - will be implemented in Phase 07
    LifecyclePhase get_current_phase() const;

    // TODO: Implement remaining functionality in Phase 07
private:
    LifecyclePhase current_phase_{LifecyclePhase::UNINITIALIZED};
};

} // namespace APFramework