// WBQtGroveBridge.h -- self-contained facade for the Qt Grove Options panel.
//
// Like WBQtPanelBridge.h this header carries ONLY int/double/char* (no Qt or MFC types), so
// the MFC GroveOptions TU and the Qt panel TU can talk without including each other's headers.
// Kept as its own header (not folded into WBQtPanelBridge.h) so the shared facade stays
// untouched. Included by qt/panels/WBQtGrovePanel.cpp (Qt side) and src/WBQtGroveBridge.cpp
// (MFC side).
//
// The MFC GroveOptions is still Create()d as the hidden OFF fallback and owns the grove-config
// dialog controls (the 11 tree-type combos + weight edits, the num-trees edit, the set-name
// combo, the three placement checkboxes) plus the object preview. GroveTool reads its choices
// through TheGroveOptions->getNumTrees/getNumType/getTypeName/getTotalTreePerc/getCanPlace*,
// which all pull straight out of those hidden controls. So the Qt panel MUST drive those MFC
// controls (via GroveOptions::qt* -> the same GetDlgItem paths + On* handlers) so the tool keeps
// seeing the right state. This bridge is the seam.
#ifndef WB_QT_GROVE_BRIDGE_H
#define WB_QT_GROVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Grove panel: reverse, Qt widget -> MFC GroveOptions (implemented MFC-side,
// WBQtGroveBridge.cpp -> GroveOptions::qt*). Strings copy into caller buffers (no ownership
// crosses the seam). "type" is the 1..11 tree slot (types 1..10 list shrubbery templates,
// type 11 lists prop templates); getters read the hidden MFC controls, setters write them and
// run the matching On* handler so the persisted state + preview stay in step.

// Tree-type combos. Count / entry text mirror the filled MFC combo (each ends with a blank
// entry). Sel is the current selection; SetSel selects an entry, latches it as the preview
// source, and re-saves the makeup (like the MFC ON_CBN_SELENDOK handler).
int  WBQtGrove_GetTreeTypeCount(int type);
int  WBQtGrove_GetTreeTypeName(int type, int index, char *out, int cap);
int  WBQtGrove_GetTreeTypeSel(int type);
void WBQtGrove_SetTreeTypeSel(int type, int index);

// Per-type weight edit (Per<type>). SetWeight writes the edit box and recomputes/persists the
// weights + running total, mirroring the MFC ON_EN_KILLFOCUS handler.
int  WBQtGrove_GetWeight(int type);
void WBQtGrove_SetWeight(int type, int value);
// The running weight total display (read-only; recomputed on any weight/props-only change).
int  WBQtGrove_GetTotalPerc(void);

// Number-of-trees edit. SetNumTrees writes the edit box and persists it (ON_EN_KILLFOCUS).
int  WBQtGrove_GetNumTrees(void);
void WBQtGrove_SetNumTrees(int value);

// Placement checkboxes. Set* checks/unchecks the button and runs the MFC placement handler so
// the persisted flags GroveTool reads (getCanPlaceInWater / getCanPlaceOnCliffs / isUsePropsOnly)
// stay correct. Toggling props-only also refreshes the weight total display.
int  WBQtGrove_GetAllowWater(void);
void WBQtGrove_SetAllowWater(int on);
int  WBQtGrove_GetAllowCliff(void);
void WBQtGrove_SetAllowCliff(int on);
int  WBQtGrove_GetUsePropsOnly(void);
void WBQtGrove_SetUsePropsOnly(int on);

// Set-name combo (20 named sets). Count / entry text mirror the filled MFC combo. SelectSet
// picks a set, persists the index, and loads that set's tree makeup + ratios (like the MFC
// OnSelchangeGroveSetName). SaveSet / OpenSettings mirror the two buttons.
// RefreshSetNames re-reads Grovesets.ini into the hidden MFC combo (== the MFC ON_CBN_DROPDOWN
// handler), so renames made via the Settings button show up without an app restart; call it
// before re-filling the panel's set-name combo.
void WBQtGrove_RefreshSetNames(void);
int  WBQtGrove_GetSetCount(void);
int  WBQtGrove_GetSetName(int index, char *out, int cap);
int  WBQtGrove_GetCurrentSet(void);
void WBQtGrove_SelectSet(int index);
void WBQtGrove_SaveSet(void);
void WBQtGrove_OpenSettings(void);

// Preview: render the last-changed tree-type combo's template to a fixed 128x128 BGR image
// (3 bytes per pixel) into bgrOut (>= 128*128*3). Returns non-zero if an image was produced.
// Reuses the exact MFC render path (ObjectPreview::qtRenderTemplatePreview).
int  WBQtGrove_GetPreviewSize(int *widthOut, int *heightOut);
int  WBQtGrove_RenderPreview(unsigned char *bgrOut, int cap);

// Forward (Qt-side, WBQtGrovePanel.cpp): a guarded WBQtGrove_PushRefresh() re-seeds the Qt
// panel (weights, total, set makeup, preview) from the current MFC state -- e.g. after a
// SelectSet load pulls in a whole new tree makeup.
void WBQtGrove_PushRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_GROVE_BRIDGE_H
