// WBQtMinimapHost.cpp -- Qt side of WBQtMinimapBridge.h: hosts the live MFC Minimap window.
//
// The MFC MinimapDialog stays 100% intact (rendering, throttles, drag-to-recenter); it is
// adopted as a child of a QWinHost inside a Qt::Tool window, so it draws and gets mouse input
// exactly as before while the window chrome (title bar, dark mode, resize) is Qt. Because the
// dialog is genuinely visible while the Qt window is shown, every IsWindowVisible-based gate in
// the codebase (camera-change refresh, selection halos, rebuild throttles, the View-menu
// checkmark) keeps working with no seam changes.
#include "WBQtMinimapBridge.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"
#include "qwinhost.h"

#include <QVBoxLayout>
#include <QWidget>

#include <qt_windows.h>

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget    *s_owner = NULL;	// owner for the floating window (created on first open)
	QWidget    *s_window = NULL;	// the Qt tool window ("Minimap")
	QWinHost   *s_winHost = NULL;	// hosts the adopted MFC dialog
	HWND        s_minimap = NULL;	// the adopted MFC MinimapDialog HWND
}

extern "C" void WBQtMinimap_Open(void *frameHwnd, void *minimapHwnd)
{
	if (frameHwnd == NULL || minimapHwnd == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	if (s_window == NULL)
	{
		s_window = new QWidget(s_owner, Qt::Tool);
		s_window->setWindowTitle("Minimap");
		s_window->resize(320, 340);
		WBQtWindowPos_Track(s_window, "Minimap");
		QVBoxLayout *lay = new QVBoxLayout(s_window);
		lay->setContentsMargins(0, 0, 0, 0);
		s_winHost = new QWinHost(s_window);
		lay->addWidget(s_winHost);
	}
	if (s_minimap == NULL)
	{
		// First open: convert the popup dialog into a child and hand it to the host. The
		// caption/sysmenu go away (the Qt window provides the chrome); QWinHost reparents it
		// and keeps it sized to the host from here on.
		s_minimap = reinterpret_cast<HWND>(minimapHwnd);
		LONG style = ::GetWindowLong(s_minimap, GWL_STYLE);
		style &= ~(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
		style |= WS_CHILD;
		::SetWindowLong(s_minimap, GWL_STYLE, style);
		LONG exStyle = ::GetWindowLong(s_minimap, GWL_EXSTYLE);
		exStyle &= ~WS_EX_TOOLWINDOW;
		::SetWindowLong(s_minimap, GWL_EXSTYLE, exStyle);
		s_winHost->setWindow(s_minimap);
		::ShowWindow(s_minimap, SW_SHOW);
	}
	// Show WITHOUT activating (the MFC window was a no-activate tool window too).
	s_window->setAttribute(Qt::WA_ShowWithoutActivating);
	s_window->show();
	s_window->raise();
}

extern "C" void WBQtMinimap_Close(void)
{
	if (s_window != NULL)
	{
		s_window->hide();
	}
}

extern "C" int WBQtMinimap_IsOpen(void)
{
	return (s_window != NULL && s_window->isVisible()) ? 1 : 0;
}
