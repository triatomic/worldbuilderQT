"""Extended WorldBuilder MCP tool contracts and wire-format adapters."""

from __future__ import annotations

import math
from typing import Any, Mapping

from .bridge import BridgeError


MUTATION = {
    "expected_revision": {"type": "integer", "minimum": 0},
    "operation_id": {"type": "string", "minLength": 1, "maxLength": 128},
}
EDITOR = {
    "editor_id": {
        "description": "Editor id from list_editors; optional with one editor.",
        "oneOf": [{"type": "string"}, {"type": "integer"}],
    }
}
POINT = {
    "type": "object",
    "properties": {
        "x": {"type": "number"},
        "y": {"type": "number"},
        "z": {"type": "number", "default": 0},
    },
    "required": ["x", "y"],
    "additionalProperties": False,
}


def schema(
    properties: Mapping[str, Any] | None = None,
    required: tuple[str, ...] = (),
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "type": "object",
        "properties": {**EDITOR, **(properties or {})},
        "additionalProperties": False,
    }
    if required:
        result["required"] = list(required)
    return result


def mutation_schema(
    properties: Mapping[str, Any], required: tuple[str, ...]
) -> dict[str, Any]:
    return schema({**properties, **MUTATION}, required)


PARAMETER = {
    "description": (
        "A script parameter value. Numbers, strings and booleans are accepted; "
        "coordinates use an {x,y,z} object. The native action/condition template "
        "determines the stored parameter type."
    ),
    "oneOf": [
        {"type": "integer"},
        {"type": "number"},
        {"type": "string"},
        {"type": "boolean"},
        POINT,
    ],
}
CONDITION = {
    "type": "object",
    "properties": {
        "or_group": {"type": "integer", "minimum": 0, "default": 0},
        "type": {"type": "integer", "minimum": 0},
        "custom_data": {"type": "integer", "default": 0},
        "parameters": {"type": "array", "maxItems": 12, "items": PARAMETER},
    },
    "required": ["type"],
    "additionalProperties": False,
}
ACTION = {
    "type": "object",
    "properties": {
        "type": {"type": "integer", "minimum": 0},
        "parameters": {"type": "array", "maxItems": 12, "items": PARAMETER},
    },
    "required": ["type"],
    "additionalProperties": False,
}


