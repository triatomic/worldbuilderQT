// WBQtFenceBridge.h -- self-contained facade for the Qt Fence Options panel.
//
// Like WBQtPanelBridge.h this header carries ONLY int/double/char* (no Qt or MFC types), so
// the MFC FenceOptions TU and the Qt panel TU can talk without including each other's
// headers. Kept as its own header (not folded into WBQtPanelBridge.h) so the shared facade
// stays untouched. Included by qt/panels/WBQtFencePanel.cpp (Qt side) and
// src/WBQtFenceBridge.cpp (MFC side).
//
// The MFC FenceOptions is still Create()d as the hidden OFF fallback and owns m_objectsList
// (its private per-map template list) + the fence selection statics (m_currentObjectIndex /
// m_fenceSpacing / m_fenceOffset). FenceOptions is itself a *front* for ObjectOptions: picking
// a leaf calls ObjectOptions::selectObject() so the placement path (which reads
// ObjectOptions::getCurGdfName / getCurObjectName / getCurObjectHeight) keeps working. This
// bridge lets the Qt panel MIRROR that filtered list by index and drive the same statics, so
// FenceTool keeps working unchanged.
#ifndef WB_QT_FENCE_BRIDGE_H
#define WB_QT_FENCE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Fence panel: reverse, Qt widget -> MFC FenceOptions (implemented MFC-side,
// WBQtFenceBridge.cpp -> FenceOptions::qt*). Strings copy into caller buffers (no ownership
// crosses the seam). The list the tree is built from is the FILTERED list: when "fence only"
// is on (showAll == 0) only templates with a non-zero fence width are enumerated, and the
// index used everywhere is the running position within that filtered list -- exactly the
// lParam the MFC tree stores. So the "show all object types" flag must be applied BEFORE any
// enumeration; the Qt panel sets it first, then rebuilds.

// Filtered enumeration (depends on the current show-all flag).
int  WBQtFence_GetCount(void);
// Fill the display path for the entry at filteredIndex: owning side, editor-sorting category,
// and leaf (display) name. Buffers are cap bytes each. Returns non-zero on success.
int  WBQtFence_GetEntry(int filteredIndex, char *sideOut, char *sortingOut, char *leafOut, int cap);
// The full (unique) template name for filteredIndex -- used by the search filter.
int  WBQtFence_GetFullName(int filteredIndex, char *nameOut, int cap);

// Selection (Qt -> MFC): make filteredIndex the current fence object. This mirrors the MFC
// TVN_SELCHANGED path: sets m_currentObjectIndex, clears the custom-spacing latch, then runs
// updateObjectOptions() (which calls ObjectOptions::selectObject, recomputes spacing/offset,
// writes the spacing edit box, and refreshes the preview). Passing < 0 selects a grouping node
// (clears the current index like the MFC "item has children" branch).
void WBQtFence_SelectIndex(int filteredIndex);
int  WBQtFence_GetSelectedIndex(void);

// "Fence only" checkbox. showAll != 0 == the MFC m_showAllObjectTypes (checkbox CHECKED means
// show all types). SetShowAll mirrors OnCheckFenceOnly's state flip WITHOUT its MFC-only search
// enable/disable + OnSearch side effects; the Qt panel rebuilds the tree itself afterwards.
int  WBQtFence_GetShowAll(void);
void WBQtFence_SetShowAll(int showAll);

// Fence spacing (world units) -- the editable field. GetSpacing reads m_fenceSpacing (kept in
// sync by SelectIndex); SetSpacing mirrors OnChangeFenceSpacingEdit (stores the value AND sets
// the custom-spacing latch so a later SelectIndex won't overwrite it from the template width).
double WBQtFence_GetSpacing(void);
void   WBQtFence_SetSpacing(double spacing);
// Fence offset (read-only in the panel; derived from the template on selection).
double WBQtFence_GetOffset(void);

// Preview: render the current fence object to a fixed 128x128 BGR image (3 bytes per pixel)
// into bgrOut (>= 128*128*3). Returns non-zero if an image was produced. Reuses the exact MFC
// render path (ObjectPreview::qtRenderTemplatePreview).
int  WBQtFence_GetPreviewSize(int *widthOut, int *heightOut);
int  WBQtFence_RenderPreview(unsigned char *bgrOut, int cap);

// Forward (Qt-side, WBQtFencePanel.cpp): FenceTool calls FenceOptions::update() on activate /
// placement; a guarded WBQtFence_PushRefresh() re-seeds the Qt panel (spacing field + preview)
// from the current selection so the two stay in step.
void WBQtFence_PushRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_FENCE_BRIDGE_H
