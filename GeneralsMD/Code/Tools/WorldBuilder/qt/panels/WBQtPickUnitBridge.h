// WBQtPickUnitBridge.h -- opaque facade for the Qt "Pick A Unit" / "Replace Missing Unit"
// dialogs (Tier 3e), the native rebuilds of PickUnitDialog/ReplaceUnitDialog. The MFC side
// (src/WBQtPickUnitBridge.cpp) supplies the filtered template catalog and the shared 128x128
// preview render. The _Run entry points return -1 when Qt is not up yet (a map opened from the
// command line validates its objects BEFORE WBQt_Startup) so callers fall back to the MFC
// dialogs. BuildListTool's modeless pick panel runs through the _BuildPickPanel entry points.
#ifndef WB_QT_PICKUNIT_BRIDGE_H
#define WB_QT_PICKUNIT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============ MFC -> Qt (implemented in qt/panels/WBQtPickUnitDialog.cpp) ============

// Run the Qt "Pick A Unit" modal (== PickUnitDialog::DoModal). allowable is the list of
// EditorSortingType values to include. Returns 1 on OK (nameOut = the picked template name;
// may be empty when a folder was selected -- callers no-op, like getPickedThing() returning
// NULL), 0 on cancel, -1 when Qt is unavailable (fall back to the MFC dialog).
int WBQtPickUnit_Run(void *frameHwnd, const int *allowable, int allowCount, int factionOnly,
	char *nameOut, int nameCap);

// Run the Qt "Replace Missing Unit" modal (== ReplaceUnitDialog::DoModal). Returns 1 on OK,
// 2 on "Continue without replacing..." (== IDIGNORE), 0 on cancel, -1 when Qt is unavailable.
// Returns 3 for "Replace All": the caller stops prompting and auto-resolves every remaining
// missing name by best name match (see WBQtReplaceUnit_BestMatch), collecting the results for
// the report window.
int WBQtReplaceUnit_Run(void *frameHwnd, const char *missingName, const int *allowable,
	int allowCount, int factionOnly, char *nameOut, int nameCap);

#define WBQT_REPLACE_OK				1
#define WBQT_REPLACE_IGNORE			2
#define WBQT_REPLACE_ALL			3

// Best name match for `missingName` among the catalog built by WBQtPickUnitData_Build, using the
// same ranking the replace dialog's suggestion uses. Returns 1 and fills nameOut when something
// clears the similarity bar, 0 otherwise (the caller leaves that name unreplaced and the report
// shows it as unresolved).
int WBQtReplaceUnit_BestMatch(const char *missingName, const int *allowable, int allowCount,
	int factionOnly, char *nameOut, int nameCap);

// The same ranking over an arbitrary candidate list, for callers whose catalog is not the
// template catalog -- e.g. command buttons. Returns 1 and fills nameOut when something clears the
// similarity bar, 0 otherwise. Takes an array of C strings rather than one joined blob: the
// catalogs are big enough (ZH has thousands of command buttons) that joining them overflows
// AsciiString's 32K MAX_LEN and throws.
int WBQtNameMatch_BestOfList(const char *missingName, const char *const *candidates,
	int candidateCount, char *nameOut, int nameCap);

// Show the "Replaced Missing Units" report over the frame: one row per missing name and what it
// became, editable so a wrong guess can be corrected. Modal; returns when the user closes it.
// Rows come from the WBQtReplaceReport_* accessors below.
void WBQtReplaceReport_Run(void *frameHwnd);

// ---- report contents, filled by the validate pass before _Run (src/WBQtPickUnitBridge.cpp) ----

// What a report's rows refer to. Map-object rows re-point placed objects and can select them in
// the viewport; script rows rewrite OBJECT_TYPE parameters and team rows rewrite the team
// templates' unit-type slots -- neither has anything to select.
#define WBQT_REPLACE_SOURCE_MAPOBJECTS	0
#define WBQT_REPLACE_SOURCE_SCRIPTS		1
#define WBQT_REPLACE_SOURCE_TEAMS		2
#define WBQT_REPLACE_SOURCE_BUILDLIST	3

// Start a batch: drop all rows and fix what this batch's rows describe. One source per batch --
// taking it as an argument here means the two cannot be set out of order.
void WBQtReplaceReport_Begin(int source);
int WBQtReplaceReport_GetSource(void);
// Record that `missingName` was replaced by `replacementName` (empty == left unreplaced).
void WBQtReplaceReport_Add(const char *missingName, const char *replacementName, int objectCount);
// True when at least one row was recorded, i.e. the report is worth showing.
int WBQtReplaceReport_HasRows(void);
// Fill in each row's object count. Call once the validate pass has re-pointed the objects.
void WBQtReplaceReport_CountObjects(void);

int WBQtReplaceReport_GetCount(void);
void WBQtReplaceReport_GetRow(int i, char *missingOut, int missingCap,
	char *replacementOut, int replacementCap, int *objectCountOut);
// Re-point every object still carrying `missingName` at `replacementName` (empty to undo the
// replacement), updating the row. Applied live as the user edits the report.
void WBQtReplaceReport_SetReplacement(int i, const char *replacementName);
// Select every object that came from row i's missing name and centre the view on the first.
void WBQtReplaceReport_SelectRow(int i);

// ---- BuildListTool's modeless pick panel (== PickUnitDialog Create/SetupAsPanel) ----

// Create (first call; allowable/factionOnly as in _Run, top/left = the saved
// BUILD_PICK_PANEL_SECTION position) and show the floating panel without activating
// (== ShowWindow(SW_SHOWNA)). Returns 1, or 0 when Qt is not up (fall back to the
// MFC panel).
int WBQtBuildPickPanel_Show(const int *allowable, int allowCount, int factionOnly,
	int top, int left);
void WBQtBuildPickPanel_Hide(void);
int WBQtBuildPickPanel_IsVisible(void);
// The LIVE tree selection (== getPickedUnit): empty when a folder/nothing is selected.
void WBQtBuildPickPanel_GetPicked(char *nameOut, int nameCap);
// == PickUnitDialog::ResetWindowPosition (View > Reset Window Positions).
void WBQtBuildPickPanel_ResetPos(int top, int left);

// ============ Qt -> MFC (implemented in src/WBQtPickUnitBridge.cpp) ============

// Build the filtered template catalog (== PickUnitDialog::OnInitDialog's allowable-sorting +
// factionOnly==isBuildableItem filter); returns the row count readable via GetInfo.
int WBQtPickUnitData_Build(const int *allowable, int allowCount, int factionOnly);
int WBQtPickUnitData_GetInfo(int i, char *nameOut, int nameCap, char *sideOut, int sideCap,
	char *sortingOut, int sortingCap, int *isTestOut);

// Render the named template through the shared MFC preview path
// (ObjectPreview::qtRenderTemplatePreview): 128x128 BGR, bottom-up rows.
int WBQtPickUnit_RenderPreview(const char *templateName, unsigned char *bgrOut, int cap);

// Persist the dialog position (== PickUnitDialog::OnMove -> the PickUnitWindow Top/Left
// profile shared with BuildListTool's pick panel).
void WBQtPickUnit_SavePos(int top, int left);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PICKUNIT_BRIDGE_H
