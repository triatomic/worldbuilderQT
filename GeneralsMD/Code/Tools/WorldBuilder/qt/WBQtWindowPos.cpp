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
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVariant>
#include <QWidget>

// MFC-side bridge (src/WBQtHostBridge.cpp): the [QtWindowPositions] INI accessors. Get returns
// 1 and fills *top/*left when a saved value exists, 0 otherwise.
extern "C" int  WBQtWindowPos_Get(const char *name, int *topOut, int *leftOut);
extern "C" void WBQtWindowPos_Save(const char *name, int top, int left);
extern "C" void WBQtWindowPos_ClearSaved(void);
// Companion [QtWindowSize] accessors; Get returns 1 and fills *width/*height when saved.
extern "C" int  WBQtWindowSize_Get(const char *name, int *widthOut, int *heightOut);
extern "C" void WBQtWindowSize_Save(const char *name, int width, int height);

namespace
{
	// Stamped on a tracked window: present (valid) means a second Track() call is a no-op, and
	// its bool value is "position is tracked too". Size-only (modal) windows store false, which
	// is how Reset Window Positions knows not to drag them out of their normal centering.
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
		// trackPos false = size only: the window still persists its Width/Height but is left to
		// be positioned normally (modal dialogs re-center on each open, which is the intended
		// behavior -- see the header).
		WBQtWindowPosTracker(QWidget *window, const QByteArray &name, bool trackPos)
			: QObject(window),
			  m_window(window),
			  m_name(name),
			  m_trackPos(trackPos),
			  m_restored(false),
			  m_hasPendingPos(false)
		{
			window->installEventFilter(this);
		}

		virtual ~WBQtWindowPosTracker()
		{
			// A modeless window can go all the way to process exit without ever being hidden,
			// so its last move/resize would otherwise never reach the INI.
			flushPending();
		}

	protected:
		virtual bool eventFilter(QObject *obj, QEvent *event)
		{
			if (obj == m_window)
			{
				if (event->type() == QEvent::Show)
				{
					if (m_trackPos)
					{
						// (Re)install the drag clamp. The native window can be recreated behind
						// the widget, and re-subclassing with the same id/proc is a cheap no-op,
						// so doing it on every Show keeps the hook alive.
						::SetWindowSubclass(reinterpret_cast<HWND>(m_window->winId()),
							wbPosSubclassProc, 1, 0);
					}
					if (!m_restored)
					{
						m_restored = true;
						restoreGeometry();
					}
				}
				else if (event->type() == QEvent::Move && m_trackPos)
				{
					// Only echo user moves back; the restore move() above happens before the
					// window is visible on screen for the FIRST show, but guard on visibility
					// anyway so programmatic seeding isn't written back. Stashed rather than
					// written here for the same reason as the size below: Move fires per tick
					// of a title-bar drag and each save is two WorldBuilder.ini writes.
					if (m_window->isVisible()
						&& !(m_window->windowState() & Qt::WindowMinimized))
					{
						m_pendingPos = m_window->frameGeometry().topLeft();
						m_hasPendingPos = true;
					}
				}
				else if (event->type() == QEvent::Resize)
				{
					// Remember the size, but do NOT write it here: Resize fires on every tick of
					// a drag-resize and every profile write hits WorldBuilder.ini. Stash it and
					// flush once on Hide (below). Skip minimized/maximized states -- a maximized
					// size restored as the normal size leaves the window wrongly huge next open.
					if (m_window->isVisible()
						&& !(m_window->windowState() & (Qt::WindowMinimized | Qt::WindowMaximized)))
					{
						m_pendingSize = m_window->size();
					}
				}
				else if (event->type() == QEvent::Hide)
				{
					// One write per close, matching the hand-rolled Team Sheet persistence this
					// generic tracker replaced.
					flushPending();
				}
			}
			return QObject::eventFilter(obj, event);
		}

	private:
		// Re-apply the stored geometry on the window's FIRST show, before it is on screen.
		void restoreGeometry()
		{
			// Size first, so the position restore below measures the final frame.
			int width = 0;
			int height = 0;
			if (WBQtWindowSize_Get(m_name.constData(), &width, &height))
			{
				// Never shrink below what the layout needs, or the dialog comes back clipped;
				// a size saved by a build with different content is stale.
				const QSize hint = m_window->minimumSizeHint();
				m_window->resize(qMax(width, hint.width()), qMax(height, hint.height()));
			}
			if (!m_trackPos)
			{
				return;		// size-only (modal): leave it to center normally
			}
			int top = 0;
			int left = 0;
			if (WBQtWindowPos_Get(m_name.constData(), &top, &left))
			{
				// Clamp the stored position back on screen first -- it can be stale (monitor
				// unplugged, resolution lowered) or from a pre-clamp build.
				const QRect frame = m_window->frameGeometry();
				RECT r = { left, top, left + frame.width(), top + frame.height() };
				clampRectToNearestWorkArea(&r);
				// move() targets the frame corner, matching the saved frameGeometry.
				m_window->move(r.left, r.top);
			}
		}

		// Write whatever the user last left us, once. Move/Resize only stash (they fire per tick
		// of a drag and every save is two WorldBuilder.ini writes); this is the single flush.
		void flushPending()
		{
			if (m_hasPendingPos)
			{
				WBQtWindowPos_Save(m_name.constData(), m_pendingPos.y(), m_pendingPos.x());
				m_hasPendingPos = false;
			}
			if (m_pendingSize.isValid())
			{
				WBQtWindowSize_Save(m_name.constData(),
					m_pendingSize.width(), m_pendingSize.height());
				m_pendingSize = QSize();
			}
		}

		QWidget *m_window;
		QByteArray m_name;
		bool m_trackPos;
		bool m_restored;
		QPoint m_pendingPos;	// last user frame top-left; flushed on Hide / destruction
		bool m_hasPendingPos;
		QSize m_pendingSize;	// last user size; flushed on Hide / destruction
	};
}

namespace
{
	// Shared body of the two public entry points; they differ only in whether position is
	// tracked as well as size.
	void trackImpl(QWidget *window, const char *name, bool trackPos)
	{
		if (window == NULL || name == NULL)
		{
			return;
		}
		if (window->property(kTrackedProp).isValid())
		{
			return;		// already tracked
		}
		// One property carries both facts: "is tracked" (valid) and "is its position tracked"
		// (the value), the latter being what ResetAll filters on.
		window->setProperty(kTrackedProp, trackPos);
		new WBQtWindowPosTracker(window, QByteArray(name), trackPos);
	}
}

void WBQtWindowPos_Track(QWidget *window, const char *name)
{
	trackImpl(window, name, true);
}

void WBQtWindowPos_TrackSize(QWidget *window, const char *name)
{
	trackImpl(window, name, false);
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
			continue;	// untracked, or size-only (modal): leave its placement alone
		}
		w->move(60 + (placed % 8) * 30, 60 + (placed % 8) * 30);
		placed++;
	}
}
