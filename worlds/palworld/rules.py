"""
Palworld Access Rules

Defines logic rules for when locations and regions become accessible.
This ensures items are obtained in a logical progression order.
"""

from typing import TYPE_CHECKING
from worlds.generic.Rules import set_rule, add_rule
from .regions import get_region_difficulty, get_required_items_for_region

if TYPE_CHECKING:
    from . import PalworldWorld


def set_rules(multiworld, player: int, options) -> None:
    """
    Set all access rules for the Palworld world.

    This function is called after regions and locations are created
    to define the logic that determines accessibility.
    """

    # Region access rules
    _set_region_rules(multiworld, player, options)

    # Location-specific rules
    _set_location_rules(multiworld, player, options)

    # Completion condition (goal)
    _set_completion_rule(multiworld, player, options)


def _set_region_rules(multiworld, player: int, options) -> None:
    """Set rules for accessing each region"""

    # For initial testing: ALL regions freely accessible
    # No entrance rules at all - everything can be reached
    # This allows us to test that the basic generation mechanics work

    pass  # No rules = all entrances accessible


def _set_location_rules(multiworld, player: int, options) -> None:
    """Set rules for individual locations within regions"""

    # For initial testing: ALL locations freely accessible within their regions
    # No location-specific rules
    # This allows us to test that the basic generation mechanics work

    pass  # No rules = all locations accessible once you can reach their region


def _set_completion_rule(multiworld, player: int, options) -> None:
    """
    Set the goal/completion condition for the world.

    This determines when the player has "beaten" Palworld in this seed.
    """

    # Ultra-simplified goal for testing: Always beatable
    # This allows us to test that the basic generation mechanics work
    print(f"[Palworld] Setting completion condition to always True for player {player}")
    multiworld.completion_condition[player] = lambda state: True
    print(f"[Palworld] Completion condition set: {multiworld.completion_condition[player]}")

    # TODO: Add proper goal checking once basic generation works
    # TODO: Add option-based goals (e.g., capture all pals, reach level 50, etc.)
    # This would read from options to determine alternate win conditions
    # TODO: Change to checking location accessibility once basic generation works
