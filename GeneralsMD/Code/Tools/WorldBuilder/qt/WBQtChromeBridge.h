// WBQtChromeBridge.h -- opaque facade for the Qt main-window chrome (Tier 4). Stage 4a-1:
// the Qt menu bar. The MFC frame stays the top-level window; the chrome lives inside the
// Phase-2 viewport-host column and every menu command is delivered as a plain WM_COMMAND
// to the frame, so all existing MFC command routing / handlers keep working untouched.
#ifndef WB_QT_CHROME_BRIDGE_H
#define WB_QT_CHROME_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============ MFC -> Qt (implemented in qt/WBQtChrome.cpp) ============

// Walk the frame's live menu bar (hMenuBar = the HMENU, walked recursively) and build the
// Qt menu bar at the top of the viewport-host column; appends the native Theme menu.
// Returns 1 on success (the caller then detaches the MFC menu via SetMenu(NULL)), 0 when
// the chrome host is not up (caller keeps the MFC menu).
int WBQtChrome_InstallMenuBar(void *frameHwnd, void *hMenuBar);

// True once InstallMenuBar succeeded (CMainFrame::OnUpdateFrameMenu no-ops on it so MFC
// can never re-attach the detached menu).
int WBQtChrome_IsInstalled(void);

// True while any chrome menu popup is open. CWB3dFrameWnd::PreTranslateMessage checks it
// FIRST and routes straight to CWnd so accelerators / the ESC-fullscreen handling cannot
// eat the keys Qt needs for menu navigation.
int WBQtChrome_PopupActive(void);

// Tier 4a-2: Alt+letter -- open the top-level menu whose mnemonic matches the (upper-case)
// letter. Returns 1 if a menu opened, 0 otherwise.
int WBQtChrome_ActivateMenu(int letter);

// Tier 4b: build the Qt toolbar (the exe's RT_TOOLBAR IDR_MAINFRAME + its bitmap strip,
// the same pieces MFC's LoadToolBar used) into the chrome column. Returns 1 on success
// (the caller then hides the MFC toolbar).
int WBQtChrome_InstallToolBar(void);

// Tier 4c: build the Qt status row (message + CAP/NUM/SCRL) at the bottom of the chrome
// column. Returns 1 on success (the caller then hides the MFC status bar).
int WBQtChrome_InstallStatusBar(void);

// Tier 4c: the push target for CMainFrame's WM_SETMESSAGESTRING override -- every frame
// status message lands in the Qt row with zero polling.
void WBQtChrome_SetStatusText(const char *text);

// ============ Qt -> MFC (implemented in src/WBQtChromeBridge.cpp) ============

// Command enable/check state == CCmdUI::DoUpdate on the frame's routing (frame -> active
// view -> doc -> app), including the disable-if-no-handler probe. checkedOut: -1 = the
// handler never set a check, else 0/1. Returns 0 when no frame.
int WBQtChromeData_QueryCommand(int id, int *enabledOut, int *checkedOut);

// The command's status prompt (the RC string-table text before '\n'). Returns 0 if none.
int WBQtChromeData_GetPrompt(int id, char *bufOut, int cap);

// The command's tooltip (the RC string-table text after '\n'). Returns 0 if none.
int WBQtChromeData_GetTooltip(int id, char *bufOut, int cap);

// The MFC recent-file list (for the File menu's dynamic MRU section).
int WBQtChromeData_GetMruCount(void);
int WBQtChromeData_GetMruPath(int i, char *bufOut, int cap);

// Menu flyby: push hover text into the frame's status bar (empty -> the idle message).
void WBQtChrome_SetFrameStatusText(const char *text);

// ID_VIEW_TOOLBAR / ID_VIEW_STATUS_BAR special case while the MFC bars are still the real
// ones: toggle the bar and re-flow the viewport host.
void WBQtChrome_ToggleMfcBar(int statusBar);
int  WBQtChrome_IsMfcBarVisible(int statusBar);

// Tier 4b: show/hide an MFC bar outright (0 = toolbar, 1 = status bar) and re-flow the
// viewport host -- used to retire the MFC toolbar once the Qt one is up.
void WBQtChrome_SetMfcBarVisible(int statusBar, int visible);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_CHROME_BRIDGE_H
