// WBQtBridge.cpp -- implements the MFC <-> Qt seam declared in WBQtBridge.h.
//
// This file (and the vendored qmfcapp/qwinwidget/qwinhost) are the only places Qt is
// touched in WorldBuilder. MFC keeps owning the message loop; Qt is pumped from inside
// CWinApp::Run() via QMfcApp::pluginInstance's WH_GETMESSAGE hook, so we never call
// QMfcApp::run(). Phase 2 hosts the live D3D8 viewport in a Qt layer rooted in the MFC
// frame (QWinWidget) that adopts the viewport HWND (QWinHost).
//
// Qt + Win32 only -- no afx here (qwinwidget.h merely forward-declares CWnd), keeping
// the opaque-facade rule. qt_windows.h (Qt's safe windows.h) comes first so the
// qtwinmigrate headers see HWND.
#include "WBQtBridge.h"

#include <qt_windows.h>

#include "qmfcapp.h"
#include "qwinwidget.h"
#include "qwinhost.h"
#include "WBQtTheme.h"

#include <QApplication>
#include <QResizeEvent>
#include <QVBoxLayout>

// A QWinHost that reports its pixel size back to the MFC side on every resize, so the
// D3D device tracks the host. No Q_OBJECT needed -- it only overrides a virtual.
class WbViewportHost : public QWinHost
{
public:
	explicit WbViewportHost(QWidget *parent)
		: QWinHost(parent)
	{
	}

protected:
	virtual void resizeEvent(QResizeEvent *e)
	{
		QWinHost::resizeEvent(e);		// QWinHost sizes the hosted view to fill us
		RECT rc;
		if (::GetClientRect(reinterpret_cast<HWND>(winId()), &rc))
		{
			WBQt_OnViewportHostResized(rc.right - rc.left, rc.bottom - rc.top);
		}
	}
};

// The Phase 2 host sandwich, owned for the process life:
//   MFC frame -> g_wbViewportHost (QWinWidget) -> g_wbViewportPane (QWinHost) -> view.
static QWinWidget    *g_wbViewportHost = NULL;
static WbViewportHost *g_wbViewportPane = NULL;

void WBQt_Startup(void)
{
	// pluginInstance(0): WorldBuilder is the executable itself, not a DLL, so there is
	// no module to pin -- 0 is the documented same-executable usage. This creates the
	// QApplication and installs the message hook that drives Qt from MFC's loop.
	QMfcApp::pluginInstance(0);

	// Apply dark-mode support before any Qt window is created so it inherits the theme.
	WBQtTheme::applyApplicationTheme();
}

void WBQt_Shutdown(void)
{
	// The viewport host is detached+destroyed earlier (from the frame's OnDestroy). Here
	// we only release Qt. Explicit because the app dtor _exit(0)s right after.
	delete qApp;
}

void WBQt_SetThemeMode(int mode)
{
	WBQtTheme::Mode m = WBQtTheme::ModeSystem;
	if (mode == 1)
	{
		m = WBQtTheme::ModeDark;
	}
	else if (mode == 2)
	{
		m = WBQtTheme::ModeLight;
	}
	WBQtTheme::setMode(m);
}

int WBQt_GetThemeMode(void)
{
	return (int)WBQtTheme::mode();
}

void WBQt_OnOsThemeChanged(void)
{
	if (qApp != NULL)
	{
		WBQtTheme::onSystemThemeChanged();
	}
}

void *WBQt_HostViewport(void *frameHwnd, void *viewHwnd)
{
	if (g_wbViewportHost != NULL)
	{
		return reinterpret_cast<void *>(g_wbViewportHost->winId());
	}
	if (frameHwnd == NULL || viewHwnd == NULL)
	{
		return NULL;
	}

	// A Qt widget rooted in the MFC frame's HWND (QWinWidget syncs Win32 activation).
	g_wbViewportHost = new QWinWidget(reinterpret_cast<HWND>(frameHwnd));
	g_wbViewportHost->setObjectName("wbViewportHost");

	QVBoxLayout *box = new QVBoxLayout(g_wbViewportHost);
	box->setContentsMargins(0, 0, 0, 0);
	box->setSpacing(0);

	g_wbViewportPane = new WbViewportHost(g_wbViewportHost);
	box->addWidget(g_wbViewportPane);

	// setWindow adopts the view as a child (SetParent), own_hwnd == false so ~QWinHost
	// never DestroyWindow's it -- the view stays MFC-owned.
	g_wbViewportPane->setWindow(reinterpret_cast<HWND>(viewHwnd));
	g_wbViewportHost->show();

	return reinterpret_cast<void *>(g_wbViewportHost->winId());
}

// Tier 4a: the chrome (WBQtChrome.cpp) inserts the Qt menu bar into this host's layout,
// turning the Phase-2 viewport host into the full chrome column.
QWidget *WBQt_GetViewportHostWidget(void)
{
	return g_wbViewportHost;
}

// Tier 4b: the hosted 3D view's HWND, so the chrome can hand keyboard focus back to it
// after a toolbar click.
void *WBQt_GetHostedViewWindow(void)
{
	return (g_wbViewportPane != NULL)
		? reinterpret_cast<void *>(g_wbViewportPane->window()) : NULL;
}

void WBQt_SetViewportHostGeometry(int x, int y, int width, int height)
{
	if (g_wbViewportHost != NULL)
	{
		// Qt owns the geometry -> resizeEvent -> the QVBoxLayout reflows -> the QWinHost
		// (and the hosted viewport) fill the host. A Win32 SetWindowPos would resize the
		// HWND but leave Qt's geometry stale, so the view never grew past Qt's default.
		g_wbViewportHost->setGeometry(x, y, width, height);
		g_wbViewportHost->show();
	}
}

void WBQt_UnhostViewport(void *frameHwnd, void *viewHwnd)
{
	// Move the view back under the frame FIRST, so destroying the Qt host's HWND does
	// not take the view (a Win32 child) with it. Do NOT call QWinHost::setWindow(0): it
	// zeroes the host's hwnd before ~QWinHost can restore the view's original WndProc,
	// which would leave the MFC view permanently subclassed. Deleting the host instead
	// lets ~QWinHost restore AfxWndProc and (own_hwnd == false) skip DestroyWindow.
	if (viewHwnd != NULL && frameHwnd != NULL
		&& ::IsWindow(reinterpret_cast<HWND>(viewHwnd)))
	{
		::SetParent(reinterpret_cast<HWND>(viewHwnd), reinterpret_cast<HWND>(frameHwnd));
	}

	delete g_wbViewportHost;		// deletes the QWinHost child too; view already detached
	g_wbViewportHost = NULL;
	g_wbViewportPane = NULL;
}
