// WBQtWindowPos.cpp -- see WBQtWindowPos.h.
//
// One tracker object per window (parented to the window, so it dies with it). It filters the
// window's events: on the FIRST Show it re-applies the saved Top/Left (before the user can
// move it), and on every subsequent Move while visible it writes the new frame Top/Left back.
// Storage is WorldBuilder.ini via the MFC-side bridge (WBQtWindowPos_Save/Get), keyed by the
// window's tracking name.
#include "WBQtWindowPos.h"

#include <QEvent>
#include <QMoveEvent>
#include <QRect>
#include <QVariant>
#include <QWidget>

// MFC-side bridge (src/WBQtHostBridge.cpp): the [QtWindowPositions] INI accessors. Get returns
// 1 and fills *top/*left when a saved value exists, 0 otherwise.
extern "C" int  WBQtWindowPos_Get(const char *name, int *topOut, int *leftOut);
extern "C" void WBQtWindowPos_Save(const char *name, int top, int left);

namespace
{
	// A property stamped on a tracked window so a second Track() call is a no-op.
	const char *const kTrackedProp = "wbPosTracked";

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
				if (event->type() == QEvent::Show && !m_restored)
				{
					m_restored = true;
					int top = 0;
					int left = 0;
					if (WBQtWindowPos_Get(m_name.constData(), &top, &left))
					{
						// move() targets the frame corner, matching the saved frameGeometry.
						m_window->move(left, top);
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
