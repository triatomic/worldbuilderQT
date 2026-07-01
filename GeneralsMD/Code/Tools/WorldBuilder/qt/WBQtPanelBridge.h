// WBQtPanelBridge.h -- opaque facade for migrating WorldBuilder option panels to Qt.
//
// Like WBQtBridge.h, this header carries ONLY int/void* (no Qt or MFC types), so the MFC
// frame / option-panel TUs can drive Qt panels and the Qt panels can drive the MFC tools,
// without either side including the other's headers. Each migrated panel adds its own
// forward (tool->widget) and reverse (widget->tool) functions below.
#ifndef WB_QT_PANEL_BRIDGE_H
#define WB_QT_PANEL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Generic options-panel host (implemented Qt-side, called from CMainFrame) ----------
// If dialogID is a migrated Qt panel, create/show it at screen (x,y) size (w,h), hide any
// other Qt panel, and return non-zero. Otherwise return 0 (caller shows the MFC panel).
int  WBQt_ShowOptionsPanel(void *frameHwnd, int dialogID, int x, int y, int w, int h);
void WBQt_HideOptionsPanel(void);

// --- Feather panel: forward push, tool -> Qt widget (implemented Qt-side) ---------------
void WBQtFeather_PushFeather(int v);
void WBQtFeather_PushRadius(int v);
void WBQtFeather_PushRate(int v);

// --- Feather panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtFeatherBridge) -
void WBQtFeather_SetFeather(int v);
void WBQtFeather_SetRadius(int v);
void WBQtFeather_SetRate(int v);
void WBQtFeather_ToggleMirror(void);
void WBQtFeather_ToggleMirrorX(void);
void WBQtFeather_ToggleMirrorY(void);
void WBQtFeather_ToggleMirrorXY(void);
int  WBQtFeather_GetFeather(void);
int  WBQtFeather_GetRadius(void);
int  WBQtFeather_GetRate(void);
int  WBQtFeather_GetMirror(void);
int  WBQtFeather_GetMirrorX(void);
int  WBQtFeather_GetMirrorY(void);
int  WBQtFeather_GetMirrorXY(void);

// --- Brush panel: forward push, tool -> Qt widget (implemented Qt-side) ------------------
void WBQtBrush_PushWidth(int v);
void WBQtBrush_PushFeather(int v);
void WBQtBrush_PushHeight(int v);

// --- Brush panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtBrushBridge) ------
void WBQtBrush_SetWidth(int v);
void WBQtBrush_SetFeather(int v);
void WBQtBrush_SetHeight(int v);
void WBQtBrush_ToggleMirror(void);
void WBQtBrush_ToggleMirrorX(void);
void WBQtBrush_ToggleMirrorY(void);
void WBQtBrush_ToggleMirrorXY(void);
int  WBQtBrush_GetWidth(void);
int  WBQtBrush_GetFeather(void);
int  WBQtBrush_GetHeight(void);
int  WBQtBrush_GetMirror(void);
int  WBQtBrush_GetMirrorX(void);
int  WBQtBrush_GetMirrorY(void);
int  WBQtBrush_GetMirrorXY(void);

// --- Mound panel: forward push, tool -> Qt widget (implemented Qt-side) ------------------
void WBQtMound_PushWidth(int v);
void WBQtMound_PushFeather(int v);
void WBQtMound_PushHeight(int v);

// --- Mound panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtMoundBridge) ------
void WBQtMound_SetWidth(int v);
void WBQtMound_SetFeather(int v);
void WBQtMound_SetHeight(int v);
void WBQtMound_ToggleMirror(void);
void WBQtMound_ToggleMirrorX(void);
void WBQtMound_ToggleMirrorY(void);
void WBQtMound_ToggleMirrorXY(void);
int  WBQtMound_GetWidth(void);
int  WBQtMound_GetFeather(void);
int  WBQtMound_GetHeight(void);
int  WBQtMound_GetMirror(void);
int  WBQtMound_GetMirrorX(void);
int  WBQtMound_GetMirrorY(void);
int  WBQtMound_GetMirrorXY(void);

