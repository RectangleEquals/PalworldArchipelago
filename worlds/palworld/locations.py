"""
Palworld Locations

Defines base game locations (checks) that can contain randomized items.
Mods can add additional locations through the capability system.
"""

from typing import Dict, NamedTuple, Optional

from BaseClasses import Location


class PalworldLocation(Location):
    """Location class for Palworld"""
    game = "Palworld"


class LocationData(NamedTuple):
    """Data structure for location definitions"""
    id: int
    region: str
    mod_id: Optional[str] = None
    needs_runtime: bool = False  # If True, location is runtime-generated (e.g., random chests)
    metadata: Optional[Dict] = None


# Base game location definitions
# ID range: 8370000 - 8379999 (offset by 20000 from items)
location_table: Dict[str, LocationData] = {
    # Tutorial/Starting Area
    "Tutorial Complete": LocationData(
        id=8370001,
        region="Tutorial Island",
        metadata={"category": "tutorial"}
    ),
    "First Pal Captured": LocationData(
        id=8370002,
        region="Tutorial Island",
        metadata={"category": "tutorial", "progression": True}
    ),
    "First Base Built": LocationData(
        id=8370003,
        region="Tutorial Island",
        metadata={"category": "tutorial", "progression": True}
    ),

    # Starting Meadows Region
    "Meadows Alpha Pal Defeated": LocationData(
        id=8370010,
        region="Starting Meadows",
        metadata={"category": "boss", "difficulty": 1}
    ),
    "Meadows Treasure Chest 1": LocationData(
        id=8370011,
        region="Starting Meadows",
        metadata={"category": "chest"}
    ),
    "Meadows Treasure Chest 2": LocationData(
        id=8370012,
        region="Starting Meadows",
        metadata={"category": "chest"}
    ),
    "Meadows Treasure Chest 3": LocationData(
        id=8370013,
        region="Starting Meadows",
        metadata={"category": "chest"}
    ),
    "Meadows Fast Travel Point": LocationData(
        id=8370014,
        region="Starting Meadows",
        metadata={"category": "fast_travel"}
    ),

    # Forest Region
    "Forest Alpha Pal Defeated": LocationData(
        id=8370020,
        region="Deep Forest",
        metadata={"category": "boss", "difficulty": 2}
    ),
    "Forest Treasure Chest 1": LocationData(
        id=8370021,
        region="Deep Forest",
        metadata={"category": "chest"}
    ),
    "Forest Treasure Chest 2": LocationData(
        id=8370022,
        region="Deep Forest",
        metadata={"category": "chest"}
    ),
    "Forest Treasure Chest 3": LocationData(
        id=8370023,
        region="Deep Forest",
        metadata={"category": "chest"}
    ),
    "Forest Treasure Chest 4": LocationData(
        id=8370024,
        region="Deep Forest",
        metadata={"category": "chest"}
    ),
    "Forest Fast Travel Point": LocationData(
        id=8370025,
        region="Deep Forest",
        metadata={"category": "fast_travel"}
    ),
    "Forest Dungeon Clear": LocationData(
        id=8370026,
        region="Deep Forest",
        metadata={"category": "dungeon", "difficulty": 2}
    ),

    # Desert Region
    "Desert Alpha Pal Defeated": LocationData(
        id=8370030,
        region="Scorching Desert",
        metadata={"category": "boss", "difficulty": 3}
    ),
    "Desert Treasure Chest 1": LocationData(
        id=8370031,
        region="Scorching Desert",
        metadata={"category": "chest"}
    ),
    "Desert Treasure Chest 2": LocationData(
        id=8370032,
        region="Scorching Desert",
        metadata={"category": "chest"}
    ),
    "Desert Treasure Chest 3": LocationData(
        id=8370033,
        region="Scorching Desert",
        metadata={"category": "chest"}
    ),
    "Desert Fast Travel Point": LocationData(
        id=8370034,
        region="Scorching Desert",
        metadata={"category": "fast_travel"}
    ),
    "Desert Ruins Clear": LocationData(
        id=8370035,
        region="Scorching Desert",
        metadata={"category": "dungeon", "difficulty": 3}
    ),

    # Mountain Region
    "Mountain Alpha Pal Defeated": LocationData(
        id=8370040,
        region="Frozen Mountains",
        metadata={"category": "boss", "difficulty": 4}
    ),
    "Mountain Treasure Chest 1": LocationData(
        id=8370041,
        region="Frozen Mountains",
        metadata={"category": "chest"}
    ),
    "Mountain Treasure Chest 2": LocationData(
        id=8370042,
        region="Frozen Mountains",
        metadata={"category": "chest"}
    ),
    "Mountain Treasure Chest 3": LocationData(
        id=8370043,
        region="Frozen Mountains",
        metadata={"category": "chest"}
    ),
    "Mountain Treasure Chest 4": LocationData(
        id=8370044,
        region="Frozen Mountains",
        metadata={"category": "chest"}
    ),
    "Mountain Fast Travel Point": LocationData(
        id=8370045,
        region="Frozen Mountains",
        metadata={"category": "fast_travel"}
    ),
    "Mountain Peak Dungeon Clear": LocationData(
        id=8370046,
        region="Frozen Mountains",
        metadata={"category": "dungeon", "difficulty": 4}
    ),

    # Volcanic Region
    "Volcano Alpha Pal Defeated": LocationData(
        id=8370050,
        region="Volcanic Wasteland",
        metadata={"category": "boss", "difficulty": 5}
    ),
    "Volcano Treasure Chest 1": LocationData(
        id=8370051,
        region="Volcanic Wasteland",
        metadata={"category": "chest"}
    ),
    "Volcano Treasure Chest 2": LocationData(
        id=8370052,
        region="Volcanic Wasteland",
        metadata={"category": "chest"}
    ),
    "Volcano Treasure Chest 3": LocationData(
        id=8370053,
        region="Volcanic Wasteland",
        metadata={"category": "chest"}
    ),
    "Volcano Fast Travel Point": LocationData(
        id=8370054,
        region="Volcanic Wasteland",
        metadata={"category": "fast_travel"}
    ),
    "Volcano Core Dungeon Clear": LocationData(
        id=8370055,
        region="Volcanic Wasteland",
        metadata={"category": "dungeon", "difficulty": 5}
    ),

    # Tower Bosses
    "Tower of the Free Pal Alliance - Floor 1": LocationData(
        id=8370060,
        region="Free Pal Alliance Tower",
        metadata={"category": "tower_boss", "difficulty": 3, "floor": 1}
    ),
    "Tower of the Free Pal Alliance - Floor 2": LocationData(
        id=8370061,
        region="Free Pal Alliance Tower",
        metadata={"category": "tower_boss", "difficulty": 4, "floor": 2}
    ),
    "Tower of the Free Pal Alliance - Floor 3": LocationData(
        id=8370062,
        region="Free Pal Alliance Tower",
        metadata={"category": "tower_boss", "difficulty": 5, "floor": 3}
    ),
    "Tower of the Free Pal Alliance - Top Floor": LocationData(
        id=8370063,
        region="Free Pal Alliance Tower",
        metadata={"category": "tower_boss", "difficulty": 6, "floor": 4, "final": True}
    ),

    "Tower of the Brothers of the Eternal Pyre - Floor 1": LocationData(
        id=8370064,
        region="Eternal Pyre Tower",
        metadata={"category": "tower_boss", "difficulty": 3, "floor": 1}
    ),
    "Tower of the Brothers of the Eternal Pyre - Floor 2": LocationData(
        id=8370065,
        region="Eternal Pyre Tower",
        metadata={"category": "tower_boss", "difficulty": 4, "floor": 2}
    ),
    "Tower of the Brothers of the Eternal Pyre - Floor 3": LocationData(
        id=8370066,
        region="Eternal Pyre Tower",
        metadata={"category": "tower_boss", "difficulty": 5, "floor": 3}
    ),
    "Tower of the Brothers of the Eternal Pyre - Top Floor": LocationData(
        id=8370067,
        region="Eternal Pyre Tower",
        metadata={"category": "tower_boss", "difficulty": 6, "floor": 4, "final": True}
    ),

    # Technology Unlocks
    "Unlock Tier 2 Technology": LocationData(
        id=8370070,
        region="Menu",
        metadata={"category": "technology", "tier": 2}
    ),
    "Unlock Tier 3 Technology": LocationData(
        id=8370071,
        region="Menu",
        metadata={"category": "technology", "tier": 3}
    ),
    "Unlock Tier 4 Technology": LocationData(
        id=8370072,
        region="Menu",
        metadata={"category": "technology", "tier": 4}
    ),
    "Unlock Tier 5 Technology": LocationData(
        id=8370073,
        region="Menu",
        metadata={"category": "technology", "tier": 5}
    ),
    "Unlock Ancient Technology": LocationData(
        id=8370074,
        region="Menu",
        metadata={"category": "technology", "ancient": True}
    ),

    # Pal Captures
    "Capture 10 Different Pal Species": LocationData(
        id=8370080,
        region="Menu",
        metadata={"category": "collection", "count": 10}
    ),
    "Capture 25 Different Pal Species": LocationData(
        id=8370081,
        region="Menu",
        metadata={"category": "collection", "count": 25}
    ),
    "Capture 50 Different Pal Species": LocationData(
        id=8370082,
        region="Menu",
        metadata={"category": "collection", "count": 50}
    ),
    "Capture 75 Different Pal Species": LocationData(
        id=8370083,
        region="Menu",
        metadata={"category": "collection", "count": 75}
    ),
    "Capture 100 Different Pal Species": LocationData(
        id=8370084,
        region="Menu",
        metadata={"category": "collection", "count": 100}
    ),

    # Base Development
    "Base Level 5": LocationData(
        id=8370090,
        region="Menu",
        metadata={"category": "base", "level": 5}
    ),
    "Base Level 10": LocationData(
        id=8370091,
        region="Menu",
        metadata={"category": "base", "level": 10}
    ),
    "Base Level 15": LocationData(
        id=8370092,
        region="Menu",
        metadata={"category": "base", "level": 15}
    ),
    "Base Level 20": LocationData(
        id=8370093,
        region="Menu",
        metadata={"category": "base", "level": 20}
    ),

    # Special Achievements
    "Defeat First Alpha Pal": LocationData(
        id=8370100,
        region="Menu",
        metadata={"category": "achievement", "milestone": True}
    ),
    "Reach Player Level 20": LocationData(
        id=8370101,
        region="Menu",
        metadata={"category": "achievement", "level": 20}
    ),
    "Reach Player Level 35": LocationData(
        id=8370102,
        region="Menu",
        metadata={"category": "achievement", "level": 35}
    ),
    "Reach Player Level 50": LocationData(
        id=8370103,
        region="Menu",
        metadata={"category": "achievement", "level": 50}
    ),
    "Craft Legendary Sphere": LocationData(
        id=8370104,
        region="Menu",
        metadata={"category": "achievement", "crafting": True}
    ),
}


