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

#endif // WB_QT_BRIDGE_H
