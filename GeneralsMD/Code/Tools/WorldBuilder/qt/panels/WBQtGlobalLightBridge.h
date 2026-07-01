// WBQtGlobalLightBridge.h -- self-contained opaque facade for the Qt Global Light Options window.
//
// Carries ONLY int/float/char* so the MFC side and the Qt side never include each other's
// headers. The hidden MFC GlobalLightOptions (a CMainFrame member, still Create()d) stays the
// owner of the working state (m_angleAzimuth/m_angleElevation/m_lighting) and of the apply logic
// (applyAngle / applyColor / OnResetLights write TheGlobalData and drive pView->setLighting); the
// Qt window reads via the Get* funcs and writes via the Set* funcs, which update the hidden
// dialog's state then run the same apply paths.
//
// Lights: 0 = Sun, 1 = Accent 1, 2 = Accent 2. Lighting mode: 1 = Terrain, 2 = Objects, 3 = Both.
#ifndef WB_QT_GLOBALLIGHT_BRIDGE_H
#define WB_QT_GLOBALLIGHT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Reverse: Qt window -> hidden MFC dialog (implemented in WBQtGlobalLightBridge.cpp) ------
int  WBQtGlobalLight_GetLighting(void);
void WBQtGlobalLight_SetLighting(int mode);

// Angles per light: azimuth 0..359, elevation 0..90 (degrees, matching the MFC popup sliders).
void WBQtGlobalLight_GetAngles(int light, int *azimuth, int *elevation);
void WBQtGlobalLight_SetAngles(int light, int azimuth, int elevation);

// Colors as 0..255 components. Ambient exists only on the Sun; diffuse is per light.
void WBQtGlobalLight_GetAmbient(int *r, int *g, int *b);
void WBQtGlobalLight_SetAmbient(int r, int g, int b);
void WBQtGlobalLight_GetDiffuse(int light, int *r, int *g, int *b);
void WBQtGlobalLight_SetDiffuse(int light, int r, int g, int b);

// The normalized light direction currently in the globals (for the "XYZ: ..." readout).
void WBQtGlobalLight_GetLightPos(int light, float *x, float *y, float *z);

void WBQtGlobalLight_GetTimeOfDayName(char *out, int cap);

void WBQtGlobalLight_ResetLights(void);	// == the "Restore To Default" button

// Re-read the hidden dialog's state from TheGlobalData (== the MFC OnShowWindow refresh); call
// when the Qt window is shown, before seeding the controls.
void WBQtGlobalLight_SyncFromGlobals(void);

// Turn off the in-view light-direction feedback arrows (== the MFC OnClose/hide path).
void WBQtGlobalLight_FeedbackOff(void);

// --- Forward: open/close the Qt window (implemented Qt-side, WBQtGlobalLightPanel.cpp) -------
// Open is called from CMainFrame::OnEditGloballightoptions in Qt mode (frameHwnd = the MFC frame).
void WBQtGlobalLight_Open(void *frameHwnd);

// Non-zero when the Qt window (or a child control) holds the Win32 keyboard focus -- the frame's
// PreTranslateMessage checks this so tool-hotkey accelerators don't eat digits typed into the
// angle/color fields.
int  WBQtGlobalLight_OwnsFocus(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_GLOBALLIGHT_BRIDGE_H
