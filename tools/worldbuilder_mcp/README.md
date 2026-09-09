# WorldBuilder MCP server

This directory contains a dependency-free MCP stdio server for the *Command &
Conquer: Generals Zero Hour* WorldBuilder editor. It controls the running editor through
a local `WM_COPYDATA` bridge compiled into WorldBuilder; it does not attempt to
rewrite the compressed `.map` format outside the editor.

## Build and run

Build the editor with its tools enabled:

```powershell
cmake --preset win32-internal -DRTS_ENABLE_WORLDBUILDER_QT=ON `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019"
cmake --build --preset win32-internal --target z_worldbuilder
```

The build must run inside an **x86** MSVC environment (`vcvarsall.bat x86`).
Output: `build/win32-internal/GeneralsMD/Release/WorldBuilderZH_Qt.exe`.

Start WorldBuilder. The MCP server can discover and open maps itself, so no map
has to be opened or saved manually first. From the repository root, run:

```powershell
python -m tools.worldbuilder_mcp
```

WorldBuilder's **MCP** menu contains a persistent **Enable MCP Server** toggle
and **Server Information...**. Disabling it removes the editor from MCP
discovery and rejects new bridge requests; it does not terminate an external
Python MCP host that was launched by the client.

To let MCP start the editor on demand, configure the executable explicitly:

```powershell
python -m tools.worldbuilder_mcp `
  --zero-hour-editor "F:\githubRepos\WorldbuilderZHAdrianeuild\win32-internal\GeneralsMD\Release\WorldBuilderZH_Qt.exe"
```

The equivalent environment variable is `WORLDBUILDER_ZERO_HOUR_PATH`. `launch_editor` never starts a second editor
when an MCP-enabled WorldBuilder window is already present. On Windows it uses
the shell's visible GUI launch path (`ShellExecuteExW` with `SW_SHOW`) instead
of creating a detached background console process.

Configure an MCP client to launch that command with the repository root as its
working directory. A typical client entry is:

```json
{
  "mcpServers": {
    "generals-worldbuilder": {
      "command": "python",
      "args": ["-m", "tools.worldbuilder_mcp"],
      "cwd": "F:\\githubRepos\\WorldbuilderZHAdriane"
    }
  }
}
```

For `agy`, put the same server under `mcpServers` in
`%USERPROFILE%\.gemini\config\mcp_config.json`. Because `agy` does not expose a
working-directory field, use an absolute Python path and set `PYTHONPATH` to
the repository root:

```json
{
  "mcpServers": {
    "generals-worldbuilder": {
      "command": "C:\\Path\\To\\python.exe",
      "args": ["-m", "tools.worldbuilder_mcp"],
      "env": {
        "PYTHONPATH": "F:\\githubRepos\\WorldbuilderZHAdriane"
      }
    }
  }
}
```

If multiple WorldBuilder windows are running, call `list_editors` and pass the
returned `editor_id` to subsequent tools.

## Tools

This fork keeps WorldBuilder changes out of the shared game engine, so the
upstream `apply_max_progression` tool and object Salvage editing are not
included: both need `GameLogic` edits (two script actions and the
`objectSalvageLevel` key). The read-only `weapon_salvager` flag is kept.

The server exposes:

- editor and bridge discovery;
- discovery and opening of maps from
  `%USERPROFILE%\Documents\Command and Conquer Generals Data\Maps`;
- active-map state, revision numbers, and map validation;
- paginated object and object-template catalogs plus single-object lookup;
- undoable single and bounded/chunked bulk object placement, updates, and deletion;
- player/team catalogs and undoable team creation, updates, and deletion;
- object selection and view focus;
- bounded terrain-height reads and undoable writes;
- save, save-as, undo, and redo.
- map creation with explicit dimensions/default height/border and asynchronous
  close with the same `error` / `save` / `discard` unsaved-change policy as
  map opening;
- waypoint CRUD and directed route-link creation/removal;
- scripting/trigger regions and water/river polygon CRUD;
- script discovery and complete condition/true-action/false-action editing,
  including scripts inside groups;
