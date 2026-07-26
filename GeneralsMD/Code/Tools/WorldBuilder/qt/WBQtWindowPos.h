// WBQtWindowPos.h -- generic geometry persistence for Qt windows (position and/or size).
//
// Call WBQtWindowPos_Track(window, "Name") once (in the window's ctor or first-open path).
// From then on the window's frame Top/Left are saved into WorldBuilder.ini's
// [QtWindowPositions] section (keys "<Name>_Top" / "<Name>_Left") and its Width/Height into
// [QtWindowSize] ("<Name>_Width" / "<Name>_Height"), and both are re-applied the first time it
// is shown. Modal dialogs use WBQtWindowPos_TrackSize instead, so they remember their size but
// still center fresh. Only windows that call one of these are persisted.
//
// Moves and resizes are stashed, not written as they happen -- both fire per tick of a drag and
// each save is two profile writes into WorldBuilder.ini -- and flushed once when the window is
// hidden or destroyed.
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
// Size is persisted too (see WBQtWindowPos_TrackSize for the size-only variant).
void WBQtWindowPos_Track(QWidget *window, const char *name);

// Size-only variant for MODAL dialogs: persists Width/Height to [QtWindowSize] but leaves
// positioning alone, so the dialog still centers fresh on each open (the behavior modal
// dialogs are meant to have). A restored size is never allowed below minimumSizeHint(), so
// a stale saved size cannot bring the dialog back clipped.
void WBQtWindowPos_TrackSize(QWidget *window, const char *name);

// Reset Window Positions: wipe the saved [QtWindowPositions]/[QtWindowSize] store and
// cascade every live tracked window back near the top-left (visible ones re-save their
// fresh spot through the normal move tracking).
void WBQtWindowPos_ResetAll(void);

#endif // WB_QT_WINDOW_POS_H
