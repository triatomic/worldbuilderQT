// WBQtWaterBridge.h -- self-contained facade for the Qt Water Options panel.
//
// Like WBQtPanelBridge.h this header carries ONLY int/double/char* (no Qt or MFC types), so
// the MFC WaterOptions TU and the Qt panel TU can talk without including each other's headers.
// Kept as its own header (not folded into WBQtPanelBridge.h) so the shared facade stays
// untouched. Included by qt/panels/WBQtWaterPanel.cpp (Qt side) and src/WBQtWaterBridge.cpp
// (MFC side).
//
// The MFC WaterOptions is still Create()d as the hidden OFF fallback. It carries a little state
// of its own -- the water height, the point spacing and the "creating water areas" toggle are
// global statics on WaterOptions -- plus it edits the single selected water-area PolygonTrigger
// (the height slider drives a MovePolygonUndoable, the name field renames the trigger, and Make
// River flips + reorders the trigger's points). WaterTool / PointerTool call WaterOptions::update()
// when the selection changes; a guarded WBQtWater_PushRefresh() re-seeds the Qt panel from the
// current selection so the two stay in step. This bridge reaches the model through
// WaterOptions::qt* statics (defined MFC-side) that reuse the same helpers the MFC On* handlers
// use, so the tools keep working unchanged.
#ifndef WB_QT_WATER_BRIDGE_H
#define WB_QT_WATER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Water panel: reverse, Qt widget -> MFC WaterOptions (implemented MFC-side, --------------
//     src/WBQtWaterBridge.cpp -> WaterOptions::qt*). Strings copy into caller buffers
//     (no ownership crosses the seam).

// Water height (the global static m_waterHeight). GetHeight reads it; SetHeight applies a new
// height to the selected water-area trigger through the same startUpdate/update/endUpdate
// MovePolygonUndoable sequence the MFC edit/slider path uses. GetHeightMin / GetHeightMax mirror
// the popup-slider range (0 .. 255*MAP_HEIGHT_SCALE).
int  WBQtWater_GetHeight(void);
void WBQtWater_SetHeight(int height);
void WBQtWater_SetHeightDragStep(int height);	// slider drag tick (one undoable across the drag)
void WBQtWater_EndHeightScrub(void);			// slider release (close that undoable)
int  WBQtWater_GetHeightMin(void);
int  WBQtWater_GetHeightMax(void);

// Point spacing (the global static m_waterPointSpacing). Set mirrors OnChangeSpacingEdit's
// accept-if-parseable behaviour (already parsed to an int on the Qt side).
int  WBQtWater_GetSpacing(void);
void WBQtWater_SetSpacing(int spacing);

// "Water Polygon" toggle (the global static m_creatingWaterAreas). Mirrors OnWaterPolygon.
int  WBQtWater_GetCreatingWaterAreas(void);
void WBQtWater_SetCreatingWaterAreas(int on);

// Selection state: non-zero when a single water-area PolygonTrigger is selected (the only case
// where the name / height / Make River controls are meaningful). Mirrors the updateTheUI branch.
int  WBQtWater_HasSelection(void);	// any single selected polygon (name + Make River shown)
int  WBQtWater_IsWaterArea(void);	// that polygon is a water area (height row shown)

// The selected trigger's name (IDC_WATERNAME_EDIT). GetName fills the current name; SetName
// reuses the MFC OnChangeWaterEdit uniqueness logic (returns non-zero if applied, zero if the
// name was already in use / there was nothing selected).
int  WBQtWater_GetName(char *nameOut, int cap);
int  WBQtWater_SetName(const char *name);

// "Make River" flag on the selected trigger. GetRiver reads isRiver(); SetRiver mirrors
// OnMakeRiver (setRiver + the flow-direction reorder when turning it on).
int  WBQtWater_GetRiver(void);
void WBQtWater_SetRiver(int on);

// Forward (Qt-side, WBQtWaterPanel.cpp): WaterOptions::update() / updateTheUI fire this so the
// Qt panel re-reads on water-area selection.
void WBQtWater_PushRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_WATER_BRIDGE_H
