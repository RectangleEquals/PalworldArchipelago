# Palworld Archipelago Framework
A system which enables UE4SS mods to become the determining factor in what exactly is possible for games within an Archipelago Multiworld session. The goal is to allow specific mods to become "AP-enabled", without disturbing or drastically modifying an existing UE4SS ecosystem.

## Main System Components
#### APFrameworkCore (C++, aka "framework core", "framework lib" or "c++ framework")
The heart & orchestrator of the system. It's the middleware between APClientLib and the (Python) AP World server. Most of the code will reside here.

##### Framework Core Classes:
- `APManager`:
	- A global singleton which manages the lifecycle of all other components and the data sent between them
	- Also manages client mod registrations and messages
- `APClient`:
	- A wrapper around  the`apclientpp` library (not to confused with the `APClientLib` library or an "AP-enabled"/ "AP Client" mod), allowing communication and management between the framework core and the (Python) AP World server
- `APIPCServer`:
	- The IPC "mailbox" system for client mods to connect to and communicate with
- `APCapabilities`:
	- Manages the capabilities system, validation and aggregation of capabilities
	- For more information about capabilities, see the "Capabilities" section below
- `APConfig`:
	- Configuration manager handling global settings for the framework, such as `APClient` connection settings (`server`, `port`, `slot_name`, `password`, `auto_reconnect`, `registration_timeout`, etc), polling interval, logging settings, etc
- `APMessageRouter`:
	- Routes AP messages to appropriate mods based on item/location ownership.
- `APModRegistry`:
	- Promise-based AP Client mod discovery and registration tracking manager
- `APPollingThread`:
	- Background thread for continuous AP Server polling.
	- Polls at configurable intervals (default: 16ms ≈ 60fps)
	- Calls ap_client_->poll() continuously
	- Retrieves messages via ap_client_->get_messages()
	- Routes via message_router_->route_ap_message()
	- Non-blocking for main thread
- **NOTE** Feel free to add any other classes you feel would be relevant here!

#### APClientLib (C++, aka "client lib", "framework client lib" or "c++ client lib")
A much more lightweight library which acts as an IPC Client, allowing for client mods to communicate with the framework.
Should be able to both send/receive data to/from APFrameworkCore's IPC Server

##### Client Lib Classes:
- `APIPCClient`:
	- Connects to the framework core's `APIPCServer`
- **NOTE** Feel free to add any other classes you feel would be relevant here!

##### APFrameworkMod (Lua, aka "framework mod" or "Lua framework")
The entry point of the entire system. This mod should be configured to be loaded by UE4SS before any other "AP-enabled" mods (via modifying the load order within `<game_root>/<game_name>/Binaries/Win64/ue4ss/Mods/mods.json` and/or `<game_root>/<game_name>/Binaries/Win64/ue4ss/Mods/mods.txt`).

##### AP Client Mods (UE4SS Lua/C++/Blueprints, aka "client mod")
Outside of `APFrameworkMod` being a "dual-purpose" framework AND client mod itself, we encourage other devs to either create new AP Client mods, or to update their pre-existing "normal" UE4SS mods into this new AP-enabled format, if they wish for it to be included within the benefits of this system.

Each client mod will have the following config structure within their respective `AP_Config.json` files:
- `mod_id`: Unique identifier in the format of `author.game.mod_name` (ie: `archipelago.palworld.framework`)
	- **NOTE:** Any mod with `archipelago.<game_name>.*` will be recognized by the framework as a "Priority Client". These mods **DO NOT** contribute to the AP Capabilities, they only contribute code/functionality to the framework/UE4SS ecosystem itself and are granted special messaging privileges to/from the framework allowing them to gather extra information, execute specific framework code, etc
- `name`: A friendly human-readable mod name
- `version`: A semantic version (ie, `0.1.0`)
- `description`: A description about the mod
- `incompatible`: A list of other mods which this mod is incompatible with. If found during the framework's mod discovery, both client mods will be actively refused registration and both client mods should be informed of the reason
	- Examples:
	```json
	{
		"incompatible": [
			{ "id": "other.mod.id", "versions": "<=0.1.0" }, // Other mod still works with this mod, provided it's version is > 0.1.0
			{ "id": "someother.mod.id", "versions": ["0.0.1", "0.0.2", "0.1.2", ...] }, // Same as above, but with more specific versions
			{ "id": "incompatible.mod.id" } // This mod is entirely incompatible with this other mod. As a result, registration of both mods will be actively refused/cancelled, and both will be notified the reason as to why
		]
	}
	```
