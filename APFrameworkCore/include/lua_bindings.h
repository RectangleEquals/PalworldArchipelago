#pragma once
#include <sol/sol.hpp>

namespace APFramework {

/**
 * Register all APFrameworkCore bindings with a Lua state
 *
 * This function exposes the APManager singleton and related types to Lua,
 * allowing the APFrameworkMod to initialize and control the framework.
 *
 * @param lua The sol::state to register bindings with
 */
void register_apframework_bindings(sol::state& lua);

} // namespace APFramework