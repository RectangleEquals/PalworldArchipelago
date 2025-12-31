"""
Palworld Archipelago World Implementation

This world supports dynamic capability discovery from UE4SS mods,
allowing the generation to adapt based on what mods the player has installed.
"""

from typing import Dict, List, Set, Any, Optional, ClassVar
from dataclasses import dataclass
import json

from worlds.AutoWorld import World, WebWorld
from worlds.generic.Rules import set_rule, add_rule
from BaseClasses import Region, Location, Item, ItemClassification, Tutorial
from .options import PalworldOptions
from .items import PalworldItem, item_table, ItemData
from .locations import PalworldLocation, location_table, LocationData
from .regions import create_regions
from .rules import set_rules
from .mod_interface import ModCapabilityManager, CapabilityManifest


class PalworldWebWorld(WebWorld):
    """Web configuration for Palworld"""
    
    theme = "jungle"
    
    tutorials = [
        Tutorial(
            "Multiworld Setup Guide",
            "A guide to setting up Palworld for Archipelago multiworld.",
            "English",
            "setup_en.md",
            "setup/en",
            ["RectangleEquals"]
        )
    ]
    
    rich_text_options_doc = True


class PalworldWorld(World):
    """
    Palworld is a monster-collecting survival game where players capture and train creatures
    called Pals. This Archipelago implementation supports dynamic randomization based on
    installed UE4SS mods, allowing for extensible community-driven content.
    """
    
    game = "Palworld"
    web = PalworldWebWorld()
    
    options_dataclass = PalworldOptions
    options: PalworldOptions
    
    # Base game data (always available)
    base_id = 8370000  # Unique ID range for Palworld
    
    item_name_to_id = {name: data.id for name, data in item_table.items()}
    location_name_to_id = {name: data.id for name, data in location_table.items()}
    
    # Item/location groups for hints and plando
    item_name_groups = {
        "weapons": {"Sword", "Bow", "Rifle", "Rocket Launcher"},
        "armor": {"Cloth Armor", "Metal Armor", "Pal Metal Armor"},
        "pal_spheres": {"Pal Sphere", "Mega Sphere", "Giga Sphere", "Legendary Sphere"},
        "technology": set(item_table.keys()) & {"Technology Point"},  # All tech unlocks
    }
    
    location_name_groups = {
        "bosses": {"Alpha Pal 1", "Alpha Pal 2", "Tower Boss 1"},
        "dungeons": {"Dungeon 1 Chest", "Dungeon 2 Chest"},
    }
    
    # Mod capability system
    capability_manager: ClassVar[ModCapabilityManager] = ModCapabilityManager()
    capability_manifest: Optional[CapabilityManifest] = None
    
    # Dynamic data populated from mods
    mod_items: Dict[str, ItemData] = {}
    mod_locations: Dict[str, LocationData] = {}
    
    # Generation state
    generated_checks: List[int] = []
    
    def __init__(self, multiworld, player):
        super().__init__(multiworld, player)

    def _load_capability_manifest(self) -> None:
        """Load mod capability manifest from player options or default location"""
        # Check if player provided a capability file (options are available after __init__)
        manifest_path = self.options.capability_manifest_path.value if hasattr(self, 'options') else ""
        
        if not manifest_path:
            # Try default location
            manifest_path = "APCapabilities.json"
        
        try:
            self.capability_manifest = self.capability_manager.load_manifest(manifest_path)
            
            if self.capability_manifest:
                self._merge_mod_capabilities()
                print(f"[Palworld] Loaded capabilities: "
                      f"{len(self.mod_locations)} locations, "
                      f"{len(self.mod_items)} items from "
                      f"{len(self.capability_manifest.mods)} mods")
        except FileNotFoundError:
            print(f"[Palworld] No capability manifest found at {manifest_path}, "
                  f"using base game only")
        except Exception as e:
            print(f"[Palworld] Error loading capability manifest: {e}")
    
    def _merge_mod_capabilities(self) -> None:
        """Merge mod-provided capabilities with base game data"""
        if not self.capability_manifest:
            return
        
        # Generate IDs for mod content
        next_item_id = self.base_id + 10000
        next_location_id = self.base_id + 20000
        
        for mod in self.capability_manifest.mods:
            mod_id = mod.mod_info.id
            
            # Merge items
            if mod.capabilities.items:
                for item in mod.capabilities.items:
                    full_name = f"{mod_id}:{item.name}"
                    
                    self.mod_items[full_name] = ItemData(
                        id=next_item_id,
                        classification=self._convert_item_classification(item.classification),
                        mod_id=mod_id,
                        metadata=item.metadata
                    )
                    
                    next_item_id += 1
            
            # Merge locations
            if mod.capabilities.locations:
                for location in mod.capabilities.locations:
                    full_name = f"{mod_id}:{location.name}"
                    
                    self.mod_locations[full_name] = LocationData(
                        id=next_location_id,
                        region=location.metadata.get("region", "Menu"),
                        mod_id=mod_id,
                        needs_runtime=location.needs_runtime,
                        metadata=location.metadata
                    )
                    
                    next_location_id += 1
        
        # Update name_to_id mappings
        self.item_name_to_id.update({
            name: data.id for name, data in self.mod_items.items()
        })
        
        self.location_name_to_id.update({
            name: data.id for name, data in self.mod_locations.items()
        })
    
    def _convert_item_classification(self, classification: str) -> ItemClassification:
        """Convert string classification to ItemClassification enum"""
        mapping = {
            "progression": ItemClassification.progression,
            "useful": ItemClassification.useful,
            "filler": ItemClassification.filler,
            "trap": ItemClassification.trap
        }
        return mapping.get(classification, ItemClassification.filler)
    
    def generate_early(self) -> None:
        """Called before any items or locations are created"""
        # Load capability manifest now that options are available
        self._load_capability_manifest()

        # Validate mod compatibility
        if self.capability_manifest:
            conflicts = self.capability_manager.check_conflicts(self.capability_manifest)
            if conflicts:
                raise Exception(f"Mod conflicts detected: {conflicts}")
    
    def create_regions(self) -> None:
        """Create all regions and locations"""
        # Create base game regions
        regions = create_regions(self.multiworld, self.player)
        
        # Add mod-defined regions if available
        if self.capability_manifest:
            mod_regions = self._create_mod_regions()
            regions.update(mod_regions)
        
        # Add locations to regions
        for region_name, region in regions.items():
            # Add base game locations
            base_locations = {
                loc_name: loc_data.id for loc_name, loc_data in location_table.items()
                if loc_data.region == region_name
            }
            if base_locations:
                region.add_locations(base_locations, PalworldLocation)

            # Add mod locations
            mod_locations = {
                loc_name: loc_data.id for loc_name, loc_data in self.mod_locations.items()
                if loc_data.region == region_name
            }
            if mod_locations:
                region.add_locations(mod_locations, PalworldLocation)
        
        # Add regions to multiworld
        self.multiworld.regions += regions.values()
    
    def _create_mod_regions(self) -> Dict[str, Region]:
        """Create regions defined by mods"""
        regions = {}
        
        if not self.capability_manifest:
            return regions
        
        for mod in self.capability_manifest.mods:
            if not mod.capabilities.regions:
                continue
            
            for region_def in mod.capabilities.regions:
                region_name = f"{mod.mod_info.id}:{region_def.name}"
                
                region = Region(region_name, self.player, self.multiworld)
                regions[region_name] = region
        
        return regions
    
    def create_items(self) -> None:
        """Create all items and add them to the pool"""
        # Create base game items
        item_pool = []
        
        for item_name, item_data in item_table.items():
            for _ in range(item_data.count):
                item_pool.append(self.create_item(item_name))
        
        # Create mod items
        for item_name, item_data in self.mod_items.items():
            for _ in range(item_data.count):
                item_pool.append(self.create_item(item_name))
        
        # Balance item pool with filler if needed
        total_locations = len(self.multiworld.get_unfilled_locations(self.player))
        if len(item_pool) < total_locations:
            filler_count = total_locations - len(item_pool)
            item_pool += [self.create_filler() for _ in range(filler_count)]
        
        self.multiworld.itempool += item_pool
    
    def create_item(self, name: str) -> PalworldItem:
        """Create an item by name"""
        # Check base game items
        if name in item_table:
            data = item_table[name]
            return PalworldItem(name, data.classification, data.id, self.player)
        
        # Check mod items
        if name in self.mod_items:
            data = self.mod_items[name]
            return PalworldItem(name, data.classification, data.id, self.player)
        
        # Default to filler
        return self.create_filler()
    
    def create_filler(self) -> PalworldItem:
        """Create a filler item"""
        # Use basic resource as filler
        return self.create_item("Wood")
    
    def set_rules(self) -> None:
        """Set access rules for locations and regions"""
        set_rules(self.multiworld, self.player, self.options)
        
        # Apply mod-defined rules if available
        if self.capability_manifest:
            self._apply_mod_rules()
    
    def _apply_mod_rules(self) -> None:
        """Apply access rules from mod capability definitions"""
        if not self.capability_manifest:
            return
        
        for mod in self.capability_manifest.mods:
            if not mod.capabilities.regions:
                continue
            
            for region_def in mod.capabilities.regions:
                if not region_def.connections:
                    continue
                
                source_region_name = f"{mod.mod_info.id}:{region_def.name}"
                source_region = self.multiworld.get_region(source_region_name, self.player)
                
                for connection in region_def.connections:
                    target_name = connection.target
                    
                    # Parse rule if provided
                    if connection.rule:
                        rule_func = self._parse_rule_string(connection.rule)
                        entrance = source_region.connect(
                            self.multiworld.get_region(target_name, self.player)
                        )
                        set_rule(entrance, rule_func)
    
    def _parse_rule_string(self, rule: str):
        """
        Parse a rule string into a callable function
        Format: "has(Item) and has(Item2, 2)"
        """
        def rule_func(state):
            # Simple parser for common patterns
            # In production, this would be more robust
            tokens = rule.replace("(", " ").replace(")", " ").split()
            
            if "has" in tokens:
                idx = tokens.index("has")
                item_name = tokens[idx + 1]
                
                # Check for count
                count = 1
                if len(tokens) > idx + 2 and tokens[idx + 2].isdigit():
                    count = int(tokens[idx + 2])
                
                return state.has(item_name, self.player, count)
            
            return True
        
        return rule_func
    
    def fill_slot_data(self) -> Dict[str, Any]:
        """Fill slot data to send to client"""
        slot_data = {
            "seed_name": self.multiworld.seed_name,
            "player_name": self.multiworld.player_name[self.player],
            "options": self.options.as_dict(),
        }
        
        # Include mod-specific data
        if self.capability_manifest:
            slot_data["mods"] = {}
            
            for mod in self.capability_manifest.mods:
                mod_id = mod.mod_info.id
                
                # Include randomization data for this mod
                mod_slot_data = {
                    "version": mod.mod_info.version,
                    "locations": [],
                    "items": []
                }
                
                # Add location assignments
                for loc_name, loc_data in self.mod_locations.items():
                    if loc_data.mod_id == mod_id:
                        location = self.multiworld.get_location(loc_name, self.player)
                        mod_slot_data["locations"].append({
                            "name": loc_name,
                            "id": loc_data.id,
                            "item": location.item.name if location.item else None
                        })
                
                # Add item data
                for item_name, item_data in self.mod_items.items():
                    if item_data.mod_id == mod_id:
                        mod_slot_data["items"].append({
                            "name": item_name,
                            "id": item_data.id,
                            "classification": item_data.classification.name
                        })
                
                slot_data["mods"][mod_id] = mod_slot_data
        
        return slot_data
    
    def generate_output(self, output_directory: str) -> None:
        """Generate output files for the player"""
        # Create .appalworld file with all necessary data
        output_data = {
            "metadata": {
                "seed": self.multiworld.seed_name,
                "player": self.multiworld.player_name[self.player],
                "slot": self.player
            },
            "slot_data": self.fill_slot_data()
        }
        
        output_filename = self.multiworld.get_out_file_name_base(self.player)
        output_path = f"{output_directory}/{output_filename}.appalworld"
        
        with open(output_path, 'w') as f:
            json.dump(output_data, f, indent=2)
