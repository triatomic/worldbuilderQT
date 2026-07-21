// WBQtEntityFinderBridge.h -- opaque facade for the Qt "Help / Entity Finder / Shortcut
// Finder" window (Tier 5a), the native rebuild of the modeless CAboutDlg in
// WorldBuilder.cpp. The MFC side (src/WBQtEntityFinderBridge.cpp) supplies the hotkey
// text, the named-object / waypoint enumerations, the center-on-name action, the dialog
// font choices, the viewport-resolution persistence + live apply, and the AboutWindow
// profile section.
#ifndef WB_QT_ENTITYFINDER_BRIDGE_H
#define WB_QT_ENTITYFINDER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ====== MFC -> Qt (implemented in qt/panels/WBQtEntityFinderDialog.cpp) ======

// Open (or raise) the modeless Qt window. Returns 1 when Qt handled it, 0 when Qt is not
// up (caller falls back to the MFC dialog -- only possible before WBQt_Startup).
int WBQtEntityFinder_Open(void *frameHwnd);

// Reset Window Positions support: move the window if it is open (no-op otherwise).
void WBQtEntityFinder_MoveTo(int left, int top);

// ====== Qt -> MFC (implemented in src/WBQtEntityFinderBridge.cpp) ======

// The formatted hotkey list (mirrors OnAppAbout's table; CRLF line ends). Returns length.
int WBQtEntityFinderData_GetHotkeyText(char *bufOut, int cap);

// Named placed objects / named waypoints (Build* returns the row count for Get*).
int  WBQtEntityFinderData_BuildObjects(void);
void WBQtEntityFinderData_GetObject(int i, char *bufOut, int cap);
int  WBQtEntityFinderData_BuildWaypoints(void);
void WBQtEntityFinderData_GetWaypoint(int i, char *bufOut, int cap);

// Center the 3D view on the named object/waypoint (== CAboutDlg::OnCenterOnSelected).
void WBQtEntityFinder_CenterOn(const char *name, int isWaypoint);
// Resolve an entity name to its placed MapObject (as void*), or NULL. Shared name-match walk used
// by CenterOn and the script editor's entity-jump. isWaypoint 0 = unit, 1 = waypoint.
void *WBQtEntityFinder_FindByName(const char *name, int isWaypoint);

// Dialog font choices (single source of truth in DialogFont.cpp; applies on restart).
int  WBQtEntityFinderData_GetFontCount(void);
void WBQtEntityFinderData_GetFontLabel(int i, char *bufOut, int cap);
int  WBQtEntityFinderData_GetFontSel(void);
void WBQtEntityFinder_SetFontSel(int i);

// Viewport resolution: the saved [MainFrame] Width/Height (0 = unset), and write+apply
// live (== CAboutDlg::saveViewportResolution -> adjustWindowSize(true)).
void WBQtEntityFinderData_GetSavedResolution(int *wOut, int *hOut);
void WBQtEntityFinder_SetResolution(int w, int h);

// The [AboutWindow] profile section (LaunchOnStartUp / ShrinkHotkeyList / Top / Left).
int  WBQtEntityFinderData_GetProfileInt(const char *key, int def);
void WBQtEntityFinder_SetProfileInt(const char *key, int value);

// Undo history depth (clamped 1..999 by the doc; default 64).
int  WBQtEntityFinderData_GetMaxUndos(void);
void WBQtEntityFinder_SetMaxUndos(int count);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_ENTITYFINDER_BRIDGE_H
