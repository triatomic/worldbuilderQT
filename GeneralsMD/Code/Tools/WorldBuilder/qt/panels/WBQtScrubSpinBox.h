// WBQtScrubSpinBox.h -- a QDoubleSpinBox you can click-drag horizontally to scrub the value.
//
// The MFC MapObjectProps dialog has a popup-slider button beside the Z / Angle edits for
// dragging the value. The Qt equivalent used here is a spinbox that scrubs on a horizontal mouse
// drag (like the number fields in Blender / 3ds Max): press and drag left/right over the field to
// decrease / increase the value, with no travel limit. Typing and the arrow buttons still work.
//
// QAbstractSpinBox hosts an internal child QLineEdit that covers the text area, so the spinbox's
// own mouse handlers never see clicks on the number. We therefore watch the line edit with an
// event filter and drive the value from there. No Q_OBJECT (no new signals/slots) -- value changes
// still emit the base valueChanged(double), so existing connections fire.
#ifndef WB_QT_SCRUB_SPINBOX_H
#define WB_QT_SCRUB_SPINBOX_H

#include <QDoubleSpinBox>

class QObject;
class QEvent;

class WBQtScrubSpinBox : public QDoubleSpinBox
{
public:
	// axisVertical: false (default) scrubs on horizontal drag (right = increase); true scrubs on
	// vertical drag (up = increase), which suits a height/elevation field like Z.
	explicit WBQtScrubSpinBox(QWidget *parent = 0, bool axisVertical = false);

	// Value change per pixel of drag (defaults to singleStep()).
	void setScrubStep(double perPixel) { m_scrubStep = perPixel; }

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event);

private:
	bool   m_vertical;		// scrub axis: false = horizontal, true = vertical
	bool   m_scrubbing;
	bool   m_dragged;		// became a real drag (moved past the threshold)
	int    m_pressPos;		// x (horizontal) or y (vertical) at press
	double m_pressValue;
	double m_scrubStep;		// value per pixel; <= 0 means use singleStep()
};

#endif // WB_QT_SCRUB_SPINBOX_H