- `capabilities`: See the "Capabilities" section below

##### World (Python, aka "AP World" or "AP Server")
This needs to remain as dynamic and versatile as possible, taking in an `AP_Capabilities_<slot_name>.json` file generated by the framework (one for each slot, if multiple people in the multiworld are using the framework; ie, `AP_Capabilities_P1.json` and `AP_Capabilities_P2.json` for two separate player slots on separate PCs with possibly different sets of AP Client mods or possibly even the same sets from two separate PCs). These capabilities configs tell the world what is available for shuffling/randomization within the multiworld, and possibly the conditional rules for doing so.

## Capabilities
This is a major part of the design philosophy: A dynamic schema for AP Client Mods to follow which effectively tells the framework what a mod "promises" to do or handle, and/or what is actually available for the AP Server to shuffle or randomize. The framework should have a "zero tolerance" policy for conflicts between any two AP Client mods living within a single UE4SS ecosystem. Mod developers should be able to place their promised capabilities within the `capabilities` field of their own respective local a `ue4ss/Mods/<mod_name>/AP_Config.json` file (which is partially what helps a standard UE4SS mod become "AP-enabled"), which the framework should automatically find before UE4SS loads the mod so the framework can process whatever it needs to during initialization/validation/generation/resynchronization.

If all goes well, the result is a `ue4ss/Mods/APFrameworkMod/AP_Capabilities_<slot_name>.json` file, which can be given to whomever is hosting the multiworld in order for the AP World to consider during it's generation step. The main idea here can be summed up through this discord conversation:
```
I'll give you a couple of example scenarios:
- Chest shuffling might be possible offline (AP side) if we preemptively knew the locations of all overworld chests. But would require runtime support (Lua/C++ mod side) to dynamically know when which chest was opened, and force some other item to be obtained instead of the original item (depending on what the AP server says to do).
- Region locking would be possible offline (AP) if we have a list of known regions and we want to enforce some kind of linear or branching logic which states where a player is allowed to go before being able to go elsewhere. But would require a runtime mechanism (mod side) to create and destroy invisible walls or whatever based on AP checks.
- These are just hypothetical examples, not actual real-world scenarios.

If we keep an extensible system, we can just allow the community to create whatever AP/mod functionality they want to see, instead of us being the ones to predetermine everything.
We can offer some basic, default mods for people to use, and allow for those to be disabled or removed or replaced with some other set of features.
Afterall, if a mod can't do it, then it can't be randomized.
```

As to what exactly *CAN* go in here is currently difficult for me to ascertain, since it depends on what *could* be acceptable by Archipelago's standards (read their docs for details), *how* we code the AP World, and then how we code the APFrameworkCore to handle everything. It also *kinda* depends on the game itself being modded, and what UE4SS is capable of when modding said game. It's likely that not everything will be 100% predeterminable with this system. So when deciding on a schema design (and the code to handle it), please keep all of this in mind. Flexibility is key, but hopefully with some amount of determinism.

## Lifecycle & Flow
1.) Run game —> UE4SS loads our APFrameworkMod, registers Tick function via `RegisterCustomEvent("Tick", function(deltaTime) ... end)`

2.) APFrameworkCore checks config (at `<game_root>\\<game_name>\\Binaries\\Win64\\ue4ss\\Mods\\APFrameworkMod\\framework_config.json`), sets up logging (if enabled), sets up IPC Server, discovers mod configs & capabilities

3.) Framework waits for registration from any expected priority clients (AP Client mods with a `mod_id` of `archipelago.<game_name>.*` as defined within their own respective `ue4ss/Mods/<mod_name>/AP_Config.json` files, including the Lua framework mod itself), then validates other mods and awaits for their registration as well. **NOTE:** Once again, bear in mind that Priority Clients **DO NOT** contribute toward generation in any way, **DO NOT** have a `capabilities` field in their `AP_Config.json`, and only exist to extend functionality or utility to the framework's ecosystem, rather than being an actual "mod" in the sense of modding the actual game. They have access to additional framework info and command execution, allowing them to do special things which regular AP Client mods can't ordinarily do. An potential example of this would be a `archipelago.palworld.framework_ui` mod which is a Blueprint Logic Mod with a UMG UI showing various framework information and allowing the user to control certain aspects — such a mod would have no reason to contribute toward AP generation, but could have a button which tells the framework to generate, or some widgets which let the player manually connect to the AP World, or even just a debug UI for devs, just for example.

