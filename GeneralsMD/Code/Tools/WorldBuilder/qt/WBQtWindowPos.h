// WBQtWindowPos.h -- generic position persistence for modeless Qt tool windows.
//
// Call WBQtWindowPos_Track(window, "Name") once (in the window's ctor or first-open path).
// From then on the window's frame Top/Left are saved on every move into WorldBuilder.ini's
// [QtWindowPositions] section (keys "<Name>_Top" / "<Name>_Left"), and its last position is
// re-applied the first time it is shown. Only windows that call this are persisted, so modal
// dialogs (which are not tracked) keep centering fresh -- the intended behavior.
//
// The store is the same WorldBuilder.ini every other window uses (via the MFC-side bridge
// WBQtWindowPos_Save/Get in src/WBQtHostBridge.cpp), so positions all live in one file.
//
// Tracked windows are also kept reachable: a WM_MOVING clamp stops title-bar drags at the
// screen edges (downward the window may hang into/below the taskbar, but the title bar
// always stays visible above it), and a stale stored position (monitor unplugged,
// resolution change) is clamped back into view on restore.
#ifndef WB_QT_WINDOW_POS_H
#define WB_QT_WINDOW_POS_H

class QWidget;

// Install the save-on-move / restore-on-first-show behavior on a top-level window, keyed by
// a stable ASCII name. Safe to call more than once for the same window (later calls no-op).
void WBQtWindowPos_Track(QWidget *window, const char *name);

// Reset Window Positions: wipe the saved [QtWindowPositions]/[QtWindowSize] store and
// cascade every live tracked window back near the top-left (visible ones re-save their
// fresh spot through the normal move tracking).
void WBQtWindowPos_ResetAll(void);

#endif // WB_QT_WINDOW_POS_H
