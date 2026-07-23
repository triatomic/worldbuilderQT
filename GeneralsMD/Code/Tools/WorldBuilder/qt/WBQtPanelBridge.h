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
// Reverse (MFC-side, src/WBQtHostBridge.cpp): persist the shared option-panel Top/Left the
// way COptionsPanel::OnMove did, so a dragged Qt panel's position survives a restart.
void WBQtPanels_SaveWindowPos(int top, int left);

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

// --- Build List panel: front-end over the hidden MFC BuildList (WBQtBuildListBridge) -------
// The MFC BuildList stays created + hidden (m_staticThis intact) so the tool's addBuilding /
// setSelectedBuildList / update keep reaching it; the Qt panel reads state + fires commands
// through the reverse funcs, and re-reads when WBQtBuildList_PushRefresh fires (from
// BuildList's own refresh path -- tool activate, map placement, undo/redo). Strings copy into
// caller buffers. Rebuilds: -1 == unlimited. PowerPercent = (1-energyUsed)*100.
void WBQtBuildList_PushRefresh(void);	// forward (Qt-side): re-seed the whole Qt panel
int  WBQtBuildList_GetSideCount(void);
int  WBQtBuildList_GetSideName(int i, char *out, int cap);
int  WBQtBuildList_GetCurSide(void);
void WBQtBuildList_SetCurSide(int i);
int  WBQtBuildList_GetBuildCount(void);
int  WBQtBuildList_GetBuildName(int i, char *out, int cap);
int  WBQtBuildList_GetCurBuild(void);
void WBQtBuildList_SetCurBuild(int i);
int    WBQtBuildList_HasCurBuild(void);
double WBQtBuildList_GetAngle(void);
double WBQtBuildList_GetZ(void);
int    WBQtBuildList_GetAlreadyBuilt(void);
int    WBQtBuildList_GetRebuilds(void);
void   WBQtBuildList_SetAngle(double deg);
void   WBQtBuildList_SetZ(double z);
void   WBQtBuildList_SetAlreadyBuilt(int on);
void   WBQtBuildList_SetRebuilds(int nr);
int  WBQtBuildList_GetPowerPercent(void);
void WBQtBuildList_MoveUp(void);
void WBQtBuildList_MoveDown(void);
void WBQtBuildList_AddBuilding(void);
void WBQtBuildList_DeleteBuilding(void);
void WBQtBuildList_Export(void);
void WBQtBuildList_Import(void);
void WBQtBuildList_EditProps(void);
int  WBQtBuildList_GetForcedShow(void);
void WBQtBuildList_SetForcedShow(int on);

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
// Fill the display path for the object at listIndex: an optional pre-side bucket (empty unless the
// template is ES_TEST, in which case it is "TEST", mirroring the MFC pre-side tier), its owning
// side, editor-sorting category, and leaf (display) name. Buffers are cap bytes each. Returns
// non-zero on success.
int  WBQtObject_GetEntry(int listIndex, char *preOut, char *sideOut, char *sortingOut, char *leafOut, int cap);
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
void WBQtObject_PreviewAmbient(void);	// play the selected template's ambient sound if the toggle is on
void WBQtObject_SetPreviewBuildZone(int on);
int  WBQtObject_GetPreviewBuildZone(void);
void WBQtObject_SetUseWaterHeight(int on);
int  WBQtObject_GetUseWaterHeight(void);
void WBQtObject_SetRenderParticles(int on);	// live particle preview (WBParticleRuntime)
int  WBQtObject_GetRenderParticles(void);
void WBQtObject_SetTutorialPrompts(int on);	// one-time hint toasts + Ctrl+A confirm; default ON
int  WBQtObject_GetTutorialPrompts(void);

// Place-all-in-category: one click places the whole tree category (side + editor
// sorting) of the selected object as a single undoable grid (ObjectTool reads it).
void WBQtObject_SetPlaceAll(int on);
int  WBQtObject_GetPlaceAll(void);
void WBQtObject_SetPlaceAllYSpacing(int spacing);	// world units; 0 = automatic
int  WBQtObject_GetPlaceAllYSpacing(void);

