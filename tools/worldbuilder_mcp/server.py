"""Dependency-free MCP stdio server for Command & Conquer WorldBuilder."""

from __future__ import annotations

import argparse
import base64
import json
import math
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, BinaryIO, Mapping, TextIO

from .bridge import BridgeError, WorldBuilderBridge, format_height_points
from .features import (
    EXTRA_ALLOWED,
    EXTRA_COMMANDS,
    EXTRA_REQUIRED,
    EXTRA_TOOL_DEFINITIONS,
    validate_and_flatten,
)


SERVER_NAME = "generals-worldbuilder"
SERVER_VERSION = "0.6.0"
DEFAULT_PROTOCOL_VERSION = "2024-11-05"
SUPPORTED_PROTOCOL_VERSIONS = (
    "2025-06-18",
    "2025-03-26",
    "2024-11-05",
)
EDITORS_RESOURCE_URI = "worldbuilder://editors"
MAPS_RESOURCE_URI = "worldbuilder://user-maps"
SCRIPTING_GUIDE_RESOURCE_URI = "worldbuilder://scripting-guide"
BULK_OBJECT_CHUNK_SIZE = 15
BULK_OBJECT_CHUNK_DELAY_SECONDS = 0.03

SCRIPT_PARAMETER_TYPE_NAMES = (
    "INT", "REAL", "SCRIPT", "TEAM", "COUNTER", "FLAG", "COMPARISON",
    "WAYPOINT", "BOOLEAN", "TRIGGER_AREA", "TEXT_STRING", "SIDE", "SOUND",
    "SCRIPT_SUBROUTINE", "UNIT", "OBJECT_TYPE", "COORD3D", "ANGLE",
    "TEAM_STATE", "RELATION", "AI_MOOD", "DIALOG", "MUSIC", "MOVIE",
    "WAYPOINT_PATH", "LOCALIZED_TEXT", "BRIDGE", "KIND_OF_PARAM",
    "ATTACK_PRIORITY_SET", "RADAR_EVENT_TYPE", "SPECIAL_POWER", "SCIENCE",
    "UPGRADE", "COMMANDBUTTON_ABILITY", "BOUNDARY", "BUILDABLE",
    "SURFACES_ALLOWED", "SHAKE_INTENSITY", "COMMAND_BUTTON", "FONT_NAME",
    "OBJECT_STATUS", "COMMANDBUTTON_ALL_ABILITIES", "SKIRMISH_WAYPOINT_PATH",
    "COLOR", "EMOTICON", "OBJECT_PANEL_FLAG", "FACTION_NAME",
    "OBJECT_TYPE_LIST", "REVEALNAME", "SCIENCE_AVAILABILITY",
)

SCRIPTING_GUIDE = """# Generals WorldBuilder MCP scripting guide

## Prefer semantic tools

- Use `list_sciences` and `list_upgrades` instead of hardcoding INI names.

## Generic scripts

1. Call `list_script_types` immediately before authoring. IDs belong to the
   running executable; select by `internal_name` when present, otherwise `name`.
2. Use the returned `parameter_type_names` and preserve parameter order.
3. A script needs a true condition to run unconditionally. Empty conditions
   do not mean true. Resolve `[Scripting] True.` from `list_script_types`.
4. Conditions with the same `or_group` are ANDed; different groups are ORed.
5. Keep each condition/action list at or below 512 entries.
6. Put global one-shot setup scripts on `PlyrCivilian` (resolve its index with
   `list_players`), set `one_shot=true`, and enable easy/normal/hard.
7. Use `expected_revision` and `operation_id`; finish with `save_map` and
   `validate_map`.

## Object progression

- `update_object(s).veterancy`: 0 Regular, 1 Veteran, 2 Elite, 3 Heroic.
- `list_objects` exposes `is_unit`.
"""


def _editor_id_schema() -> dict[str, Any]:
    return {
        "description": (
            "Editor id returned by list_editors. It may be omitted when exactly "
            "one editor is running."
        ),
        "oneOf": [{"type": "string"}, {"type": "integer"}],
    }


def _object_id_schema() -> dict[str, Any]:
    return {
        "description": "WorldBuilder object id.",
        "oneOf": [{"type": "string"}, {"type": "integer"}],
    }


def _object_owner_schema() -> dict[str, Any]:
    return {
        "description": (
            "For a lobby player, always pass the known one-based player number "
            "as an integer: Player 1 => 1, Player 2 => 2, through Player 8 => 8. "
            "Do not ask the user for an internal owner name when the player "
            "number is known. The server converts Player 1 to the engine runtime "
            "team teamplayer0. Pass a string only when targeting an explicit "
            "map team name returned by list_teams."
        ),
        "oneOf": [
            {
                "type": "integer",
                "minimum": 1,
                "maximum": 8,
                "description": "Preferred lobby-player form; Player 1 is 1.",
            },
            {
                "type": "string",
                "minLength": 1,
                "description": "Exact explicit map-team name from list_teams.",
            },
        ],
    }


def _object_properties(
    include_id: bool = False, *, include_progression: bool = False
) -> dict[str, Any]:
    properties: dict[str, Any] = {
        "template": {
            "type": "string",
            "minLength": 1,
            "description": "Object template name.",
        },
        "x": {"type": "number"},
        "y": {"type": "number"},
        "z": {"type": "number"},
        "angle": {
            "type": "number",
            "description": "Object yaw in degrees.",
        },
        "owner": _object_owner_schema(),
    }
    if include_progression:
        properties.update(
            {
                "veterancy": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 3,
                    "description": "Initial unit veterancy; 3 is Heroic.",
                },
            }
        )
    if include_id:
        properties = {
            "object_id": _object_id_schema(),
            "script_name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 128,
                "description": "Unique map object name used by script UNIT actions.",
            },
            **properties,
        }
    return properties


