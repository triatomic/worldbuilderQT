// WBQtToast.h -- opaque facade for the Qt toast popups (Tier 5b), replacing CToastDialog
// in Qt mode. Pure extern-C so the MFC call sites can include it.
#ifndef WB_QT_TOAST_H
#define WB_QT_TOAST_H

#ifdef __cplusplus
extern "C" {
#endif

// Show a transient hint toast (== CToastDialog): a topmost tool window at screen
// (10, 100 + stack of open toasts), auto-dismissing after durationMs, an OK button when
// showOkButton, click-to-dismiss, with the tooltip.wav chirp. Returns 1 when shown,
// 0 when Qt is not up (the caller falls back to the MFC toast).
int WBQtToast_Show(const char *text, int durationMs, int showOkButton);

// Whether the one-time tutorial hints (these toasts + the Ctrl+A confirm) should be shown.
// Persisted via the Entity Finder "Tutorial popups" checkbox; ON by default. Gate every hint
// site on this so experienced users can silence them. (Defined in WBQtObjectBridge.cpp.)
int WBQtObject_GetTutorialPrompts(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_TOAST_H