// NewSearch ([QtSearch] NewSearch in WorldBuilder.ini, default off): when on, the
// tree pickers (object/fence/road/terrain + the Edit Action / Pick Unit modals) filter
// live as you type instead of needing a Search/Find click. Off = the old behavior.
int  WBQtConfig_GetNewSearch(void);
void WBQtConfig_SetNewSearch(int on);

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
// flagsOut bits: 1 active, 2 hasWarnings, 4 subroutine (0 for player nodes).
int  WBQtScript_GetNode(int i, int *depthOut, int *listTypeOut, int *flagsOut, char *labelOut, int cap);
void WBQtScript_SetSelection(int listTypeInt);
int  WBQtScript_HasScript(void);
int  WBQtScript_HasGroup(void);
void WBQtScript_NewFolder(void);
void WBQtScript_NewScript(void);
void WBQtScript_EditScript(void);
void WBQtScript_CopyScript(void);
void WBQtScript_Delete(void);
// Rename the current script/folder in place (undoable). Returns 1 if renamed, 0 on empty/no-change.
int  WBQtScript_RenameSelection(const char *newName);
void WBQtScript_GetSelectionName(char *buf, int cap);	// current selection's bare name (rename prefill)
// "Confirm before delete" preference (default 1). The Qt Delete path reads it; a "don't ask again".
int  WBQtScript_GetConfirmDelete(void);
void WBQtScript_SetConfirmDelete(int on);
// Select + center a named map entity (isWaypoint 0 = placed unit, 1 = waypoint). Returns 1 if found.
int  WBQtScript_SelectEntity(const char *name, int isWaypoint);
void WBQtScript_Commit(void);	// == OnOK  (commit the working model, save)
void WBQtScript_Cancel(void);	// == OnCancel (discard)
// 9b: drag-drop reorder/move (reuses doDropOn incl. Ctrl auto-merge) and search. Both listType
// args are packed ListType ints. FindNext returns the next matching node's ListType via *out
// (non-zero return on a hit); fromListType 0 starts from the top.
void WBQtScript_DropOn(int dragListType, int targetListType);
int  WBQtScript_FindNext(const char *text, int fromListType, int *outListType);
// Live tree filter: 1 if the node (label, or script deep-scan) matches text; empty text => 1.
int  WBQtScript_NodeMatches(int listType, const char *text, const char *label);
// Find/replace across script condition/action parameter values. Returns the match count.
// doReplace 0 = count only; 1 = rewrite matches (undoable). matchCase/wholeValue toggle the mode.
// scopeListType -1 = all scripts; else a packed ListType limiting to that one selected script.
int  WBQtScript_ReplaceInParams(const char *find, const char *replace,
	int matchCase, int wholeValue, int doReplace, int scopeListType);
// Next script (tree order, after fromListType) with a matching parameter value; 1 + outListType, or 0.
int  WBQtScript_FindNextParamMatch(int fromListType, const char *find,
	int matchCase, int wholeValue, int *outListType);
// Autocomplete: distinct parameter values containing substr, as "value\tcount\n" lines (most-used
// first) in buf. Returns the total distinct count. scopeListType -1 = all; else that one script.
int  WBQtScript_CollectParamValues(const char *substr, char *buf, int cap, int scopeListType);
// 9c: recompute warnings (Verify) and toggle the current selection's active flag.
void WBQtScript_Verify(void);
void WBQtScript_ToggleActive(void);
// 9d: fill the description + comment detail panels for the node at listTypeInt.
void WBQtScript_GetDetail(int listTypeInt, char *descOut, int descCap, char *commentOut, int commentCap);

// 9d option checkboxes. `which` is one of these ids; Get reads the current state, Set applies
// it (updates the hidden MFC checkbox + runs the real On* handler so persistence + side
// effects match). Setting a label-affecting box (e.g. CleanScriptName) may rebuild the tree,
// so the Qt window rebuilds after any Set.
enum {
	WBQT_SCK_COMPRESS = 0, WBQT_SCK_NEWICONS, WBQT_SCK_CLEANNAME, WBQT_SCK_AUTOVERIFY,
	WBQT_SCK_SMARTCOPY, WBQT_SCK_FASTLOAD, WBQT_SCK_SCRIPTMERGE, WBQT_SCK_REFBYPARAM,
	WBQT_SCK_DISABLEREF
};
int  WBQtScript_GetCheckbox(int which);
void WBQtScript_SetCheckbox(int which, int checked);

// 9d remaining buttons -> the existing handlers (each may pop MFC dialogs / mutate the model).
void WBQtScript_AddDebug(void);
void WBQtScript_RemoveDebug(void);
void WBQtScript_PatchGC(void);
void WBQtScript_ExportScripts(void);
void WBQtScript_ImportScripts(void);
void WBQtScript_SaveNow(void);

// Editor-local undo/redo over the uncommitted working copy (snapshots taken by every
// mutating command). Returns 1 when a state was restored (the Qt window then rebuilds
// its tree), 0 when the stack is empty.
int  WBQtScript_Undo(void);
int  WBQtScript_Redo(void);

// Exact-name script lookup (the detail pane's clickable "[Referenced in]" links):
// returns the packed ListType int, or -1 when no script has that name.
int  WBQtScript_FindScriptByName(const char *name);

// Locate the first condition/action of the CURRENT script carrying an OBJECT_TYPE parameter
// with this value ("???" matches empty values): tabOut = the edit dialog's tab (1=Conditions,
// 2=Actions if true, 3=Actions if false), rowOut = the row in that tab's list. Returns 1 when
// found, 0 otherwise. Backs the detail pane's clickable "[Missing]" links.
int  WBQtScript_FindObjectParamLocation(const char *value, int *tabOut, int *rowOut);

// Forward (Qt-side, WBQtScriptWindow): open/close the Qt Script window. WBQtScript_Open is
// called from CMainFrame::onEditScripts after the hidden MFC dialog is created; it builds
// the window rooted in frameHwnd. WBQtScript_Close tears it down. IsOpen/Focus let
// onEditScripts re-focus an already-open editor instead of recreating the session.
void WBQtScript_Open(void *frameHwnd, int x, int y);
void WBQtScript_Close(void);
int  WBQtScript_IsOpen(void);
void WBQtScript_Focus(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PANEL_BRIDGE_H
