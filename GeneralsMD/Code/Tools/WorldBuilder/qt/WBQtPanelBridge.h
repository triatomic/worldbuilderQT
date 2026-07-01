// WBQtPanelBridge.h -- opaque facade for migrating WorldBuilder option panels to Qt.
//
// Like WBQtBridge.h, this header carries ONLY int/void* (no Qt or MFC types), so the MFC
// frame / option-panel TUs can drive Qt panels and the Qt panels can drive the MFC tools,
// without either side including the other's headers. Each migrated panel adds its own
// forward (tool->widget) and reverse (widget->tool) functions below.
#ifndef WB_QT_PANEL_BRIDGE_H
#define WB_QT_PANEL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// --- Generic options-panel host (implemented Qt-side, called from CMainFrame) ----------
// If dialogID is a migrated Qt panel, create/show it at screen (x,y) size (w,h), hide any
// other Qt panel, and return non-zero. Otherwise return 0 (caller shows the MFC panel).
int  WBQt_ShowOptionsPanel(void *frameHwnd, int dialogID, int x, int y, int w, int h);
void WBQt_HideOptionsPanel(void);

// --- Feather panel: forward push, tool -> Qt widget (implemented Qt-side) ---------------
void WBQtFeather_PushFeather(int v);
void WBQtFeather_PushRadius(int v);
void WBQtFeather_PushRate(int v);

// --- Feather panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtFeatherBridge) -
void WBQtFeather_SetFeather(int v);
void WBQtFeather_SetRadius(int v);
void WBQtFeather_SetRate(int v);
void WBQtFeather_ToggleMirror(void);
void WBQtFeather_ToggleMirrorX(void);
void WBQtFeather_ToggleMirrorY(void);
void WBQtFeather_ToggleMirrorXY(void);
int  WBQtFeather_GetFeather(void);
int  WBQtFeather_GetRadius(void);
int  WBQtFeather_GetRate(void);
int  WBQtFeather_GetMirror(void);
int  WBQtFeather_GetMirrorX(void);
int  WBQtFeather_GetMirrorY(void);
int  WBQtFeather_GetMirrorXY(void);

// --- Brush panel: forward push, tool -> Qt widget (implemented Qt-side) ------------------
void WBQtBrush_PushWidth(int v);
void WBQtBrush_PushFeather(int v);
void WBQtBrush_PushHeight(int v);

// --- Brush panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtBrushBridge) ------
void WBQtBrush_SetWidth(int v);
void WBQtBrush_SetFeather(int v);
void WBQtBrush_SetHeight(int v);
void WBQtBrush_ToggleMirror(void);
void WBQtBrush_ToggleMirrorX(void);
void WBQtBrush_ToggleMirrorY(void);
void WBQtBrush_ToggleMirrorXY(void);
int  WBQtBrush_GetWidth(void);
int  WBQtBrush_GetFeather(void);
int  WBQtBrush_GetHeight(void);
int  WBQtBrush_GetMirror(void);
int  WBQtBrush_GetMirrorX(void);
int  WBQtBrush_GetMirrorY(void);
int  WBQtBrush_GetMirrorXY(void);

// --- Mound panel: forward push, tool -> Qt widget (implemented Qt-side) ------------------
void WBQtMound_PushWidth(int v);
void WBQtMound_PushFeather(int v);
void WBQtMound_PushHeight(int v);

// --- Mound panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtMoundBridge) ------
void WBQtMound_SetWidth(int v);
void WBQtMound_SetFeather(int v);
void WBQtMound_SetHeight(int v);
void WBQtMound_ToggleMirror(void);
void WBQtMound_ToggleMirrorX(void);
void WBQtMound_ToggleMirrorY(void);
void WBQtMound_ToggleMirrorXY(void);
int  WBQtMound_GetWidth(void);
int  WBQtMound_GetFeather(void);
int  WBQtMound_GetHeight(void);
int  WBQtMound_GetMirror(void);
int  WBQtMound_GetMirrorX(void);
int  WBQtMound_GetMirrorY(void);
int  WBQtMound_GetMirrorXY(void);

// --- Ruler panel: reverse, Qt widget -> tool (implemented MFC-side, WBQtRulerBridge) -------
// No forward push: like the MFC RulerOptions, the panel only reads tool state at seed time
// and when the user changes a control. Lengths cross the seam as doubles in FEET (world
// units); the tool stores a radius, so the panel does the diameter<->radius halving/doubling
// and the feet<->meters display conversion. GetType returns RulerTypeEnum: 0 = none,
// 1 = line, 2 = circle (== RULER_CIRCLE) -- only 2 is a circle.
void   WBQtRuler_SetLengthFeet(double radiusFeet);
double WBQtRuler_GetLengthFeet(void);
int    WBQtRuler_SwitchType(void);	// toggles line<->circle, returns non-zero if it changed
int    WBQtRuler_GetType(void);
void   WBQtRuler_SetUseMeters(int on);
int    WBQtRuler_GetUseMeters(void);
double WBQtRuler_ToDisplayUnits(double feet);
void   WBQtRuler_SetShowGrid(int on);
int    WBQtRuler_GetShowGrid(void);
void   WBQtRuler_RepaintViews(void);	// == pDoc->updateAllViews() so the in-view label refreshes

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PANEL_BRIDGE_H
