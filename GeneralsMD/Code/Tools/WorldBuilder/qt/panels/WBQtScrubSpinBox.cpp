// WBQtScrubSpinBox.cpp -- see WBQtScrubSpinBox.h.
#include "WBQtScrubSpinBox.h"

#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>

// Pixels the cursor must move before a press turns into a scrub (so a plain click that lands in
// the edit field to place the caret / select text isn't hijacked).
static const int kScrubThreshold = 3;

WBQtScrubSpinBox::WBQtScrubSpinBox(QWidget *parent, bool axisVertical)
	: QDoubleSpinBox(parent),
	  m_vertical(axisVertical),
	  m_scrubbing(false),
	  m_dragged(false),
	  m_pressPos(0),
	  m_pressValue(0.0),
	  m_scrubStep(0.0)
{
	// Watch the internal line edit: it covers the number, so mouse events over the text go to it,
	// not to the spinbox itself. The arrow buttons are outside the line edit and keep working.
	if (lineEdit() != NULL)
	{
		lineEdit()->installEventFilter(this);
	}
}

bool WBQtScrubSpinBox::eventFilter(QObject *watched, QEvent *event)
{
	if (lineEdit() != NULL && watched == lineEdit())
	{
		switch (event->type())
		{
			case QEvent::MouseButtonPress:
			{
				QMouseEvent *me = static_cast<QMouseEvent *>(event);
				if (me->button() == Qt::LeftButton && isEnabled())
				{
					// Arm a potential scrub, but let the line edit handle the press normally so a
					// plain click still places the caret. We only take over once it becomes a drag.
					m_scrubbing = true;
					m_dragged = false;
					m_pressPos = m_vertical ? me->pos().y() : me->pos().x();
					m_pressValue = value();
				}
				break;	// don't consume -- fall through to the line edit
			}
			case QEvent::MouseMove:
			{
				if (m_scrubbing)
				{
					QMouseEvent *me = static_cast<QMouseEvent *>(event);
					// Vertical: up (smaller y) increases, so negate dy. Horizontal: right increases.
					int delta = m_vertical ? (m_pressPos - me->pos().y())
						: (me->pos().x() - m_pressPos);
					if (!m_dragged && (delta > kScrubThreshold || delta < -kScrubThreshold))
					{
						m_dragged = true;
						lineEdit()->setCursor(m_vertical ? Qt::SizeVerCursor : Qt::SizeHorCursor);
						// Drop any text selection the initial press started.
						lineEdit()->deselect();
					}
					if (m_dragged)
					{
						double per = (m_scrubStep > 0.0) ? m_scrubStep : singleStep();
						// setValue clamps + emits valueChanged(double) on a real change, so the
						// panel's slot fires and drives the tool.
						setValue(m_pressValue + delta * per);
						return true;	// consume: keep the line edit from extending a selection
					}
				}
				break;
			}
			case QEvent::MouseButtonRelease:
			{
				bool wasDrag = m_dragged;
				m_scrubbing = false;
				m_dragged = false;
				lineEdit()->unsetCursor();
				if (wasDrag)
				{
					return true;	// consume the release that ended a scrub
				}
				break;	// a plain click: let the line edit finish normally
			}
			default:
				break;
		}
	}
	return QDoubleSpinBox::eventFilter(watched, event);
}
