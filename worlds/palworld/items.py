"""
Palworld Items

Defines base game items that can be randomized in Archipelago.
Mods can add additional items through the capability system.
"""

from typing import Dict, NamedTuple, Optional
from enum import IntEnum

from BaseClasses import Item, ItemClassification


class PalworldItem(Item):
    """Item class for Palworld"""
    game = "Palworld"


class ItemData(NamedTuple):
    """Data structure for item definitions"""
    id: int
    classification: ItemClassification
    count: int = 1  # Number of this item in the pool
    mod_id: Optional[str] = None
    metadata: Optional[Dict] = None


# Base game item definitions
# ID range: 8370000 - 8379999
item_table: Dict[str, ItemData] = {
    # Progression Items - Required to advance through the game
    "Pal Sphere": ItemData(
        id=8370001,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "capture", "tier": 1}
    ),
    "Mega Sphere": ItemData(
        id=8370002,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "capture", "tier": 2}
    ),
    "Giga Sphere": ItemData(
        id=8370003,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "capture", "tier": 3}
    ),
    "Legendary Sphere": ItemData(
        id=8370004,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "capture", "tier": 4}
    ),

    # Technology Points - Unlock crafting recipes
    "Technology Point": ItemData(
        id=8370010,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "technology"}
    ),
    "Ancient Technology Point": ItemData(
        id=8370011,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "technology", "special": True}
    ),

    # Tools and Equipment
    "Stone Pickaxe": ItemData(
        id=8370020,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 1}
    ),
    "Metal Pickaxe": ItemData(
        id=8370021,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 2}
    ),
    "Refined Metal Pickaxe": ItemData(
        id=8370022,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 3}
    ),

    "Stone Axe": ItemData(
        id=8370023,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 1}
    ),
    "Metal Axe": ItemData(
        id=8370024,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 2}
    ),
    "Refined Metal Axe": ItemData(
        id=8370025,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "tool", "tier": 3}
    ),

    # Weapons (reduced to fit 61 locations exactly)
    "Wooden Club": ItemData(
        id=8370030,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 1, "type": "melee"}
    ),
    "Metal Sword": ItemData(
        id=8370032,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 2, "type": "melee"}
    ),
    "Crossbow": ItemData(
        id=8370034,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 2, "type": "ranged"}
    ),
    "Handgun": ItemData(
        id=8370035,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 3, "type": "ranged"}
    ),
    "Assault Rifle": ItemData(
        id=8370036,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 4, "type": "ranged"}
    ),
    "Rocket Launcher": ItemData(
        id=8370037,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "weapon", "tier": 5, "type": "ranged"}
    ),

    # Armor
    "Cloth Outfit": ItemData(
        id=8370040,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "armor", "tier": 1}
    ),
    "Pelt Armor": ItemData(
        id=8370041,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "armor", "tier": 2}
    ),
    "Metal Armor": ItemData(
        id=8370042,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "armor", "tier": 3}
    ),
    "Refined Metal Armor": ItemData(
        id=8370043,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "armor", "tier": 4}
    ),
    "Pal Metal Armor": ItemData(
        id=8370044,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "armor", "tier": 5}
    ),

    # Base Building
    "Palbox": ItemData(
        id=8370050,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "building", "essential": True}
    ),
    "Campfire": ItemData(
        id=8370051,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "building"}
    ),
    "Crafting Bench": ItemData(
        id=8370052,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "building"}
    ),
    "Advanced Workbench": ItemData(
        id=8370053,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "building"}
    ),

    # Resources (Filler) - Will be auto-generated to fill remaining locations
    "Wood": ItemData(
        id=8370100,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "resource", "tier": 1}
    ),

    # Mounts and Mobility
    "Saddle": ItemData(
        id=8370120,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "mobility"}
    ),
    "Glider": ItemData(
        id=8370121,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "mobility"}
    ),
    "Grappling Gun": ItemData(
        id=8370122,
        classification=ItemClassification.filler,
        count=1,
        metadata={"category": "mobility"}
    ),

    # Special Items
    "Boss Key Fragment": ItemData(
        id=8370200,
        classification=ItemClassification.filler,
        count=5,
        metadata={"category": "key_item"}
    ),
    "Dungeon Key": ItemData(
        id=8370201,
        classification=ItemClassification.filler,
        count=3,
        metadata={"category": "key_item"}
    ),
}


# Item groups for logical organization
item_groups: Dict[str, set] = {
    "weapons": {
        "Wooden Club", "Spear", "Metal Sword",
        "Bow", "Crossbow", "Handgun", "Assault Rifle", "Rocket Launcher"
    },
    "armor": {
        "Cloth Outfit", "Pelt Armor", "Metal Armor",
        "Refined Metal Armor", "Pal Metal Armor"
    },
    "tools": {
        "Stone Pickaxe", "Metal Pickaxe", "Refined Metal Pickaxe",
        "Stone Axe", "Metal Axe", "Refined Metal Axe"
    },
    "pal_spheres": {
        "Pal Sphere", "Mega Sphere", "Giga Sphere", "Legendary Sphere"
    },
    "technology": {
        "Technology Point", "Ancient Technology Point"
    },
    "building": {
        "Palbox", "Campfire", "Crafting Bench", "Advanced Workbench"
    },
    "resources": {
        "Wood", "Stone", "Fiber", "Ore", "Coal", "Ingot", "Refined Ingot"
    },
    "mobility": {
        "Saddle", "Glider", "Grappling Gun"
    },
    "key_items": {
        "Boss Key Fragment", "Dungeon Key"
    },
}


def get_item_group(item_name: str) -> Optional[str]:
    """Get the group name for a given item"""
    for group_name, items in item_groups.items():
        if item_name in items:
            return group_name
    return None
