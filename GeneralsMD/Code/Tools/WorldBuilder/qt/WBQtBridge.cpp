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
#include "WBQtMainWindow.h"
#include "WBQtTheme.h"

#include <QApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

// A QWinHost that reports its pixel size back to the MFC side so the D3D device tracks the
// host. The device reset is COALESCED through a short single-shot timer: a theme switch or a
// live drag-resize emits a burst of resizeEvents as the layout settles, and doing a full
// (expensive, on a heavy map) DX8 device reset synchronously in each one storms -- if one
// reset fails mid-cascade the failure loop never converges and the viewport freezes. The
// timer collapses the burst to a single reset once the size stops changing.
class WbViewportHost : public QWinHost
{
	Q_OBJECT
public:
	explicit WbViewportHost(QWidget *parent)
		: QWinHost(parent),
		m_resizeTimer(new QTimer(this))
	{
		m_resizeTimer->setSingleShot(true);
		m_resizeTimer->setInterval(60);
		connect(m_resizeTimer, SIGNAL(timeout()), this, SLOT(applyDeviceSize()));
	}

protected:
	virtual void resizeEvent(QResizeEvent *e)
	{
		QWinHost::resizeEvent(e);		// QWinHost sizes the hosted view to fill us
		m_resizeTimer->start();			// (re)arm; the last event in a burst wins
	}

private slots:
	void applyDeviceSize()
	{
		RECT rc;
		if (::GetClientRect(reinterpret_cast<HWND>(winId()), &rc))
		{
			WBQt_OnViewportHostResized(rc.right - rc.left, rc.bottom - rc.top);
		}
	}

private:
	QTimer *m_resizeTimer;
};

// The Phase 2 host sandwich, owned for the process life:
//   MFC frame -> g_wbViewportHost (QWinWidget) -> g_wbViewportPane (QWinHost) -> view.
// Stage 1 inverted: g_wbViewportHost stays NULL; the pane is the QMainWindow's central
// widget instead (QMainWindow -> g_wbViewportPane -> view).
static QWinWidget    *g_wbViewportHost = NULL;
static WbViewportHost *g_wbViewportPane = NULL;

// Stage 1: the inversion is the default in Qt builds; it flips off only if the main
// window cannot be created (then the legacy chrome-in-frame path runs).
static bool g_inversionActive = true;
static WBQtMainWindow *g_wbMainWindow = NULL;

int WBQt_InversionActive(void)
{
	return g_inversionActive ? 1 : 0;
}

void WBQt_DisableInversion(void)
{
	g_inversionActive = false;
}

// The window's CLIENT area (and with it the viewport pane / D3D backbuffer) must never
// exceed the display mode: a windowed D3D8 backbuffer larger than the screen cannot
// Present (D3DERR_DEVICELOST every frame -> an endless reset loop that reads as a frozen
// white viewport). The stored [MainFrame] Width/Height historically sized the MFC frame's
// OUTER rect (always screen-clamped by the WM), so applied as a Qt client size they can
// be oversized -- clamp against the available screen area.
static void wbClampToAvailableScreen(int &x, int &y, int &w, int &h)
{
	QScreen *screen = QApplication::primaryScreen();
	if (screen == NULL)
	{
		return;
	}
	QRect avail = screen->availableGeometry();
	// Approximate frame decorations (move() positions the frame corner while resize()
	// sizes the client) so the WHOLE window rect ends up on-screen, not just the client.
	const int frameX = 16;		// left+right resize borders
	const int frameY = 39;		// caption + top/bottom borders
	if (w > avail.width() - frameX)
	{
		w = avail.width() - frameX;
	}
	if (h > avail.height() - frameY)
	{
		h = avail.height() - frameY;
	}
	if (x + w + frameX > avail.right())
	{
		x = avail.right() - w - frameX;
	}
	if (y + h + frameY > avail.bottom())
	{
		y = avail.bottom() - h - frameY;
	}
	if (x < avail.left())
	{
		x = avail.left();
	}
	if (y < avail.top())
	{
		y = avail.top();
	}
}

