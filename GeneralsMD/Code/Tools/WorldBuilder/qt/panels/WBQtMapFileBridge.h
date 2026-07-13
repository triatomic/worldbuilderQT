// WBQtMapFileBridge.h -- opaque facade for the Qt Open Map / Save Map pickers (Tier 3d).
//
// Open Map: the MFC OpenMap dialog is created HIDDEN and stays the logic owner -- the
// system/user map enumeration, the search filter and the packed-.big browse/extract machinery
// run verbatim; the Qt dialog mirrors its listbox and drives the real handlers. A pick either
// COMPLETES (a map filename or the browse fallback was captured -- the Qt dialog closes) or
// merely drills deeper (packed mode listing the maps inside an archive -- the list reloads).
//
// Save Map: small enough to be native -- the bridge only enumerates the map folders, does the
// overwrite confirmation, and keeps the UseSystemDir profile setting.
#ifndef WB_QT_MAPFILE_BRIDGE_H
#define WB_QT_MAPFILE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// mode values for the Open Map radio strip
#define WB_QT_MAPMODE_USER		0
#define WB_QT_MAPMODE_SYSTEM	1
#define WB_QT_MAPMODE_PACKED	2

// ================= MFC -> Qt (implemented in qt/panels/WBQtMapFileDialogs.cpp) =================

// Run the Qt Open Map picker. Returns 1 with either a filename (browseOut==0) or the standard-
// file-dialog fallback request (browseOut!=0); 0 on cancel.
int WBQtOpenMap_Run(void *frameHwnd, char *filenameOut, int cap, int *browseOut);

// Run the Qt Save Map picker seeded with the current name. Returns 1 with the chosen map name
// (browseOut==0, usingSystemDirOut valid) or the browse fallback (browseOut!=0); 0 on cancel.
int WBQtSaveMap_Run(void *frameHwnd, const char *initialFilename,
	char *filenameOut, int cap, int *browseOut, int *usingSystemDirOut);

// ================= Qt -> MFC (implemented in src/WBQtMapFileBridge.cpp) =================

// Whether the System/User radio strip is available (debug/internal builds only, == the MFC
// #if !defined(_DEBUG) && !defined(_INTERNAL) hide).
int  WBQtMapFileData_RadiosVisible(void);

// --- Open Map (hidden-dialog driven) ---
void WBQtOpenMapData_Open(void);
void WBQtOpenMapData_Close(void);
int  WBQtOpenMapData_ListCount(void);
void WBQtOpenMapData_ListItem(int i, char *buf, int cap);
// Preview thumbnail bytes for row i (the <name>.tga next to the .map, read from disk
// or straight out of the .big); returns the byte count, 0 = no preview.
int  WBQtOpenMapData_ItemPreviewData(int i, unsigned char *buf, int cap);
int  WBQtOpenMapData_ListCurSel(void);
int  WBQtOpenMapData_OkEnabled(void);
int  WBQtOpenMapData_GetMode(void);
void WBQtOpenMap_SetMode(int mode);
void WBQtOpenMap_Search(const char *text);
void WBQtOpenMap_ResetSearch(void);
// Pick row (== OK/double-click). Returns 1 if COMPLETED (result captured), 0 if it only
// drilled into an archive / did nothing (the Qt list should reload).
int  WBQtOpenMap_Pick(int row);
// The Browse button (== OnBrowse): packed mode pops the .big chooser then relists (returns 0);
// normal mode completes with the browse fallback (returns 1).
int  WBQtOpenMap_BrowsePick(void);
void WBQtOpenMapData_GetResult(char *filenameOut, int cap, int *browseOut);

// --- Save Map (native; the bridge provides data + confirmation) ---
int  WBQtSaveMapData_GetUseSystemDir(void);
// Enumerate the map folders for the given mode; also persists UseSystemDir like the MFC
// populate did. Returns the count (items read back per index).
int  WBQtSaveMapData_Enumerate(int systemMaps);
void WBQtSaveMapData_GetMapName(int i, char *buf, int cap);
// The overwrite confirmation (== SaveMap::OnOK's GetStatus + IDS_REPLACEFILE prompt).
// Returns 1 to proceed, 0 to stay in the dialog.
int  WBQtSaveMap_ConfirmOverwrite(const char *filename, int systemMaps);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAPFILE_BRIDGE_H