- loaded science/general-power and upgrade catalogs with grantability/scope;
- terrain texture catalogs, cell texture/blend painting, cliff/passability,
  and playable-area boundaries;
- road and bridge segment creation/listing/deletion, including landmark
  bridges;
- map metadata, time of day, weather, water height, terrain/object lighting;
- minimap preview generation and BMP capture of the current 3D view.

`list_script_types` returns native condition/action ids plus ordered numeric
and semantic parameter types (`SIDE`, `SCIENCE`, `UPGRADE`, and so on).
Resolve an action or condition by its semantic name on every connection rather
than remembering an integer id. `get_script` returns each parameter with its
native type, value, and UI text. `upsert_script` accepts conditions as a flat
array with an `or_group` number; conditions sharing the number are ANDed and
different groups are ORed. An unconditional script must contain the native
`[Scripting] True.` condition; an empty condition list evaluates false. The
entire player-side list is replaced through one native undo operation, so a
malformed script never leaves a partial edit.

MCP clients can read `worldbuilder://scripting-guide` for the compact authoring
workflow, parameter semantics, limits, and verification rules. This resource
is intended to be injected into an LLM's context before generic script work.

Terrain cell coordinates refer to quads (their maximum is one less than the
height-map vertex extent). `set_terrain_cells` can change `texture_class`,
`passable`, or both. With `auto_blend` enabled, WorldBuilder regenerates its
native blend layers around the painted cell.

Object `x`, `y`, and `z` are WorldBuilder world coordinates. Object angles are
degrees. Terrain `x` and `y` are integer height-map indices, while terrain
height is the raw integer value from 0 through 255. `get_map_state` reports the
world cell size, height scale, dimensions, and border.

Object reads expose `is_unit`, `weapon_salvager`, and `veterancy`. Object
updates accept veterancy 0..3 (3 is Heroic).

For object mutations, `owner` accepts either an exact map-team name or a
one-based multiplayer/skirmish player slot number from `1` through `8`. Slot
numbers are stored as the engine runtime teams `teamplayer0` through
`teamplayer7`; this is the correct way to give a preplaced object to a real
lobby player rather than to an inactive `SkirmishAmerica` template player.
Agents should use `owner: 1` immediately when the user says "Player 1"; they
must not ask the user to supply the internal `teamplayer0` name.

`list_user_maps` accepts no path. `open_map` and `save_map_as` accept an
absolute path, a path relative to the user Maps directory, or a map-directory
path whose `.map` file has the same name. Paths outside that directory are
rejected. `open_map` has explicit `error`, `save`, and `discard` policies for
unsaved changes.

Mutating tools accept `expected_revision` for optimistic concurrency control.
Object batches of more than 15 items are automatically split into bounded
native requests with a 30 ms UI-message gap and a 45-second per-chunk timeout.
The server chains returned revision numbers and merges the object results; a
chunked request therefore creates one native undo step per chunk. Smaller bulk
operations remain a single undo step. Retried mutations can carry an
`operation_id`; the server returns the cached successful response rather than
applying the operation twice.

## Safety model

- Requests are queued to the MFC UI thread, so the transport never waits inside
  `WM_COPYDATA` while a long map open/save is running.
- Mutations use WorldBuilder's existing undo stack.
- Map open/save paths are confined to the current user's Generals Maps
  directory; traversal and reparse-point escapes are rejected.
- Bridge files are accepted only under
  `%TEMP%\GeneralsWorldBuilderMcp`; nested and reparse-point paths are rejected,
  and response files are created exclusively.
- Requests are capped at 1 MiB, list pages at 500 items, and terrain operations
  at 4096 samples.
- View captures are created only as direct `.bmp` children of
  `%TEMP%\GeneralsWorldBuilderMcp`; preview output remains beside the active
  user map.
- The bridge is same-user desktop automation. It does not open a network port;
  do not run untrusted processes in the editor's desktop session.

## Tests

The tests use a fake editor window and require no running game:

```powershell
python -m unittest discover -s tools/worldbuilder_mcp/tests -v
```
