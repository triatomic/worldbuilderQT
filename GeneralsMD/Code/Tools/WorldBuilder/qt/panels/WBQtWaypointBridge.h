// WBQtWaypointBridge.h -- self-contained facade for the Qt Waypoint Options panel.
//
// Like WBQtPanelBridge.h this header carries ONLY int/double/char* (no Qt or MFC types), so
// the MFC WaypointOptions TU and the Qt panel TU can talk without including each other's
// headers. Kept as its own header (not folded into WBQtPanelBridge.h) so the shared facade
// stays untouched. Included by qt/panels/WBQtWaypointPanel.cpp (Qt side) and
// src/WBQtWaypointBridge.cpp (MFC side).
//
// The MFC WaypointOptions is still Create()d as the hidden OFF fallback. It has no state of
// its own -- the panel edits the single selected waypoint MapObject and/or the single selected
// PolygonTrigger, exactly like the MFC dialog. WaypointTool / PolygonTool / PointerTool call
// WaypointOptions::update() when the selection changes; a guarded WBQtWaypoint_PushRefresh()
// re-seeds the Qt panel from the current selection so the two stay in step. This bridge reaches
// the model through WaypointOptions::qt* statics (defined MFC-side) that reuse the same getters
// / setters the MFC On* handlers use, so the tools keep working unchanged.
#ifndef WB_QT_WAYPOINT_BRIDGE_H
#define WB_QT_WAYPOINT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Kind of the current single selection (mirrors the MFC updateTheUI branches):
//   0 == nothing editable, 1 == a single waypoint, 2 == a single trigger area.
#define WBQT_WAYPOINT_KIND_NONE     0
#define WBQT_WAYPOINT_KIND_WAYPOINT 1
#define WBQT_WAYPOINT_KIND_TRIGGER  2

// --- Waypoint panel: reverse, Qt widget -> MFC WaypointOptions (implemented MFC-side, --------
//     src/WBQtWaypointBridge.cpp -> WaypointOptions::qt*). Strings copy into caller buffers
//     (no ownership crosses the seam).

// Selection state.
int  WBQtWaypoint_GetKind(void);

// The name field is dual purpose: a waypoint's name or a trigger area's name, matching the
// single IDC_WAYPOINTNAME_EDIT combo. GetName fills the current name; SetName reuses the MFC
// OnChangeWaypointnameEdit uniqueness logic (returns non-zero if applied, zero if the name was
// already in use / there was nothing selected).
int  WBQtWaypoint_GetName(char *nameOut, int cap);
int  WBQtWaypoint_SetName(const char *name);

// The name combo's preset drop-down entries depend on the selection kind (player-start keys for
// a waypoint, inner/outer perimeter + CombatZone for a trigger), mirroring updateTheUI's two
// ResetContent() branches. Count is for the current kind.
int  WBQtWaypoint_GetNamePresetCount(void);
int  WBQtWaypoint_GetNamePreset(int index, char *nameOut, int cap);

// Waypoint location (world units). Valid only when the kind is a waypoint. Set mirrors
// OnEditWaypointLocationX / Y (updates just the one axis, z left at 0).
double WBQtWaypoint_GetLocationX(void);
double WBQtWaypoint_GetLocationY(void);
void   WBQtWaypoint_SetLocationX(double x);
void   WBQtWaypoint_SetLocationY(double y);

// Path labels + bi-directional flag are only meaningful for a waypoint that is LINKED into a
// path. IsLinked mirrors CWorldBuilderDoc::isWaypointLinked for the current waypoint.
int  WBQtWaypoint_IsLinked(void);
// labelIndex is 1..3 (TheKey_waypointPathLabel1..3). Set mirrors changeWaypointLabel (stores
// the string and re-pushes linked labels through the doc).
int  WBQtWaypoint_GetLabel(int labelIndex, char *nameOut, int cap);
void WBQtWaypoint_SetLabel(int labelIndex, const char *name);
int  WBQtWaypoint_GetBiDirectional(void);
void WBQtWaypoint_SetBiDirectional(int on);

// Forward (Qt-side, WBQtWaypointPanel.cpp): WaypointOptions::update() / updateTheUI fire this so
// the Qt panel re-reads on selection.
void WBQtWaypoint_PushRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_WAYPOINT_BRIDGE_H