EXTRA_TOOL_DEFINITIONS: list[dict[str, Any]] = [
    {"name": "launch_editor", "description": "Launch the configured WorldBuilder executable if it is not already running.", "inputSchema": {"type": "object", "properties": {"edition": {"type": "string", "enum": ["zero_hour"], "default": "zero_hour"}, "wait_ms": {"type": "integer", "minimum": 0, "maximum": 120000, "default": 15000}}, "additionalProperties": False}},
    {"name": "new_map", "description": "Create a new map with explicit dimensions, initial height and border, applying the requested unsaved-change policy.", "inputSchema": mutation_schema({"width": {"type": "integer", "minimum": 2}, "height": {"type": "integer", "minimum": 2}, "default_height": {"type": "integer", "minimum": 0, "maximum": 255}, "border_size": {"type": "integer", "minimum": 0}, "on_unsaved": {"type": "string", "enum": ["error", "save", "discard"], "default": "error"}}, ("width", "height", "default_height", "border_size"))},
    {"name": "close_map", "description": "Close the active map using error, save or discard for unsaved changes.", "inputSchema": schema({"on_unsaved": {"type": "string", "enum": ["error", "save", "discard"], "default": "error"}, "operation_id": MUTATION["operation_id"]})},
    {"name": "list_waypoints", "description": "List waypoints and directed route links.", "inputSchema": schema()},
    {"name": "create_waypoint", "description": "Create a named waypoint.", "inputSchema": mutation_schema({"name": {"type": "string", "minLength": 1}, "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}, ("name", "x", "y"))},
    {"name": "update_waypoint", "description": "Rename or move a waypoint.", "inputSchema": mutation_schema({"waypoint_id": {"type": "integer", "minimum": 1}, "name": {"type": "string", "minLength": 1}, "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}, ("waypoint_id",))},
    {"name": "delete_waypoint", "description": "Delete a waypoint and all of its route links.", "inputSchema": mutation_schema({"waypoint_id": {"type": "integer", "minimum": 1}}, ("waypoint_id",))},
    {"name": "connect_waypoints", "description": "Add a directed route link between two waypoints.", "inputSchema": mutation_schema({"from_id": {"type": "integer", "minimum": 1}, "to_id": {"type": "integer", "minimum": 1}}, ("from_id", "to_id"))},
    {"name": "disconnect_waypoints", "description": "Remove a directed route link.", "inputSchema": mutation_schema({"from_id": {"type": "integer", "minimum": 1}, "to_id": {"type": "integer", "minimum": 1}}, ("from_id", "to_id"))},
    {"name": "list_areas", "description": "List scripting/trigger polygons and water polygons.", "inputSchema": schema({"kind": {"type": "string", "enum": ["all", "scripting", "water"], "default": "all"}})},
    {"name": "create_area", "description": "Create a scripting region or water polygon.", "inputSchema": mutation_schema({"name": {"type": "string", "minLength": 1}, "kind": {"type": "string", "enum": ["scripting", "water"]}, "points": {"type": "array", "minItems": 3, "maxItems": 256, "items": POINT}, "export_with_scripts": {"type": "boolean", "default": True}, "river": {"type": "boolean", "default": False}, "river_start": {"type": "integer", "minimum": 0}, "layer": {"type": "string"}}, ("name", "kind", "points"))},
    {"name": "update_area", "description": "Update polygon metadata or replace all polygon points.", "inputSchema": mutation_schema({"area_id": {"type": "integer", "minimum": 0}, "name": {"type": "string", "minLength": 1}, "kind": {"type": "string", "enum": ["scripting", "water"]}, "points": {"type": "array", "minItems": 3, "maxItems": 256, "items": POINT}, "export_with_scripts": {"type": "boolean"}, "river": {"type": "boolean"}, "river_start": {"type": "integer", "minimum": 0}, "layer": {"type": "string"}}, ("area_id",))},
    {"name": "delete_area", "description": "Delete a trigger/scripting/water polygon.", "inputSchema": mutation_schema({"area_id": {"type": "integer", "minimum": 0}}, ("area_id",))},
    {"name": "list_script_types", "description": "List native condition/action type ids, names and parameter types.", "inputSchema": schema()},
    {"name": "list_sciences", "description": "List the editor's loaded science/general-power catalog, including grantability and purchase cost.", "inputSchema": schema()},
    {"name": "list_upgrades", "description": "List the editor's loaded upgrade catalog and whether each upgrade is player- or object-scoped.", "inputSchema": schema()},
    {"name": "list_scripts", "description": "List scripts for one player or every map player.", "inputSchema": schema({"player_index": {"type": "integer", "minimum": 0}})},
    {"name": "get_script", "description": "Read a script including its OR/AND conditions and true/false actions.", "inputSchema": schema({"player_index": {"type": "integer", "minimum": 0}, "name": {"type": "string", "minLength": 1}, "group": {"type": "string", "minLength": 1}}, ("player_index", "name"))},
    {"name": "upsert_script", "description": "Create or replace a top-level or grouped script, its conditions and actions as one undoable edit. Type ids are returned by list_script_types.", "inputSchema": mutation_schema({"player_index": {"type": "integer", "minimum": 0}, "name": {"type": "string", "minLength": 1}, "group": {"type": "string", "minLength": 1}, "comment": {"type": "string"}, "condition_comment": {"type": "string"}, "action_comment": {"type": "string"}, "active": {"type": "boolean"}, "one_shot": {"type": "boolean"}, "subroutine": {"type": "boolean"}, "easy": {"type": "boolean"}, "normal": {"type": "boolean"}, "hard": {"type": "boolean"}, "delay_seconds": {"type": "integer", "minimum": 0}, "conditions": {"type": "array", "maxItems": 512, "items": CONDITION}, "actions": {"type": "array", "maxItems": 512, "items": ACTION}, "false_actions": {"type": "array", "maxItems": 512, "items": ACTION}}, ("player_index", "name"))},
    {"name": "delete_script", "description": "Delete a top-level or grouped script.", "inputSchema": mutation_schema({"player_index": {"type": "integer", "minimum": 0}, "name": {"type": "string", "minLength": 1}, "group": {"type": "string", "minLength": 1}}, ("player_index", "name"))},
    {"name": "list_terrain_textures", "description": "List terrain texture classes available to the editor.", "inputSchema": schema()},
    {"name": "get_terrain_cells", "description": "Read base/blended texture classes, cliff/passability and flip state for a rectangle.", "inputSchema": schema({"x": {"type": "integer"}, "y": {"type": "integer"}, "width": {"type": "integer", "minimum": 1}, "height": {"type": "integer", "minimum": 1}}, ("x", "y", "width", "height"))},
    {"name": "set_terrain_cells", "description": "Paint texture classes and/or cliff passability. auto_blend updates blend layers around painted cells.", "inputSchema": mutation_schema({"cells": {"type": "array", "minItems": 1, "maxItems": 4096, "items": {"type": "object", "properties": {"x": {"type": "integer"}, "y": {"type": "integer"}, "texture_class": {"type": "integer", "minimum": 0}, "passable": {"type": "boolean"}, "auto_blend": {"type": "boolean", "default": True}}, "required": ["x", "y"], "additionalProperties": False}}}, ("cells",))},
    {"name": "list_linear_features", "description": "List road or bridge segments, including landmark bridge objects.", "inputSchema": schema({"kind": {"type": "string", "enum": ["all", "road", "bridge"], "default": "all"}})},
    {"name": "create_linear_feature", "description": "Create a road/bridge segment from two endpoints. A bridge template marked as a landmark is placed as one object.", "inputSchema": mutation_schema({"kind": {"type": "string", "enum": ["road", "bridge"]}, "template": {"type": "string", "minLength": 1}, "start": POINT, "end": POINT, "corner": {"type": "string", "enum": ["smooth", "angled", "tight"], "default": "smooth"}}, ("kind", "template", "start", "end"))},
    {"name": "delete_linear_feature", "description": "Delete a road/bridge segment by its first endpoint/object id.", "inputSchema": mutation_schema({"feature_id": {"oneOf": [{"type": "string"}, {"type": "integer"}]}}, ("feature_id",))},
    {"name": "get_playable_areas", "description": "Read playable-area boundaries.", "inputSchema": schema()},
    {"name": "set_playable_areas", "description": "Replace playable-area boundaries. Each boundary is a top-right cell coordinate; lower-left is (0,0).", "inputSchema": mutation_schema({"boundaries": {"type": "array", "minItems": 1, "maxItems": 64, "items": {"type": "object", "properties": {"x": {"type": "integer", "minimum": 1}, "y": {"type": "integer", "minimum": 1}}, "required": ["x", "y"], "additionalProperties": False}}}, ("boundaries",))},
    {"name": "get_map_settings", "description": "Read map title/description/player count, time of day, weather, water plane and global lighting.", "inputSchema": schema()},
    {"name": "update_map_settings", "description": "Update map metadata, environment and selected global-light values.", "inputSchema": mutation_schema({"name": {"type": "string"}, "description": {"type": "string"}, "player_count": {"type": "integer", "minimum": 1, "maximum": 8}, "time_of_day": {"type": "integer", "minimum": 1, "maximum": 4}, "weather": {"type": "integer", "minimum": 0}, "water_height": {"type": "number"}, "lighting_target": {"type": "string", "enum": ["terrain", "objects"]}, "light_index": {"type": "integer", "minimum": 0, "maximum": 2}, "ambient": POINT, "diffuse": POINT, "direction": POINT}, ())},
    {"name": "generate_preview", "description": "Generate/overwrite the current map's .tga minimap preview.", "inputSchema": schema({**MUTATION})},
    {"name": "capture_view", "description": "Capture the current WorldBuilder client view to a BMP file and return its absolute path.", "inputSchema": schema({"path": {"type": "string", "description": "Optional absolute .bmp path under the system temp directory."}})},
]


EXTRA_COMMANDS = {name: name for name in (
    "new_map", "close_map", "list_waypoints", "create_waypoint", "update_waypoint",
    "delete_waypoint", "connect_waypoints", "disconnect_waypoints", "list_areas",
    "create_area", "update_area", "delete_area", "list_script_types", "list_sciences", "list_upgrades", "list_scripts", "get_script",
    "upsert_script", "delete_script", "list_terrain_textures", "get_terrain_cells",
    "set_terrain_cells", "list_linear_features", "create_linear_feature",
    "delete_linear_feature", "get_playable_areas", "set_playable_areas",
    "get_map_settings", "update_map_settings", "generate_preview", "capture_view",
)}

EXTRA_ALLOWED = {
    tool["name"]: set(tool["inputSchema"].get("properties", {}))
    for tool in EXTRA_TOOL_DEFINITIONS
}
EXTRA_REQUIRED = {
    tool["name"]: set(tool["inputSchema"].get("required", []))
    for tool in EXTRA_TOOL_DEFINITIONS
    if tool["inputSchema"].get("required")
}


def _number(value: Any, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise BridgeError("invalid_arguments", f"{field} must be a finite number.")
    return float(value)


def _integer(value: Any, field: str, minimum: int | None = None) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise BridgeError("invalid_arguments", f"{field} must be an integer.")
    if minimum is not None and value < minimum:
        raise BridgeError("invalid_arguments", f"{field} must be at least {minimum}.")
    return value


def _flatten_point(result: dict[str, Any], prefix: str, point: Any) -> None:
    if not isinstance(point, Mapping) or "x" not in point or "y" not in point:
        raise BridgeError("invalid_arguments", f"{prefix} must contain x and y.")
    result[f"{prefix}_x"] = _number(point["x"], f"{prefix}.x")
    result[f"{prefix}_y"] = _number(point["y"], f"{prefix}.y")
    result[f"{prefix}_z"] = _number(point.get("z", 0), f"{prefix}.z")


def _flatten_parameters(result: dict[str, Any], prefix: str, values: Any) -> None:
    if values is None:
        values = []
    if not isinstance(values, list) or len(values) > 12:
        raise BridgeError("invalid_arguments", f"{prefix} parameters must be an array of at most 12 values.")
    result[f"{prefix}_param_count"] = len(values)
    for index, value in enumerate(values):
        key = f"{prefix}_param{index}"
        if isinstance(value, bool):
            result[key] = 1 if value else 0
        elif isinstance(value, str):
            result[key] = value
        elif isinstance(value, int) and not isinstance(value, bool):
            result[key] = value
        elif isinstance(value, float):
            result[key] = _number(value, key)
        elif isinstance(value, Mapping):
            _flatten_point(result, key, value)
        else:
            raise BridgeError("invalid_arguments", f"{key} has an unsupported value.")


def validate_and_flatten(name: str, arguments: dict[str, Any]) -> dict[str, Any]:
    """Validate nested extended inputs and adapt them to tabular bridge fields."""

    if name == "update_waypoint" and not (
        set(arguments) - {"editor_id", "waypoint_id", "expected_revision", "operation_id"}
    ):
        raise BridgeError("invalid_arguments", "update_waypoint needs at least one changed field.")

    if name == "update_area" and not (
        set(arguments) - {"editor_id", "area_id", "expected_revision", "operation_id"}
    ):
        raise BridgeError("invalid_arguments", "update_area needs at least one changed field.")

    if name == "update_map_settings" and not (
        set(arguments) - {"editor_id", "expected_revision", "operation_id"}
    ):
        raise BridgeError("invalid_arguments", "update_map_settings needs at least one changed field.")

    if name in {"create_area", "update_area"} and "points" in arguments:
        points = arguments.pop("points")
        if not isinstance(points, list) or not 3 <= len(points) <= 256:
            raise BridgeError("invalid_arguments", "points must contain 3 to 256 points.")
        arguments["point_count"] = len(points)
        for index, point in enumerate(points):
            _flatten_point(arguments, f"point{index}", point)

    if name == "set_terrain_cells":
        cells = arguments.pop("cells")
        if not isinstance(cells, list) or not 1 <= len(cells) <= 4096:
            raise BridgeError("invalid_arguments", "cells must contain 1 to 4096 entries.")
        arguments["cell_count"] = len(cells)
        for index, cell in enumerate(cells):
            if not isinstance(cell, Mapping):
                raise BridgeError("invalid_arguments", f"cells[{index}] must be an object.")
            for key in ("x", "y"):
                if key not in cell:
                    raise BridgeError("invalid_arguments", f"cells[{index}].{key} is required.")
                arguments[f"cell{index}_{key}"] = _integer(cell[key], f"cells[{index}].{key}")
            for key in ("texture_class", "passable", "auto_blend"):
                if key in cell:
                    value = cell[key]
                    if key == "texture_class":
                        value = _integer(value, f"cells[{index}].{key}", 0)
                    elif not isinstance(value, bool):
                        raise BridgeError("invalid_arguments", f"cells[{index}].{key} must be boolean.")
                    arguments[f"cell{index}_{key}"] = value

    if name == "set_playable_areas":
        boundaries = arguments.pop("boundaries")
        if not isinstance(boundaries, list) or not 1 <= len(boundaries) <= 64:
            raise BridgeError("invalid_arguments", "boundaries must contain 1 to 64 entries.")
        arguments["boundary_count"] = len(boundaries)
        for index, boundary in enumerate(boundaries):
            if not isinstance(boundary, Mapping):
                raise BridgeError("invalid_arguments", f"boundaries[{index}] must be an object.")
            arguments[f"boundary{index}_x"] = _integer(boundary.get("x"), f"boundaries[{index}].x", 1)
            arguments[f"boundary{index}_y"] = _integer(boundary.get("y"), f"boundaries[{index}].y", 1)

    if name == "create_linear_feature":
        _flatten_point(arguments, "start", arguments.pop("start"))
        _flatten_point(arguments, "end", arguments.pop("end"))

    if name == "update_map_settings":
        for key in ("ambient", "diffuse", "direction"):
            if key in arguments:
                _flatten_point(arguments, key, arguments.pop(key))

    if name == "upsert_script":
        for collection, prefix in (("conditions", "condition"), ("actions", "action"), ("false_actions", "false_action")):
            items = arguments.pop(collection, [])
            if not isinstance(items, list) or len(items) > 512:
                raise BridgeError("invalid_arguments", f"{collection} must contain at most 512 entries.")
            arguments[f"{prefix}_count"] = len(items)
            for index, item in enumerate(items):
                if not isinstance(item, Mapping) or "type" not in item:
                    raise BridgeError("invalid_arguments", f"{collection}[{index}].type is required.")
                item_prefix = f"{prefix}{index}"
                arguments[f"{item_prefix}_type"] = _integer(item["type"], f"{collection}[{index}].type", 0)
                if collection == "conditions":
                    arguments[f"{item_prefix}_or_group"] = _integer(item.get("or_group", 0), f"{collection}[{index}].or_group", 0)
                    arguments[f"{item_prefix}_custom_data"] = _integer(item.get("custom_data", 0), f"{collection}[{index}].custom_data")
                _flatten_parameters(arguments, item_prefix, item.get("parameters", []))

    return arguments
