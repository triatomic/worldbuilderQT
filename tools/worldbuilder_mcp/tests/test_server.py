from __future__ import annotations

import io
import json
import unittest
from typing import Any

from tools.worldbuilder_mcp.server import (
    InvalidToolArguments,
    McpServer,
    TOOL_DEFINITIONS,
    _validate_arguments,
    serve,
)


class RecordingBridge:
    def __init__(self) -> None:
        self.calls: list[tuple[str, dict[str, Any], Any]] = []

    def list_editors(self) -> list[dict[str, Any]]:
        return [
            {
                "id": "77",
                "hwnd": "77",
                "title": "WorldBuilder",
                "process_id": 10,
            }
        ]

    def list_blocking_dialogs(self, editor_id: Any = None) -> list[dict[str, Any]]:
        return [{"title": "WorldBuilder", "message": "Sharing violation", "buttons": ["OK"]}]

    def call(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
        editor_id: Any = None,
    ) -> dict[str, Any]:
        copied_arguments = dict(arguments or {})
        self.calls.append((command, copied_arguments, editor_id))
        return {"command": command, "arguments": copied_arguments}


class BatchRecordingBridge(RecordingBridge):
    def call(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
        editor_id: Any = None,
    ) -> dict[str, Any]:
        copied_arguments = dict(arguments or {})
        self.calls.append((command, copied_arguments, editor_id))
        count = copied_arguments["item_count"]
        revision = copied_arguments.get("expected_revision", 100) + 1
        return {
            "objects": [
                {"id": copied_arguments[f"item{index}_template"] + f" {index}"}
                for index in range(count)
            ],
            "added" if command == "add_objects" else "updated": count,
            "revision": revision,
        }


class ProgressionBridge(RecordingBridge):
    def call(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
        editor_id: Any = None,
    ) -> dict[str, Any]:
        copied_arguments = dict(arguments or {})
        self.calls.append((command, copied_arguments, editor_id))
        if command == "get_state":
            return {
                "map_loaded": True,
                "path": r"C:\Maps\Preset\Preset.map",
                "read_only": False,
                "revision": 10,
            }
        if command == "list_objects":
            return {
                "objects": [
                    {
                        "id": "AmericaInfantryRanger 1",
                        "is_unit": True,
                        "veterancy": None,
                    },
                    {
                        "id": "GLAVehicleTechnical 1",
                        "is_unit": True,
                        "veterancy": 2,
                    },
                ],
                "next_offset": None,
            }
        if command == "update_objects":
            count = copied_arguments["item_count"]
            return {
                "objects": [{"id": copied_arguments[f"item{i}_object_id"]} for i in range(count)],
                "updated": count,
                "revision": copied_arguments["expected_revision"] + 1,
            }
        if command == "list_script_types":
            return {
                "conditions": [
                    {
                        "id": 3,
                        "name": "Scripting_/ True.",
                        "internal_name": "CONDITION_TRUE",
                        "parameter_types": [],
                    }
                ],
                "actions": [
                    {
                        "id": 275,
                        "name": "Map_/Experience/Set Rank Level Limit for current Map.",
                        "internal_name": "PLAYER_SET_RANKLEVELLIMIT",
                        "parameter_types": [0],
                    },
                    {
                        "id": 274,
                        "name": "Player_/Experience/Set Rank Level.",
                        "internal_name": "PLAYER_SET_RANKLEVEL",
                        "parameter_types": [11, 0],
                    },
                    {
                        "id": 330,
                        "name": "Player_/Science/Grant all player upgrades immediately.",
                        "internal_name": "PLAYER_GRANT_ALL_UPGRADES",
                        "parameter_types": [11],
                    },
                    {
                        "id": 331,
                        "name": "Player_/Science/Grant all general powers immediately.",
                        "internal_name": "PLAYER_GRANT_ALL_SCIENCES",
                        "parameter_types": [11],
                    },
                ],
            }
        if command == "list_sciences":
            return {
                "sciences": [
                    {"name": "SCIENCE_Rank1", "grantable": False},
                    {"name": "SCIENCE_Paradrop3", "grantable": True},
                ],
                "total": 2,
            }
        if command == "list_upgrades":
            return {
                "upgrades": [
                    {"name": "Upgrade_A", "player_scoped": True},
                    {"name": "Upgrade_B", "player_scoped": False},
                ],
                "total": 2,
            }
        if command == "list_players":
            return {
                "players": [
                    {"index": 0, "name": ""},
                    {"index": 1, "name": "PlyrCivilian"},
                ],
                "total": 2,
            }
        if command == "upsert_script":
            return {
                "name": copied_arguments["name"],
                "revision": copied_arguments["expected_revision"] + 1,
            }
        if command == "save_map":
            return {
                "path": r"C:\Maps\Preset\Preset.map",
                "revision": copied_arguments["expected_revision"],
            }
        if command == "validate_map":
            return {"valid": True, "errors": 0, "warnings": 0}
        raise AssertionError(f"Unexpected command: {command}")


