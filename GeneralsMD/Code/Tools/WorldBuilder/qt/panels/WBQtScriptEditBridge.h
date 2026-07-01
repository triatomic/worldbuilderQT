// WBQtScriptEditBridge.h -- opaque facade for the Qt Script *edit* dialog (Tier 2a): the native
// Qt tabbed modal that replaces the MFC CPropertySheet (ScriptProperties / ScriptConditionsDlg /
// ScriptActionsTrue / ScriptActionsFalse) opened by ScriptDialog::OnNewScript / OnEditScript.
//
// Model: the dialog edits a caller-owned Script* (a fresh script for New, a duplicate for Edit)
// passed through as an opaque void*. All reads/writes/list surgery run MFC-side in
// src/WBQtScriptEditBridge.cpp (ports of the property pages' handlers, calling the same
// Script/OrCondition/Condition/ScriptAction accessors); the Qt side only renders lists and
// forwards row-indexed commands. The New/Edit ops pop the Qt condition/action editor
// (WBQtCondActBridge.h, Tier 2b). Commit/cancel semantics are the caller's (ScriptDialog),
// exactly as with the old sheet: accepted -> insertScript()/updateFrom(), rejected ->
// deleteInstance().
//
// Plain C surface (int / const char* / void* only) so no Qt header reaches the MFC side and
// no afx reaches the Qt lib.
#ifndef WB_QT_SCRIPT_EDIT_BRIDGE_H
#define WB_QT_SCRIPT_EDIT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ---- text fields (WBQtScriptEditData_GetText/SetText) ----
#define WB_QT_SCRIPTEDIT_TEXT_NAME				0
#define WB_QT_SCRIPTEDIT_TEXT_COMMENT			1
#define WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT	2
#define WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT	3

// ---- boolean flags (WBQtScriptEditData_GetFlag/SetFlag) ----
#define WB_QT_SCRIPTEDIT_FLAG_ACTIVE		0
#define WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT		1
#define WB_QT_SCRIPTEDIT_FLAG_EASY			2
#define WB_QT_SCRIPTEDIT_FLAG_NORMAL		3
#define WB_QT_SCRIPTEDIT_FLAG_HARD			4
#define WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE	5

// ================= MFC -> Qt (implemented in qt/panels/WBQtScriptEditDialog.cpp) =================

// Run the modal Qt editor over `script` (an opaque Script*). `frameHwnd` is the MFC main frame,
// disabled for the dialog's lifetime (== the old DoModal discipline). Returns 1 if accepted (OK),
// 0 if cancelled. The window title is the script's name (live-updates as it is edited).
int WBQtScriptEdit_Run(void *script, void *frameHwnd);

// ================= Qt -> MFC (implemented in src/WBQtScriptEditBridge.cpp) =================

// --- Properties tab ---
void WBQtScriptEditData_GetText(void *script, int field, char *buf, int cap);
void WBQtScriptEditData_SetText(void *script, int field, const char *text);
int  WBQtScriptEditData_GetFlag(void *script, int flag);
void WBQtScriptEditData_SetFlag(void *script, int flag, int value);
int  WBQtScriptEditData_GetDelaySeconds(void *script);
void WBQtScriptEditData_SetDelaySeconds(void *script, int seconds);

// --- Conditions tab ---
// Rows mirror the MFC listbox: "*** IF ***"/"*** OR ***" header rows interleaved with
// condition rows. Count refreshes the script's warning state first (== loadList()).
int  WBQtScriptEditData_GetConditionRowCount(void *script);
// Fills the row label; returns 1 if the row is a condition, 0 if an OR header, -1 if out of range.
int  WBQtScriptEditData_GetConditionRow(void *script, int row, char *buf, int cap);
// Commands keyed by the selected row (-1 = no selection). Each returns the row to select after
// the rebuild, or -1 if nothing changed (cancelled modal / no-op).
int  WBQtScriptEdit_ConditionNew(void *script, int row);
int  WBQtScriptEdit_ConditionEdit(void *script, int row);
int  WBQtScriptEdit_ConditionOr(void *script, int row);
int  WBQtScriptEdit_ConditionCopy(void *script, int row);
int  WBQtScriptEdit_ConditionDelete(void *script, int row);
int  WBQtScriptEdit_ConditionMoveUp(void *script, int row);
int  WBQtScriptEdit_ConditionMoveDown(void *script, int row);

// --- Actions tabs (isFalse: 0 = "if true" list, 1 = "if false" list) ---
int  WBQtScriptEditData_GetActionCount(void *script, int isFalse);
void WBQtScriptEditData_GetActionLabel(void *script, int isFalse, int index, char *buf, int cap);
// Commands keyed by the selected action index (-1 = no selection); return the index to select
// after the rebuild, or -1 if nothing changed.
int  WBQtScriptEdit_ActionNew(void *script, int isFalse, int index);
int  WBQtScriptEdit_ActionEdit(void *script, int isFalse, int index);
int  WBQtScriptEdit_ActionCopy(void *script, int isFalse, int index);
int  WBQtScriptEdit_ActionDelete(void *script, int isFalse, int index);
int  WBQtScriptEdit_ActionMoveUp(void *script, int isFalse, int index);
int  WBQtScriptEdit_ActionMoveDown(void *script, int isFalse, int index);
// Move the action to the other list (true<->false); returns 0 (select the top) or -1.
int  WBQtScriptEdit_ActionMoveToOther(void *script, int isFalse, int index);

// --- Smart Copy setting (registry-backed, shared by the three list tabs) ---
int  WBQtScriptEdit_GetSmartCopy(void);
void WBQtScriptEdit_SetSmartCopy(int enabled);

// --- sub-modal ownership: the Qt dialog registers its HWND while open, for MFC sub-modals
//     that need an explicit owner (the parameter editors resolve theirs via GetActiveWindow) ---
void WBQtScriptEdit_SetModalOwner(void *hwnd);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_SCRIPT_EDIT_BRIDGE_H
