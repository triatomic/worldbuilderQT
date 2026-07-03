// WBQtMessageBox.h -- stage-1 phase-3: route AfxMessageBox traffic through Qt.
//
// After the inversion the visible top-level is the Qt main window, but AfxMessageBox still
// pops a bare Win32 box owned by the (hidden) MFC frame -- it appears un-parented, doesn't
// center over the app, and (being a native window) is not fenced by Qt modality. This
// bridge shows the same box as a QMessageBox parented to the Qt main window instead.
//
// CWorldBuilderApp::DoMessageBox forwards here; the MB_* type flags and the ID* return
// codes are passed through as their raw Win32 values so the MFC caller is unchanged.
#ifndef WB_QT_MESSAGE_BOX_H
#define WB_QT_MESSAGE_BOX_H

#ifdef __cplusplus
extern "C" {
#endif

// Show a message box parented to the Qt main window. text/caption are local-8-bit; mbType
// is the Win32 MB_* flag set (button set + icon); returns the Win32 ID* code (IDOK, IDYES,
// IDNO, IDCANCEL, IDRETRY, IDABORT, IDIGNORE). Returns 0 if the Qt main window isn't up
// (the caller then falls back to the MFC default box).
int WBQtMessageBox_Show(const char *text, const char *caption, unsigned mbType);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_MESSAGE_BOX_H
