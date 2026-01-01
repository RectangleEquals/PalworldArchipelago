"""
Palworld Mod Capability Interface

This module handles loading and validating capability manifests from UE4SS mods.
It bridges the gap between the Lua runtime (APFramework) and the Python world
generation, allowing mods to dynamically contribute items, locations, and regions.
"""

import json
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field

# jsonschema is optional - only used for validation if available
try:
    import jsonschema
    HAS_JSONSCHEMA = True
except ImportError:
    HAS_JSONSCHEMA = False


@dataclass
class ModInfo:
    """Metadata about a mod"""
    id: str
    name: str
    version: str
    author: Optional[str] = None
    description: Optional[str] = None


@dataclass
class ItemCapability:
    """Defines an item that a mod provides"""
    name: str
    classification: str  # "progression", "useful", "filler", "trap"
    count: int = 1
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class LocationCapability:
    """Defines a location/check that a mod provides"""
    name: str
    region: str  # Which region this location belongs to
    needs_runtime: bool = False  # If True, populated at runtime (e.g., random chests)
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RegionConnection:
    """Defines a connection between two regions"""
    target: str  # Target region name
    rule: Optional[str] = None  # Logic rule (e.g., "has(Item)")


@dataclass
class RegionCapability:
    """Defines a region that a mod provides"""
    name: str
    connections: List[RegionConnection] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class GameplayOption:
    """Defines a configurable option for a mod"""
    name: str
    type: str  # "toggle", "choice", "range"
    default: Any
    display_name: str
    description: Optional[str] = None
    choices: Optional[List[str]] = None  # For choice type
    range_min: Optional[int] = None  # For range type
    range_max: Optional[int] = None


@dataclass
class ConflictDeclaration:
    """Declares incompatibility with another mod"""
    mod_id: str
    reason: str


@dataclass
class ModCapabilities:
    """All capabilities provided by a mod"""
    items: List[ItemCapability] = field(default_factory=list)
    locations: List[LocationCapability] = field(default_factory=list)
    regions: List[RegionCapability] = field(default_factory=list)
    options: List[GameplayOption] = field(default_factory=list)
    conflicts: List[ConflictDeclaration] = field(default_factory=list)


@dataclass
class ModDefinition:
    """Complete definition of a mod and its capabilities"""
    mod_info: ModInfo
    capabilities: ModCapabilities


@dataclass
class CapabilityManifest:
    """Complete manifest containing all mod capabilities"""
    version: str
    generated_at: str
    mods: List[ModDefinition] = field(default_factory=list)
    totals: Dict[str, int] = field(default_factory=dict)


