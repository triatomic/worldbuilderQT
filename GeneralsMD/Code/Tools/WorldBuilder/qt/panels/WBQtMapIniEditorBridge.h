// WBQtMapIniEditorBridge.h -- opaque facade for the Qt map.ini editor (File > Map.ini >
// Open map.ini (internal)). The editor itself is pure Qt: it loads the file, edits it, finds
// in it and saves it, with QPlainTextEdit supplying undo/redo. The MFC side
// (src/WBQtMapIniEditorBridge.cpp) supplies only what Qt cannot see: the current map's
// map.ini path and the template catalog the name checking validates against.
#ifndef WB_QT_MAPINI_EDITOR_BRIDGE_H
#define WB_QT_MAPINI_EDITOR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ====== MFC -> Qt (implemented in qt/panels/WBQtMapIniEditorDialog.cpp) ======

// Open (or raise) the modeless editor on `iniPath`. Returns 1 when Qt handled it, 0 when Qt
// is not up (the caller falls back to shelling the file out to the system editor).
int WBQtMapIniEditor_Open(void *frameHwnd, const char *iniPath);

// ====== Qt -> MFC (implemented in src/WBQtMapIniEditorBridge.cpp) ======

// The current map's map.ini path (empty when no map is open). Mirrors what File > Map.ini >
// Open map.ini resolves, so both items act on the same file.
void WBQtMapIniEditorData_GetPath(char *bufOut, int cap);

// The template-name catalog the editor validates object names against: every name
// TheThingFactory knows. Build* returns the row count readable via Get*.
int  WBQtMapIniEditorData_BuildTemplates(void);
void WBQtMapIniEditorData_GetTemplate(int i, char *bufOut, int cap);

// 1 when `name` is a template TheThingFactory can resolve. Used per token rather than a
// linear walk of the catalog, since the check runs over every line of the file.
int  WBQtMapIniEditorData_IsTemplate(const char *name);

// The [MapIniEditor] profile section (window Top/Left/Width/Height).
int  WBQtMapIniEditorData_GetProfileInt(const char *key, int def);
void WBQtMapIniEditor_SetProfileInt(const char *key, int value);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAPINI_EDITOR_BRIDGE_H