def _schema(
    properties: Mapping[str, Any] | None = None,
    required: tuple[str, ...] = (),
) -> dict[str, Any]:
    all_properties = {"editor_id": _editor_id_schema()}
    if properties:
        all_properties.update(properties)
    schema: dict[str, Any] = {
        "type": "object",
        "properties": all_properties,
        "additionalProperties": False,
    }
    if required:
        schema["required"] = list(required)
    return schema


def _revision_and_operation_properties() -> dict[str, Any]:
    return {
        "expected_revision": {
            "type": "integer",
            "minimum": 0,
            "description": "Reject the mutation if the map revision changed.",
        },
        "operation_id": {
            "type": "string",
            "minLength": 1,
            "maxLength": 128,
            "description": "Optional idempotency key for a mutating operation.",
        },
    }


def _object_item_schema(include_id: bool = False) -> dict[str, Any]:
    properties = _object_properties(
        include_id=include_id, include_progression=include_id
    )
    required = ["object_id"] if include_id else ["template", "x", "y"]
    return {
        "type": "object",
        "properties": properties,
        "required": required,
        "additionalProperties": False,
    }


TOOL_DEFINITIONS: list[dict[str, Any]] = [
    {
        "name": "list_editors",
        "description": "List running WorldBuilder instances exposing the MCP bridge.",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False,
        },
    },
    {
        "name": "list_blocking_dialogs",
        "description": "List visible blocking WorldBuilder message dialogs for one editor.",
        "inputSchema": _schema(),
    },
    {
        "name": "get_bridge_info",
        "description": "Get native bridge version, edition, and capabilities.",
        "inputSchema": _schema(),
    },
    {
        "name": "list_user_maps",
        "description": "List maps in the current Generals user's Maps directory.",
        "inputSchema": _schema(),
    },
    {
        "name": "open_map",
        "description": "Open a map from the current Generals user's Maps directory.",
        "inputSchema": _schema(
            {
                "path": {"type": "string", "minLength": 1},
                "on_unsaved": {
                    "type": "string",
                    "enum": ["error", "save", "discard"],
                    "default": "error",
                },
                "operation_id": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 128,
                },
            },
            required=("path",),
        ),
    },
    {
        "name": "get_map_state",
        "description": "Get the active map and editor state.",
        "inputSchema": _schema(),
    },
    {
        "name": "list_objects",
        "description": "List objects on the active map.",
        "inputSchema": _schema(
            {
                "filter": {
                    "type": "string",
                    "description": "Optional server-side object filter.",
                },
                "offset": {"type": "integer", "minimum": 0, "default": 0},
                "limit": {
                    "type": "integer",
                    "minimum": 1,
                    "default": 200,
                },
                "template": {"type": "string"},
                "owner": _object_owner_schema(),
                "selected": {"type": "boolean"},
                "waypoint": {"type": "boolean"},
                "min_x": {"type": "number"},
                "max_x": {"type": "number"},
                "min_y": {"type": "number"},
                "max_y": {"type": "number"},
            }
        ),
    },
    {
        "name": "get_object",
        "description": "Get one map object by persistent or session id.",
        "inputSchema": _schema(
            {"object_id": _object_id_schema()}, required=("object_id",)
        ),
    },
    {
        "name": "list_templates",
        "description": "List object templates known to WorldBuilder.",
        "inputSchema": _schema(
            {
                "filter": {
                    "type": "string",
                    "description": "Optional template name filter.",
                },
                "offset": {"type": "integer", "minimum": 0, "default": 0},
                "limit": {
                    "type": "integer",
                    "minimum": 1,
                    "default": 200,
                },
            }
        ),
    },
    {
        "name": "add_object",
        "description": (
            "Place a new object on the map. For Player 1, set owner to the "
            "integer 1 without asking for an internal team name."
        ),
        "inputSchema": _schema(
            {**_object_properties(), **_revision_and_operation_properties()},
            required=("template", "x", "y"),
        ),
    },
    {
        "name": "add_objects",
        "description": (
            "Place multiple objects as one undoable operation. Use integer "
            "owner 1 for Player 1, 2 for Player 2, and so on."
        ),
        "inputSchema": _schema(
            {
                "items": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 500,
                    "items": _object_item_schema(),
                },
                **_revision_and_operation_properties(),
            },
            required=("items",),
        ),
    },
    {
        "name": "update_object",
        "description": (
            "Update transform, template, owner, script name, or veterancy. Use "
            "integer owner 1 for Player 1; no internal owner-name lookup is needed."
        ),
        "inputSchema": _schema(
            {
                **_object_properties(include_id=True, include_progression=True),
                **_revision_and_operation_properties(),
            },
            required=("object_id",),
        ),
    },
    {
        "name": "update_objects",
        "description": (
            "Update multiple objects as one undoable operation. Lobby-player "
            "owners use one-based integers: Player 1 is owner 1."
        ),
        "inputSchema": _schema(
            {
                "items": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 500,
                    "items": _object_item_schema(include_id=True),
                },
                **_revision_and_operation_properties(),
            },
            required=("items",),
        ),
    },
    {
        "name": "delete_object",
        "description": "Delete an object from the map.",
        "inputSchema": _schema(
            {
                "object_id": _object_id_schema(),
                **_revision_and_operation_properties(),
            },
            required=("object_id",),
        ),
    },
    {
        "name": "delete_objects",
        "description": "Delete multiple objects as one undoable operation.",
        "inputSchema": _schema(
            {
                "object_ids": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 500,
                    "items": _object_id_schema(),
                },
                **_revision_and_operation_properties(),
            },
            required=("object_ids",),
        ),
    },
    {
        "name": "select_object",
        "description": "Select an object in WorldBuilder.",
        "inputSchema": _schema(
            {"object_id": _object_id_schema()}, required=("object_id",)
        ),
    },
    {
        "name": "get_terrain_heights",
        "description": "Read a rectangular terrain height region.",
        "inputSchema": _schema(
            {
                "x": {"type": "integer"},
                "y": {"type": "integer"},
                "width": {"type": "integer", "minimum": 1},
                "height": {"type": "integer", "minimum": 1},
            },
            required=("x", "y", "width", "height"),
        ),
    },
    {
        "name": "set_terrain_heights",
        "description": "Set terrain heights at one or more grid points.",
        "inputSchema": _schema(
            {
                "points": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 4096,
                    "items": {
                        "type": "object",
                        "properties": {
                            "x": {"type": "integer"},
                            "y": {"type": "integer"},
                            "height": {
                                "type": "integer",
                                "minimum": 0,
                                "maximum": 255,
                            },
                        },
                        "required": ["x", "y", "height"],
                        "additionalProperties": False,
                    },
                },
                **_revision_and_operation_properties(),
            },
            required=("points",),
        ),
    },
    {
        "name": "focus_view",
        "description": "Move the WorldBuilder view to map coordinates.",
        "inputSchema": _schema(
            {"x": {"type": "number"}, "y": {"type": "number"}},
            required=("x", "y"),
        ),
    },
    {
        "name": "list_players",
        "description": "List map players/sides.",
        "inputSchema": _schema(),
    },
    {
        "name": "list_teams",
        "description": "List map teams and their owners.",
        "inputSchema": _schema(),
    },
    {
        "name": "create_team",
        "description": "Create an explicit map team as one undoable operation.",
        "inputSchema": _schema(
            {
                "name": {"type": "string", "minLength": 1},
                "owner": {"type": "string"},
                "singleton": {"type": "boolean", "default": False},
                **_revision_and_operation_properties(),
            },
            required=("name", "owner"),
        ),
    },
    {
        "name": "update_team",
        "description": "Update a map team's owner or singleton flag.",
        "inputSchema": _schema(
            {
                "name": {"type": "string", "minLength": 1},
                "owner": {"type": "string"},
                "singleton": {"type": "boolean"},
                **_revision_and_operation_properties(),
            },
            required=("name",),
        ),
    },
    {
        "name": "delete_team",
        "description": "Delete an unused non-default map team.",
        "inputSchema": _schema(
            {
                "name": {"type": "string", "minLength": 1},
                **_revision_and_operation_properties(),
            },
            required=("name",),
        ),
    },
    {
        "name": "validate_map",
        "description": "Validate object ids, owners, templates, bounds, and starts.",
        "inputSchema": _schema(),
    },
    {
        "name": "save_map",
        "description": "Save the active map.",
        "inputSchema": _schema(_revision_and_operation_properties()),
    },
    {
        "name": "save_map_as",
        "description": "Save the active map under the current user's Maps directory.",
        "inputSchema": _schema(
            {
                "path": {"type": "string", "minLength": 1},
                "overwrite": {"type": "boolean", "default": False},
                **_revision_and_operation_properties(),
            },
            required=("path",),
        ),
    },
    {
        "name": "undo",
        "description": "Undo the most recent WorldBuilder edit.",
        "inputSchema": _schema(_revision_and_operation_properties()),
    },
    {
        "name": "redo",
        "description": "Redo the most recently undone WorldBuilder edit.",
        "inputSchema": _schema(_revision_and_operation_properties()),
    },
]

