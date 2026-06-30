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

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PANEL_BRIDGE_H
