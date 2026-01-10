#include "ap_manager.h"

namespace APFramework {

APManager::APManager() {
}

APManager::~APManager() {
}

LifecyclePhase APManager::get_current_phase() const {
    return current_phase_;
}

// TODO: Implement remaining functionality in Phase 07

} // namespace APFramework