# Qt Designer (.ui) files — what's editable, what isn't

Every WorldBuilder Qt dialog/panel now keeps its **static widget tree** in a `.ui` file next to
its `.cpp`. `uic` (AUTOUIC, enabled on the `z_worldbuilder_qt` target) turns each one into a
`ui_<Class>.h` at build time; the class calls `m_ui->setupUi(this)` and then binds its existing
`m_*` members from `m_ui-><objectName>`.

Open any `.ui` with **`C:\Qt\5.15.2\msvc2019\bin\designer.exe`**. Qt Designer 6 can open them too
(the format is `<ui version="4.0">` in both), but 5.15.2's Designer matches the `uic` that actually
compiles these files, so there is no save-format drift to worry about. **Rebuild after each
Designer session** rather than batching many edits — a bad save surfaces immediately that way.

## The split

| Lives in the `.ui` | Stays in C++ |
| --- | --- |
| Widget hierarchy, layouts, stretches, spacers | Window flags (`Qt::Tool`, …) — they're base-ctor args |
| Text, titles, tooltips, placeholders | `WBQtWindowPos_Track`, `WBQtTreeStyle::applyTreeLines`, theme calls |
| Spin ranges/decimals/steps, checkable/checked | Runtime-computed values (ranges from the engine bridge) |
| Fixed sizes (as min+max) | Any tweak whose *why* is documented in a comment |
| | Widgets built from engine data or a runtime mode |

Everything behavioral — slots, bridge calls, refresh functions, `extern "C"` entry points — was
left untouched by the conversion.

## Fully visual (the normal case)

Camera, Water, Brush, Feather, Mound, Ramp, Scorch, Ruler, BlendMaterial, Fence, GlobalLight,
Road, Layers, Object, ObjectProps, Wave, Teams, BuildList, MeshMold, TracingOverlay, TerrainModal,
CondAct, MapFile (Open/Save), EntityFinder, ScriptEdit, TeamSheet, PlayerList, and the eight
MiscModals dialogs. Drag things around in Designer, save, rebuild — that's the whole loop.

## Partly visual — dialogs whose shape is decided at runtime

These are **not broken**. Each has a real reason its form can't be fully static, listed here so the
next person doesn't "fix" a `.ui` that looks suspiciously empty.

### The three that look genuinely odd

**`WBQtParamDialog.ui` — nearly an empty shell.**
An empty group box (`boxLay`) above an OK/Cancel row, and nothing else. The dialog's contents
depend on the parameter *kind* (edit vs list vs audio widgets), so C++ fills `boxLay` at runtime
and inserts the optional audio pair into `buttonsRow`. In Designer you can move the buttons and
retitle the box; that's all. This is the weakest case for having a `.ui` at all — it's ~90%
dynamic. If it ever gets in the way, reverting this one class to plain C++ is defensible.

**`WBQtMapIniReportDialog.ui` — the decision buttons are missing from the canvas.**
The filter row, tree and Copy button are static, but the mode-dependent buttons (OK/Cancel when
previewing a map.ini, a single Close for Check) are built in C++ and appended to the `.ui`'s
`buttonsLayout`. Designer shows the dialog without the buttons that matter most.

**`WBQtPickUnitDialog.ui` — shows a dialog that never exists.**
Both modes' widgets sit in the `.ui` at once: `missingLabel` (replace-missing-unit mode) *and*
`searchRow` (pick mode). The constructor hides whichever the mode doesn't use, so the canvas shows
both variants stacked. `searchRow` is also wrapped in a native `QWidget` purely so it can be hidden
as a unit (a bare layout can't be). The bottom button column (`buttonColHost`) is empty for the
same reason — the two modes need different buttons.

### The rest, with their dynamic part

| Dialog | Built in C++ | Fill point in the `.ui` |
| --- | --- | --- |
| `WBQtScriptWindow` | The script tree (`WBQtScriptTree` needs its owning window as a ctor arg) | inserted into `split` at index 0 |
| `WBQtTerrainMaterialPanel` | Texture/favorite tree items; three `QButtonGroup`s | trees are static; items filled at runtime |
| `WBQtTeamSheetDialog` | 7 member rows + 16 generic script rows | `membersGrid` (rows 1–7), `genericGrid` |
| `WBQtGrovePanel` | 11 tree type/weight rows | `treesGrid` (from row 1) |
| `WBQtEntityFinderDialog` | The logo tile (resource bitmap + white-matte keying) | `logoRow` |
| `WBQtScriptEditDialog` | The conditional Or / Move-to-Other button | inserted into `buttonsLayout` at index 5 |
| `WBQtNewHeightMapDialog` | The 3×3 anchor grid (resize mode only) | `anchorsHost` |

## Gotchas worth knowing before you edit

**Promoted custom widgets render as their base class.** `WBQtScrubSpinBox` (the drag-to-scrub
spinbox) appears as a plain `QDoubleSpinBox` in Designer — promotion doesn't load your compiled
widget. Layout is still accurate. Used in Camera, GlobalLight, ObjectProps, TerrainMaterial.

**Promotion can't pass constructor arguments.** `WBQtScrubSpinBox`'s `axisVertical` ctor arg
becomes a `setAxisVertical(true)` call after `setupUi`. If you add a scrub spinbox that should
scrub vertically, remember that second step.

**Don't move `setChecked(true)` into a `.ui` when a `QButtonGroup` is involved.** In
`WBQtTerrainMaterialPanel` the default checks are deliberately interleaved with the group
construction (`m_impassable->setChecked(true)` *before* `passGroup`, `m_rot0->setChecked(true)`
*before* `rotGroup`). Radios parented to the same panel are implicitly exclusive until their real
groups exist — set them from XML and `rot0` would uncheck `impassable`.

**Two objectNames can't follow the `m_fooBar` → `fooBar` convention.** `m_delete` and `m_export`
become `deleteBtn` / `exportBtn` (and `m_import` → `importBtn` for symmetry) because `delete` and
`export` are C++ keywords and would break the generated header.

**Some values stayed in C++ on purpose.** Combo width caps
(`AdjustToMinimumContentsLengthWithIcon` + `setMinimumContentsLength`), slider-vs-typed-max ranges,
MFC dialog-unit sizing notes — each carries a comment explaining a hard-won decision. Moving the
number to XML would strand the reason. If you change one of these, change it in the `.cpp`.

## Adding a new dialog

Model it on `WBQtCameraPanel` (`.ui` + `.h` + `.cpp`) — the simplest complete example. Add the
`.ui` to the `z_worldbuilder_qt` source list in `CMakeLists.txt` next to the class's `.h`, and
reconfigure (a new file needs it). AUTOUIC does the rest.
