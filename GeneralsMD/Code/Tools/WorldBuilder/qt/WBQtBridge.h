// WBQtBridge.h -- the seam between the MFC application and the Qt world.
//
// This header is deliberately a plain facade: NO Qt and NO MFC types, so the big
// MFC translation unit (WorldBuilder.cpp) can call into Qt without ever including a
// Qt header (which would drag Qt's keywords/macros into an MFC+windows.h compile).
// All the Qt mixing lives in WBQtBridge.cpp and the vendored qmfcapp.cpp.
//
// Phase 1: bring up a Qt event loop that coexists with MFC's. Phase 2: host the live
// D3D8 viewport in-place inside a Qt layer. See qt/3rdparty/qtwinmigrate for the loop
// merge (qmfcapp) and the Win32-window-in-Qt hosting (qwinwidget/qwinhost).
#ifndef WB_QT_BRIDGE_H
#define WB_QT_BRIDGE_H

// Create the QApplication (hooked into MFC's existing message loop) and apply the
// theme. Safe to call once, after the MFC main window exists.
void WBQt_Startup(void);

// Destroy the QApplication. Must be called explicitly from CWorldBuilderApp::
// ExitInstance: the app's global dtor calls _exit(0) right after, so static/atexit
// teardown never runs.
void WBQt_Shutdown(void);

// --- Stage 1 (full-Qt port): the QMainWindow inversion ---------------------------
// A Qt QMainWindow becomes the visible top-level window (chrome + viewport); the MFC
// frame stays alive but hidden as the command-routing hub.

// True while the inversion is intended/active for this run (default in Qt builds).
// MFC guards (ActivateFrame, F11, adjustWindowSize, ...) key off this. It flips to 0
// only if WBQt_CreateMainWindow fails, after which the legacy chrome-in-frame path runs.
int WBQt_InversionActive(void);

// Explicitly fall back to the legacy chrome-in-frame path (called when InitInstance
// cannot even attempt the inversion, e.g. no 3D view/frame yet).
void WBQt_DisableInversion(void);

// Create the top-level QMainWindow (not shown yet). x/y/w/h seed its geometry (the
// [MainFrame] profile values; pass w/h <= 0 for defaults); maximized non-zero makes
// WBQt_ShowMainWindow open it maximized like the last session. Returns 1 on success, 0
// on failure (which also clears WBQt_InversionActive so the guards fall back).
int WBQt_CreateMainWindow(void *frameHwnd, int x, int y, int w, int h, int maximized);

// Show the main window once chrome + viewport are in place (one clean layout pass).
void WBQt_ShowMainWindow(void);

// The QMainWindow's HWND (NULL when not inverted) -- the native owner for floating
// tool windows and dialogs.
void *WBQt_MainWindowHwnd(void);

// Bring the main window to the foreground -- the inverted stand-in for
// CFrameWnd::ActivateFrame (called on every doc open).
void WBQt_ActivateMainWindow(void);

// Resize the main window (an explicit View > resolution pick / Entity Finder combo);
// the central pane's resizeEvent then drives the D3D device from the real client area.
void WBQt_ResizeMainWindow(int width, int height);

// Move the main window (the Reset Window Positions command).
void WBQt_MoveMainWindow(int x, int y);

// F11 / Esc fullscreen toggle on the main window; IsFullscreen for the Esc gate.
void WBQt_ToggleFullscreen(void);
int  WBQt_IsFullscreen(void);

// Mirror of the MFC frame title (map name + modified marker) -- pushed from
// CMainFrame::OnUpdateFrameTitle so the visible window tracks the document exactly.
void WBQt_SetMainWindowTitle(const char *title);

// Put keyboard focus back on the hosted 3D viewport (tool hotkeys + GetAsyncKeyState
// tools resume without an extra click).
void WBQt_FocusViewport(void);

// Reverse callbacks, DEFINED on the MFC side (src/WBQtHostBridge.cpp), called from the
// Qt main window. extern "C" so the Qt static lib resolves them against the exe.
#ifdef __cplusplus
extern "C" {
#endif
// Debounced [MainFrame] Top/Left/Width/Height writes (the hidden frame's OnMove no
// longer fires, so the Qt window persists its own placement through the same INI keys).
void WBQt_SaveMainWindowPlacement(int x, int y, int width, int height);
// The [MainFrame] Maximized flag, saved alongside the placement and read back at launch.
void WBQt_SaveMainWindowMaximized(int maximized);
// A .map dropped on the main window -> CWinApp::OpenDocumentFile (the frame's
// OnDropFiles is unreachable while it is hidden).
void WBQt_OpenMapFileFromShell(const char *path);
#ifdef __cplusplus
}
#endif

// --- Phase 2: in-place viewport hosting -----------------------------------------
// Reparent the existing 3D viewport (an MFC CView HWND) into a Qt host that is a child
// of the MFC frame, so it renders THROUGH Qt while the frame stays its Win32 ancestor
// (MFC accelerators / Esc / F11 keep routing). Returns the Qt host's HWND so the frame
// can size it; NULL on failure. Call once, after the frame and 3D view both exist.
void *WBQt_HostViewport(void *frameHwnd, void *viewHwnd);

// Reparent the viewport back under the frame and destroy the Qt host, leaving the view
// HWND (owned by MFC) intact. Safe if the HWNDs are already gone. Call from the frame's
// OnDestroy, while the view is still alive, before MFC tears the frame's children down.
void WBQt_UnhostViewport(void *frameHwnd, void *viewHwnd);

// Position+size the Qt host through Qt's geometry (NOT Win32 SetWindowPos), so the
// QWinWidget's layout reflows and sizes the hosted viewport to fill. x/y are in the MFC
// frame's client coords (Qt treats the host as embedded in the frame). Called by the
// frame whenever the pane rect changes.
void WBQt_SetViewportHostGeometry(int x, int y, int width, int height);

// Reverse callback: DEFINED on the MFC side (src/WBQtHostBridge.cpp), CALLED from the
// Qt host when it resizes, so the D3D device tracks the host's pixel size. extern "C"
// so the Qt static lib resolves it against the exe at the final link.
#ifdef __cplusplus
extern "C"
#endif
void WBQt_OnViewportHostResized(int width, int height);

// Theme control driving WBQtTheme (0 = System/follow-Windows, 1 = Dark, 2 = Light).
// Set persists the choice and applies it live; Get returns the current mode.
void WBQt_SetThemeMode(int mode);
int  WBQt_GetThemeMode(void);

// Tier 5: the MFC frame forwards WM_SETTINGCHANGE ("ImmersiveColorSet") here so the
// System theme mode follows a live Windows light/dark switch.
void WBQt_OnOsThemeChanged(void);

#endif // WB_QT_BRIDGE_H
