// WBQtMeshMoldBridge.h -- self-contained facade for the Qt MeshMold Options panel.
//
// Like WBQtPanelBridge.h this header carries ONLY int/double/char* (no Qt or MFC types), so
// the MFC MeshMoldOptions TU and the Qt panel TU can talk without including each other's
// headers. Kept as its own header (not folded into WBQtPanelBridge.h) so the shared facade
// stays untouched. Included by qt/panels/WBQtMeshMoldPanel.cpp (Qt side) and
// src/WBQtMeshMoldBridge.cpp (MFC side).
//
// The MFC MeshMoldOptions is still Create()d as the hidden OFF fallback and owns the selection
// statics MeshMoldTool reads: m_meshModelName (the selected .w3d mold), m_currentAngle,
// m_currentScale, m_currentHeight, m_doingPreview, and m_raiseOnly / m_lowerOnly. This bridge
// lets the Qt panel drive those statics + fire the same command handlers (OnApplyMesh /
// OnOpenMoldsFolder / OnOpenLinkCreateMolds), so MeshMoldTool keeps working unchanged.
#ifndef WB_QT_MESHMOLD_BRIDGE_H
#define WB_QT_MESHMOLD_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- MeshMold panel: reverse, Qt widget -> MFC MeshMoldOptions (implemented MFC-side,
// WBQtMeshMoldBridge.cpp -> MeshMoldOptions::qt*). Strings copy into caller buffers (no
// ownership crosses the seam).

// Mold list. The .w3d models are enumerated from disk under data\Editor\Molds via TheFileSystem
// (the same list OnInitDialog builds), with the ".w3d" extension and any leading path stripped
// -- the leaf display name that MeshMoldTool selects.
int  WBQtMeshMold_GetCount(void);
// The display (leaf) name of mold entry i. Buffer is cap bytes. Returns non-zero on success.
int  WBQtMeshMold_GetName(int index, char *nameOut, int cap);

// Selection (Qt -> MFC): make the named mold current. Mirrors the MFC TVN_SELCHANGED path --
// stores m_meshModelName, and if preview is on re-runs MeshMoldTool::updateMeshLocation.
void WBQtMeshMold_SelectName(const char *name);
// The currently selected mold name (m_meshModelName). Buffer is cap bytes.
int  WBQtMeshMold_GetSelectedName(char *nameOut, int cap);

// Angle in degrees (MIN_ANGLE..MAX_ANGLE). GetAngle reads m_currentAngle; SetAngle mirrors the
// angle popup-slider / edit path (stores the value; angle does not move the preview mesh).
int  WBQtMeshMold_GetAngle(void);
void WBQtMeshMold_SetAngle(int angleDegrees);

// Scale as a percentage (MIN_SCALE..MAX_SCALE). The MFC stores m_currentScale as a fraction
// (percent/100); the bridge converts. SetScalePercent mirrors the scale slider/edit path
// (stores + MeshMoldTool::updateMeshLocation).
int  WBQtMeshMold_GetScalePercent(void);
void WBQtMeshMold_SetScalePercent(int scalePercent);

// Height as the RAW slider unit (MIN_HEIGHT..MAX_HEIGHT); the MFC stores m_currentHeight =
// rawVal * MAP_HEIGHT_SCALE (the engine cells->feet factor), so the conversion lives MFC-side.
// SetHeightRaw mirrors the height slider/edit path (stores + MeshMoldTool::updateMeshLocation).
int  WBQtMeshMold_GetHeightRaw(void);
void WBQtMeshMold_SetHeightRaw(int heightRaw);

// Preview toggle. GetPreview reads m_doingPreview; SetPreview mirrors OnPreview (sets the flag
// then MeshMoldTool::updateMeshLocation(true) so the preview mesh appears / clears).
int  WBQtMeshMold_GetPreview(void);
void WBQtMeshMold_SetPreview(int on);

// Apply Mesh button -- mirrors OnApplyMesh (MeshMoldTool::apply on the active document).
void WBQtMeshMold_ApplyMesh(void);

// Raise mode: 0 = raise only, 1 = raise+lower (the default), 2 = lower only. GetRaiseMode reads
// m_raiseOnly / m_lowerOnly; SetRaiseMode mirrors OnRaise / OnRaiseLower / OnLower (sets the two
// flags, and re-runs updateMeshLocation when preview is on).
int  WBQtMeshMold_GetRaiseMode(void);
void WBQtMeshMold_SetRaiseMode(int mode);

// The two shell-out buttons -- mirror OnOpenMoldsFolder / OnOpenLinkCreateMolds (ShellExecute).
void WBQtMeshMold_OpenMoldsFolder(void);
void WBQtMeshMold_OpenLink(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MESHMOLD_BRIDGE_H
