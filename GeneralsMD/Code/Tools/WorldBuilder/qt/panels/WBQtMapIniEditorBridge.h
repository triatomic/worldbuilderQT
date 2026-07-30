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

// The other two name kinds the editor checks, each with the same Build/Get/Is trio:
//   UPGRADE  -- the Upgrade = name of an upgrade module, against TheUpgradeCenter
//   COMMAND  -- the Command = entries of a CommandSet block, against TheControlBar
// A kind whose catalog is unavailable (the subsystem is NULL in WorldBuilder) reports
// everything as known, so the editor never underlines what it cannot actually verify.
int  WBQtMapIniEditorData_BuildUpgrades(void);
void WBQtMapIniEditorData_GetUpgrade(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_IsUpgrade(const char *name);

int  WBQtMapIniEditorData_BuildCommandButtons(void);
void WBQtMapIniEditorData_GetCommandButton(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_IsCommandButton(const char *name);

int  WBQtMapIniEditorData_BuildSciences(void);
void WBQtMapIniEditorData_GetScience(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_IsScience(const char *name);

// Command SET names (what "CommandSet = X" references). Validation only: the engine's command
// set list is protected, so there is no enumeration to suggest from without editing shared
// engine code. Build returns 0 and Get yields nothing; Is still answers correctly.
int  WBQtMapIniEditorData_BuildCommandSets(void);
void WBQtMapIniEditorData_GetCommandSet(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_IsCommandSet(const char *name);

// Side names (what "SideInfo <name>" names), from the player templates. AI::parseSideInfo
// matches these by exact string, so a typo silently creates a side nothing ever reads.
int  WBQtMapIniEditorData_BuildSides(void);
void WBQtMapIniEditorData_GetSide(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_IsSide(const char *name);

// ---- the whole-INI-tree scan, for autocomplete ----
//
// The engine's stores are the authority for VALIDATION, but most of them keep their maps
// private, so they cannot be enumerated for suggestions. For autocomplete the game's own INI
// files are scanned instead, through TheFileSystem (which resolves a mod's .big archives exactly
// as the engine does). Two catalogs come out of it:
//
//   NAMES -- every block name declared anywhere ("Locomotor BasicCarLocomotor" -> the name)
//   KEYS  -- every "Key =" seen, so the key side of a line can be completed too
//
// Scanned once and cached (the tree is thousands of files); Build returns the row count.
// Returns 0 when the scan finds nothing, in which case autocomplete simply has less to offer.
int  WBQtMapIniEditorData_BuildIniNames(void);
void WBQtMapIniEditorData_GetIniName(int i, char *bufOut, int cap);
int  WBQtMapIniEditorData_BuildIniKeys(void);
void WBQtMapIniEditorData_GetIniKey(int i, char *bufOut, int cap);

// The values seen with ONE specific key, rather than every value in the tree. This is what makes
// autocomplete useful: "Surfaces =" offers GROUND/WATER/CLIFF/AIR/RUBBLE instead of thousands of
// unrelated names, and it works for every key without hardcoding a single table -- the engine's
// per-key name tables are compile-time and not reachable from here, but how each key is USED in
// the game's own INI files reveals the same set. Returns 0 when the key was never seen, in which
// case the caller falls back to the whole-tree catalog.
int  WBQtMapIniEditorData_BuildValuesForKey(const char *key);
void WBQtMapIniEditorData_GetValueForKey(int i, char *bufOut, int cap);

// The [MapIniEditor] profile section (window Top/Left/Width/Height, and the recent-file list).
int  WBQtMapIniEditorData_GetProfileInt(const char *key, int def);
void WBQtMapIniEditor_SetProfileInt(const char *key, int value);
void WBQtMapIniEditorData_GetProfileString(const char *key, char *bufOut, int cap);
void WBQtMapIniEditor_SetProfileString(const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAPINI_EDITOR_BRIDGE_H