TOOL_DEFINITIONS.extend(EXTRA_TOOL_DEFINITIONS)


_TOOL_NAMES = {tool["name"] for tool in TOOL_DEFINITIONS}
_COMMANDS = {
    "get_bridge_info": "get_bridge_info",
    "list_user_maps": "list_user_maps",
    "open_map": "open_map",
    "get_map_state": "get_state",
    "list_objects": "list_objects",
    "get_object": "get_object",
    "list_templates": "list_templates",
    "add_object": "add_object",
    "add_objects": "add_objects",
    "update_object": "update_object",
    "update_objects": "update_objects",
    "delete_object": "delete_object",
    "delete_objects": "delete_objects",
    "select_object": "select_object",
    "list_players": "list_players",
    "list_teams": "list_teams",
    "create_team": "create_team",
    "update_team": "update_team",
    "delete_team": "delete_team",
    "validate_map": "validate_map",
    "get_terrain_heights": "get_terrain_heights",
    "set_terrain_heights": "set_terrain_heights",
    "focus_view": "focus_view",
    "save_map": "save_map",
    "save_map_as": "save_map_as",
    "undo": "undo",
    "redo": "redo",
}
_COMMANDS.update(EXTRA_COMMANDS)

_MUTATION_META = {"expected_revision", "operation_id"}
_ALLOWED_ARGUMENTS: dict[str, set[str]] = {
    "list_editors": set(),
    "list_blocking_dialogs": {"editor_id"},
    "get_bridge_info": {"editor_id"},
    "list_user_maps": {"editor_id"},
    "open_map": {"editor_id", "path", "on_unsaved", "operation_id"},
    "get_map_state": {"editor_id"},
    "list_objects": {
        "editor_id",
        "filter",
        "offset",
        "limit",
        "template",
        "owner",
        "selected",
        "waypoint",
        "min_x",
        "max_x",
        "min_y",
        "max_y",
    },
    "get_object": {"editor_id", "object_id"},
    "list_templates": {"editor_id", "filter", "offset", "limit"},
    "add_object": {
        "editor_id",
        "template",
        "x",
        "y",
        "z",
        "angle",
        "owner",
    } | _MUTATION_META,
    "add_objects": {"editor_id", "items"} | _MUTATION_META,
    "update_object": {
        "editor_id",
        "object_id",
        "template",
        "x",
        "y",
        "z",
        "angle",
        "owner",
        "veterancy",
        "script_name",
    } | _MUTATION_META,
    "update_objects": {"editor_id", "items"} | _MUTATION_META,
    "delete_object": {"editor_id", "object_id"} | _MUTATION_META,
    "delete_objects": {"editor_id", "object_ids"} | _MUTATION_META,
    "select_object": {"editor_id", "object_id"},
    "list_players": {"editor_id"},
    "list_teams": {"editor_id"},
    "create_team": {
        "editor_id",
        "name",
        "owner",
        "singleton",
    } | _MUTATION_META,
    "update_team": {
        "editor_id",
        "name",
        "owner",
        "singleton",
    } | _MUTATION_META,
    "delete_team": {"editor_id", "name"} | _MUTATION_META,
    "validate_map": {"editor_id"},
    "get_terrain_heights": {
        "editor_id",
        "x",
        "y",
        "width",
        "height",
    },
    "set_terrain_heights": {"editor_id", "points"} | _MUTATION_META,
    "focus_view": {"editor_id", "x", "y"},
    "save_map": {"editor_id"} | _MUTATION_META,
    "save_map_as": {
        "editor_id",
        "path",
        "overwrite",
    } | _MUTATION_META,
    "undo": {"editor_id"} | _MUTATION_META,
    "redo": {"editor_id"} | _MUTATION_META,
}
_ALLOWED_ARGUMENTS.update(EXTRA_ALLOWED)