class ModCapabilityManager:
    """
    Manages loading, validating, and merging mod capability manifests.
    """

    def __init__(self):
        self.schema: Optional[Dict] = None
        self._load_schema()

    def _load_schema(self) -> None:
        """Load the JSON schema for capability validation"""
        # Try to load schema from the APFramework schemas directory
        schema_paths = [
            Path(__file__).parent.parent.parent / "APFramework" / "Scripts" / "schemas" / "capability_schema.json",
            Path(__file__).parent / "schemas" / "capability_schema.json",
            Path("capability_schema.json"),
        ]

        for schema_path in schema_paths:
            if schema_path.exists():
                try:
                    with open(schema_path, 'r') as f:
                        self.schema = json.load(f)
                    return
                except Exception as e:
                    print(f"[ModInterface] Warning: Failed to load schema from {schema_path}: {e}")

        print("[ModInterface] Warning: No capability schema found, validation disabled")

    def load_manifest(self, manifest_path: str) -> Optional[CapabilityManifest]:
        """
        Load a capability manifest from a JSON file.

        Args:
            manifest_path: Path to APCapabilities.json

        Returns:
            CapabilityManifest object, or None if loading fails
        """
        path = Path(manifest_path)

        if not path.exists():
            raise FileNotFoundError(f"Manifest not found: {manifest_path}")

        try:
            with open(path, 'r') as f:
                data = json.load(f)

            # Validate against schema if available
            if self.schema and HAS_JSONSCHEMA:
                try:
                    jsonschema.validate(instance=data, schema=self.schema)
                except jsonschema.ValidationError as e:
                    print(f"[ModInterface] Validation error: {e.message}")
                    return None
            elif self.schema and not HAS_JSONSCHEMA:
                print("[ModInterface] Warning: jsonschema not available, skipping validation")

            # Parse into dataclass structure
            manifest = self._parse_manifest(data)
            return manifest

        except json.JSONDecodeError as e:
            print(f"[ModInterface] JSON parse error: {e}")
            return None
        except Exception as e:
            print(f"[ModInterface] Error loading manifest: {e}")
            return None

    def _parse_manifest(self, data: Dict) -> CapabilityManifest:
        """Parse JSON data into CapabilityManifest dataclass"""

        mods = []
        for mod_data in data.get("mods", []):
            # Parse mod info
            mod_info = ModInfo(
                id=mod_data["mod_info"]["id"],
                name=mod_data["mod_info"]["name"],
                version=mod_data["mod_info"]["version"],
                author=mod_data["mod_info"].get("author"),
                description=mod_data["mod_info"].get("description")
            )

            # Parse capabilities
            cap_data = mod_data.get("capabilities", {})

            items = [
                ItemCapability(
                    name=item["name"],
                    classification=item["classification"],
                    count=item.get("count", 1),
                    metadata=item.get("metadata", {})
                )
                for item in cap_data.get("items", [])
            ]

            locations = [
                LocationCapability(
                    name=loc["name"],
                    region=loc.get("region", "Menu"),
                    needs_runtime=loc.get("needs_runtime", False),
                    metadata=loc.get("metadata", {})
                )
                for loc in cap_data.get("locations", [])
            ]

            regions = [
                RegionCapability(
                    name=reg["name"],
                    connections=[
                        RegionConnection(
                            target=conn["target"],
                            rule=conn.get("rule")
                        )
                        for conn in reg.get("connections", [])
                    ],
                    metadata=reg.get("metadata", {})
                )
                for reg in cap_data.get("regions", [])
            ]

            options = [
                GameplayOption(
                    name=opt["name"],
                    type=opt["type"],
                    default=opt["default"],
                    display_name=opt["display_name"],
                    description=opt.get("description"),
                    choices=opt.get("choices"),
                    range_min=opt.get("range_min"),
                    range_max=opt.get("range_max")
                )
                for opt in cap_data.get("options", [])
            ]

            conflicts = [
                ConflictDeclaration(
                    mod_id=conf["mod_id"],
                    reason=conf["reason"]
                )
                for conf in cap_data.get("conflicts", [])
            ]

            capabilities = ModCapabilities(
                items=items,
                locations=locations,
                regions=regions,
                options=options,
                conflicts=conflicts
            )

            mods.append(ModDefinition(
                mod_info=mod_info,
                capabilities=capabilities
            ))

        manifest = CapabilityManifest(
            version=data.get("version", "1.0.0"),
            generated_at=data.get("generated_at", "unknown"),
            mods=mods,
            totals=data.get("totals", {})
        )

        return manifest

    def check_conflicts(self, manifest: CapabilityManifest) -> List[str]:
        """
        Check for conflicts between mods in the manifest.

        Returns:
            List of conflict descriptions (empty if no conflicts)
        """
        conflicts = []

        # Build set of loaded mod IDs
        loaded_mods = {mod.mod_info.id for mod in manifest.mods}

        # Check each mod's conflict declarations
        for mod in manifest.mods:
            for conflict in mod.capabilities.conflicts:
                if conflict.mod_id in loaded_mods:
                    conflicts.append(
                        f"{mod.mod_info.name} conflicts with {conflict.mod_id}: {conflict.reason}"
                    )

        # Check for duplicate item/location IDs (namespace collisions)
        item_names = {}
        location_names = {}

        for mod in manifest.mods:
            mod_id = mod.mod_info.id

            for item in mod.capabilities.items:
                full_name = f"{mod_id}:{item.name}"
                if full_name in item_names:
                    conflicts.append(
                        f"Duplicate item: {full_name} in both {item_names[full_name]} and {mod_id}"
                    )
                item_names[full_name] = mod_id

            for location in mod.capabilities.locations:
                full_name = f"{mod_id}:{location.name}"
                if full_name in location_names:
                    conflicts.append(
                        f"Duplicate location: {full_name} in both {location_names[full_name]} and {mod_id}"
                    )
                location_names[full_name] = mod_id

        return conflicts

    def get_mod_by_id(self, manifest: CapabilityManifest, mod_id: str) -> Optional[ModDefinition]:
        """Get a specific mod definition by its ID"""
        for mod in manifest.mods:
            if mod.mod_info.id == mod_id:
                return mod
        return None

    def get_statistics(self, manifest: CapabilityManifest) -> Dict[str, int]:
        """Get statistics about the manifest"""
        total_items = sum(len(mod.capabilities.items) for mod in manifest.mods)
        total_locations = sum(len(mod.capabilities.locations) for mod in manifest.mods)
        total_regions = sum(len(mod.capabilities.regions) for mod in manifest.mods)
        total_options = sum(len(mod.capabilities.options) for mod in manifest.mods)

        return {
            "mod_count": len(manifest.mods),
            "item_count": total_items,
            "location_count": total_locations,
            "region_count": total_regions,
            "option_count": total_options
        }