int WBQt_CreateMainWindow(void *frameHwnd, int x, int y, int w, int h)
{
	if (g_wbMainWindow != NULL)
	{
		return 1;
	}
	if (qApp == NULL || frameHwnd == NULL)
	{
		g_inversionActive = false;
		return 0;
	}
	g_wbMainWindow = new WBQtMainWindow(frameHwnd);
	if (w > 0 && h > 0)
	{
		wbClampToAvailableScreen(x, y, w, h);
		g_wbMainWindow->move(x, y);
		g_wbMainWindow->resize(w, h);
	}
	return 1;
}

void WBQt_ShowMainWindow(void)
{
	if (g_wbMainWindow != NULL)
	{
		g_wbMainWindow->show();
	}
}

void *WBQt_MainWindowHwnd(void)
{
	return (g_wbMainWindow != NULL)
		? reinterpret_cast<void *>(g_wbMainWindow->winId()) : NULL;
}

// The Qt main window as a QWidget* (for parenting Qt dialogs / message boxes). NULL when
// not inverted. Not in the C facade -- callers in the Qt lib declare it themselves.
QWidget *WBQt_MainWindowWidget(void)
{
	return g_wbMainWindow;
}

// The correct parent for a modal Qt dialog: the currently-active modal (so a nested
// picker centers over and is owned by the dialog that opened it), else the main window.
// Stage 1 phase 3: replaces the manual EnableWindow(frame) discipline -- an ApplicationModal
// QDialog parented here fences every Qt window incl. the hosted viewport (via QWinHost
// WindowBlocked), which the old native-frame disable did for the pre-inversion viewport.
QWidget *WBQt_DialogParent(void)
{
	QWidget *active = QApplication::activeModalWidget();
	if (active != NULL)
	{
		return active;
	}
	return g_wbMainWindow;
}

void WBQt_ActivateMainWindow(void)
{
	if (g_wbMainWindow != NULL && g_wbMainWindow->isVisible())
	{
		g_wbMainWindow->raise();
		g_wbMainWindow->activateWindow();
	}
}

void WBQt_ResizeMainWindow(int width, int height)
{
	if (g_wbMainWindow != NULL && width > 0 && height > 0 && !g_wbMainWindow->isFullScreen())
	{
		// Same oversized-backbuffer guard as at creation (a 2560x1440 resolution pick on
		// a 2560x1400 desktop would otherwise wedge the device).
		int x = g_wbMainWindow->frameGeometry().left();
		int y = g_wbMainWindow->frameGeometry().top();
		wbClampToAvailableScreen(x, y, width, height);
		g_wbMainWindow->showNormal();		// a maximized window must leave that state first
		g_wbMainWindow->resize(width, height);
	}
}

void WBQt_MoveMainWindow(int x, int y)
{
	if (g_wbMainWindow != NULL)
	{
		g_wbMainWindow->showNormal();
		g_wbMainWindow->move(x, y);
	}
}

void WBQt_ToggleFullscreen(void)
{
	if (g_wbMainWindow != NULL)
	{
		g_wbMainWindow->toggleFullscreen();
	}
}

int WBQt_IsFullscreen(void)
{
	return (g_wbMainWindow != NULL && g_wbMainWindow->fullscreenActive()) ? 1 : 0;
}

void WBQt_SetMainWindowTitle(const char *title)
{
	if (g_wbMainWindow != NULL && title != NULL)
	{
		g_wbMainWindow->setWindowTitle(QString::fromLocal8Bit(title));
	}
}

void WBQt_FocusViewport(void)
{
	if (g_wbMainWindow != NULL)
	{
		g_wbMainWindow->activateWindow();
	}
	if (g_wbViewportPane != NULL)
	{
		// Qt focus lands on the QWinHost, whose focusInEvent forwards Win32 focus to the
		// hosted view HWND -- hotkeys and the GetAsyncKeyState tools resume immediately.
		g_wbViewportPane->setFocus();
	}
}