_REQUIRED_ARGUMENTS: dict[str, set[str]] = {
    "open_map": {"path"},
    "get_object": {"object_id"},
    "add_object": {"template", "x", "y"},
    "add_objects": {"items"},
    "update_object": {"object_id"},
    "update_objects": {"items"},
    "delete_object": {"object_id"},
    "delete_objects": {"object_ids"},
    "select_object": {"object_id"},
    "create_team": {"name", "owner"},
    "update_team": {"name"},
    "delete_team": {"name"},
    "get_terrain_heights": {"x", "y", "width", "height"},
    "set_terrain_heights": {"points"},
    "focus_view": {"x", "y"},
    "save_map_as": {"path"},
}
_REQUIRED_ARGUMENTS.update(EXTRA_REQUIRED)


class InvalidToolArguments(BridgeError):
    def __init__(self, message: str) -> None:
        super().__init__("invalid_arguments", message)


def _is_number(value: Any) -> bool:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    if isinstance(value, int):
        return True
    return math.isfinite(value)


def _require_number(arguments: Mapping[str, Any], key: str) -> None:
    if key in arguments and not _is_number(arguments[key]):
        raise InvalidToolArguments(f"{key} must be a finite number.")


def _require_integer(
    arguments: Mapping[str, Any], key: str, minimum: int | None = None
) -> None:
    if key not in arguments:
        return
    value = arguments[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise InvalidToolArguments(f"{key} must be an integer.")
    if minimum is not None and value < minimum:
        raise InvalidToolArguments(f"{key} must be at least {minimum}.")


def _validate_object_item(
    item: Any, index: int, *, include_id: bool
) -> dict[str, Any]:
    if not isinstance(item, dict):
        raise InvalidToolArguments(f"items[{index}] must be an object.")
    allowed = set(
        _object_properties(
            include_id=include_id, include_progression=include_id
        )
    )
    unknown = set(item) - allowed
    if unknown:
        raise InvalidToolArguments(
            f"items[{index}] has unknown field(s): "
            + ", ".join(sorted(unknown))
            + "."
        )
    required = {"object_id"} if include_id else {"template", "x", "y"}
    missing = required - set(item)
    if missing:
        raise InvalidToolArguments(
            f"items[{index}] is missing " + ", ".join(sorted(missing)) + "."
        )
    if include_id and len(set(item) - {"object_id"}) == 0:
        raise InvalidToolArguments(
            f"items[{index}] needs at least one field to update."
        )
    for key in ("x", "y", "z", "angle"):
        if key in item and not _is_number(item[key]):
            raise InvalidToolArguments(
                f"items[{index}].{key} must be a finite number."
            )
    for key, maximum in (("veterancy", 3),):
        if key in item and (
            isinstance(item[key], bool)
            or not isinstance(item[key], int)
            or not 0 <= item[key] <= maximum
        ):
            raise InvalidToolArguments(
                f"items[{index}].{key} must be an integer between 0 and {maximum}."
            )
    if "template" in item and (
        not isinstance(item["template"], str) or not item["template"]
    ):
        raise InvalidToolArguments(
            f"items[{index}].template must be a non-empty string."
        )
    if "script_name" in item and (
        not isinstance(item["script_name"], str)
        or not 1 <= len(item["script_name"]) <= 128
    ):
        raise InvalidToolArguments(
            f"items[{index}].script_name must contain 1 to 128 characters."
        )
    for key in ("owner", "object_id"):
        if key in item and (
            isinstance(item[key], bool)
            or not isinstance(item[key], (str, int))
            or (isinstance(item[key], str) and not item[key])
        ):
            raise InvalidToolArguments(
                f"items[{index}].{key} must be a non-empty string or integer."
            )
    validated = dict(item)
    if "owner" in validated:
        validated["owner"] = _normalize_object_owner(
            validated["owner"], f"items[{index}].owner"
        )
    return validated


def _normalize_object_owner(value: Any, field: str = "owner") -> str:
    if isinstance(value, int) and not isinstance(value, bool):
        if not 1 <= value <= 8:
            raise InvalidToolArguments(
                f"{field} player slot must be between 1 and 8."
            )
        return f"teamplayer{value - 1}"
    if not isinstance(value, str) or not value:
        raise InvalidToolArguments(
            f"{field} must be a non-empty map team name or player slot."
        )
    return value


def _flatten_bulk_arguments(name: str, arguments: dict[str, Any]) -> dict[str, Any]:
    if name in ("add_objects", "update_objects"):
        items = arguments.pop("items")
        arguments["item_count"] = len(items)
        for index, item in enumerate(items):
            for key, value in item.items():
                arguments[f"item{index}_{key}"] = value
    elif name == "delete_objects":
        object_ids = arguments.pop("object_ids")
        arguments["item_count"] = len(object_ids)
        for index, object_id in enumerate(object_ids):
            arguments[f"item{index}_object_id"] = object_id
    return arguments


def _validate_arguments(name: str, raw_arguments: Any) -> dict[str, Any]:
    if raw_arguments is None:
        arguments: dict[str, Any] = {}
    elif isinstance(raw_arguments, dict):
        arguments = dict(raw_arguments)
    else:
        raise InvalidToolArguments("arguments must be an object.")

    unknown = set(arguments) - _ALLOWED_ARGUMENTS[name]
    if unknown:
        raise InvalidToolArguments(
            "Unknown argument(s): " + ", ".join(sorted(unknown)) + "."
        )
    missing = _REQUIRED_ARGUMENTS.get(name, set()) - set(arguments)
    if missing:
        raise InvalidToolArguments(
            "Missing required argument(s): " + ", ".join(sorted(missing)) + "."
        )

    editor_id = arguments.get("editor_id")
    if (
        editor_id is not None
        and (
            isinstance(editor_id, bool)
            or not isinstance(editor_id, (str, int))
        )
    ):
        raise InvalidToolArguments("editor_id must be a string or integer.")

    if "filter" in arguments and not isinstance(arguments["filter"], str):
        raise InvalidToolArguments("filter must be a string.")
    for key in ("path", "name", "operation_id", "template"):
        if key in arguments and (
            not isinstance(arguments[key], str) or not arguments[key]
        ):
            raise InvalidToolArguments(f"{key} must be a non-empty string.")
    if "operation_id" in arguments and len(arguments["operation_id"]) > 128:
        raise InvalidToolArguments("operation_id must not exceed 128 characters.")
    if "on_unsaved" in arguments and arguments["on_unsaved"] not in (
        "error",
        "save",
        "discard",
    ):
        raise InvalidToolArguments(
            "on_unsaved must be error, save, or discard."
        )
    for key in ("offset",):
        _require_integer(arguments, key, minimum=0)
    for key in ("limit", "width", "height"):
        _require_integer(arguments, key, minimum=1)
    _require_integer(arguments, "expected_revision", minimum=0)
    for key in ("x", "y"):
        if name == "get_terrain_heights":
            _require_integer(arguments, key)
        else:
            _require_number(arguments, key)
    for key in ("z", "angle", "min_x", "max_x", "min_y", "max_y"):
        _require_number(arguments, key)
    for key, maximum in (("veterancy", 3),):
        if key in arguments:
            value = arguments[key]
            if (
                isinstance(value, bool)
                or not isinstance(value, int)
                or not 0 <= value <= maximum
            ):
                raise InvalidToolArguments(
                    f"{key} must be an integer between 0 and {maximum}."
                )
    for key in (
        "selected",
        "waypoint",
        "overwrite",
        "singleton",
        "save",
        "validate_after",
    ):
        if key in arguments and not isinstance(arguments[key], bool):
            raise InvalidToolArguments(f"{key} must be a boolean.")


    if "template" in arguments and (
        not isinstance(arguments["template"], str)
        or not arguments["template"]
    ):
        raise InvalidToolArguments("template must be a non-empty string.")
    if "script_name" in arguments and (
        not isinstance(arguments["script_name"], str)
        or not 1 <= len(arguments["script_name"]) <= 128
    ):
        raise InvalidToolArguments("script_name must contain 1 to 128 characters.")
    if "owner" in arguments:
        if name in ("add_object", "update_object", "list_objects"):
            arguments["owner"] = _normalize_object_owner(arguments["owner"])
        elif not isinstance(arguments["owner"], str) or not arguments["owner"]:
            raise InvalidToolArguments("owner must be a non-empty string.")
    if "object_id" in arguments and (
        isinstance(arguments["object_id"], bool)
        or not isinstance(arguments["object_id"], (str, int))
        or (isinstance(arguments["object_id"], str) and not arguments["object_id"])
    ):
        raise InvalidToolArguments(
            "object_id must be a non-empty string or integer."
        )

    if name == "update_object":
        updates = set(arguments) - {
            "editor_id",
            "object_id",
            "expected_revision",
            "operation_id",
        }
        if not updates:
            raise InvalidToolArguments(
                "update_object needs at least one field to update."
            )
    if name == "update_team":
        updates = set(arguments) - {
            "editor_id",
            "name",
            "expected_revision",
            "operation_id",
        }
        if not updates:
            raise InvalidToolArguments(
                "update_team needs owner or singleton."
            )
    if name in ("add_objects", "update_objects"):
        items = arguments["items"]
        if not isinstance(items, list) or not 1 <= len(items) <= 500:
            raise InvalidToolArguments(
                "items must contain between 1 and 500 objects."
            )
        arguments["items"] = [
            _validate_object_item(
                item, index, include_id=name == "update_objects"
            )
            for index, item in enumerate(items)
        ]
    if name == "delete_objects":
        object_ids = arguments["object_ids"]
        if not isinstance(object_ids, list) or not 1 <= len(object_ids) <= 500:
            raise InvalidToolArguments(
                "object_ids must contain between 1 and 500 ids."
            )
        seen: set[str] = set()
        for index, object_id in enumerate(object_ids):
            if (
                isinstance(object_id, bool)
                or not isinstance(object_id, (str, int))
                or (isinstance(object_id, str) and not object_id)
            ):
                raise InvalidToolArguments(
                    f"object_ids[{index}] must be a non-empty string or integer."
                )
            key = str(object_id)
            if key in seen:
                raise InvalidToolArguments("object_ids must be unique.")
            seen.add(key)
    if name == "set_terrain_heights":
        # This also performs strict point validation.
        arguments["points"] = format_height_points(arguments["points"])
    if name in EXTRA_ALLOWED:
        arguments = validate_and_flatten(name, arguments)
    return arguments


def _json_text(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        separators=(",", ":"),
    )


def _annotate_script_types(result: Any) -> Any:
    """Attach stable semantic names to native numeric parameter types."""

    if not isinstance(result, dict):
        return result
    for collection in ("conditions", "actions"):
        items = result.get(collection)
        if not isinstance(items, list):
            continue
        for item in items:
            if not isinstance(item, dict):
                continue
            raw_types = item.get("parameter_types")
            if not isinstance(raw_types, list):
                continue
            names: list[str] = []
            parameters: list[dict[str, Any]] = []
            for raw_type in raw_types:
                if (
                    isinstance(raw_type, int)
                    and not isinstance(raw_type, bool)
                    and 0 <= raw_type < len(SCRIPT_PARAMETER_TYPE_NAMES)
                ):
                    type_name = SCRIPT_PARAMETER_TYPE_NAMES[raw_type]
                else:
                    type_name = f"UNKNOWN_{raw_type}"
                names.append(type_name)
                parameters.append({"type": raw_type, "type_name": type_name})
            item["parameter_type_names"] = names
            item["parameters"] = parameters
    return result


def _tool_success(result: Any) -> dict[str, Any]:
    response: dict[str, Any] = {
        "content": [{"type": "text", "text": _json_text(result)}]
    }
    # structuredContent is defined as an object by MCP. Preserve native
    # object results for clients that support it while content stays universal.
    if isinstance(result, dict):
        response["structuredContent"] = result
    return response


def _capture_success(result: Any) -> dict[str, Any]:
    response = _tool_success(result)
    if not isinstance(result, dict) or not isinstance(result.get("path"), str):
        return response
    try:
        capture = Path(result["path"])
        if capture.stat().st_size > 64 * 1024 * 1024:
            return response
        encoded = base64.b64encode(capture.read_bytes()).decode("ascii")
    except OSError:
        return response
    response["content"].append(
        {"type": "image", "data": encoded, "mimeType": "image/bmp"}
    )
    return response


def _tool_failure(error: BridgeError) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "error": {"code": error.code, "message": error.message}
    }
    if error.data is not None:
        payload["error"]["data"] = error.data
    return {
        "content": [{"type": "text", "text": _json_text(payload)}],
        "isError": True,
    }


