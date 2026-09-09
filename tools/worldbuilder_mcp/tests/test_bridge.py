from __future__ import annotations

import ctypes
import json
import tempfile
import threading
import time
import unittest
from pathlib import Path
from typing import Any, Callable
from unittest.mock import patch
from urllib.parse import unquote

from tools.worldbuilder_mcp.bridge import (
    BridgeError,
    COPYDATA_MAGIC,
    EditorWindow,
    RemoteError,
    Win32WindowApi,
    WorldBuilderBridge,
    format_height_points,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


class FakeWindowApi:
    def __init__(
        self,
        editors: list[EditorWindow] | None = None,
        response: dict[str, Any] | Callable[[int, dict[str, str]], dict[str, Any]] | None = None,
        dialogs: list[dict[str, Any]] | None = None,
    ) -> None:
        self.editors = (
            list(editors)
            if editors is not None
            else [EditorWindow(handle=12345, title="WorldBuilder", process_id=99)]
        )
        self.response = response
        self.dialogs = list(dialogs or [])
        self.requests: list[dict[str, str]] = []
        self.sent: list[tuple[int, int, int]] = []

    def list_editors(self) -> list[EditorWindow]:
        return list(self.editors)

    def list_blocking_dialogs(self, process_id: int) -> list[dict[str, Any]]:
        if process_id != self.editors[0].process_id:
            return []
        return list(self.dialogs)

    def send_request(
        self, handle: int, request_path: Path, magic: int, timeout_ms: int
    ) -> None:
        fields: dict[str, str] = {}
        for line in request_path.read_text(encoding="utf-8").splitlines():
            key, encoded_value = line.split("\t", 1)
            fields[key] = unquote(encoded_value)
        self.requests.append(fields)
        self.sent.append((handle, magic, timeout_ms))
        response = (
            self.response(handle, fields)
            if callable(self.response)
            else self.response
        )
        if response is None:
            response = {"ok": True, "result": {"accepted": True}}
        Path(fields["response_path"]).write_text(
            json.dumps(response, ensure_ascii=False), encoding="utf-8"
        )


class BridgeTests(unittest.TestCase):
    def test_lists_editors_with_string_pointer_ids(self) -> None:
        api = FakeWindowApi()
        bridge = WorldBuilderBridge(api=api)

        self.assertEqual(
            bridge.list_editors(),
            [
                {
                    "id": "12345",
                    "hwnd": "12345",
                    "title": "WorldBuilder",
                    "process_id": 99,
                    "bridge_version": 1,
                }
            ],
        )

    def test_lists_blocking_dialogs_for_selected_editor(self) -> None:
        dialogs = [
            {
                "hwnd": "777",
                "title": "WorldBuilder",
                "message": "Encountered a sharing violation.",
                "buttons": ["OK"],
            }
        ]
        bridge = WorldBuilderBridge(api=FakeWindowApi(dialogs=dialogs))

        self.assertEqual(bridge.list_blocking_dialogs("12345"), dialogs)

    def test_launch_editor_reuses_only_matching_edition(self) -> None:
        api = FakeWindowApi(
            editors=[
                EditorWindow(100, "Generals", 1, 7),
                EditorWindow(200, "Zero Hour", 2, 7),
            ],
            response=lambda handle, _fields: {
                "ok": True,
                "result": {
                    "edition": "generals" if handle == 100 else "zero_hour"
                },
            },
        )
        bridge = WorldBuilderBridge(api=api)

        result = bridge.launch_editor("zero_hour", wait_ms=50)

        self.assertFalse(result["launched"])
        self.assertTrue(result["already_running"])
        self.assertEqual([editor["id"] for editor in result["editors"]], ["200"])
        self.assertEqual(
            [(request["command"], sent[0]) for request, sent in zip(api.requests, api.sent)],
            [("get_bridge_info", 100), ("get_bridge_info", 200)],
        )

    def test_launch_editor_requires_upgrade_for_edition_ambiguous_bridge(self) -> None:
        api = FakeWindowApi(
            editors=[EditorWindow(100, "Zero Hour", 1, 6)],
            response={"ok": True, "result": {"edition": "generals"}},
        )
        bridge = WorldBuilderBridge(api=api)

        with self.assertRaises(BridgeError) as raised:
            bridge.launch_editor("zero_hour", wait_ms=0)

        self.assertEqual(raised.exception.code, "editor_upgrade_required")
        self.assertEqual(api.requests, [])

    def test_launch_editor_zero_wait_does_not_probe_or_duplicate(self) -> None:
        class BusyProbeBridge(WorldBuilderBridge):
            def __init__(self) -> None:
                super().__init__(
                    api=FakeWindowApi(
                        editors=[EditorWindow(100, "Busy", 1, 7)]
                    )
                )
                self.probe_timeouts: list[int] = []

            def call(
                self,
                command: str,
                arguments: dict[str, Any] | None = None,
                editor_id: Any = None,
                timeout_ms: int | None = None,
            ) -> Any:
                assert command == "get_bridge_info"
                assert timeout_ms is not None
                self.probe_timeouts.append(timeout_ms)
                raise BridgeError("editor_timeout", "busy")

        bridge = BusyProbeBridge()
        with self.assertRaises(BridgeError) as raised:
            bridge.launch_editor("zero_hour", wait_ms=0)

        self.assertEqual(raised.exception.code, "editor_edition_unknown")
        self.assertEqual(bridge.probe_timeouts, [])

    def test_launch_editor_uses_visible_shell_launcher(self) -> None:
        class FakeProcess:
            pid = 4321

            def __init__(self) -> None:
                self.closed = False

            def poll(self) -> int | None:
                return None

            def close(self) -> None:
                self.closed = True

        process = FakeProcess()
        with tempfile.TemporaryDirectory() as temporary_directory:
            executable = Path(temporary_directory) / "WorldBuilderV.exe"
            executable.touch()
            bridge = WorldBuilderBridge(
                api=FakeWindowApi(editors=[]),
                editor_paths={"generals": executable},
            )
            with patch(
                "tools.worldbuilder_mcp.bridge._launch_editor_process",
                return_value=process,
            ) as launcher:
                result = bridge.launch_editor("generals", wait_ms=0)

        launcher.assert_called_once_with(executable.resolve())
        self.assertEqual(result["process_id"], 4321)
        self.assertFalse(result["bridge_ready"])
        self.assertTrue(process.closed)

    def test_launch_editor_starts_requested_edition_when_other_is_running(self) -> None:
        class FakeProcess:
            pid = 9876

            def poll(self) -> int | None:
                return None

            def close(self) -> None:
                return None

        api = FakeWindowApi(
            editors=[EditorWindow(100, "Generals", 1, 7)],
            response={"ok": True, "result": {"edition": "generals"}},
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            executable = Path(temporary_directory) / "WorldBuilderZH.exe"
            executable.touch()
            bridge = WorldBuilderBridge(
                api=api,
                editor_paths={"zero_hour": executable},
            )
            with patch(
                "tools.worldbuilder_mcp.bridge._launch_editor_process",
                return_value=FakeProcess(),
            ) as launcher:
                result = bridge.launch_editor("zero_hour", wait_ms=50)

        launcher.assert_called_once_with(executable.resolve())
        self.assertTrue(result["launched"])
        self.assertFalse(result["already_running"])
        self.assertFalse(result["bridge_ready"])
        self.assertEqual(result["editors"], [])

    def test_request_file_round_trip_and_cleanup(self) -> None:
        def response(_handle: int, fields: dict[str, str]) -> dict[str, Any]:
            return {
                "ok": True,
                "result": {
                    "command": fields["command"],
                    "filter": fields["filter"],
                },
            }

        api = FakeWindowApi(response=response)
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(
                api=api, temp_root=temporary_directory, timeout_ms=1234
            )
            result = bridge.call(
                "list_objects",
                {"filter": "坦克 100%\nline", "offset": 4},
            )

            self.assertEqual(
                result,
                {"command": "list_objects", "filter": "坦克 100%\nline"},
            )
            self.assertEqual(api.sent, [(12345, COPYDATA_MAGIC, 1234)])
            request = api.requests[0]
            self.assertEqual(request["command"], "list_objects")
            self.assertEqual(request["filter"], "坦克 100%\nline")
            self.assertEqual(request["offset"], "4")
            self.assertTrue(Path(request["response_path"]).is_absolute())
            request_directory = Path(temporary_directory) / "GeneralsWorldBuilderMcp"
            self.assertEqual(list(request_directory.iterdir()), [])

    def test_retries_partial_response_file_until_native_write_finishes(self) -> None:
        class RacingResponseApi(FakeWindowApi):
            def __init__(self) -> None:
                super().__init__()
                self.writer: threading.Thread | None = None

            def send_request(
                self, handle: int, request_path: Path, magic: int, timeout_ms: int
            ) -> None:
                fields: dict[str, str] = {}
                for line in request_path.read_text(encoding="utf-8").splitlines():
                    key, encoded_value = line.split("\t", 1)
                    fields[key] = unquote(encoded_value)
                self.requests.append(fields)
                self.sent.append((handle, magic, timeout_ms))

                def write_response() -> None:
                    with Path(fields["response_path"]).open(
                        "x", encoding="utf-8"
                    ) as stream:
                        stream.write('{"ok":')
                        stream.flush()
                        time.sleep(0.03)
                        stream.write('true,"result":{"accepted":true}}')

                self.writer = threading.Thread(target=write_response)
                self.writer.start()

        api = RacingResponseApi()
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(
                api=api, temp_root=temporary_directory, timeout_ms=1000
            )
            result = bridge.call("get_state")
            assert api.writer is not None
            api.writer.join()

        self.assertEqual(result, {"accepted": True})

    def test_win32_send_rejects_false_window_result_immediately(self) -> None:
        class CopyDataStruct(ctypes.Structure):
            _fields_ = [
                ("dwData", ctypes.c_size_t),
                ("cbData", ctypes.c_ulong),
                ("lpData", ctypes.c_void_p),
            ]

        class RejectingUser32:
            def IsWindow(self, _handle: int) -> int:
                return 1

            def GetPropW(self, _handle: int, _name: str) -> int:
                return 1

            def SendMessageTimeoutW(self, *_arguments: Any) -> int:
                return 1

        api = Win32WindowApi.__new__(Win32WindowApi)
        api._ctypes = ctypes
        api._user32 = RejectingUser32()
        api._copydata_struct = CopyDataStruct

        with self.assertRaises(BridgeError) as raised:
            api.send_request(123, Path("request"), COPYDATA_MAGIC, 1000)

        self.assertEqual(raised.exception.code, "send_failed")

    def test_long_map_commands_use_long_timeout(self) -> None:
        api = FakeWindowApi()
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(
                api=api, temp_root=temporary_directory, timeout_ms=1234
            )
            bridge.call("open_map", {"path": "Demo"})

        self.assertEqual(api.sent, [(12345, COPYDATA_MAGIC, 120_000)])

    def test_object_batches_use_extended_timeout(self) -> None:
        api = FakeWindowApi()
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(
                api=api, temp_root=temporary_directory, timeout_ms=1234
            )
            bridge.call("add_objects", {"item_count": 1})

        self.assertEqual(api.sent, [(12345, COPYDATA_MAGIC, 45_000)])

    def test_full_map_mutations_use_extended_timeout(self) -> None:
        api = FakeWindowApi()
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(
                api=api, temp_root=temporary_directory, timeout_ms=1234
            )
            for command in (
                "set_terrain_heights",
                "set_terrain_cells",
                "set_playable_areas",
            ):
                bridge.call(command)

        self.assertEqual(
            api.sent,
            [(12345, COPYDATA_MAGIC, 45_000)] * 3,
        )

    def test_selects_requested_editor(self) -> None:
        api = FakeWindowApi(
            editors=[
                EditorWindow(100, "First", 1),
                EditorWindow(200, "Second", 2),
            ]
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(api=api, temp_root=temporary_directory)
            bridge.call("save_map", editor_id="0xc8")

        self.assertEqual(api.sent[0][0], 200)

    def test_requires_id_when_multiple_editors_exist(self) -> None:
        api = FakeWindowApi(
            editors=[
                EditorWindow(100, "First", 1),
                EditorWindow(200, "Second", 2),
            ]
        )
        bridge = WorldBuilderBridge(api=api)

        with self.assertRaisesRegex(BridgeError, "Multiple"):
            bridge.call("save_map")

    def test_remote_error_envelope_is_preserved(self) -> None:
        api = FakeWindowApi(
            response={
                "ok": False,
                "error": {
                    "code": "map_read_only",
                    "message": "Map is read-only.",
                    "data": {"map": "Demo"},
                },
            }
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            bridge = WorldBuilderBridge(api=api, temp_root=temporary_directory)
            with self.assertRaises(RemoteError) as raised:
                bridge.call("save_map")

        self.assertEqual(raised.exception.code, "map_read_only")
        self.assertEqual(raised.exception.data, {"map": "Demo"})

    def test_formats_height_points(self) -> None:
        self.assertEqual(
            format_height_points(
                [
                    {"x": 1, "y": 2, "height": 3},
                    {"x": -4, "y": 5, "height": 255},
                ]
            ),
            "1,2,3;-4,5,255",
        )

    def test_rejects_invalid_height_points(self) -> None:
        invalid_points = (
            [{"x": 1.5, "y": 2, "height": 3}],
            [{"x": 1, "y": 2, "height": 256}],
            [{"x": 1, "y": 2, "height": True}],
        )
        for points in invalid_points:
            with self.subTest(points=points), self.assertRaises(BridgeError):
                format_height_points(points)

        with self.assertRaisesRegex(BridgeError, "at most 4096"):
            format_height_points(
                [{"x": 1, "y": 2, "height": 3}] * 4097
            )


class NativeZeroHourParityTests(unittest.TestCase):
    def source(self, relative_path: str) -> str:
        return (REPOSITORY_ROOT / relative_path).read_text(encoding="utf-8")

    def test_bridge_reports_zero_hour_and_round_trips_area_layers(self) -> None:
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        self.assertIn('const char BRIDGE_EDITION[] = "zero_hour";', bridge)
        self.assertIn('fields.find("layer")', bridge)
        self.assertIn('polygon->getLayerName()', bridge)
        self.assertIn('polygon->setLayerName(state.layerName)', bridge)
        self.assertIn("internal_name", bridge)
        self.assertIn(
            "TheLayersList->changePolygonTriggerLayer(m_polygon, state.layerName)",
            bridge,
        )
        undoables = self.source(
            "GeneralsMD/Code/Tools/WorldBuilder/src/CUndoable.cpp"
        )
        self.assertGreaterEqual(
            undoables.count(
                "addPolygonTriggerToLayersList(m_trigger, m_trigger->getLayerName())"
            ),
            2,
        )

    def test_new_map_checks_revision_before_resetting_existing_document(self) -> None:
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        new_map = bridge[
            bridge.index("CommandResult NewMap") : bridge.index(
                "CommandResult CloseMap"
            )
        ]
        self.assertLess(
            new_map.index("CheckExpectedRevision"),
            new_map.index("OpenDocumentFile"),
        )
        self.assertLess(
            new_map.index("CheckExpectedRevision"),
            new_map.index("HandleUnsaved"),
        )

    def test_automation_new_map_clears_path_and_restores_dialog_mode(self) -> None:
        # This fork ships the Zero Hour editor only; Generals is not wired for MCP.
        for path in (
            "GeneralsMD/Code/Tools/WorldBuilder/src/WorldBuilderDoc.cpp",
        ):
            with self.subTest(path=path):
                document = self.source(path)
                automation = document[
                    document.index("Bool CWorldBuilderDoc::createMapForAutomation") :
                    document.index("void CWorldBuilderDoc::invalObject")
                ]
                self.assertIn("m_strPathName.Empty();", automation)
                self.assertNotIn('SetPathName(_T(""), FALSE);', automation)

        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        new_map = bridge[
            bridge.index("CommandResult NewMap") : bridge.index(
                "CommandResult CloseMap"
            )
        ]
        self.assertIn("catch (...)", new_map)
        self.assertEqual(new_map.count("setAutomationNewDocument(false)"), 2)
        self.assertLess(
            new_map.index("catch (...)"),
            new_map.index("setAutomationNewDocument(false)", new_map.index("catch (...)")),
        )

    def test_native_mutations_return_chainable_revisions(self) -> None:
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        for name, following, expected in (
            ("AddObject", "IndexedField", "ObjectToJson(object, document)"),
            ("UpdateObject", "UpdateObjects", "ObjectToJson(object, document)"),
            ("DeleteObject", "DeleteObjects", '\\"revision\\"'),
            ("SetTerrainHeights", "FocusView", '\\"revision\\"'),
        ):
            with self.subTest(name=name):
                function = bridge[
                    bridge.index(f"CommandResult {name}") : bridge.index(
                        following, bridge.index(f"CommandResult {name}")
                    )
                ]
                self.assertIn(expected, function)

    def test_template_listing_exposes_native_geometry_for_layout_audits(self) -> None:
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        geometry = bridge[
            bridge.index("std::string GeometryToJson") : bridge.index(
                "CommandResult ListTemplates", bridge.index("std::string GeometryToJson")
            )
        ]
        for field in (
            "getGeomType()",
            "getMajorRadius()",
            "getMinorRadius()",
            "getBoundingCircleRadius()",
            "getBoundingSphereRadius()",
            "getFootprintArea()",
            "getMaxHeightAbovePosition()",
            "getMaxHeightBelowPosition()",
        ):
            self.assertIn(field, geometry)
        templates = bridge[
            bridge.index("CommandResult ListTemplates") : bridge.index(
                "CommandResult AddObject", bridge.index("CommandResult ListTemplates")
            )
        ]
        self.assertIn("GeometryToJson(thing->getTemplateGeometryInfo())", templates)
        for field in (
            "bounding_circle_radius",
            "major_radius",
            "minor_radius",
            "max_height_above_position",
            "max_height_below_position",
        ):
            self.assertIn(field, geometry)

    def test_native_shutdown_and_edition_contracts_are_explicit(self) -> None:
        header = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.h")
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        document = self.source(
            "GeneralsMD/Code/Tools/WorldBuilder/src/WorldBuilderDoc.cpp"
        )
        self.assertIn("const ULONG_PTR BRIDGE_VERSION = 8;", header)
        self.assertLess(
            bridge.index('#include "resource.h"'),
            bridge.index('#include "LayersList.h"'),
        )
        self.assertIn("PeekMessageW", bridge)
        self.assertIn("CancelQueuedRequest", bridge)
        self.assertIn('"request_cancelled"', bridge)
        self.assertIn("delete request_path;", bridge)
        self.assertIn("HasUnsupportedAreaLayer(fields)", bridge)
        self.assertIn("layer is supported only by Zero Hour WorldBuilder", bridge)
        new_document = document[
            document.index("BOOL CWorldBuilderDoc::OnNewDocument") : document.index(
                "void CWorldBuilderDoc::setAutomationNewDocument"
            )
        ]
        self.assertEqual(new_document.count("++m_changeSerial"), 1)
        self.assertIn("Bool regular_reset = !firstTime && !gAutomationNewDocument;", new_document)

    def test_script_name_contract_targets_named_script_objects(self) -> None:
        bridge = self.source("Core/Tools/WorldBuilderMcp/WorldBuilderMcpBridge.cpp")
        keys = self.source(
            "GeneralsMD/Code/GameEngine/Include/Common/WellKnownKeys.h"
        )
        update = bridge[
            bridge.index("CommandResult UpdateObject") : bridge.index(
                "CommandResult UpdateObjects", bridge.index("CommandResult UpdateObject")
            )
        ]
        batch = bridge[
            bridge.index("CommandResult UpdateObjects") : bridge.index(
                "CommandResult DeleteObject", bridge.index("CommandResult UpdateObjects")
            )
        ]
        self.assertIn("DEFINE_KEY(objectName)", keys)
        self.assertIn("\\\"script_name\\\"", bridge)
        self.assertIn("IsUniqueScriptName", update)
        self.assertIn("TheKey_objectName", bridge)
        self.assertIn("duplicate_script_name", batch)
        self.assertIn("script_names", batch)




if __name__ == "__main__":
    unittest.main()