4.) Upon successful validation, the Framework generates `APCapabilities_<slot_name>.json` once all mods have registered to the framework. If there were any conflicts or errors, we instead tell the conflicting mods why their registration failed and we don't generate anything until everything has been manually resolved

5.) **For local hosting:** Open Archipelago —> Install our palworld.apworld (if not already installed) —> click Generate (this should take in our generated `APCapabilities_<slot_name>.json` file(s) so the apworld knows what can be randomized, generates a multiworld `AP_*.zip` file) —> click Host (and select our generated `AP_*.zip` multiworld)

6.) Restart game (or APFrameworkCore receives "CMD_RESYNC" request from a priority client, which broadcasts "RESYNC" lifecycle message to all non-priority AP Clients) —> Framework waits for all discovered mods to register again (as described in step #3) —> framework connects to AP Server -> framework caches data packages from AP World and synchronizes state with the world —> main loop begins (player can start playing the game)

7a.) AP server receives item from some other game —> AP server sends item to framework —> framework sees item, knows which mod to send it to —> mod enforces it's earlier "promise" by enforcing it's capability rule (ie, forcing a specific tech to be unlocked, etc) 

7b.) Mod finds location check —> mod sends notification to framework —> framework notifies server —> server notifies other game about sent item

7c.) Framework receives location scout —> routes notification to correct mod —> mod reacts & replies with relevant data

## Dependency Tree
- APFrameworkCore:
	- apclientpp (for AP World management/communication) — added as submodule to `third_party/apclientpp` via `https://github.com/black-sliver/apclientpp.git`
		- wswrap — added as submodule to `third_party/wswrap` via `https://github.com/black-sliver/wswrap.git`
		- asio (version 1.12 standalone) — automatically fetched by `CMakeLists.txt` into `third_party/asio`
			- example CMake code:
				```cmake
				include(FetchContent)
				
				# Define the Asio dependency
				FetchContent_Declare(
					asio_fetch
					GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
					GIT_TAG asio-1-12-2
					# This forces the download to land in your third_party folder
					SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio
				)
				
				# Download asio now (during the 'Configure' step)
				FetchContent_MakeAvailable(asio_fetch)
				```
		- websocketpp — added as submodule to `third_party/websocketpp` via `https://github.com/zaphoyd/websocketpp.git`
		- valijson — added as submodule to `third_party/valijson` via `https://github.com/tristanpenman/valijson.git`
	- nlohmann::json (for JSON encoding/decoding within the framework lib) — added as a header-only library directly to the project at `third_party/nlohmann` (ie, `third_party/nlohmann/json.hpp`)
	- sol2 (For Lua bindings) — added as a header-only library directly to the project at `third_party/sol2` (ie, `third_party/sol2/sol.hpp`)
	- lua-5.4.7 (Not sure if actually needed, but included just in case) — added as a static library directly to the project at `third_party/lua-5.4.7` (ie, `third_party/lua-5.4.7/CMakelists.txt`, `third_party/lua-5.4.7/src/*.h` and `third_party/lua-5.4.7/src/*.c`)

- APFrameworkMod:
	- `APFramework.lua` (A Lua wrapper for the APFrameworkCore lib) alongside `APFrameworkCore.dll`
	- `APClient.lua` (A Lua wrapper for the APClientLib lib) alongside `APClientLib.dll` — Allows the Lua framework (`APFrameworkMod/Scripts/main.lua`) to send/receive IPC Messages and register itself as a client mod
	- lunajson (For encoding/decoding JSON data from within Lua code) — added directly to the project at `third_party/lua/lunajson` (ie, `third_party/lua/lunajson.lua`, `third_party/lua/lunajson/decoder.lua`, `third_party/lua/lunajson/encoder.lua` and `third_party/lua/lunajson/sax.lua`) — **NOTE:** Should be automatically copied to `/APFrameworkMod/Scripts` during CMake builds, and manually added to any AP Client mod such as `ue4ss/Mods/<mod_name>/Scripts`

- AP Client Mods:
	- `APClient.lua` alongside `APClientLib.dll`
	- lunajson
	- A configured `AP_Config.json` file located at `ue4ss/Mods/<mod_name>/` so the framework can discover it