// WBQtParamBridge.h -- opaque facade for the Qt parameter editors (Tier 2c): the native Qt
// dialogs replacing EditParameter (the generic single-value editor), EditCoordParameter,
// EditObjectParameter, the CColorDialog color picker, and EditGroup.
//
// Model: the dialogs edit a caller-owned Parameter* / ScriptGroup* passed as an opaque void*.
// The MFC side (src/WBQtParamBridge.cpp) reuses EditParameter's load* combo fillers verbatim
// by loading into a hidden CBS_SORT CComboBox and marshalling the rows out by index -- so the
// option contents/order and the index-based write-back (BOOLEAN/COMPARISON/KIND_OF/...) match
// the MFC dialog exactly. WBQtParam_Edit is the full dispatcher (== EditParameter::edit).
#ifndef WB_QT_PARAM_BRIDGE_H
#define WB_QT_PARAM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// which control the generic dialog shows
#define WB_QT_PARAM_KIND_EDIT	0	// free-text edit (INT/REAL/ANGLE/PERCENT/TEXT_STRING/...)
#define WB_QT_PARAM_KIND_COMBO	1	// editable combo (free text + option list)
#define WB_QT_PARAM_KIND_LIST	2	// selection-only list

// which editor WBQtParam_Edit dispatches to
#define WB_QT_PARAM_EDITOR_GENERIC	0
#define WB_QT_PARAM_EDITOR_COORD	1
#define WB_QT_PARAM_EDITOR_OBJECT	2
#define WB_QT_PARAM_EDITOR_COLOR	3

// ================= MFC -> Qt (implemented in qt/panels/WBQtParamDialog.cpp) =================

// Run the right Qt editor for the parameter (== EditParameter::edit's dispatch). Returns 1 on
// OK, 0 on cancel. unitName feeds the ability loaders (may be NULL/empty).
int WBQtParam_Edit(void *parameter, const char *unitName);

// Run the Qt group editor (== EditGroup). frameHwnd is disabled for the modal's lifetime.
int WBQtEditGroup_Run(void *scriptGroup, void *frameHwnd);

// ================= Qt -> MFC (implemented in src/WBQtParamBridge.cpp) =================

int  WBQtParamData_GetEditorKind(void *parameter);

// Fill the hidden combo with the parameter's options; report caption / control kind / audio
// preview row / initial selection / initial text. Returns the option count.
int  WBQtParamData_Describe(void *parameter, const char *unitName,
	char *captionOut, int captionCap, int *kindOut, int *showAudioOut, int *initialSelOut,
	char *initialTextOut, int textCap);
void WBQtParamData_GetOption(int i, char *buf, int cap);

// Fill the hidden combo with the subroutine-script list (read back via GetOption); returns the
// count. Used by dialogs outside the parameter editors (BaseBuildProps' script combo).
int  WBQtParamData_LoadSubroutineScripts(void);

// Write the value back (== EditParameter::OnOK). `text` is the edit/selected-row text,
// `selIndex` the selected row (-1 none). Returns 0 if the input does not parse (beep + stay).
int  WBQtParam_Store(void *parameter, const char *text, int selIndex);

// Play the audio event by name (SOUND/DIALOG/MUSIC parameters only; == Preview Sound).
void WBQtParam_PreviewAudio(void *parameter, const char *eventName);

// coordinate parameter
void WBQtParamData_GetCoord(void *parameter, double *x, double *y, double *z);
void WBQtParamData_SetCoord(void *parameter, double x, double y, double z);

// color parameter (raw aarrggbb int)
int  WBQtParamData_GetColor(void *parameter);
void WBQtParamData_SetColor(void *parameter, int argb);
void WBQtParamData_GetString(void *parameter, char *buf, int cap);

// object-type picker: the template catalog ([TEST/]side/editor-sorting/name) + object lists
int  WBQtParamData_GetTemplateCount(void);
int  WBQtParamData_GetTemplateInfo(int i, char *nameOut, int nameCap,
	char *sideOut, int sideCap, char *sortingOut, int sortingCap, int *isTestOut);
int  WBQtParamData_GetObjectListCount(void);
void WBQtParamData_GetObjectList(int i, char *buf, int cap);

// group editor (name / active / subroutine on the ScriptGroup)
void WBQtGroupData_Get(void *scriptGroup, char *nameOut, int cap, int *activeOut, int *subroutineOut);
void WBQtGroupData_Set(void *scriptGroup, const char *name, int active, int subroutine);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PARAM_BRIDGE_H