// --- Ruler panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtRulerBridge) -------
// No forward push: like the MFC RulerOptions, the panel only reads tool state at seed time
// and when the user changes a control. Lengths cross the seam as doubles in FEET (world
// units); the tool stores a radius, so the panel does the diameter<->radius halving/doubling
// and the feet<->meters display conversion. GetType returns RulerTypeEnum: 0 = none,
// 1 = line, 2 = circle (== RULER_CIRCLE) -- only 2 is a circle.
void   WBQtRuler_SetLengthFeet(double radiusFeet);
double WBQtRuler_GetLengthFeet(void);
int    WBQtRuler_SwitchType(void);	// toggles line<->circle, returns non-zero if it changed
int    WBQtRuler_GetType(void);
void   WBQtRuler_SetUseMeters(int on);
int    WBQtRuler_GetUseMeters(void);
double WBQtRuler_ToDisplayUnits(double feet);
void   WBQtRuler_SetShowGrid(int on);
int    WBQtRuler_GetShowGrid(void);
void   WBQtRuler_RepaintViews(void);	// == pDoc->updateAllViews() so the in-view label refreshes

// --- Ramp panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtRampBridge) --------
// RampTool reads width + the apply latch off the MFC RampOptions object (TheRampOptions),
// which is still created as the hidden OFF fallback, so the Qt panel writes THAT object's
// state via these calls; mirror state lives in RampTool statics as usual. Width is a double
// in world units. No forward push (nothing pushes width back to the panel).
void   WBQtRamp_SetWidth(double width);
double WBQtRamp_GetWidth(void);
void   WBQtRamp_Apply(void);
void   WBQtRamp_ToggleMirror(void);
void   WBQtRamp_ToggleMirrorX(void);
void   WBQtRamp_ToggleMirrorY(void);
void   WBQtRamp_ToggleMirrorXY(void);
int    WBQtRamp_GetMirror(void);
int    WBQtRamp_GetMirrorX(void);
int    WBQtRamp_GetMirrorY(void);
int    WBQtRamp_GetMirrorXY(void);

// --- Object panel: the object browser (tree + preview + owning team + height + search) ----
// The MFC ObjectOptions is still created as the hidden OFF fallback and owns m_objectsList
// (the full template list) + the statics the placement tools read (m_currentObjectIndex /
// m_currentObjectName / m_curOwnerName). The Qt panel MIRRORS that list by index and writes
// those statics through the bridge, so ObjectTool / FenceTool / etc. keep working unchanged.
// All strings are copied into caller-provided char buffers (no ownership crosses the seam).

// Object list (MFC-side, WBQtObjectBridge): enumerate the template list to build the tree.
int  WBQtObject_GetCount(void);
// Fill the display path for the object at listIndex: its owning side, editor-sorting category,
// and leaf (display) name. Buffers are cap bytes each. Returns non-zero on success.
int  WBQtObject_GetEntry(int listIndex, char *sideOut, char *sortingOut, char *leafOut, int cap);
// The full (unique) template name for listIndex -- used as the tree item's identity/tooltip.
int  WBQtObject_GetFullName(int listIndex, char *nameOut, int cap);

// Selection (Qt -> MFC): make listIndex the current object (drives placement).
void WBQtObject_SelectIndex(int listIndex);
int  WBQtObject_GetSelectedIndex(void);

// Owning-team combo. GetTeamCount/GetTeamName give the list (with the "(neutral)" relabel);
// GetDefaultTeamForCurrent returns the index to preselect for the current object; SetTeam
// applies a choice to m_curOwnerName (mirrors OnEditchangeOwningteam).
int  WBQtObject_GetTeamCount(void);
int  WBQtObject_GetTeamName(int teamIndex, char *nameOut, int cap);
int  WBQtObject_GetDefaultTeamForCurrent(void);
void WBQtObject_SetTeam(int teamIndex);