@dataclass
class RpcError(Exception):
    code: int
    message: str
    data: Any = None


class McpServer:
    def __init__(self, bridge: WorldBuilderBridge | None = None) -> None:
        self.bridge = bridge or WorldBuilderBridge()
        self._operation_results: dict[
            tuple[str, str, str], dict[str, Any]
        ] = {}

    def _call_object_batch(
        self,
        name: str,
        arguments: dict[str, Any],
        editor_id: Any,
    ) -> Any:
        """Send bounded object chunks and merge native bridge responses."""

        items = arguments.pop("items")
        chunks = [
            items[offset : offset + BULK_OBJECT_CHUNK_SIZE]
            for offset in range(0, len(items), BULK_OBJECT_CHUNK_SIZE)
        ]
        results: list[dict[str, Any]] = []
        completed_items = 0
        for chunk_index, chunk in enumerate(chunks):
            chunk_arguments = dict(arguments)
            chunk_arguments["items"] = chunk
            if chunk_index and "expected_revision" in chunk_arguments:
                previous_revision = results[-1].get("revision")
                if (
                    isinstance(previous_revision, bool)
                    or not isinstance(previous_revision, int)
                ):
                    raise BridgeError(
                        "invalid_response",
                        "WorldBuilder batch response omitted its revision.",
                        {
                            "chunk_index": chunk_index - 1,
                            "items_completed": completed_items,
                        },
                    )
                chunk_arguments["expected_revision"] = previous_revision
            try:
                result = self.bridge.call(
                    _COMMANDS[name],
                    arguments=_flatten_bulk_arguments(name, chunk_arguments),
                    editor_id=editor_id,
                )
            except BridgeError as error:
                data = {
                    "chunk_index": chunk_index,
                    "chunk_count": len(chunks),
                    "items_completed": completed_items,
                    "items_total": len(items),
                }
                if error.data is not None:
                    data["cause"] = error.data
                raise BridgeError(error.code, error.message, data) from error
            if not isinstance(result, dict):
                raise BridgeError(
                    "invalid_response",
                    "WorldBuilder object batch response must be an object.",
                    {
                        "chunk_index": chunk_index,
                        "items_completed": completed_items,
                    },
                )
            results.append(result)
            completed_items += len(chunk)
            if chunk_index + 1 < len(chunks):
                time.sleep(BULK_OBJECT_CHUNK_DELAY_SECONDS)

        if len(results) == 1:
            return results[0]

        objects: list[Any] = []
        for chunk_index, result in enumerate(results):
            chunk_objects = result.get("objects")
            if not isinstance(chunk_objects, list):
                raise BridgeError(
                    "invalid_response",
                    "WorldBuilder object batch response omitted its objects.",
                    {
                        "chunk_index": chunk_index,
                        "items_completed": sum(
                            len(chunk) for chunk in chunks[:chunk_index]
                        ),
                    },
                )
            objects.extend(chunk_objects)
        count_key = "added" if name == "add_objects" else "updated"
        return {
            "objects": objects,
            count_key: len(items),
            "revision": results[-1].get("revision"),
            "chunk_count": len(chunks),
        }


    def call_tool(self, name: str, raw_arguments: Any) -> dict[str, Any]:
        if name not in _TOOL_NAMES:
            return _tool_failure(
                InvalidToolArguments(f"Unknown tool {name!r}.")
            )
        try:
            arguments = _validate_arguments(name, raw_arguments)
            if name == "list_editors":
                return _tool_success(self.bridge.list_editors())
            if name == "list_blocking_dialogs":
                return _tool_success(
                    self.bridge.list_blocking_dialogs(arguments.get("editor_id"))
                )
            if name == "launch_editor":
                return _tool_success(
                    self.bridge.launch_editor(
                        arguments.get("edition", "zero_hour"),
                        arguments.get("wait_ms", 15_000),
                    )
                )

            editor_id = arguments.pop("editor_id", None)
            operation_id = arguments.pop("operation_id", None)
            cache_key: tuple[str, str, str] | None = None
            if operation_id is not None:
                cache_key = (name, str(editor_id or ""), operation_id)
                cached = self._operation_results.get(cache_key)
                if cached is not None:
                    return cached
            if name in ("add_objects", "update_objects"):
                result = self._call_object_batch(name, arguments, editor_id)
            else:
                result = self.bridge.call(
                    _COMMANDS[name],
                    arguments=_flatten_bulk_arguments(name, arguments),
                    editor_id=editor_id,
                )
            if name == "list_script_types":
                result = _annotate_script_types(result)
            response = (
                _capture_success(result)
                if name == "capture_view"
                else _tool_success(result)
            )
            if cache_key is not None:
                if len(self._operation_results) >= 128:
                    self._operation_results.pop(
                        next(iter(self._operation_results))
                    )
                self._operation_results[cache_key] = response
            return response
        except BridgeError as error:
            return _tool_failure(error)
        except Exception as error:
            # The server must remain alive if a single editor call fails in an
            # unexpected way. Do not expose a traceback over the MCP channel.
            return _tool_failure(
                BridgeError(
                    "internal_error",
                    f"Unexpected WorldBuilder MCP error: {error}",
                )
            )

    def dispatch(self, method: str, params: Any) -> Any:
        if params is None:
            params = {}
        if not isinstance(params, dict):
            raise RpcError(-32602, "params must be an object")

        if method == "initialize":
            requested_version = params.get("protocolVersion")
            protocol_version = (
                requested_version
                if requested_version in SUPPORTED_PROTOCOL_VERSIONS
                else DEFAULT_PROTOCOL_VERSION
            )
            return {
                "protocolVersion": protocol_version,
                "capabilities": {
                    "tools": {"listChanged": False},
                    "resources": {
                        "subscribe": False,
                        "listChanged": False,
                    },
                    "prompts": {"listChanged": False},
                },
                "serverInfo": {
                    "name": SERVER_NAME,
                    "version": SERVER_VERSION,
                },
                "instructions": (
                    "Control a running Command & Conquer Generals WorldBuilder "
                    "instance. User maps are resolved from the current Windows "
                    "user's Generals data directory. Call list_editors first "
                    "when multiple instances may be running. For object owner, "
                    "use the one-based integer player number whenever the user "
                    "identifies a lobby player: Player 1 means owner 1, Player 2 "
                    "means owner 2, and so on. Never ask for an internal owner "
                    "name in that case; the server maps Player 1 to teamplayer0."
                    " Read "
                    "worldbuilder://scripting-guide before authoring generic scripts."
                ),
            }
        if method == "ping":
            return {}
        if method == "tools/list":
            return {"tools": TOOL_DEFINITIONS}
        if method == "tools/call":
            name = params.get("name")
            if not isinstance(name, str) or not name:
                raise RpcError(-32602, "tools/call requires a non-empty name")
            return self.call_tool(name, params.get("arguments", {}))
        if method == "resources/list":
            return {
                "resources": [
                    {
                        "uri": EDITORS_RESOURCE_URI,
                        "name": "Connected WorldBuilder editors",
                        "description": (
                            "Currently running WorldBuilder MCP endpoints."
                        ),
                        "mimeType": "application/json",
                    },
                    {
                        "uri": MAPS_RESOURCE_URI,
                        "name": "Generals user maps",
                        "description": (
                            "Maps in the current Windows user's Generals Maps "
                            "directory."
                        ),
                        "mimeType": "application/json",
                    },
                    {
                        "uri": SCRIPTING_GUIDE_RESOURCE_URI,
                        "name": "WorldBuilder scripting guide",
                        "description": (
                            "Semantic script authoring, player-slot mapping, "
                            "progression fields, limits, and verification workflow."
                        ),
                        "mimeType": "text/markdown",
                    },
                ]
            }
        if method == "resources/read":
            uri = params.get("uri")
            if uri not in (
                EDITORS_RESOURCE_URI,
                MAPS_RESOURCE_URI,
                SCRIPTING_GUIDE_RESOURCE_URI,
            ):
                raise RpcError(-32602, f"Unknown resource URI {uri!r}")
            try:
                if uri == EDITORS_RESOURCE_URI:
                    payload: Any = {"editors": self.bridge.list_editors()}
                    mime_type = "application/json"
                    text_value = _json_text(payload)
                elif uri == MAPS_RESOURCE_URI:
                    payload = self.bridge.call("list_user_maps")
                    mime_type = "application/json"
                    text_value = _json_text(payload)
                else:
                    mime_type = "text/markdown"
                    text_value = SCRIPTING_GUIDE
            except BridgeError as error:
                raise RpcError(
                    -32001,
                    error.message,
                    {"bridge_code": error.code, "data": error.data},
                ) from error
            return {
                "contents": [
                    {
                        "uri": uri,
                        "mimeType": mime_type,
                        "text": text_value,
                    }
                ]
            }
        if method == "prompts/list":
            return {"prompts": []}
        if method in (
            "notifications/initialized",
            "notifications/cancelled",
            "exit",
        ):
            return None
        raise RpcError(-32601, f"Method not found: {method}")

    def handle_message(self, message: Any) -> dict[str, Any] | None:
        if not isinstance(message, dict):
            return _error_response(None, -32600, "Invalid Request")

        has_id = "id" in message
        request_id = message.get("id")
        if message.get("jsonrpc") != "2.0" or not isinstance(
            message.get("method"), str
        ):
            if not has_id:
                return None
            return _error_response(request_id, -32600, "Invalid Request")

        try:
            result = self.dispatch(message["method"], message.get("params", {}))
        except RpcError as error:
            if not has_id:
                return None
            return _error_response(
                request_id, error.code, error.message, error.data
            )
        except Exception:
            if not has_id:
                return None
            return _error_response(request_id, -32603, "Internal error")

        if not has_id:
            return None
        return {"jsonrpc": "2.0", "id": request_id, "result": result}


