// WBQtCameraBridge.h -- self-contained opaque facade for the Qt Camera Options window.
//
// Carries ONLY int/double/float* so the MFC side and the Qt side never include each other's
// headers. Pitch and the readouts go straight to the 3D view (the same calls the MFC handlers
// make); Drop Waypoint / Center On Selected reuse the hidden MFC dialog's button handlers (they
// build the same AddObjectUndoable / setCenterInView).
#ifndef WB_QT_CAMERA_BRIDGE_H
#define WB_QT_CAMERA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt window -> view / hidden MFC dialog (implemented in WBQtCameraBridge.cpp) ----
double WBQtCamera_GetPitch(void);	// 0.0 .. 1.0 (the MFC popup slider was 0..100 x 0.01)
void   WBQtCamera_SetPitch(double pitch);
void   WBQtCamera_Reset(void);		// == "Restore To Default" (setDefaultCamera + refresh)
void   WBQtCamera_DropWaypoint(void);
void   WBQtCamera_CenterOnSelected(void);
// Fills the read-only camera info: height above ground, zoom (1.0 max .. 0.0 min), position and
// target XY. Returns 0 when no 3D view exists.
int    WBQtCamera_GetInfo(float *height, float *zoom, float *posX, float *posY,
                          float *tgtX, float *tgtY);

// --- Forward: MFC -> Qt window (implemented Qt-side, WBQtCameraPanel.cpp) --------------------
// Called from CameraOptions::update() -- i.e. from handleCameraChange on EVERY camera move --
// so it must stay cheap: it no-ops unless the Qt window exists AND is visible.
void WBQtCamera_PushRefresh(void);

// Open the Qt window (from CMainFrame::OnEditCameraoptions in Qt mode).
void WBQtCamera_Open(void *frameHwnd);

// Non-zero when the Qt window (or a child) holds the Win32 keyboard focus (frame accelerator skip).
int  WBQtCamera_OwnsFocus(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_CAMERA_BRIDGE_H