// Qt-internal (not in the C facade): the owner/parent for floating Qt tool windows.
// Inverted, that is the main window itself (Qt::Tool children float above their
// top-level natively); legacy, an invisible QWinWidget bridge rooted in the MFC frame.
// The caller caches the result either way and must NEVER hide() it (inverted it is the
// visible main window).
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd)
{
	if (g_wbMainWindow != NULL)
	{
		return g_wbMainWindow;
	}
	if (frameHwnd == NULL)
	{
		return NULL;
	}
	QWinWidget *owner = new QWinWidget(reinterpret_cast<HWND>(frameHwnd));
	owner->hide();
	return owner;
}

// Qt-internal: the native owner HWND for standalone Qt top-levels (script window,
// entity finder) -- the main window when inverted, else the MFC frame.
void *WBQt_EffectiveOwnerHwnd(void *frameHwnd)
{
	void *mainWin = WBQt_MainWindowHwnd();
	return (mainWin != NULL) ? mainWin : frameHwnd;
}

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
	// we release the main window (already hidden by the unhost) and Qt itself. Explicit
	// because the app dtor _exit(0)s right after.
	delete g_wbMainWindow;
	g_wbMainWindow = NULL;
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
	if (g_wbViewportPane != NULL)
	{
		return reinterpret_cast<void *>(g_wbViewportPane->winId());
	}
	if (frameHwnd == NULL || viewHwnd == NULL)
	{
		return NULL;
	}

	// The MFC view carries the default CView WS_EX_CLIENTEDGE sunken border; Windows
	// draws its highlight edge (right/bottom) in a light colour, which reads as an ugly
	// white line around the viewport against the dark theme. The Qt chrome column does
	// the visual framing now, so strip the edge from the hosted view.
	{
		HWND view = reinterpret_cast<HWND>(viewHwnd);
		LONG exStyle = ::GetWindowLong(view, GWL_EXSTYLE);
		if (exStyle & WS_EX_CLIENTEDGE)
		{
			::SetWindowLong(view, GWL_EXSTYLE, exStyle & ~WS_EX_CLIENTEDGE);
			::SetWindowPos(view, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}

	// Stage 1 inverted: the pane IS the QMainWindow's central widget -- no QWinWidget
	// layer; the main window's layout sizes the pane, whose resizeEvent drives the device.
	if (g_wbMainWindow != NULL)
	{
		g_wbViewportPane = new WbViewportHost(NULL);
		g_wbMainWindow->setCentralWidget(g_wbViewportPane);
		g_wbViewportPane->setWindow(reinterpret_cast<HWND>(viewHwnd));
		return reinterpret_cast<void *>(g_wbViewportPane->winId());
	}

	// Legacy (non-inverted): a Qt widget rooted in the MFC frame's HWND (QWinWidget syncs
	// Win32 activation).
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

// Tier 4a: the widget the chrome (WBQtChrome.cpp) installs into. Inverted, that is the
// QMainWindow itself (native menuBar/addToolBar/statusBar); legacy, the Phase-2 viewport
// host column (QLayout::setMenuBar + row inserts).
QWidget *WBQt_GetViewportHostWidget(void)
{
	if (g_wbMainWindow != NULL)
	{
		return g_wbMainWindow;
	}
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

	// Stage 1 inverted: the pane is the main window's central widget. Hide the (now
	// viewport-less) main window so no dead top-level lingers through the MFC/engine
	// teardown; the window itself is deleted in WBQt_Shutdown.
	if (g_wbMainWindow != NULL)
	{
		g_wbMainWindow->hide();
		delete g_wbViewportPane;	// ~QWinHost restores the view's AfxWndProc
		g_wbViewportPane = NULL;
		return;
	}

	delete g_wbViewportHost;		// deletes the QWinHost child too; view already detached
	g_wbViewportHost = NULL;
	g_wbViewportPane = NULL;
}

// Q_OBJECT for WbViewportHost (defined in this TU) -- AUTOMOC generates this from the
// Q_OBJECT above; the include pulls in the moc output.
#include "WBQtBridge.moc"
