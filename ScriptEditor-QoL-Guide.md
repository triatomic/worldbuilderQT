# WorldBuilder Script Editor — Quality-of-Life Guide

A user guide to the new Script Editor features in the Qt WorldBuilder. Open the
Script Editor from the toolbar / menu as usual; everything below lives in that
window (the tree on the left, the detail pane on the right, the search row on
top).

> Tip: several features are gated by the **NewSearch** toggle in
> *Help / Entity finder → Visual Settings*. Turn it on to get live, type-as-you-go
> search behavior.

---

## 1. Live search in the script tree

Type in the **Search** box at the top. With NewSearch on, the tree filters as you
type — non-matching scripts hide, and matches stay visible under their folder/
player (which auto-expands). It matches the script name *and* a deep scan of the
comment, readable text, and every condition/action parameter.

- Clear the box to show the whole tree again.
- With NewSearch off, the box behaves the classic way: press **Enter** / **Find
  Next** to jump to the next match.

## 2. "Show:" filter chips

A row of toggles under the search box narrows the tree by state:

| Chip | Shows |
|------|-------|
| Warnings | only scripts (and their folders) that have warnings |
| Active / Inactive | by active state |
| Easy / Normal / Hard | by difficulty (OR'd — tick Easy+Hard to see both) |

Chips combine with each other **and** with the search text. Clear them (and the
box) to return to the full tree.

## 3. Rename, duplicate, delete (tree shortcuts)

Select a script or folder in the tree, then:

- **F2** — rename it. A small dialog opens prefilled with the current name; type
  the new name and press Enter. Undoable with **Ctrl+Z**.
- **Ctrl+D** — duplicate it in place (same as the Copy button).
- **Delete** — delete it. You'll be asked to confirm first; tick **Don't ask
  again** to skip the prompt in future (stored in the editor settings).

All of these are also on the **right-click menu**, which now includes: New Script,
New Folder, Activate/Deactivate, Edit, Rename, Duplicate, Delete, and the Debug
caption helpers.

## 4. Copy/paste conditions & actions between scripts

Open a script (double-click it) to get the tabbed editor with **Conditions**,
**Actions if true**, and **Actions if false** lists.

- Select a condition or action and press **Ctrl+C** to copy it.
- Open another script, click into the matching list, and press **Ctrl+V** to
  paste a copy.
- Same-type only: a copied *condition* pastes into a Conditions list; a copied
  *action* pastes into either action list (true or false). The clipboard keeps
  the item until you copy something else, so you can paste it into several
  scripts.

## 5. Reference links in the detail pane

Select a script and look at the detail pane on the right. It shows clickable
reference lines:

- **[Referenced in]** — the scripts that call *this* script. Click a name to jump
  to it in the tree.
- **[Uses]** — the scripts *this* one calls. Click to jump.
- **[Units]** / **[Waypoints]** — the placed units and waypoints this script
  names in its parameters. Click one to **select it in the 3D view and center the
  camera on it**. (If it was renamed or deleted since, you'll just hear a beep.)

## 6. Find/replace parameter values across scripts

Press **Ctrl+H** (or the **Replace…** button in the search row) to open the
find/replace bar. It renames a value everywhere it's used in scripting — object,
team, waypoint, or script names in condition/action parameters.

The bar:

```
[ Find parameter value...  ] [Aa] [=] [ ] This script only   N parameter values   ↑ ↓  ×
[ Replace with             ]                                          [Replace All]
```

- **Find** — type a value. A dropdown suggests the actual values in your scripts
  with their use counts (e.g. `GLAInfantry01 (7)`); click one to fill Find with
  it. The count label shows how many parameter values match.
- **Aa** — match case. **=** — whole value only (the whole parameter must equal
  the text, not just contain it).
- **↑ ↓** — step through the scripts that use a matching value.
- **Replace with** — type the new value (its dropdown offers existing values to
  rename *to*). **Replace All** rewrites every match. Undo with **Ctrl+Z**.

### Search only the selected script

- Click the **In script** button in the search row, **or** tick **This script
  only** in the bar, to limit find/replace to the currently selected script.
- When scoped, the count, suggestions, and Replace All act only on that one
  script. (The rename-to suggestions still include values from anywhere, so you
  can rename to something used elsewhere.)

Only **parameter values** are searched/replaced — script names, folder names, and
comments are never touched.

---

## Work example — rename a unit everywhere

You renamed a unit on the map from `GLAInfantry01` to `GLA_Rifle_01`, and now
several scripts reference the old name.

1. Open the **Script Editor**.
2. Press **Ctrl+H** to open the find/replace bar.
3. In **Find**, start typing `GLA` — the dropdown shows the real values with
   counts, e.g. `GLAInfantry01 (5)`. Click it. The label reads *"5 parameter
   values"*.
4. Press **↓** a couple of times to jump through the 5 scripts that use it and
   confirm they're the ones you expect.
5. In **Replace with**, type `GLA_Rifle_01`.
6. Click **Replace All**. All 5 references are rewritten; the count drops to *0
   parameter values*.
7. Not what you wanted? **Ctrl+Z** reverts the whole replace.

To do the same but only inside the script you have selected, click **In script**
(or tick **This script only**) before step 3 — the count and Replace All then
cover just that script.

### Bonus: find where a script points on the map

1. Select a script that moves a unit to a waypoint.
2. In the detail pane, find the **[Units]** or **[Waypoints]** line.
3. Click the unit/waypoint name — WorldBuilder selects that object and centers
   the 3D view on it, so you can see exactly what the script targets.
