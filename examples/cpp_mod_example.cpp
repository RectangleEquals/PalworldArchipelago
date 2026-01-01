// Example C++ Mod Using APClientLib
// This demonstrates how to use APClientLib.dll in a C++ UE4SS mod (pseudo-code, not full UE4SS syntax)

#include <ap_client_lib.h>
#include <iostream>
#include <unordered_set>

// Global client handle
static APClientHandle g_client = nullptr;
static std::unordered_set<int64_t> items_given;

// Callback: Item received from Archipelago
void on_item_received(int64_t item_id, int64_t location_id, int player_slot, void* user_data) {
    std::cout << "[MyMod] Received item: " << item_id
              << " from location: " << location_id
              << " (player slot: " << player_slot << ")\n";

    // Example: Give specific item to player (only once)
    if (item_id == 100000 && items_given.find(item_id) == items_given.end()) {
        std::cout << "[MyMod] Giving Super Pickaxe to player!\n";

        // TODO: Game-specific code to spawn item in player inventory
        // Example (pseudo-code):
        // Player->AddItemToInventory("SuperPickaxe", 1);

        items_given.insert(item_id);
    }
}

// Callback: Location checked confirmation
void on_location_checked(int64_t location_id, void* user_data) {
    std::cout << "[MyMod] Location checked confirmed: " << location_id << "\n";
}

// Callback: Connection status changed
void on_connection_status(bool connected, const char* slot_name, void* user_data) {
    if (connected) {
        std::cout << "[MyMod] Connected to Archipelago as: " << slot_name << "\n";
    } else {
        std::cout << "[MyMod] Disconnected from Archipelago\n";
    }
}

// Callback: All mods registered
void on_registration_complete(void* user_data) {
    std::cout << "[MyMod] All mods registered! Framework is ready.\n";

    // Optional: Auto-request connection
    // ap_client_request_connection(g_client, "localhost", 38281, "Player1", "");
}

// UE4SS Mod Entry Point
extern "C" __declspec(dllexport) void InitializeMod() {
    std::cout << "[MyMod] Initializing APClient...\n";

    // Create client instance
    g_client = ap_client_create("MyModID");
    if (!g_client) {
        std::cout << "[MyMod] ERROR: Failed to create APClient!\n";
        return;
    }

    // Set up callbacks
    ap_client_set_item_received_callback(g_client, on_item_received, nullptr);
    ap_client_set_location_checked_callback(g_client, on_location_checked, nullptr);
    ap_client_set_connection_status_callback(g_client, on_connection_status, nullptr);
    ap_client_set_registration_complete_callback(g_client, on_registration_complete, nullptr);

    // Register mod capabilities with framework
    const char* capabilities = R"({
        "items": [
            {"id": 100000, "name": "Super Pickaxe", "classification": "useful"},
            {"id": 100001, "name": "Mega Sword", "classification": "progression"}
        ],
        "locations": [
            {"id": 200000, "name": "Chest in Cave", "region": "Starting Area"},
            {"id": 200001, "name": "Boss Chest", "region": "Boss Arena"}
        ],
        "regions": [
            {"name": "Starting Area", "connects_to": ["Boss Arena"]},
            {"name": "Boss Arena", "connects_to": []}
        ]
    })";

    if (!ap_client_register(g_client, capabilities)) {
        std::cout << "[MyMod] ERROR: Failed to register with framework!\n";
        std::cout << "[MyMod] Error: " << ap_client_get_last_error(g_client) << "\n";
        return;
    }

    std::cout << "[MyMod] Registered with APFramework successfully!\n";
}

// Called every game tick (or frequently)
extern "C" __declspec(dllexport) void OnGameTick() {
    if (g_client) {
        // Poll for messages from framework
        ap_client_poll(g_client);
    }
}

// Example: Check a location when player opens a specific chest
extern "C" __declspec(dllexport) void OnChestOpened(int chest_id) {
    if (!g_client) {
        return;
    }

    // Map game chest IDs to AP location IDs
    if (chest_id == 42) {  // Example: Chest #42 in game
        std::cout << "[MyMod] Checking location: Chest in Cave\n";
        ap_client_check_location(g_client, 200000);
    } else if (chest_id == 99) {  // Boss chest
        std::cout << "[MyMod] Checking location: Boss Chest\n";
        ap_client_check_location(g_client, 200001);
    }
}

// UE4SS Mod Shutdown
extern "C" __declspec(dllexport) void UninitializeMod() {
    if (g_client) {
        std::cout << "[MyMod] Shutting down APClient...\n";
        ap_client_destroy(g_client);
        g_client = nullptr;
    }
}
