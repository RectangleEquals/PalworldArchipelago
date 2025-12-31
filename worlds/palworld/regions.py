"""
Palworld Regions

Defines the world structure and connections between areas.
Regions group locations together and define progression through the game.
"""

from typing import Dict, List
from BaseClasses import Region, Entrance


def create_regions(multiworld, player: int) -> Dict[str, Region]:
    """
    Create all base game regions and their connections.

    Returns a dictionary mapping region names to Region objects.
    """
    regions = {}

    # Menu region - Always accessible, contains meta-achievements
    menu = Region("Menu", player, multiworld)
    regions["Menu"] = menu

    # Tutorial Island - Starting area
    tutorial = Region("Tutorial Island", player, multiworld)
    regions["Tutorial Island"] = tutorial

    # Starting Meadows - First main area
    meadows = Region("Starting Meadows", player, multiworld)
    regions["Starting Meadows"] = meadows

    # Deep Forest - Requires basic progression
    forest = Region("Deep Forest", player, multiworld)
    regions["Deep Forest"] = forest

    # Scorching Desert - Mid-game area
    desert = Region("Scorching Desert", player, multiworld)
    regions["Scorching Desert"] = desert

    # Frozen Mountains - High-level area
    mountains = Region("Frozen Mountains", player, multiworld)
    regions["Frozen Mountains"] = mountains

    # Volcanic Wasteland - Late-game area
    volcano = Region("Volcanic Wasteland", player, multiworld)
    regions["Volcanic Wasteland"] = volcano

    # Towers - Special boss areas
    tower_free_pal = Region("Free Pal Alliance Tower", player, multiworld)
    regions["Free Pal Alliance Tower"] = tower_free_pal

    tower_eternal_pyre = Region("Eternal Pyre Tower", player, multiworld)
    regions["Eternal Pyre Tower"] = tower_eternal_pyre

    # Create connections between regions
    # Menu is the origin and connects to all regions for testing
    menu.connect(tutorial, "Start Game")
    menu.connect(meadows, "To Starting Meadows")
    menu.connect(forest, "To Deep Forest")
    menu.connect(desert, "To Scorching Desert")
    menu.connect(mountains, "To Frozen Mountains")
    menu.connect(volcano, "To Volcanic Wasteland")
    menu.connect(tower_free_pal, "To Free Pal Alliance Tower")
    menu.connect(tower_eternal_pyre, "To Eternal Pyre Tower")

    # All regions connect back to menu
    tutorial.connect(menu, "Return to Menu")
    meadows.connect(menu, "Return to Menu")
    forest.connect(menu, "Return to Menu")
    desert.connect(menu, "Return to Menu")
    mountains.connect(menu, "Return to Menu")
    volcano.connect(menu, "Return to Menu")
    tower_free_pal.connect(menu, "Return to Menu")
    tower_eternal_pyre.connect(menu, "Return to Menu")

    return regions


def get_region_difficulty(region_name: str) -> int:
    """
    Get the difficulty level of a region (1-6).
    Used by rules.py to determine access requirements.
    """
    difficulty_map = {
        "Menu": 0,
        "Tutorial Island": 0,
        "Starting Meadows": 1,
        "Deep Forest": 2,
        "Scorching Desert": 3,
        "Frozen Mountains": 4,
        "Volcanic Wasteland": 5,
        "Free Pal Alliance Tower": 3,  # Accessible mid-game
        "Eternal Pyre Tower": 5,       # Late-game tower
    }

    return difficulty_map.get(region_name, 1)


def get_region_description(region_name: str) -> str:
    """Get a description of the region for documentation/UI"""
    descriptions = {
        "Menu": "Meta-achievements and progression tracking",
        "Tutorial Island": "The starting area where you learn the basics",
        "Starting Meadows": "A peaceful grassland perfect for beginners",
        "Deep Forest": "Dense woodlands with stronger Pals and resources",
        "Scorching Desert": "A harsh environment requiring heat resistance",
        "Frozen Mountains": "Treacherous peaks requiring cold resistance",
        "Volcanic Wasteland": "The most dangerous region with rare Pals",
        "Free Pal Alliance Tower": "Challenge tower with progressive boss battles",
        "Eternal Pyre Tower": "Elite challenge tower for experienced players",
    }

    return descriptions.get(region_name, "Unknown region")


def get_required_items_for_region(region_name: str) -> List[str]:
    """
    Get a list of items that are typically required to access a region.
    This is used by rules.py to set access rules.
    """
    requirements = {
        "Deep Forest": ["Stone Axe", "Pal Sphere"],
        "Scorching Desert": ["Metal Pickaxe", "Pelt Armor"],
        "Frozen Mountains": ["Metal Armor", "Giga Sphere"],
        "Volcanic Wasteland": ["Refined Metal Armor", "Legendary Sphere"],
        "Free Pal Alliance Tower": ["Metal Sword", "Crossbow"],
        "Eternal Pyre Tower": ["Pal Metal Armor", "Assault Rifle"],
    }

    return requirements.get(region_name, [])
