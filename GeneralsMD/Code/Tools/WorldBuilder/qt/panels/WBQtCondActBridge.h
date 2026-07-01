// WBQtCondActBridge.h -- opaque facade for the Qt condition/action editor (Tier 2b): the native
// Qt dialog replacing the MFC EditCondition / EditAction modals (IDD_ScriptCondition /
// IDD_ScriptAction). One dialog serves both -- the two MFC classes are structural twins over
// Condition vs ScriptAction -- selected by the isAction flag on every call.
//
// Model: the dialog edits a caller-owned Condition* / ScriptAction* passed through as an opaque
// void*. The MFC side (src/WBQtCondActBridge.cpp) walks TheScriptEngine's template catalog
// (name paths split on '/' build the category tree), reads the item's interleaved UI segments
// (uiStrings[0], param[0], uiStrings[1], param[1], ...) and per-parameter warning/info text,
// and pops the still-MFC EditParameter editors when a parameter link is clicked (they resolve
// their owner via GetActiveWindow == the Qt dialog; they go native in the next stage).
#ifndef WB_QT_CONDACT_BRIDGE_H
#define WB_QT_CONDACT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ================= MFC -> Qt (implemented in qt/panels/WBQtCondActDialog.cpp) =================

// Run the modal Qt editor over `item` (Condition* if isAction==0, ScriptAction* if 1).
// Returns 1 if accepted (OK), 0 if cancelled. Parents to the active Qt modal (the script-edit
// dialog), so no frame bookkeeping is needed here.
int WBQtCondAct_Run(void *item, int isAction);

// ================= Qt -> MFC (implemented in src/WBQtCondActBridge.cpp) =================

// --- the template catalog (static per session) ---
int  WBQtCondActData_GetTemplateCount(int isAction);
void WBQtCondActData_GetTemplateName(int isAction, int i, char *buf, int cap);	// '/'-separated path
void WBQtCondActData_GetTemplateName2(int isAction, int i, char *buf, int cap);	// alt path or ""
void WBQtCondActData_GetTemplateHelp(int isAction, int i, char *buf, int cap);

// --- the edited item ---
int  WBQtCondActData_GetType(void *item, int isAction);
void WBQtCondActData_SetType(void *item, int isAction, int type);
// Clear the item's warning flag on open (== the MFC OnInitDialog's setWarnings(false)).
void WBQtCondActData_ClearWarningFlag(void *item, int isAction);

// --- the parameter sentence: uiStrings[0], param[0], uiStrings[1], param[1], ... ---
int  WBQtCondActData_GetUiStringCount(void *item, int isAction);
void WBQtCondActData_GetUiString(void *item, int isAction, int i, char *buf, int cap);
int  WBQtCondActData_GetParameterCount(void *item, int isAction);
void WBQtCondActData_GetParameterText(void *item, int isAction, int i, char *buf, int cap);

// Pop the MFC parameter editor for parameter i (== clicking the link in the rich edit).
void WBQtCondAct_EditParameter(void *item, int isAction, int i);

// Concatenated per-parameter warning and information text (either may come back empty).
void WBQtCondActData_GetWarnings(void *item, int isAction, char *warnBuf, int warnCap, char *infoBuf, int infoCap);

// --- the "Compress Script" tree-density toggle (registry-backed, "CompressScripts") ---
int  WBQtCondAct_GetCompress(void);
void WBQtCondAct_SetCompress(int enabled);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_CONDACT_BRIDGE_H
