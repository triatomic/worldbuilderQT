// WBQtWindowPos.cpp -- see WBQtWindowPos.h.
//
// One tracker object per window (parented to the window, so it dies with it). It filters the
// window's events: on the FIRST Show it re-applies the saved Top/Left (before the user can
// move it), and on every subsequent Move while visible it writes the new frame Top/Left back.
// Storage is WorldBuilder.ini via the MFC-side bridge (WBQtWindowPos_Save/Get), keyed by the
// window's tracking name.
#include "WBQtWindowPos.h"

#include <qt_windows.h>
#include <commctrl.h>
#include <QApplication>
#include <QEvent>
#include <QMoveEvent>
#include <QRect>
#include <QVariant>
#include <QWidget>

// MFC-side bridge (src/WBQtHostBridge.cpp): the [QtWindowPositions] INI accessors. Get returns
// 1 and fills *top/*left when a saved value exists, 0 otherwise.
extern "C" int  WBQtWindowPos_Get(const char *name, int *topOut, int *leftOut);
extern "C" void WBQtWindowPos_Save(const char *name, int top, int left);
extern "C" void WBQtWindowPos_ClearSaved(void);

namespace
{
	// A property stamped on a tracked window so a second Track() call is a no-op.
	const char *const kTrackedProp = "wbPosTracked";

	// Shift the rect (size preserved) so the window stays reachable on the nearest monitor:
	// fully inside the work area horizontally and at the top, but downward it may hang into
	// or below the taskbar as long as the whole title bar stays visible above it -- enough
	// to always grab the window back. Used for the live drag clamp and the restore clamp.
	void clampRectToNearestWorkArea(RECT *r)
	{
		HMONITOR monitor = ::MonitorFromRect(r, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (monitor == NULL || !::GetMonitorInfo(monitor, &mi))
		{
			return;
		}
		LONG dx = 0;
		LONG dy = 0;
		if (r->right > mi.rcWork.right)
		{
			dx = mi.rcWork.right - r->right;
		}
		if (r->left + dx < mi.rcWork.left)
		{
			dx = mi.rcWork.left - r->left;
		}
		// The full standard caption height covers a tool window's smaller caption too.
		const LONG minVisibleY =
			::GetSystemMetrics(SM_CYCAPTION) + ::GetSystemMetrics(SM_CYSIZEFRAME);
		if (r->top > mi.rcWork.bottom - minVisibleY)
		{
			dy = mi.rcWork.bottom - minVisibleY - r->top;
		}
		if (r->top + dy < mi.rcWork.top)
		{
			dy = mi.rcWork.top - r->top;
		}
		::OffsetRect(r, dx, dy);
	}

	// Live drag clamp: adjust the WM_MOVING rect so the user cannot drag a tracked window
	// outside the monitor work area -- the window just stops at the edge. Clamping the
	// proposed rect (instead of move()-ing back afterwards) is jitter-free because the OS
	// itself places the window at the adjusted spot. Crossing to another monitor still
	// works: once most of the proposed rect overlaps the other monitor, MonitorFromRect
	// picks that one and the window snaps fully onto it.
	//
	// This must be a Win32 subclass (SetWindowSubclass), NOT a QAbstractNativeEventFilter:
	// WM_MOVING is not one of the messages Qt's window proc translates, so it never reaches
	// the app-level native filters -- only a real wndproc-chain hook sees it.
	LRESULT CALLBACK wbPosSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
		UINT_PTR subclassId, DWORD_PTR refData)
	{
		Q_UNUSED(refData);
		if (msg == WM_MOVING)
		{
			::DefSubclassProc(hwnd, msg, wParam, lParam);
			clampRectToNearestWorkArea(reinterpret_cast<RECT *>(lParam));
			return TRUE;
		}
		if (msg == WM_NCDESTROY)
		{
			::RemoveWindowSubclass(hwnd, wbPosSubclassProc, subclassId);
		}
		return ::DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	class WBQtWindowPosTracker : public QObject
	{
	public:
		WBQtWindowPosTracker(QWidget *window, const QByteArray &name)
			: QObject(window),
			  m_window(window),
			  m_name(name),
			  m_restored(false)
		{
			window->installEventFilter(this);
		}

	protected:
		virtual bool eventFilter(QObject *obj, QEvent *event)
		{
			if (obj == m_window)
			{
				if (event->type() == QEvent::Show)
				{
					// (Re)install the drag clamp. The native window can be recreated behind
					// the widget, and re-subclassing with the same id/proc is a cheap no-op,
					// so doing it on every Show keeps the hook alive.
					::SetWindowSubclass(reinterpret_cast<HWND>(m_window->winId()),
						wbPosSubclassProc, 1, 0);
				}
				if (event->type() == QEvent::Show && !m_restored)
				{
					m_restored = true;
					int top = 0;
					int left = 0;
					if (WBQtWindowPos_Get(m_name.constData(), &top, &left))
					{
						// Clamp the stored position back on screen first -- it can be stale
						// (monitor unplugged, resolution lowered) or from a pre-clamp build.
						const QRect frame = m_window->frameGeometry();
						RECT r = { left, top, left + frame.width(), top + frame.height() };
						clampRectToNearestWorkArea(&r);
						// move() targets the frame corner, matching the saved frameGeometry.
						m_window->move(r.left, r.top);
					}
				}
				else if (event->type() == QEvent::Move)
				{
					// Only echo user moves back; the restore move() above happens before the
					// window is visible on screen for the FIRST show, but guard on visibility
					// anyway so programmatic seeding isn't written back.
					if (m_window->isVisible()
						&& !(m_window->windowState() & Qt::WindowMinimized))
					{
						const QRect frame = m_window->frameGeometry();
						WBQtWindowPos_Save(m_name.constData(), frame.top(), frame.left());
					}
				}
			}
			return QObject::eventFilter(obj, event);
		}

	private:
		QWidget *m_window;
		QByteArray m_name;
		bool m_restored;
	};
}

void WBQtWindowPos_Track(QWidget *window, const char *name)
{
	if (window == NULL || name == NULL)
	{
		return;
	}
	if (window->property(kTrackedProp).toBool())
	{
		return;		// already tracked
	}
	window->setProperty(kTrackedProp, true);
	new WBQtWindowPosTracker(window, QByteArray(name));
}

void WBQtWindowPos_ResetAll(void)
{
	// Wipe the saved store first, then cascade the live tracked windows -- a visible
	// window's Move event re-saves its fresh spot through the normal tracking, and a
	// hidden one just keeps the new position for its next show.
	WBQtWindowPos_ClearSaved();
	int placed = 0;
	QWidgetList tops = QApplication::topLevelWidgets();
	for (int i = 0; i < tops.size(); i++)
	{
		QWidget *w = tops.at(i);
		if (!w->property(kTrackedProp).toBool())
		{
			continue;
		}
		w->move(60 + (placed % 8) * 30, 60 + (placed % 8) * 30);
		placed++;
	}
}
