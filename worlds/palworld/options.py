"""
Palworld Player Options

Defines configurable options that players can set in their YAML files
to customize their Palworld Archipelago experience.
"""

from dataclasses import dataclass
from Options import (
    PerGameCommonOptions,
    Toggle,
    DefaultOnToggle,
    Choice,
    Range,
    OptionSet,
    DeathLink,
    FreeText
)


class GoalType(Choice):
    """What condition is required to complete the game?"""
    display_name = "Goal"
    option_both_towers = 0
    option_tower_free_pal = 1
    option_tower_eternal_pyre = 2
    option_capture_all_pals = 3
    option_player_level_50 = 4
    option_all_alpha_pals = 5
    default = 0


class StartingEquipment(Choice):
    """What equipment do you start with?"""
    display_name = "Starting Equipment"
    option_nothing = 0
    option_basic_tools = 1
    option_basic_tools_and_spheres = 2
    default = 1


class TechnologyPointProgression(Choice):
    """How are technology points obtained?"""
    display_name = "Technology Point Progression"
    option_vanilla = 0  # Earned through gameplay
    option_shuffled = 1  # Randomized in the item pool
    option_progressive = 2  # Progressive item unlocks tiers
    default = 1


class RandomizePalCaptures(Toggle):
    """Randomize which Pals can be captured at different locations?"""
    display_name = "Randomize Pal Captures"


class IncludeChests(DefaultOnToggle):
    """Include treasure chests as check locations?"""
    display_name = "Include Treasure Chests"


class IncludeFastTravelPoints(Toggle):
    """Include fast travel points as check locations?"""
    display_name = "Include Fast Travel Points"


class IncludeAlphaPals(DefaultOnToggle):
    """Include Alpha Pal defeats as check locations?"""
    display_name = "Include Alpha Pals"


class IncludeTowerBosses(DefaultOnToggle):
    """Include Tower boss battles as check locations?"""
    display_name = "Include Tower Bosses"


class IncludeDungeons(DefaultOnToggle):
    """Include dungeon completions as check locations?"""
    display_name = "Include Dungeons"


class IncludeTechnologyUnlocks(DefaultOnToggle):
    """Include technology tier unlocks as check locations?"""
    display_name = "Include Technology Unlocks"


class IncludePalMilestones(Toggle):
    """Include Pal capture milestones as check locations?"""
    display_name = "Include Pal Capture Milestones"


class IncludeBaseMilestones(Toggle):
    """Include base level milestones as check locations?"""
    display_name = "Include Base Level Milestones"


class DifficultyAdjustment(Range):
    """
    Adjust the overall difficulty.
    Higher values make combat harder and resources scarcer.
    """
    display_name = "Difficulty Adjustment"
    range_start = 1
    range_end = 10
    default = 5


class StartingRegionAccess(Choice):
    """Which regions are accessible at the start?"""
    display_name = "Starting Region Access"
    option_tutorial_only = 0
    option_tutorial_and_meadows = 1
    option_all_regions = 2
    default = 1


class PalSphereQuantity(Range):
    """How many Pal Spheres of each tier are in the item pool?"""
    display_name = "Pal Sphere Quantity"
    range_start = 1
    range_end = 50
    default = 10


class BossKeyFragmentCount(Range):
    """How many Boss Key Fragments are required to enter towers?"""
    display_name = "Boss Key Fragment Requirement"
    range_start = 0
    range_end = 10
    default = 5


class DungeonKeyCount(Range):
    """How many Dungeon Keys are in the item pool?"""
    display_name = "Dungeon Key Count"
    range_start = 1
    range_end = 10
    default = 3


class ProgressiveWeapons(DefaultOnToggle):
    """Weapons unlock progressively rather than all at once?"""
    display_name = "Progressive Weapons"


class ProgressiveArmor(DefaultOnToggle):
    """Armor unlocks progressively rather than all at once?"""
    display_name = "Progressive Armor"


class ProgressiveTools(DefaultOnToggle):
    """Tools unlock progressively rather than all at once?"""
    display_name = "Progressive Tools"


class EnableMods(DefaultOnToggle):
    """Enable mod capability system for additional content?"""
    display_name = "Enable Mod Support"


class CapabilityManifestPath(FreeText):
    """Path to APCapabilities.json file (leave empty for default)"""
    display_name = "Capability Manifest Path"
    default = ""


class DeathLinkEnabled(DeathLink):
    """When you die, everyone dies. When others die, you die."""
    display_name = "Death Link"


@dataclass
class PalworldOptions(PerGameCommonOptions):
    """All options for Palworld"""

    # Goal Options
    goal: GoalType

    # Progression Options
    starting_equipment: StartingEquipment
    starting_region_access: StartingRegionAccess
    technology_point_progression: TechnologyPointProgression
    randomize_pal_captures: RandomizePalCaptures

    # Location Inclusion Options
    include_chests: IncludeChests
    include_fast_travel_points: IncludeFastTravelPoints
    include_alpha_pals: IncludeAlphaPals
    include_tower_bosses: IncludeTowerBosses
    include_dungeons: IncludeDungeons
    include_technology_unlocks: IncludeTechnologyUnlocks
    include_pal_milestones: IncludePalMilestones
    include_base_milestones: IncludeBaseMilestones

    # Item Pool Options
    pal_sphere_quantity: PalSphereQuantity
    boss_key_fragment_count: BossKeyFragmentCount
    dungeon_key_count: DungeonKeyCount

    # Progressive Items
    progressive_weapons: ProgressiveWeapons
    progressive_armor: ProgressiveArmor
    progressive_tools: ProgressiveTools

    # Difficulty
    difficulty_adjustment: DifficultyAdjustment

    # Mod Support
    enable_mods: EnableMods
    capability_manifest_path: CapabilityManifestPath

    # Multiplayer
    death_link: DeathLinkEnabled