def _error_response(
    request_id: Any, code: int, message: str, data: Any = None
) -> dict[str, Any]:
    error: dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        error["data"] = data
    return {"jsonrpc": "2.0", "id": request_id, "error": error}


class JsonRpcStream:
    """Read newline-delimited MCP messages, with Content-Length compatibility."""

    def __init__(
        self,
        input_stream: BinaryIO | TextIO,
        output_stream: BinaryIO | TextIO,
    ) -> None:
        self.input_stream = input_stream
        self.output_stream = output_stream

    @staticmethod
    def _as_bytes(value: bytes | str) -> bytes:
        return value.encode("utf-8") if isinstance(value, str) else value

    def read_message(self) -> tuple[Any, str] | None:
        while True:
            first = self.input_stream.readline()
            if first in (b"", ""):
                return None
            first_bytes = self._as_bytes(first)
            if first_bytes.strip():
                break

        framing = "newline"
        if not first_bytes.lstrip().startswith((b"{", b"[")):
            framing = "headers"
            headers: dict[str, str] = {}
            line = first_bytes
            while line.strip():
                try:
                    name, value = line.decode("ascii").split(":", 1)
                except (UnicodeDecodeError, ValueError) as exc:
                    raise ValueError("Malformed JSON-RPC header") from exc
                headers[name.strip().lower()] = value.strip()
                next_line = self.input_stream.readline()
                if next_line in (b"", ""):
                    raise ValueError("Unexpected EOF in JSON-RPC headers")
                line = self._as_bytes(next_line)
            try:
                content_length = int(headers["content-length"])
            except (KeyError, ValueError) as exc:
                raise ValueError("Missing or invalid Content-Length") from exc
            if content_length < 0:
                raise ValueError("Negative Content-Length")
            payload = self._as_bytes(self.input_stream.read(content_length))
            if len(payload) != content_length:
                raise ValueError("Unexpected EOF in JSON-RPC payload")
        else:
            payload = first_bytes.strip()

        try:
            return json.loads(payload.decode("utf-8-sig")), framing
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ValueError(f"Invalid JSON-RPC payload: {exc}") from exc

    def write_message(self, message: Mapping[str, Any], framing: str) -> None:
        payload = _json_text(message).encode("utf-8")
        if framing == "headers":
            encoded = (
                f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
                + payload
            )
        else:
            encoded = payload + b"\n"
        try:
            self.output_stream.write(encoded)
        except TypeError:
            self.output_stream.write(encoded.decode("utf-8"))
        self.output_stream.flush()


