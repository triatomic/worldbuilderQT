// WBQtMapIniReport.h -- native Qt viewer for the map.ini loader's report (shown on map
// open / Reload / Check). Replaces the hand-rolled MFC scrollable-edit dialog with a
// resizable window: a monospace read-only view, a live name filter, collapsible sections
// (the report's ';' comment headers), and Copy-to-clipboard.
//
// The report text is produced in WorldBuilderDoc.cpp (doLoadMapIni) in map.ini's own
// syntax. This is a pure C facade (no Qt includes) so the MFC side can call it without
// pulling Qt headers into an MFC translation unit; the QDialog lives in the .cpp.
#ifndef WB_QT_MAPINI_REPORT_H
#define WB_QT_MAPINI_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

// Show the report modally.
//   applyMode == 0: informational (Check) -- a single Close button.
//   applyMode == 1: confirm -- OK / Cancel buttons; the caller applies on OK, discards on Cancel.
// Returns: 2 = shown, user clicked OK/accepted;  1 = shown, user clicked Cancel/closed;
//          0 = Qt not available (caller falls back to the MFC dialog / its own prompt).
// title/text are local-8bit C strings.
int WBQtMapIniReport_Show(const char *title, const char *text, int applyMode);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MAPINI_REPORT_H