class ScriptCatalogBridge(RecordingBridge):
    def call(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
        editor_id: Any = None,
    ) -> dict[str, Any]:
        self.calls.append((command, dict(arguments or {}), editor_id))
        return {
            "conditions": [],
            "actions": [
                {
                    "id": 276,
                    "name": "Grant science",
                    "parameter_types": [11, 31],
                }
            ],
        }


def request(
    identifier: int, method: str, params: dict[str, Any] | None = None
) -> dict[str, Any]:
    message: dict[str, Any] = {
        "jsonrpc": "2.0",
        "id": identifier,
        "method": method,
    }
    if params is not None:
        message["params"] = params
    return message


class McpServerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.bridge = RecordingBridge()
        self.server = McpServer(self.bridge)  # type: ignore[arg-type]

    def test_initialize_and_ping(self) -> None:
        initialized = self.server.handle_message(
            request(
                1,
                "initialize",
                {"protocolVersion": "2025-03-26", "capabilities": {}},
            )
        )
        assert initialized is not None
        self.assertEqual(
            initialized["result"]["protocolVersion"], "2025-03-26"
        )
        self.assertIn("tools", initialized["result"]["capabilities"])
        self.assertIn(
            "Player 1 means owner 1",
            initialized["result"]["instructions"],
        )
        self.assertEqual(
            self.server.handle_message(request(2, "ping"))["result"],  # type: ignore[index]
            {},
        )

    def test_all_required_tools_are_advertised(self) -> None:
        expected = {
            "list_editors",
            "list_blocking_dialogs",
            "get_bridge_info",
            "list_user_maps",
            "open_map",
            "get_map_state",
            "list_objects",
            "get_object",
            "list_templates",
            "add_object",
            "add_objects",
            "update_object",
            "update_objects",
            "delete_object",
            "delete_objects",
            "select_object",
            "list_players",
            "list_teams",
            "create_team",
            "update_team",
            "delete_team",
            "validate_map",
            "get_terrain_heights",
            "set_terrain_heights",
            "focus_view",
            "save_map",
            "save_map_as",
            "undo",
            "redo",
            "launch_editor",
            "new_map",
            "close_map",
            "list_waypoints",
            "create_waypoint",
            "update_waypoint",
            "delete_waypoint",
            "connect_waypoints",
            "disconnect_waypoints",
            "list_areas",
            "create_area",
            "update_area",
            "delete_area",
            "list_script_types",
            "list_sciences",
            "list_upgrades",
            "list_scripts",
            "get_script",
            "upsert_script",
            "delete_script",
            "list_terrain_textures",
            "get_terrain_cells",
            "set_terrain_cells",
            "list_linear_features",
            "create_linear_feature",
            "delete_linear_feature",
            "get_playable_areas",
            "set_playable_areas",
            "get_map_settings",
            "update_map_settings",
            "generate_preview",
            "capture_view",
        }
        advertised = {definition["name"] for definition in TOOL_DEFINITIONS}
        self.assertEqual(advertised, expected)

        response = self.server.handle_message(request(1, "tools/list"))
        assert response is not None
        self.assertEqual(
            {tool["name"] for tool in response["result"]["tools"]}, expected
        )
        set_heights = next(
            tool
            for tool in TOOL_DEFINITIONS
            if tool["name"] == "set_terrain_heights"
        )
        points_schema = set_heights["inputSchema"]["properties"]["points"]
        self.assertEqual(points_schema["maxItems"], 4096)
        self.assertEqual(
            points_schema["items"]["properties"]["height"],
            {
                "type": "integer",
                "minimum": 0,
                "maximum": 255,
            },
        )
        add_object = next(
            tool
            for tool in TOOL_DEFINITIONS
            if tool["name"] == "add_object"
        )
        owner_schema = add_object["inputSchema"]["properties"]["owner"]
        self.assertIn("Do not ask the user", owner_schema["description"])
        self.assertEqual(
            owner_schema["oneOf"][0],
            {
                "type": "integer",
                "minimum": 1,
                "maximum": 8,
                "description": "Preferred lobby-player form; Player 1 is 1.",
            },
        )

    def test_map_state_uses_get_state_bridge_command(self) -> None:
        response = self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "get_map_state",
                    "arguments": {"editor_id": "77"},
                },
            )
        )

        self.assertEqual(self.bridge.calls, [("get_state", {}, "77")])
        assert response is not None
        self.assertFalse(response["result"].get("isError", False))

    def test_lists_blocking_dialogs_for_selected_editor(self) -> None:
        response = self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "list_blocking_dialogs",
                    "arguments": {"editor_id": "77"},
                },
            )
        )

        assert response is not None
        self.assertEqual(
            json.loads(response["result"]["content"][0]["text"]),
            [{"title": "WorldBuilder", "message": "Sharing violation", "buttons": ["OK"]}],
        )

    def test_open_map_is_idempotent_and_routes_policy(self) -> None:
        call = request(
            1,
            "tools/call",
            {
                "name": "open_map",
                "arguments": {
                    "path": "Demo",
                    "on_unsaved": "discard",
                    "operation_id": "open-demo",
                },
            },
        )
        first = self.server.handle_message(call)
        second = self.server.handle_message(call)

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "open_map",
                    {"path": "Demo", "on_unsaved": "discard"},
                    None,
                )
            ],
        )
        self.assertEqual(first, second)

    def test_bulk_objects_are_flattened_for_native_bridge(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "add_objects",
                    "arguments": {
                        "expected_revision": 7,
                        "items": [
                            {
                                "template": "AmericaTankCrusader",
                                "x": 10,
                                "y": 20,
                                "owner": "teamPlyrAmerica",
                            },
                            {
                                "template": "AmericaVehicleTomahawk",
                                "x": 30,
                                "y": 40,
                            },
                        ],
                    },
                },
            )
        )

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "add_objects",
                    {
                        "expected_revision": 7,
                        "item_count": 2,
                        "item0_template": "AmericaTankCrusader",
                        "item0_x": 10,
                        "item0_y": 20,
                        "item0_owner": "teamPlyrAmerica",
                        "item1_template": "AmericaVehicleTomahawk",
                        "item1_x": 30,
                        "item1_y": 40,
                    },
                    None,
                )
            ],
        )

    def test_large_object_batches_are_chunked_and_revisions_chained(self) -> None:
        bridge = BatchRecordingBridge()
        server = McpServer(bridge)  # type: ignore[arg-type]
        response = server.call_tool(
            "add_objects",
            {
                "editor_id": "77",
                "expected_revision": 7,
                "items": [
                    {
                        "template": "AmericaPowerPlant",
                        "x": index,
                        "y": index + 1,
                        "owner": 1,
                    }
                    for index in range(32)
                ],
            },
        )

        self.assertEqual(len(bridge.calls), 3)
        self.assertEqual(
            [call[1]["item_count"] for call in bridge.calls], [15, 15, 2]
        )
        self.assertEqual(
            [call[1]["expected_revision"] for call in bridge.calls],
            [7, 8, 9],
        )
        self.assertEqual(bridge.calls[1][1]["item0_x"], 15)
        self.assertEqual(bridge.calls[2][1]["item1_x"], 31)
        self.assertEqual(bridge.calls[0][1]["item0_owner"], "teamplayer0")
        self.assertEqual(response["structuredContent"]["added"], 32)
        self.assertEqual(response["structuredContent"]["chunk_count"], 3)
        self.assertEqual(len(response["structuredContent"]["objects"]), 32)
        self.assertEqual(response["structuredContent"]["revision"], 10)

    def test_extended_geometry_is_flattened_for_native_bridge(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "create_area",
                    "arguments": {
                        "name": "CombatZone",
                        "kind": "scripting",
                        "layer": "Gameplay",
                        "expected_revision": 3,
                        "points": [
                            {"x": 1, "y": 2},
                            {"x": 3, "y": 4, "z": 5},
                            {"x": 6, "y": 7},
                        ],
                    },
                },
            )
        )

        command, arguments, _ = self.bridge.calls[-1]
        self.assertEqual(command, "create_area")
        self.assertEqual(arguments["layer"], "Gameplay")
        self.assertEqual(arguments["expected_revision"], 3)
        self.assertEqual(arguments["point_count"], 3)
        self.assertEqual(arguments["point1_z"], 5.0)
        self.assertEqual(arguments["point2_z"], 0.0)

        for identifier, name, call_arguments in (
            (2, "update_area", {"area_id": 7, "layer": "Water", "expected_revision": 4}),
            (3, "undo", {"expected_revision": 5}),
            (4, "redo", {"expected_revision": 6}),
        ):
            self.server.handle_message(
                request(
                    identifier,
                    "tools/call",
                    {"name": name, "arguments": call_arguments},
                )
            )

        self.assertEqual(
            [(call[0], call[1]) for call in self.bridge.calls[-3:]],
            [
                ("update_area", {"area_id": 7, "layer": "Water", "expected_revision": 4}),
                ("undo", {"expected_revision": 5}),
                ("redo", {"expected_revision": 6}),
            ],
        )

    def test_script_structure_is_flattened_for_native_bridge(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "upsert_script",
                    "arguments": {
                        "player_index": 0,
                        "name": "Intro",
                        "conditions": [
                            {"or_group": 0, "type": 3, "parameters": []}
                        ],
                        "actions": [
                            {"type": 0, "parameters": ["Hello", 7]}
                        ],
                        "false_actions": [],
                    },
                },
            )
        )

        command, arguments, _ = self.bridge.calls[-1]
        self.assertEqual(command, "upsert_script")
        self.assertEqual(arguments["condition_count"], 1)
        self.assertEqual(arguments["condition0_or_group"], 0)
        self.assertEqual(arguments["action0_param0"], "Hello")
        self.assertEqual(arguments["action0_param1"], 7)

    def test_one_based_object_owner_slot_is_normalized_for_engine(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "add_object",
                    "arguments": {
                        "template": "AmericaTankPaladin",
                        "x": 10,
                        "y": 20,
                        "owner": 1,
                    },
                },
            )
        )
        self.server.handle_message(
            request(
                2,
                "tools/call",
                {
                    "name": "update_objects",
                    "arguments": {
                        "items": [
                            {
                                "object_id": "AmericaTankPaladin 1",
                                "owner": 8,
                            }
                        ]
                    },
                },
            )
        )

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "add_object",
                    {
                        "template": "AmericaTankPaladin",
                        "x": 10,
                        "y": 20,
                        "owner": "teamplayer0",
                    },
                    None,
                ),
                (
                    "update_objects",
                    {
                        "item_count": 1,
                        "item0_object_id": "AmericaTankPaladin 1",
                        "item0_owner": "teamplayer7",
                    },
                    None,
                ),
            ],
        )

    def test_object_owner_slot_must_be_between_one_and_eight(self) -> None:
        for owner in (0, 9):
            with self.subTest(owner=owner):
                response = self.server.handle_message(
                    request(
                        owner + 10,
                        "tools/call",
                        {
                            "name": "update_object",
                            "arguments": {
                                "object_id": "AmericaTankPaladin 1",
                                "owner": owner,
                            },
                        },
                    )
                )
                assert response is not None
                self.assertTrue(response["result"]["isError"])

        self.assertEqual(self.bridge.calls, [])

    def test_object_progression_fields_are_validated_and_flattened(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "update_objects",
                    "arguments": {
                        "items": [
                            {
                                "object_id": "GLAVehicleScorpion 1",
                                "veterancy": 3,
                            }
                        ]
                    },
                },
            )
        )

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "update_objects",
                    {
                        "item_count": 1,
                        "item0_object_id": "GLAVehicleScorpion 1",
                        "item0_veterancy": 3,
                    },
                    None,
                )
            ],
        )

        for key, value in (("veterancy", 4),):
            with self.subTest(key=key):
                response = self.server.handle_message(
                    request(
                        value + 10,
                        "tools/call",
                        {
                            "name": "update_object",
                            "arguments": {
                                "object_id": "GLAVehicleScorpion 1",
                                key: value,
                            },
                        },
                    )
                )
                assert response is not None
                self.assertTrue(response["result"]["isError"])

    def test_script_names_are_validated_and_flattened(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "update_objects",
                    "arguments": {
                        "items": [
                            {
                                "object_id": "AirF_AmericaJetRaptor 1",
                                "script_name": "IronDragonUSAPatrol01",
                            }
                        ]
                    },
                },
            )
        )

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "update_objects",
                    {
                        "item_count": 1,
                        "item0_object_id": "AirF_AmericaJetRaptor 1",
                        "item0_script_name": "IronDragonUSAPatrol01",
                    },
                    None,
                )
            ],
        )

        for value in ("", "x" * 129, 7):
            with self.subTest(value=value):
                response = self.server.handle_message(
                    request(
                        2,
                        "tools/call",
                        {
                            "name": "update_object",
                            "arguments": {
                                "object_id": "AirF_AmericaJetRaptor 1",
                                "script_name": value,
                            },
                        },
                    )
                )
                assert response is not None
                self.assertTrue(response["result"]["isError"])

    def test_script_type_catalog_includes_semantic_parameter_names(self) -> None:
        server = McpServer(ScriptCatalogBridge())  # type: ignore[arg-type]
        response = server.call_tool("list_script_types", {})

        action = response["structuredContent"]["actions"][0]
        self.assertEqual(action["parameter_type_names"], ["SIDE", "SCIENCE"])
        self.assertEqual(
            action["parameters"],
            [
                {"type": 11, "type_name": "SIDE"},
                {"type": 31, "type_name": "SCIENCE"},
            ],
        )



    def test_set_heights_compacts_points(self) -> None:
        self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "set_terrain_heights",
                    "arguments": {
                        "points": [
                            {"x": 1, "y": 2, "height": 3},
                            {"x": 4, "y": 6, "height": 7},
                        ]
                    },
                },
            )
        )

        self.assertEqual(
            self.bridge.calls,
            [
                (
                    "set_terrain_heights",
                    {"points": "1,2,3;4,6,7"},
                    None,
                )
            ],
        )

    def test_invalid_update_is_a_tool_error(self) -> None:
        response = self.server.handle_message(
            request(
                1,
                "tools/call",
                {
                    "name": "update_object",
                    "arguments": {"object_id": 42},
                },
            )
        )

        assert response is not None
        result = response["result"]
        self.assertTrue(result["isError"])
        error_payload = json.loads(result["content"][0]["text"])
        self.assertEqual(error_payload["error"]["code"], "invalid_arguments")

    def test_resources_and_prompts(self) -> None:
        listed = self.server.handle_message(request(1, "resources/list"))
        assert listed is not None
        uri = listed["result"]["resources"][0]["uri"]
        read = self.server.handle_message(
            request(2, "resources/read", {"uri": uri})
        )
        assert read is not None
        resource_payload = json.loads(
            read["result"]["contents"][0]["text"]
        )
        self.assertEqual(resource_payload["editors"][0]["id"], "77")

        guide_uri = next(
            resource["uri"]
            for resource in listed["result"]["resources"]
            if resource["uri"] == "worldbuilder://scripting-guide"
        )
        guide = self.server.handle_message(
            request(4, "resources/read", {"uri": guide_uri})
        )
        assert guide is not None
        guide_content = guide["result"]["contents"][0]
        self.assertEqual(guide_content["mimeType"], "text/markdown")
        self.assertIn("Empty conditions", guide_content["text"])

        prompts = self.server.handle_message(request(3, "prompts/list"))
        assert prompts is not None
        self.assertEqual(prompts["result"], {"prompts": []})

    def test_notifications_produce_no_response(self) -> None:
        self.assertIsNone(
            self.server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "method": "notifications/initialized",
                }
            )
        )

    def test_newline_stdio_transport(self) -> None:
        messages = [
            request(1, "ping"),
            {
                "jsonrpc": "2.0",
                "method": "notifications/initialized",
            },
            request(2, "tools/call", {"name": "list_editors"}),
        ]
        input_bytes = b"".join(
            json.dumps(message).encode("utf-8") + b"\n"
            for message in messages
        )
        output = io.BytesIO()

        serve(io.BytesIO(input_bytes), output, self.server)

        responses = [
            json.loads(line)
            for line in output.getvalue().decode("utf-8").splitlines()
        ]
        self.assertEqual([response["id"] for response in responses], [1, 2])
        editors = json.loads(
            responses[1]["result"]["content"][0]["text"]
        )
        self.assertEqual(editors[0]["id"], "77")

    def test_content_length_transport(self) -> None:
        payload = json.dumps(
            request(5, "initialize", {"protocolVersion": "тест"}),
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        framed = (
            f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
            + payload
        )
        output = io.BytesIO()

        serve(io.BytesIO(framed), output, self.server)

        headers, response_payload = output.getvalue().split(b"\r\n\r\n", 1)
        length = int(headers.split(b":", 1)[1].strip())
        self.assertEqual(length, len(response_payload))
        response = json.loads(response_payload)
        self.assertEqual(response["result"]["protocolVersion"], "2024-11-05")


class DeclaredSchemaValidationTests(unittest.TestCase):
    """Limits a tool advertises in its schema must actually be enforced."""

    def test_out_of_range_and_unknown_enum_values_are_rejected(self) -> None:
        cases = (
            ("update_map_settings", {"time_of_day": 99}),
            ("update_map_settings", {"light_index": 77}),
            ("update_map_settings", {"player_count": 0}),
            ("new_map", {"width": 1, "height": 100,
                         "default_height": 10, "border_size": 5}),
            ("new_map", {"width": 100, "height": 100,
                         "default_height": 999, "border_size": 5}),
            ("new_map", {"width": 100, "height": 100,
                         "default_height": 10, "border_size": -5}),
            ("upsert_script", {"player_index": -4, "name": "s"}),
            ("connect_waypoints", {"from_id": 0, "to_id": 1}),
            ("launch_editor", {"wait_ms": 999_999_999}),
            ("create_area", {"kind": "bogus", "name": "a", "points": [
                {"x": 1, "y": 1}, {"x": 2, "y": 2}, {"x": 3, "y": 3}]}),
        )
        for name, arguments in cases:
            with self.subTest(tool=name, arguments=arguments):
                with self.assertRaises(InvalidToolArguments):
                    _validate_arguments(name, dict(arguments))

    def test_values_inside_the_declared_range_are_accepted(self) -> None:
        cases = (
            ("update_map_settings", {"time_of_day": 2}),
            ("new_map", {"width": 100, "height": 100,
                         "default_height": 10, "border_size": 5}),
            ("connect_waypoints", {"from_id": 1, "to_id": 2}),
            ("create_area", {"kind": "water", "name": "a", "points": [
                {"x": 1, "y": 1}, {"x": 2, "y": 2}, {"x": 3, "y": 3}]}),
        )
        for name, arguments in cases:
            with self.subTest(tool=name, arguments=arguments):
                _validate_arguments(name, dict(arguments))


if __name__ == "__main__":
    unittest.main()
