"""Windows bridge used by the WorldBuilder MCP server.

The game-side plugin exposes a marker property on its top-level window.  A
request is written to a temporary file and the path is delivered synchronously
with WM_COPYDATA.  Keeping the command payload in a file avoids the rather low
practical size limits of window messages.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping, Protocol
from urllib.parse import quote


WINDOW_PROPERTY = "GeneralsWorldBuilderMcp"
COPYDATA_MAGIC = 0x57424D43
WM_COPYDATA = 0x004A
DEFAULT_TIMEOUT_MS = 5_000
BATCH_COMMAND_TIMEOUT_MS = 45_000
LONG_COMMAND_TIMEOUT_MS = 120_000
BATCH_COMMANDS = frozenset(
    {
        "add_objects",
        "set_playable_areas",
        "set_terrain_cells",
        "set_terrain_heights",
        "update_objects",
    }
)
LONG_COMMANDS = frozenset(
    {
        "open_map",
        "save_map",
        "save_map_as",
        "new_map",
        "close_map",
        "generate_preview",
        "capture_view",
    }
)
MAX_RESPONSE_BYTES = 32 * 1024 * 1024
EDITION_PROBE_TIMEOUT_MS = 250
EDITION_AWARE_BRIDGE_VERSION = 7
_KEY_PATTERN = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")


class BridgeError(RuntimeError):
    """A bridge error with a stable machine-readable code."""

    def __init__(self, code: str, message: str, data: Any = None) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data


class RemoteError(BridgeError):
    """An error reported by the WorldBuilder side of the bridge."""


class _LaunchedProcess(Protocol):
    """Minimal process surface needed while waiting for bridge discovery."""

    @property
    def pid(self) -> int:
        """Return the launched process id."""

    def poll(self) -> int | None:
        """Return the exit code, or None while the process is running."""

    def close(self) -> None:
        """Release local process resources without terminating the editor."""


class _PopenProcess:
    def __init__(self, process: subprocess.Popen[bytes]) -> None:
        self._process = process

    @property
    def pid(self) -> int:
        return self._process.pid

    def poll(self) -> int | None:
        return self._process.poll()

    def close(self) -> None:
        return None


class _ShellExecuteProcess:
    def __init__(self, kernel32: Any, handle: int, process_id: int) -> None:
        self._kernel32 = kernel32
        self._handle = handle
        self._process_id = process_id

    @property
    def pid(self) -> int:
        return self._process_id

    def poll(self) -> int | None:
        if not self._handle:
            return None
        import ctypes
        from ctypes import wintypes

        exit_code = wintypes.DWORD()
        if not self._kernel32.GetExitCodeProcess(
            self._handle, ctypes.byref(exit_code)
        ):
            return None
        return None if exit_code.value == 259 else int(exit_code.value)

    def close(self) -> None:
        if self._handle:
            self._kernel32.CloseHandle(self._handle)
            self._handle = 0


def _launch_editor_process(executable: Path) -> _LaunchedProcess:
    """Launch a GUI editor visibly through the Windows shell."""

    if os.name != "nt":
        process = subprocess.Popen(
            [str(executable)], cwd=str(executable.parent), close_fds=True
        )
        return _PopenProcess(process)

    import ctypes
    from ctypes import wintypes

    shell32 = ctypes.WinDLL("shell32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    class ShellExecuteInfo(ctypes.Structure):
        _fields_ = [
            ("cbSize", wintypes.DWORD),
            ("fMask", wintypes.ULONG),
            ("hwnd", wintypes.HWND),
            ("lpVerb", wintypes.LPCWSTR),
            ("lpFile", wintypes.LPCWSTR),
            ("lpParameters", wintypes.LPCWSTR),
            ("lpDirectory", wintypes.LPCWSTR),
            ("nShow", ctypes.c_int),
            ("hInstApp", wintypes.HINSTANCE),
            ("lpIDList", wintypes.LPVOID),
            ("lpClass", wintypes.LPCWSTR),
            ("hkeyClass", wintypes.HANDLE),
            ("dwHotKey", wintypes.DWORD),
            ("hMonitor", wintypes.HANDLE),
            ("hProcess", wintypes.HANDLE),
        ]

    shell32.ShellExecuteExW.argtypes = [ctypes.POINTER(ShellExecuteInfo)]
    shell32.ShellExecuteExW.restype = wintypes.BOOL
    kernel32.GetProcessId.argtypes = [wintypes.HANDLE]
    kernel32.GetProcessId.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.DWORD),
    ]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    info = ShellExecuteInfo()
    info.cbSize = ctypes.sizeof(info)
    info.fMask = 0x00000040 | 0x00000400  # NOCLOSEPROCESS | FLAG_NO_UI
    info.lpVerb = "open"
    info.lpFile = str(executable)
    info.lpDirectory = str(executable.parent)
    info.nShow = 5  # SW_SHOW: attach the GUI to the visible desktop.

    ctypes.set_last_error(0)
    if not shell32.ShellExecuteExW(ctypes.byref(info)):
        error = ctypes.get_last_error()
        raise OSError(error, f"ShellExecuteExW failed with Win32 error {error}")
    if not info.hProcess:
        raise OSError("ShellExecuteExW returned no process handle")
    process_id = int(kernel32.GetProcessId(info.hProcess))
    if not process_id:
        error = ctypes.get_last_error()
        kernel32.CloseHandle(info.hProcess)
        raise OSError(error, f"GetProcessId failed with Win32 error {error}")
    return _ShellExecuteProcess(kernel32, int(info.hProcess), process_id)


@dataclass(frozen=True)
class EditorWindow:
    handle: int
    title: str
    process_id: int
    property_value: int = 1

    @property
    def editor_id(self) -> str:
        # A string prevents loss of precision in JSON implementations whose
        # number type cannot exactly represent a pointer-sized integer.
        return str(self.handle)

    def as_dict(self) -> dict[str, Any]:
        return {
            "id": self.editor_id,
            "hwnd": self.editor_id,
            "title": self.title,
            "process_id": self.process_id,
            "bridge_version": self.property_value,
        }


class WindowApi(Protocol):
    def list_editors(self) -> list[EditorWindow]:
        """Return all top-level windows carrying WINDOW_PROPERTY."""

    def list_blocking_dialogs(self, process_id: int) -> list[dict[str, Any]]:
        """Return visible message dialogs owned by one editor process."""

    def send_request(
        self, handle: int, request_path: Path, magic: int, timeout_ms: int
    ) -> None:
        """Deliver a request path to a WorldBuilder window."""


class Win32WindowApi:
    """Small ctypes-only wrapper around the required User32 calls."""

    def __init__(self) -> None:
        if os.name != "nt":
            raise BridgeError(
                "unsupported_platform",
                "The WorldBuilder window bridge is available only on Windows.",
            )

        # Imports stay local so this module remains importable and testable on
        # non-Windows hosts.
        import ctypes
        from ctypes import wintypes

        self._ctypes = ctypes
        self._wintypes = wintypes
        self._user32 = ctypes.WinDLL("user32", use_last_error=True)
        self._enum_callback_type = ctypes.WINFUNCTYPE(
            wintypes.BOOL, wintypes.HWND, wintypes.LPARAM
        )

        self._user32.EnumWindows.argtypes = [
            self._enum_callback_type,
            wintypes.LPARAM,
        ]
        self._user32.EnumWindows.restype = wintypes.BOOL

        self._user32.GetPropW.argtypes = [wintypes.HWND, wintypes.LPCWSTR]
        self._user32.GetPropW.restype = wintypes.HANDLE

        self._user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
        self._user32.GetWindowTextLengthW.restype = ctypes.c_int
        self._user32.GetWindowTextW.argtypes = [
            wintypes.HWND,
            wintypes.LPWSTR,
            ctypes.c_int,
        ]
        self._user32.GetWindowTextW.restype = ctypes.c_int
        self._user32.GetClassNameW.argtypes = [
            wintypes.HWND,
            wintypes.LPWSTR,
            ctypes.c_int,
        ]
        self._user32.GetClassNameW.restype = ctypes.c_int
        self._user32.IsWindowVisible.argtypes = [wintypes.HWND]
        self._user32.IsWindowVisible.restype = wintypes.BOOL
        self._user32.EnumChildWindows.argtypes = [
            wintypes.HWND,
            self._enum_callback_type,
            wintypes.LPARAM,
        ]
        self._user32.EnumChildWindows.restype = wintypes.BOOL

        self._user32.GetWindowThreadProcessId.argtypes = [
            wintypes.HWND,
            ctypes.POINTER(wintypes.DWORD),
        ]
        self._user32.GetWindowThreadProcessId.restype = wintypes.DWORD

        self._user32.IsWindow.argtypes = [wintypes.HWND]
        self._user32.IsWindow.restype = wintypes.BOOL

        class CopyDataStruct(ctypes.Structure):
            _fields_ = [
                ("dwData", ctypes.c_size_t),
                ("cbData", wintypes.DWORD),
                ("lpData", ctypes.c_void_p),
            ]

        self._copydata_struct = CopyDataStruct
        self._user32.SendMessageTimeoutW.argtypes = [
            wintypes.HWND,
            wintypes.UINT,
            wintypes.WPARAM,
            ctypes.c_void_p,
            wintypes.UINT,
            wintypes.UINT,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._user32.SendMessageTimeoutW.restype = wintypes.LPARAM

    def list_editors(self) -> list[EditorWindow]:
        ctypes = self._ctypes
        wintypes = self._wintypes
        editors: list[EditorWindow] = []

        def visit(raw_handle: int, _parameter: int) -> bool:
            marker = self._user32.GetPropW(raw_handle, WINDOW_PROPERTY)
            if not marker:
                return True

            title_length = self._user32.GetWindowTextLengthW(raw_handle)
            title_buffer = ctypes.create_unicode_buffer(max(title_length + 1, 1))
            self._user32.GetWindowTextW(
                raw_handle, title_buffer, len(title_buffer)
            )
            process_id = wintypes.DWORD()
            self._user32.GetWindowThreadProcessId(
                raw_handle, ctypes.byref(process_id)
            )
            editors.append(
                EditorWindow(
                    handle=int(raw_handle),
                    title=title_buffer.value,
                    process_id=int(process_id.value),
                    property_value=int(marker),
                )
            )
            return True

        callback = self._enum_callback_type(visit)
        ctypes.set_last_error(0)
        if not self._user32.EnumWindows(callback, 0):
            error = ctypes.get_last_error()
            if error:
                raise BridgeError(
                    "window_enumeration_failed",
                    f"EnumWindows failed with Win32 error {error}.",
                    {"win32_error": error},
                )
        return editors

    def _window_text(self, handle: int) -> str:
        length = self._user32.GetWindowTextLengthW(handle)
        buffer = self._ctypes.create_unicode_buffer(max(length + 1, 1))
        self._user32.GetWindowTextW(handle, buffer, len(buffer))
        return buffer.value

    def _window_class(self, handle: int) -> str:
        buffer = self._ctypes.create_unicode_buffer(256)
        self._user32.GetClassNameW(handle, buffer, len(buffer))
        return buffer.value

    def list_blocking_dialogs(self, process_id: int) -> list[dict[str, Any]]:
        dialogs: list[dict[str, Any]] = []
        ctypes = self._ctypes
        wintypes = self._wintypes

        def visit(raw_handle: int, _parameter: int) -> bool:
            dialog_process_id = wintypes.DWORD()
            self._user32.GetWindowThreadProcessId(
                raw_handle, ctypes.byref(dialog_process_id)
            )
            if (
                int(dialog_process_id.value) != process_id
                or not self._user32.IsWindowVisible(raw_handle)
                or self._window_class(raw_handle) != "#32770"
            ):
                return True

            buttons: list[str] = []
            messages: list[str] = []

            def visit_child(child_handle: int, _child_parameter: int) -> bool:
                if not self._user32.IsWindowVisible(child_handle):
                    return True
                text = self._window_text(child_handle).strip()
                if not text:
                    return True
                class_name = self._window_class(child_handle)
                if class_name == "Button":
                    buttons.append(text)
                elif class_name == "Static":
                    messages.append(text)
                return True

            callback = self._enum_callback_type(visit_child)
            self._user32.EnumChildWindows(raw_handle, callback, 0)
            if messages and len(buttons) <= 3:
                dialogs.append(
                    {
                        "hwnd": str(raw_handle),
                        "title": self._window_text(raw_handle),
                        "message": "\n".join(messages),
                        "buttons": buttons,
                    }
                )
            return True

        callback = self._enum_callback_type(visit)
        self._user32.EnumWindows(callback, 0)
        return dialogs

    def send_request(
        self, handle: int, request_path: Path, magic: int, timeout_ms: int
    ) -> None:
        ctypes = self._ctypes
        if not self._user32.IsWindow(handle):
            raise BridgeError(
                "editor_not_found",
                f"WorldBuilder editor window {handle} no longer exists.",
            )
        if not self._user32.GetPropW(handle, WINDOW_PROPERTY):
            raise BridgeError(
                "editor_not_found",
                f"Window {handle} is not a WorldBuilder MCP endpoint.",
            )

        # The request path is UTF-16LE including its terminator.  This is the
        # native Windows representation and avoids code-page ambiguity.
        path_buffer = ctypes.create_unicode_buffer(str(request_path))
        copy_data = self._copydata_struct(
            dwData=magic,
            cbData=ctypes.sizeof(path_buffer),
            lpData=ctypes.cast(path_buffer, ctypes.c_void_p),
        )
        result = ctypes.c_size_t()
        flags = 0x0001 | 0x0002  # SMTO_BLOCK | SMTO_ABORTIFHUNG
        ctypes.set_last_error(0)
        sent = self._user32.SendMessageTimeoutW(
            handle,
            WM_COPYDATA,
            0,
            ctypes.byref(copy_data),
            flags,
            timeout_ms,
            ctypes.byref(result),
        )
        if not sent:
            error = ctypes.get_last_error()
            if error == 1460:
                raise BridgeError(
                    "editor_timeout",
                    f"WorldBuilder editor {handle} did not respond in "
                    f"{timeout_ms} ms.",
                    {"win32_error": error},
                )
            raise BridgeError(
                "send_failed",
                f"WM_COPYDATA delivery to editor {handle} failed"
                + (f" with Win32 error {error}." if error else "."),
                {"win32_error": error},
            )
        if not result.value:
            raise BridgeError(
                "send_failed",
                f"WorldBuilder editor {handle} rejected the request.",
            )


def _encode_value(value: Any) -> str:
    if isinstance(value, str):
        text = value
    elif isinstance(value, bool):
        text = "true" if value else "false"
    elif value is None:
        text = "null"
    else:
        try:
            text = json.dumps(
                value,
                ensure_ascii=False,
                allow_nan=False,
                separators=(",", ":"),
            )
        except (TypeError, ValueError) as exc:
            raise BridgeError(
                "invalid_argument",
                f"Request value is not JSON-serializable: {exc}",
            ) from exc
    return quote(text, safe="")


def format_height_points(points: Any) -> str:
    """Convert MCP terrain point objects into the compact bridge format."""

    if not isinstance(points, list) or not points:
        raise BridgeError(
            "invalid_argument", "points must be a non-empty array."
        )
    if len(points) > 4096:
        raise BridgeError(
            "invalid_argument", "points must contain at most 4096 entries."
        )

    encoded_points: list[str] = []
    for index, point in enumerate(points):
        if not isinstance(point, Mapping):
            raise BridgeError(
                "invalid_argument", f"points[{index}] must be an object."
            )
        missing = [key for key in ("x", "y", "height") if key not in point]
        if missing:
            raise BridgeError(
                "invalid_argument",
                f"points[{index}] is missing {', '.join(missing)}.",
            )
        values: list[str] = []
        for key in ("x", "y", "height"):
            value = point[key]
            if isinstance(value, bool) or not isinstance(value, int):
                raise BridgeError(
                    "invalid_argument",
                    f"points[{index}].{key} must be an integer.",
                )
            if key == "height" and not 0 <= value <= 255:
                raise BridgeError(
                    "invalid_argument",
                    f"points[{index}].height must be between 0 and 255.",
                )
            values.append(str(value))
        encoded_points.append(",".join(values))
    return ";".join(encoded_points)


class WorldBuilderBridge:
    def __init__(
        self,
        api: WindowApi | None = None,
        temp_root: Path | str | None = None,
        timeout_ms: int = DEFAULT_TIMEOUT_MS,
        editor_paths: Mapping[str, Path | str] | None = None,
    ) -> None:
        if timeout_ms <= 0:
            raise ValueError("timeout_ms must be positive")
        self._api = api
        base = Path(temp_root) if temp_root is not None else Path(tempfile.gettempdir())
        self._request_directory = base / WINDOW_PROPERTY
        self.timeout_ms = timeout_ms
        self._editor_paths = {
            str(edition): Path(path).expanduser()
            for edition, path in (editor_paths or {}).items()
            if path
        }

    def _get_api(self) -> WindowApi:
        if self._api is None:
            self._api = Win32WindowApi()
        return self._api

    def list_editors(self) -> list[dict[str, Any]]:
        editors = self._get_api().list_editors()
        return [editor.as_dict() for editor in editors]

    def list_blocking_dialogs(self, editor_id: Any = None) -> list[dict[str, Any]]:
        """Return visible blocking message dialogs for the selected editor."""

        editor = self._select_editor(editor_id)
        return self._get_api().list_blocking_dialogs(editor.process_id)

    def _editors_for_edition(
        self, edition: str, probe_timeout_ms: int = EDITION_PROBE_TIMEOUT_MS
    ) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
        matches: list[dict[str, Any]] = []
        unknown: list[dict[str, Any]] = []
        editors = self.list_editors()
        deadline = time.monotonic() + (probe_timeout_ms / 1000.0)
        for index, editor in enumerate(editors):
            if editor["bridge_version"] < EDITION_AWARE_BRIDGE_VERSION:
                unknown.append(
                    {
                        **editor,
                        "edition": None,
                        "probe_error": "bridge_upgrade_required",
                    }
                )
                continue
            remaining_ms = int((deadline - time.monotonic()) * 1000)
            if remaining_ms <= 0:
                unknown.extend({**item, "edition": None} for item in editors[index:])
                break
            try:
                information = self.call(
                    "get_bridge_info",
                    editor_id=editor["id"],
                    timeout_ms=remaining_ms,
                )
            except BridgeError as error:
                # Count an editor we could not probe as unknown, including one that
                # vanished from the enumeration mid-probe. Dropping it entirely would
                # let launch_editor start a second copy of a running editor.
                unknown.append(
                    {**editor, "edition": None, "probe_error": error.code}
                )
                continue
            reported_edition = (
                information.get("edition")
                if isinstance(information, Mapping)
                else None
            )
            if reported_edition == edition:
                matches.append({**editor, "edition": edition})
            elif reported_edition not in ("generals", "zero_hour"):
                unknown.append({**editor, "edition": None})
        return matches, unknown

    def launch_editor(self, edition: str = "zero_hour", wait_ms: int = 15_000) -> dict[str, Any]:
        """Launch a configured editor and optionally wait for bridge discovery."""

        if edition not in ("generals", "zero_hour"):
            raise BridgeError(
                "invalid_argument", "edition must be generals or zero_hour."
            )
        if isinstance(wait_ms, bool) or not isinstance(wait_ms, int) or not 0 <= wait_ms <= 120_000:
            raise BridgeError(
                "invalid_argument", "wait_ms must be between 0 and 120000."
            )
        deadline = time.monotonic() + (wait_ms / 1000.0)
        initial_probe_ms = min(
            EDITION_PROBE_TIMEOUT_MS,
            max(0, int((deadline - time.monotonic()) * 1000)),
        )
        running, unknown = self._editors_for_edition(edition, initial_probe_ms)
        if running:
            return {"launched": False, "already_running": True, "editors": running}
        if unknown:
            upgrade_required = any(
                editor.get("probe_error") == "bridge_upgrade_required"
                for editor in unknown
            )
            raise BridgeError(
                "editor_upgrade_required"
                if upgrade_required
                else "editor_edition_unknown",
                (
                    "A marked WorldBuilder editor uses a bridge version that cannot "
                    "identify its edition; upgrade it before launching another editor."
                    if upgrade_required
                    else "A marked WorldBuilder editor did not answer the edition "
                    "probe; refusing to launch a possible duplicate."
                ),
                {"editors": unknown},
            )

        executable = self._editor_paths.get(edition)
        if executable is None:
            env_name = (
                "WORLDBUILDER_GENERALS_PATH"
                if edition == "generals"
                else "WORLDBUILDER_ZERO_HOUR_PATH"
            )
            configured = os.environ.get(env_name)
            executable = Path(configured).expanduser() if configured else None
        if executable is None:
            raise BridgeError(
                "editor_path_not_configured",
                "No WorldBuilder executable is configured for " + edition + ".",
                {
                    "cli_option": (
                        "--generals-editor"
                        if edition == "generals"
                        else "--zero-hour-editor"
                    )
                },
            )
        executable = executable.resolve()
        if not executable.is_file():
            raise BridgeError(
                "editor_executable_not_found",
                f"Configured WorldBuilder executable does not exist: {executable}",
            )

        try:
            process = _launch_editor_process(executable)
        except OSError as exc:
            raise BridgeError(
                "editor_launch_failed", f"Could not launch WorldBuilder: {exc}"
            ) from exc

        try:
            while wait_ms and time.monotonic() < deadline:
                remaining_ms = int((deadline - time.monotonic()) * 1000)
                if remaining_ms <= 0:
                    break
                editors, _unknown = self._editors_for_edition(
                    edition, min(EDITION_PROBE_TIMEOUT_MS, remaining_ms)
                )
                if editors:
                    return {
                        "launched": True,
                        "already_running": False,
                        "process_id": process.pid,
                        "editors": editors,
                    }
                exit_code = process.poll()
                if exit_code is not None:
                    raise BridgeError(
                        "editor_exited",
                        "WorldBuilder exited before exposing the MCP bridge "
                        f"(code {exit_code}).",
                        {"exit_code": exit_code},
                    )
                time.sleep(min(0.1, max(0.0, deadline - time.monotonic())))
            return {
                "launched": True,
                "already_running": False,
                "process_id": process.pid,
                "editors": [],
                "bridge_ready": False,
            }
        finally:
            process.close()

    def _select_editor(self, editor_id: Any = None) -> EditorWindow:
        editors = self._get_api().list_editors()
        if editor_id is None:
            if not editors:
                raise BridgeError(
                    "editor_not_found",
                    "No running WorldBuilder MCP editor was found.",
                )
            if len(editors) != 1:
                raise BridgeError(
                    "editor_required",
                    "Multiple WorldBuilder editors are running; pass editor_id.",
                    {"editors": [editor.as_dict() for editor in editors]},
                )
            return editors[0]

        if isinstance(editor_id, bool) or not isinstance(editor_id, (str, int)):
            raise BridgeError(
                "invalid_argument", "editor_id must be a string or integer."
            )
        try:
            requested_handle = (
                editor_id
                if isinstance(editor_id, int)
                else int(editor_id.strip(), 0)
            )
        except ValueError as exc:
            raise BridgeError(
                "invalid_argument", f"Invalid editor_id {editor_id!r}."
            ) from exc
        for editor in editors:
            if editor.handle == requested_handle:
                return editor
        raise BridgeError(
            "editor_not_found",
            f"WorldBuilder editor {editor_id!r} was not found.",
        )

    def _write_request(
        self,
        request_path: Path,
        response_path: Path,
        command: str,
        arguments: Mapping[str, Any],
    ) -> None:
        if not command or "\n" in command or "\r" in command:
            raise BridgeError("invalid_command", "Invalid bridge command.")
        if "command" in arguments or "response_path" in arguments:
            raise BridgeError(
                "invalid_argument",
                "command and response_path are reserved request fields.",
            )

        fields: list[tuple[str, Any]] = [
            ("command", command),
            ("response_path", str(response_path)),
            *arguments.items(),
        ]
        lines: list[str] = []
        for key, value in fields:
            if not isinstance(key, str) or not _KEY_PATTERN.fullmatch(key):
                raise BridgeError(
                    "invalid_argument", f"Invalid request key {key!r}."
                )
            lines.append(f"{key}\t{_encode_value(value)}\n")

        try:
            with request_path.open("x", encoding="utf-8", newline="\n") as stream:
                stream.writelines(lines)
        except OSError as exc:
            raise BridgeError(
                "request_file_failed",
                f"Could not create bridge request file: {exc}",
            ) from exc

    def _read_response(self, response_path: Path, deadline: float) -> Any:
        last_error: OSError | UnicodeError | json.JSONDecodeError | None = None
        while True:
            try:
                size = response_path.stat().st_size
                if size > MAX_RESPONSE_BYTES:
                    raise BridgeError(
                        "response_too_large",
                        f"WorldBuilder response exceeds {MAX_RESPONSE_BYTES} bytes.",
                    )
                raw_response = response_path.read_text(encoding="utf-8-sig")
                envelope = json.loads(raw_response)
                break
            except FileNotFoundError:
                last_error = None
            except BridgeError:
                raise
            except (OSError, UnicodeError, json.JSONDecodeError) as exc:
                last_error = exc
            if time.monotonic() >= deadline:
                if last_error is None:
                    raise BridgeError(
                        "response_timeout",
                        "WorldBuilder returned without creating its response file.",
                    )
                raise BridgeError(
                    "invalid_response",
                    f"Could not read the WorldBuilder JSON response: {last_error}",
                ) from last_error
            time.sleep(0.01)

        if not isinstance(envelope, dict) or not isinstance(
            envelope.get("ok"), bool
        ):
            raise BridgeError(
                "invalid_response",
                "WorldBuilder response must be an object with a boolean ok field.",
            )
        if envelope["ok"]:
            return envelope.get("result")

        error = envelope.get("error")
        if not isinstance(error, dict):
            raise BridgeError(
                "invalid_response",
                "Failed WorldBuilder response has no error object.",
            )
        code = error.get("code", "worldbuilder_error")
        message = error.get("message", "WorldBuilder command failed.")
        if not isinstance(code, str) or not isinstance(message, str):
            raise BridgeError(
                "invalid_response",
                "WorldBuilder error code and message must be strings.",
            )
        raise RemoteError(code, message, error.get("data"))

    def call(
        self,
        command: str,
        arguments: Mapping[str, Any] | None = None,
        editor_id: Any = None,
        timeout_ms: int | None = None,
    ) -> Any:
        if arguments is None:
            arguments = {}
        if not isinstance(arguments, Mapping):
            raise BridgeError(
                "invalid_argument", "Bridge arguments must be an object."
            )

        editor = self._select_editor(editor_id)
        try:
            self._request_directory.mkdir(parents=True, exist_ok=True)
        except OSError as exc:
            raise BridgeError(
                "request_file_failed",
                f"Could not create bridge temporary directory: {exc}",
            ) from exc

        token = f"{os.getpid()}-{uuid.uuid4().hex}"
        request_path = self._request_directory / f"{token}.request"
        response_path = self._request_directory / f"{token}.response"
        if timeout_ms is not None:
            if isinstance(timeout_ms, bool) or not isinstance(timeout_ms, int) or timeout_ms <= 0:
                raise BridgeError("invalid_argument", "timeout_ms must be positive.")
            command_timeout_ms = timeout_ms
        elif command in LONG_COMMANDS:
            command_timeout_ms = max(self.timeout_ms, LONG_COMMAND_TIMEOUT_MS)
        elif command in BATCH_COMMANDS:
            command_timeout_ms = max(self.timeout_ms, BATCH_COMMAND_TIMEOUT_MS)
        else:
            command_timeout_ms = self.timeout_ms
        deadline = time.monotonic() + (command_timeout_ms / 1000.0)
        delivered = False
        try:
            self._write_request(
                request_path, response_path, command, arguments
            )
            self._get_api().send_request(
                editor.handle, request_path, COPYDATA_MAGIC, command_timeout_ms
            )
            delivered = True
            result = self._read_response(response_path, deadline)
            delivered = False
            return result
        finally:
            # The editor only queues the request during WM_COPYDATA and reads the
            # file later, off its message loop. Deleting it after a timeout would
            # drop a command the editor is still about to run, with no error
            # anywhere, so leave a delivered-but-unanswered request in place.
            leftovers = () if delivered else (request_path, response_path)
            for path in leftovers:
                try:
                    path.unlink(missing_ok=True)
                except OSError:
                    # Stale UUID-named files are harmless and can be cleaned by
                    # the operating system's regular temporary-file policy.
                    pass


__all__ = [
    "BridgeError",
    "COPYDATA_MAGIC",
    "EditorWindow",
    "RemoteError",
    "WINDOW_PROPERTY",
    "Win32WindowApi",
    "WorldBuilderBridge",
    "format_height_points",
]
