// WBQtMinimapBridge.h -- opaque facade for hosting the Minimap inside a Qt window.
//
// The Minimap is a custom-rendered surface (composited pixel buffer + GDI overlays + drag-to-
// recenter) whose visibility gates (IsWindowVisible checks in handleCameraChange,
// notifySelectionChanged, the rebuild throttles) are scattered across the codebase. So instead
// of re-plumbing the renderer through a bridge, the LIVE MFC MinimapDialog window is adopted
// into a Qt tool window via QWinHost (the Phase-2 viewport pattern): every behavior -- rendering,
// throttles, selection halos, click/drag recentering -- keeps working unchanged, the visibility
// gates read the true (hosted) visibility, and the window gets the Qt chrome + dark title bar.
//
// All three funcs are implemented Qt-side (WBQtMinimapHost.cpp); no MFC-side bridge TU exists.
#ifndef WB_QT_MINIMAP_BRIDGE_H
#define WB_QT_MINIMAP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Adopt the MFC minimap window (minimapHwnd) into the Qt host window and show it. On the first
// call the popup/caption styles are stripped and the dialog becomes a child of the QWinHost.
void WBQtMinimap_Open(void *frameHwnd, void *minimapHwnd);

// Hide the Qt host window (the hosted minimap hides with it, so IsWindowVisible-based gates
// and the View-menu checkmark keep working).
void WBQtMinimap_Close(void);

// Non-zero while the Qt host window is visible.
int  WBQtMinimap_IsOpen(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MINIMAP_BRIDGE_H
