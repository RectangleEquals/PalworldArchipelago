--[[
    APTest - Minimal Archipelago Test Mod

    This mod provides test capabilities for verifying APFramework functionality.
    As a submodule of APFramework, it's discovered automatically during framework init.
]]

print("[APTest] Initializing APTest submodule...")

-- This file is loaded by APFramework after discovery
-- All registration happens via ap_config.json

local APTest = {}

function APTest:Initialize()
    print("[APTest] Test mod initialized")
    print("[APTest] Capabilities registered via ap_config.json")
end

-- Auto-initialize
APTest:Initialize()

return APTest