# Location groups for logical organization
location_groups: Dict[str, set] = {
    "bosses": {
        "Meadows Alpha Pal Defeated",
        "Forest Alpha Pal Defeated",
        "Desert Alpha Pal Defeated",
        "Mountain Alpha Pal Defeated",
        "Volcano Alpha Pal Defeated",
    },
    "tower_bosses": {
        "Tower of the Free Pal Alliance - Floor 1",
        "Tower of the Free Pal Alliance - Floor 2",
        "Tower of the Free Pal Alliance - Floor 3",
        "Tower of the Free Pal Alliance - Top Floor",
        "Tower of the Brothers of the Eternal Pyre - Floor 1",
        "Tower of the Brothers of the Eternal Pyre - Floor 2",
        "Tower of the Brothers of the Eternal Pyre - Floor 3",
        "Tower of the Brothers of the Eternal Pyre - Top Floor",
    },
    "dungeons": {
        "Forest Dungeon Clear",
        "Desert Ruins Clear",
        "Mountain Peak Dungeon Clear",
        "Volcano Core Dungeon Clear",
    },
    "chests": {
        loc for loc in location_table.keys() if "Treasure Chest" in loc
    },
    "fast_travel": {
        loc for loc in location_table.keys() if "Fast Travel Point" in loc
    },
    "technology": {
        "Unlock Tier 2 Technology",
        "Unlock Tier 3 Technology",
        "Unlock Tier 4 Technology",
        "Unlock Tier 5 Technology",
        "Unlock Ancient Technology",
    },
    "collection": {
        "Capture 10 Different Pal Species",
        "Capture 25 Different Pal Species",
        "Capture 50 Different Pal Species",
        "Capture 75 Different Pal Species",
        "Capture 100 Different Pal Species",
    },
}


def get_location_group(location_name: str) -> Optional[str]:
    """Get the group name for a given location"""
    for group_name, locations in location_groups.items():
        if location_name in locations:
            return group_name
    return None
