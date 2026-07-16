// WBQtRampPanel.h -- Qt replacement for the MFC RampOptions dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and
// gets the dark title bar automatically. Its controls drive the ramp tool via the reverse
// callbacks in WBQtPanelBridge.h. Unlike the other panels the ramp width + "apply" latch
// live on the MFC RampOptions object (which RampTool reads); this widget writes that state
// through the bridge. Mirror state is on RampTool statics. The MFC RampOptions stays as the
// toggle-OFF fallback; this is the RTS_HAS_QT path.
#ifndef WB_QT_RAMP_PANEL_H
#define WB_QT_RAMP_PANEL_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;

namespace Ui { class WBQtRampPanel; }	// generated from WBQtRampPanel.ui

class WBQtRampPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtRampPanel(QWidget *owner);
	virtual ~WBQtRampPanel();

	static WBQtRampPanel *instance() { return s_instance; }

private slots:
	void onWidthChanged(double v);
	void onApply();
	void onMirror();
	void onMirrorX();
	void onMirrorY();
	void onMirrorXY();

private:
	Ui::WBQtRampPanel *m_ui;	// owns the static widget tree (WBQtRampPanel.ui)

	QDoubleSpinBox *m_width;
	QCheckBox      *m_mirror;
	QCheckBox      *m_mirrorX;
	QCheckBox      *m_mirrorY;
	QCheckBox      *m_mirrorXY;

	bool m_updating;	// re-entrancy guard while seeding

	static WBQtRampPanel *s_instance;
};

#endif // WB_QT_RAMP_PANEL_H
