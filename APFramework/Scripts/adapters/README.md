# APFramework Adapters

This directory contains community-created adapters for integrating existing Palworld mods with the Archipelago Framework.

## What are Adapters?

Adapters allow existing mods to work with APFramework **without any modifications** to the original mod. The adapter acts as a bridge between the mod and APFramework.

## When to Create an Adapter

Create an adapter when:
- A popular mod doesn't have built-in AP support
- The mod author isn't available to add AP support
- You want to quickly prototype AP integration for a mod
- The community wants to use a mod with AP

## Adapter Structure

An adapter must implement two functions:

```lua
local Adapter = {}

-- Check if the target mod is present and loaded
function Adapter.IsModPresent()
    return package.loaded["ModName"] ~= nil
end

-- Create capability configuration by reading the mod's data
function Adapter.CreateCapabilities()
    local Mod = require("ModName")

    return {
        mod_info = {
            id = "mod_name",
            name = "Mod Name",
            version = "1.0.0",
            author = "Original Mod Author"
        },
        capabilities = {
            locations = { ... },
            items = { ... },
            -- etc.
        },
        callbacks = {
            onInit = function() ... end,
            onSeedReceived = function(data) ... end,
            -- etc.
        }
    }
end

return Adapter
```

## Example: ChestMod Adapter

See `.claude/REAL_WORLD_INTEGRATION_EXAMPLE.md` for a complete example of creating an adapter for an existing mod.

## Testing Your Adapter

1. Place your adapter file in this directory (e.g., `MyMod.lua`)
2. Launch Palworld with APFramework
3. Check console for: `[ModRegistry] Adapter loaded: MyMod.lua`
4. Generate capability manifest
5. Verify your mod's capabilities appear in the manifest

## Submitting Adapters

If you've created an adapter for a popular mod, consider:
1. Testing it thoroughly
2. Documenting any special requirements
3. Submitting a pull request to the main repository
4. Sharing it on the Palworld modding Discord

## Adapter Guidelines

- **Don't modify the original mod** - Only read its data
- **Handle errors gracefully** - Use pcall for mod interactions
- **Respect mod authors** - Credit them in your adapter
- **Keep it updated** - Watch for mod updates that might break your adapter
- **Document behavior** - Explain what your adapter does

## Community Adapters

(This section will list community-created adapters once they're available)

## Need Help?

- Check the Integration Guide: `../../docs/INTEGRATION_GUIDE.md`
- See example adapters in this directory
- Ask on Discord: #palworld-archipelago
- Open an issue on GitHub

Happy adapting! 🔌
