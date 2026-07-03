// WBQtShortcuts.h -- stage-1 phase-2: the keyboard flip.
//
// After the QMainWindow inversion the MFC accelerator table is dead for viewport-focused
// keys (the 3D view lives under the Qt QWinHost, so MFC's PreTranslate tree-walk never
// reaches the frame that owns IDR_MAINFRAME). This module replaces it: a static table
// transcribed from res/WorldBuilder.rc:505-584 that maps (virtual key, modifier mask) ->
// MFC command id. CWorldBuilderApp::PreTranslateMessage feeds every message here first;
// on a hit we deliver the command as a posted WM_COMMAND to the (hidden) frame -- exactly
// how the Qt menu/toolbar already dispatch -- so all existing MFC command routing is reused.
//
// Focus gate (replaces the 7 *_OwnsFocus guards): the translate only acts when the hosted
// viewport HWND (or the Qt main window) owns Win32 focus, so text fields in the separate
// floating Qt tool windows are never intercepted.
#ifndef WB_QT_SHORTCUTS_H
#define WB_QT_SHORTCUTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Feed a pumped message. Returns 1 if the message was a WorldBuilder hotkey and was
// consumed (the caller returns TRUE and does NOT call the base PreTranslateMessage);
// 0 to let the message continue (WbView key handling, Qt dialogs, menu navigation, ...).
// pMsg is a Win32 MSG* (void* to keep this header afx/qt-free).
int WBQtShortcuts_TranslateKey(void *pMsg);

// Implemented on the MFC side (src/WBQtChromeBridge.cpp): run the command's CCmdUI to
// check it is enabled, and if so PostMessage(WM_COMMAND) to the frame. Swallows a
// disabled command (returns without posting). Called by the translate function.
void WBQtShortcuts_PostCommand(int commandId);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_SHORTCUTS_H
