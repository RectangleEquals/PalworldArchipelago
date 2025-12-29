# Palworld Archipelago Framework - Core Architecture

## System Overview

The framework consists of three interconnected layers:

1. **AP Python World** - Server-side generation and logic
2. **Lua/UE4SS Framework** - Client runtime coordination
3. **Mod Integration Layer** - Plugin system for 3rd-party mods

```
┌─────────────────────────────────────────────────────────────┐
│                    AP Server (Python)                       │
│  ┌────────────────────────────────────────────────────────┐ │
│  │         Palworld World Implementation                  │ │
│  │  - Generation logic                                    │ │
│  │  - Location/Item definitions                           │ │
│  │  - Rules and logic                                     │ │
│  │  - Dynamic capability discovery                        │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                           ▲ │
                   JSON    │ │ JSON
                 Capability│ │ Seed Data
                   Query   │ ▼
┌─────────────────────────────────────────────────────────────┐
│              Palworld Client (UE4SS + Lua)                  │
│  ┌────────────────────────────────────────────────────────┐ │
│  │          AP Framework Core (Lua)                       │ │
│  │  - Mod discovery and registration                      │ │
│  │  - Capability aggregation                              │ │
│  │  - AP client connection (lua-apclientpp)               │ │
│  │  - Event coordination                                  │ │
│  │  - State synchronization                               │ │
│  └────────────────────────────────────────────────────────┘ │
│                           ▲ │                               │
│                  Register │ │ Callbacks/Events              │
│                           │ ▼                               │
│  ┌────────────────────────────────────────────────────────┐ │
│  │           Mod Integration Layer                        │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │ │
│  │  │ Chest Shuffle│  │ Region Lock  │  │  Item Rando  │  │ │
│  │  │     Mod      │  │     Mod      │  │     Mod      │  │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

### 1. AP Python World (`worlds/palworld/`)

**Purpose:** Server-side generation, logic definition, and capability negotiation

**Key Responsibilities:**
- Define base game items, locations, and regions
- Implement generation logic with seeded RNG
- **Dynamic capability discovery:** Query connected clients for available mod features
- Validate and merge mod-provided capabilities
- Generate seed data with mod-specific randomization
- Provide slot data to clients

**Key Files:**
- `__init__.py` - World class implementation
- `items.py` - Item definitions and groups
- `locations.py` - Location definitions and groups  
- `regions.py` - Region definitions and connections
- `rules.py` - Access rules and logic
- `options.py` - Player options (including mod toggles)
- `mod_interface.py` - **NEW:** Mod capability discovery and validation

### 2. Lua/UE4SS Framework Core

**Purpose:** Runtime coordination, mod management, and AP connectivity

**Key Responsibilities:**
- Discover and register AP-compatible mods
- Aggregate mod capabilities and constraints
- Communicate capabilities to AP server (for offline generation)
- Receive and distribute seed/slot data to mods
- Connect to AP server via lua-apclientpp
- Coordinate location checks and item receipts
- Manage save state for AP data
- Provide event system for mods

**Key Modules:**
- `APFramework.lua` - Main framework entry point
- `ModRegistry.lua` - Mod discovery and registration
- `APClient.lua` - AP server connection wrapper
- `EventBus.lua` - Event coordination system
- `CapabilityManager.lua` - Capability aggregation and validation
- `StateManager.lua` - Save/load AP state
- `ConfigManager.lua` - Configuration handling

### 3. Mod Integration Layer

**Purpose:** Extensible plugin system for randomization features

**Key Responsibilities:**
- Implement specific randomization features (chest shuffle, region lock, etc.)
- Declare capabilities and requirements to framework
- React to framework events (location checks, item receipts, state changes)
- Minimal modification to existing mods
- Coordinate with other mods when necessary

**Integration Pattern:**
```lua
-- Mod declares itself to framework
APFramework.RegisterMod({
    id = "chest_shuffle",
    name = "Chest Location Randomizer",
    version = "1.0.0",
    
    -- Declare what this mod can randomize
    capabilities = {
        locations = {
            type = "chest",
            count = 247, -- discovered at runtime
            needs_runtime = true, -- requires runtime enforcement
            metadata = { ... } -- chest positions, IDs, etc.
        }
    },
    
    -- Declare dependencies/conflicts
    requires = {},
    conflicts = {},
    
    -- Framework will call these
    callbacks = {
        onInit = function() end,
        onCapabilityQuery = function() end,
        onSeedReceived = function(seed_data) end,
        onLocationChecked = function(location_id) end,
        onItemReceived = function(item_data) end,
        onSave = function() end,
        onLoad = function(save_data) end
    }
})
```

## Data Flow

### Generation Flow (Offline)

1. **Mod Discovery Phase**
   - Player launches Palworld with AP framework + mods
   - Framework discovers and registers all AP-compatible mods
   - Framework aggregates mod capabilities into JSON manifest
   - Manifest saved to `APCapabilities.json`

2. **Generation Phase**
   - Player provides capability manifest to AP generator (upload to webhost or local)
   - AP Python world reads capabilities, validates compatibility
   - Generates seed considering available mod features
   - Outputs `.appalworld` file with seed data

3. **Runtime Enforcement Phase**
   - Player loads `.appalworld` in Palworld
   - Framework distributes relevant seed data to each mod
   - Mods enforce randomization based on seed data
   - Framework connects to AP server for item sync

### Generation Flow (Online - Future Enhancement)

1. Framework connects to AP server with capability manifest
2. AP server generates seed on-demand with known capabilities
3. Seed data pushed directly to client
4. Runtime enforcement as above

### Runtime Flow

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   Player    │────────▶│  Framework   │◀───────▶│  AP Server  │
│   Action    │         │    Core      │         │             │
└─────────────┘         └──────────────┘         └─────────────┘
      │                        │                        │
      │ Opens Chest           │                        │
      ├──────────────────────▶│                        │
      │                       │ Lookup Location ID     │
      │                       │ Check Rules            │
      │                       ├───────────────────────▶│
      │                       │    LocationCheck       │
      │                       │                        │
      │                       │◀───────────────────────│
      │                       │  Items for Player      │
      │                       │                        │
      │                       │ Route to Mods          │
      │                       ├──────────────────────▶ Mods
      │                       │                        │
      │◀──────────────────────┤                        │
      │  Receive Item(s)      │                        │
```

## Key Design Principles

### 1. Minimal Mod Modification
- Existing mods need only add APFramework registration code
- Framework handles all AP-specific logic
- Mods focus on their feature implementation

### 2. Capability-Driven Generation
- AP generation adapts to available mods
- Validates mod compatibility before generation
- Graceful degradation if mods unavailable

### 3. Runtime Flexibility
- Some randomization can be pre-computed (offline)
- Some requires runtime enforcement (dynamic spawns, events)
- Framework coordinates both approaches

### 4. Crash Resilience
- Avoid extensive UE4SS C++ modifications
- Lua-based with minimal native dependencies
- Graceful handling of Palworld updates

### 5. Extensibility First
- Clear plugin API for mod developers
- Example mods as reference implementations
- Community can extend without core changes

## Next Steps

1. **Phase 2:** Define mod capability schema and registration API
2. **Phase 3:** Design AP Python world with capability discovery
3. **Phase 4:** Implement Lua framework core
4. **Phase 5:** Create example mods (chest shuffle, region lock)
5. **Phase 6:** Testing and documentation