def serve(
    input_stream: BinaryIO | TextIO,
    output_stream: BinaryIO | TextIO,
    server: McpServer | None = None,
) -> None:
    mcp = server or McpServer()
    transport = JsonRpcStream(input_stream, output_stream)
    while True:
        framing = "newline"
        try:
            incoming = transport.read_message()
            if incoming is None:
                return
            message, framing = incoming
            response = mcp.handle_message(message)
        except ValueError as error:
            response = _error_response(None, -32700, "Parse error", str(error))
        if response is not None:
            transport.write_message(response, framing)


def _binary_input(stream: TextIO) -> BinaryIO | TextIO:
    return getattr(stream, "buffer", stream)


def _binary_output(stream: TextIO) -> BinaryIO | TextIO:
    return getattr(stream, "buffer", stream)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="WorldBuilder MCP stdio server"
    )
    parser.add_argument(
        "--timeout-ms",
        type=int,
        default=5_000,
        help="WorldBuilder command timeout in milliseconds (default: 5000)",
    )
    parser.add_argument(
        "--temp-root",
        type=Path,
        help="Override the system temporary directory (primarily for tests)",
    )
    parser.add_argument(
        "--zero-hour-editor",
        type=Path,
        help="Path to the Zero Hour WorldBuilder executable for launch_editor",
    )
    options = parser.parse_args(argv)
    if options.timeout_ms <= 0:
        parser.error("--timeout-ms must be positive")

    bridge = WorldBuilderBridge(
        temp_root=options.temp_root,
        timeout_ms=options.timeout_ms,
        editor_paths={
            edition: path
            for edition, path in {
                "zero_hour": options.zero_hour_editor,
            }.items()
            if path is not None
        },
    )
    try:
        serve(
            _binary_input(sys.stdin),
            _binary_output(sys.stdout),
            McpServer(bridge),
        )
    except (BrokenPipeError, KeyboardInterrupt):
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