// Placement height: the panel writes it, ObjectOptions::getCurObjectHeight() reads it back.
void WBQtObject_SetHeight(int height);
int  WBQtObject_GetHeight(void);

// Preview: render the current object to a PREVIEW_WIDTH*PREVIEW_HEIGHT BGR image (3 bytes
// per pixel) into rgbOut (>= w*h*3). Returns non-zero if an image was produced.
int  WBQtObject_GetPreviewSize(int *widthOut, int *heightOut);
int  WBQtObject_RenderPreview(unsigned char *bgrOut, int cap);

// Preview toggles (persisted in the registry like the MFC panel).
void WBQtObject_SetPreviewSound(int on);
int  WBQtObject_GetPreviewSound(void);
void WBQtObject_SetPreviewBuildZone(int on);
int  WBQtObject_GetPreviewBuildZone(void);
void WBQtObject_SetUseWaterHeight(int on);
int  WBQtObject_GetUseWaterHeight(void);

// Forward (Qt-side): WB calls ObjectOptions::update()/selectObject() on selection changes;
// a guarded WBQtObject_PushFromSelection() re-seeds the Qt panel (label/team/preview) and
// WBQtObject_PushSelectIndex() moves the tree selection to match a programmatic selectObject.
void WBQtObject_PushFromSelection(void);
void WBQtObject_PushSelectIndex(int listIndex);

// --- Script editor (Phase 9a): Qt window is a front-end over the hidden MFC ScriptDialog ---
// The MFC ScriptDialog is still Create()d (hidden) so its working model (m_sides), the
// sub-editors, and OK/Cancel commit all stay MFC-side. These reverse callbacks (MFC-side,
// WBQtScriptBridge -> ScriptDialog::qt*) let the Qt tree read the flat model, drive the
// selection, and invoke the same command handlers. isActive is non-zero while the MFC
// dialog exists. tree nodes: depth 0 = player, 1 = group or ungrouped script, 2 = script in
// group; listType is the packed ListType int (opaque -- only round-tripped back via
// SetSelection). The Qt window OWNS its lifetime; on close it calls Commit or Cancel.
int  WBQtScript_IsActive(void);
int  WBQtScript_GetNodeCount(void);
int  WBQtScript_GetNode(int i, int *depthOut, int *listTypeOut, char *labelOut, int cap);
void WBQtScript_SetSelection(int listTypeInt);
int  WBQtScript_HasScript(void);
int  WBQtScript_HasGroup(void);
void WBQtScript_NewFolder(void);
void WBQtScript_NewScript(void);
void WBQtScript_EditScript(void);
void WBQtScript_CopyScript(void);
void WBQtScript_Delete(void);
void WBQtScript_Commit(void);	// == OnOK  (commit the working model, save)
void WBQtScript_Cancel(void);	// == OnCancel (discard)
// 9b: drag-drop reorder/move (reuses doDropOn incl. Ctrl auto-merge) and search. Both listType
// args are packed ListType ints. FindNext returns the next matching node's ListType via *out
// (non-zero return on a hit); fromListType 0 starts from the top.
void WBQtScript_DropOn(int dragListType, int targetListType);
int  WBQtScript_FindNext(const char *text, int fromListType, int *outListType);

// Forward (Qt-side, WBQtScriptWindow): open/close the Qt Script window. WBQtScript_Open is
// called from CMainFrame::onEditScripts after the hidden MFC dialog is created; it builds
// the window rooted in frameHwnd. WBQtScript_Close tears it down.
void WBQtScript_Open(void *frameHwnd, int x, int y);
void WBQtScript_Close(void);

// Non-zero when the Qt Script window (or one of its child controls) has the Win32 keyboard
// focus. The MFC frame's PreTranslateMessage checks this and SKIPS accelerator translation
// so single-key tool shortcuts don't eat keystrokes meant for the script editor's fields.
int  WBQtScript_OwnsFocus(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PANEL_BRIDGE_H
