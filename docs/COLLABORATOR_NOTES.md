# Palworld Archipelago - Collaborator Notes

**Last Updated**: December 31, 2024

## Quick Status

**Current Phase**: Phase 4 - Runtime Integration (Partial)

**What's Working**:
- ✅ Connection to AP server
- ✅ Authentication (slot connected)
- ✅ Continuous polling (temporary blocking loop solution)
- ✅ Item receiving handler fires correctly
- ✅ Frame callback system for submods

**What's Next**:
- ❌ Permanent polling solution (non-blocking)
- ❌ Item granting to player inventory
- ❌ Location checking
- ❌ State persistence

---

## Testing Instructions

### Prerequisites
1. Archipelago server running on `localhost:38281`
2. Palworld installed with UE4SS
3. APFramework installed to `<game>/ue4ss/Mods/APFramework/`
4. Valid slot created on the server

### Configuration Setup

1. **Edit Framework Config** (`APFramework/config.json`):
```json
{
  "framework": {
    "version": "1.0.0",
    "debug_mode": true,
    "auto_connect": true,
    "poll_interval_ms": 16
  },
  "ap_connection": {
    "enabled": true,
    "server": "localhost",
    "port": 38281,
    "slot_name": "YOUR_SLOT_NAME",
    "password": "",
    "auto_reconnect": true
  },
  "ui": {
    "show_notifications": true,
    "show_debug_overlay": false
  }
}
```

2. **Replace** `YOUR_SLOT_NAME` with your actual slot name

### Testing Item Receiving

1. **Launch Palworld** with APFramework enabled
2. **Check UE4SS log** for:
   ```
   [APFramework] Successfully connected to AP server
   [APFramework] Starting continuous polling (interval: 16ms)
   ```
3. **In Archipelago Text Client**, run:
   ```
   !getitem <item_name>
   ```
   Example: `!getitem Palbox`

4. **Check UE4SS log** for:
   ```
   [APClient] Item received: <item_id>
   ```

### Expected Behavior

**Successful Connection Flow**:
```
[APFramework] Loading modules...
[APFramework] Framework initialized successfully
[APFramework] Discovered 1 AP-compatible mods
[APFramework] Found AP connection config: localhost:38281
[APClient] Socket connected to server
[APClient] Received room info, authenticating...
[APClient] Slot connected! Authenticated successfully
[APClient] Final state: 4 (connected=true, authenticated=true)
[APFramework] Starting continuous polling (interval: 16ms)
```

**Item Receipt**:
```
[APClient] Item received: 8370050
```

### Known Issues & Limitations

#### Temporary Polling Solution
The current continuous polling uses a **blocking while loop** in the main Lua state:

**Implications**:
- ⚠️ **Blocks all mods loaded after APFramework**
  - UE4SS loads mods sequentially on shared thread
  - If APFramework is in load order before other mods, those mods won't initialize
  - Workaround: Ensure APFramework is last in load order, or disable other mods for testing

- ⚠️ **No UE4 operations available yet**
  - Event handlers run in polling loop (not game thread)
  - Cannot grant items to player inventory yet
  - Cannot hook game events for location checking yet

**Why This Approach**:
- lua-apclientpp requires polling in the **same Lua state** where client was created
- All threading approaches (`LoopAsync`, `ExecuteWithDelay`, `ExecuteInGameThread`) create **new Lua states**
- This causes "Lua state changed" errors
- Blocking loop runs in original state, preventing errors

**Future Solution**:
- Need to find UE4 game tick hook that runs in same Lua state
- Possible: RegisterHook on PlayerController:Tick
- Investigating UE4SS event system alternatives

#### Item Granting Not Implemented
- Handlers fire and log item data correctly
- No actual item granting to player yet
- Need to research Palworld inventory modification functions
- Need to map AP item IDs to Palworld items

#### Location Checking Not Implemented
- No hooks into Palworld game events
- Need to detect chest opens, pal captures, etc.
- Need to map game events to AP location IDs

### Debugging Tips

**Check Connection State**:
```
[APClient] Final state: 4 (connected=true, authenticated=true)
```
- State 0 = DISCONNECTED
- State 1 = SOCKET_CONNECTING
- State 2 = SOCKET_CONNECTED
- State 3 = ROOM_INFO
- State 4 = SLOT_CONNECTED ← Target state

**Connection Fails**:
- Check AP server is running
- Check firewall settings
- Verify slot name matches server
- Check UE4SS log for errors

**TLS Handshake Errors**:
```
[APClient] Socket error: TLS handshake failed
```
- This is normal on first connection attempt
- Client retries automatically
- Should succeed on second attempt

**Polling Stops**:
- Check if blocking loop exited due to error
- Look for `Poll error:` messages in log
- Verify APClient instance still exists

### Testing Frame Callbacks

Frame callbacks allow submods to execute code during polling loop:

**Example** (add to APTest or create test mod):
```lua
-- Register callback during mod initialization
APFramework.RegisterFrameCallback("test_callback", function()
    -- Runs every ~16ms
    -- Can access same Lua state as APClient
    -- Cannot use UE4 functions yet
end)
```

**Expected Log Output**:
```
[EventBus] Registered frame callback for mod: test_callback
```

**Error Handling**:
- Callbacks wrapped in pcall() for error isolation
- One callback error won't crash others
- Errors logged with mod_id for debugging

---

## Development Workflow

### File Structure
```
APFramework/
├── config.json               # Framework configuration
├── config.example.json       # Template for users
├── Scripts/
│   ├── main.lua              # Entry point, polling loop
│   ├── APFramework.lua       # Core framework
│   ├── APClient.lua          # WebSocket client
│   ├── EventBus.lua          # Event system + frame callbacks
│   ├── ConfigManager.lua     # Config management
│   ├── ModRegistry.lua       # Mod discovery
│   ├── StateManager.lua      # Save/load state
│   ├── CapabilityManager.lua # Manifest generation
│   └── lib/
│       ├── lua-apclientpp.dll  # Native client library
│       └── lunajson/           # JSON parsing
└── Mods/
    └── APTest/
        ├── ap_config.json    # Test mod capabilities
        └── main.lua          # (optional)
```

### Making Changes

**Local Development**:
- Work in the development workspace
- Test changes by copying files to game directory
- Verify in UE4SS log

**Committing Changes**:
- Update documentation if adding/changing features
- Test thoroughly before pushing
- Document any known issues or limitations

### Key Documentation Files

- `PROJECT_STATE.md` - Comprehensive project status (for all collaborators)
- `COLLABORATOR_NOTES.md` - This file - testing and quick reference
- `.claude/CONTINUOUS_POLLING_TEMPORARY_SOLUTION.md` - Technical details on polling solution
- `.claude/CONNECTION_CONFIG_REDESIGN.md` - Config architecture documentation
- `.claude/PHASE_4_IMPLEMENTATION_PLAN.md` - Phase 4 implementation details

---

## Common Tasks

### Update Connection Settings
Edit `APFramework/config.json`:
```json
"ap_connection": {
  "enabled": true,
  "server": "example.com",  ← Change this
  "port": 38281,            ← Or this
  "slot_name": "NewSlot",   ← Or this
  "password": "secret"      ← Or this
}
```

### Change Poll Interval
Edit `APFramework/config.json`:
```json
"framework": {
  "poll_interval_ms": 32  ← Slower polling (default: 16)
}
```

### Enable/Disable Auto-Connect
Edit `APFramework/config.json`:
```json
"framework": {
  "auto_connect": false  ← Disable automatic connection
}
```

### Add Debug Logging
Edit `APFramework/config.json`:
```json
"framework": {
  "debug_mode": true  ← Enable verbose logging
}
```

---

## Troubleshooting

### Game Freezes on Load
**Cause**: Blocking polling loop + mods after APFramework in load order
**Solution**:
- Ensure APFramework is last in `mods.txt` or load order
- Or disable other mods for testing

### "Lua state changed" Errors
**Cause**: Polling attempted from different thread/state
**Solution**: This should be fixed with current blocking loop implementation
- If you see this error, blocking loop may have exited
- Check for poll errors in log before this message

### Connection Timeout
**Cause**: Server not reachable or firewall blocking
**Solution**:
- Verify AP server is running
- Check `server` and `port` in config
- Test connection with text client first
- Check Windows Firewall settings

### Items Not Granted
**Cause**: Item granting not implemented yet
**Expected**: Item handler fires and logs item ID, but nothing happens in-game
**Status**: Planned for next phase

---

## Next Steps for Development

### High Priority
1. **Permanent Polling Solution**
   - Research UE4 game tick hooks in same Lua state
   - Test RegisterHook on PlayerController:Tick
   - Verify no "Lua state changed" errors
   - Ensure doesn't block other mods

2. **Item Granting**
   - Research Palworld UE4 inventory functions
   - Create item ID mapping (AP ↔ Palworld)
   - Implement item spawning in handler
   - Test with different item types

### Medium Priority
3. **Location Checking**
   - Research Palworld game event hooks
   - Start with simple events (chest opens)
   - Map game events to AP location IDs
   - Test with AP server

4. **State Persistence**
   - Extend StateManager for AP state
   - Save `last_received_index`
   - Save `checked_locations`
   - Test save/load across sessions

### Future Work
5. **Restore AP World Rules** (progression logic)
6. **Full Integration Testing** (multi-game multiworld)
7. **Polish & Release** (user-friendly errors, installation guide)

---

## Questions or Issues

- Check `PROJECT_STATE.md` for detailed technical information
- Check `.claude/` directory for implementation documentation
- Test with debug_mode enabled for verbose logging
- Share UE4SS log output when reporting issues

**Testing Focus**: Currently validating connection stability and item receiving. Item granting and location checking are next priorities.
