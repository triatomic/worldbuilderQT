// WBQtRulerPanel.h -- Qt replacement for the MFC RulerOptions dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and
// gets the dark title bar automatically. Its controls drive RulerTool via the reverse
// callbacks in WBQtPanelBridge.h. Unlike the terrain-brush panels this one has no slider
// rows: a "Circle ruler" toggle (which enables the width field), a width field (circle
// diameter in the selected display unit), and "Use meters" / "Show ruler grid" checkboxes.
// The MFC RulerOptions stays as the toggle-OFF fallback; this is the RTS_HAS_QT path.
#ifndef WB_QT_RULER_PANEL_H
#define WB_QT_RULER_PANEL_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

namespace Ui { class WBQtRulerPanel; }	// generated from WBQtRulerPanel.ui

class WBQtRulerPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtRulerPanel(QWidget *owner);
	virtual ~WBQtRulerPanel();

	static WBQtRulerPanel *instance() { return s_instance; }

private slots:
	void onCircleToggled();
	void onWidthChanged(double v);
	void onUseMetersToggled();
	void onShowGridToggled();

private:
	void refreshWidthFromTool();	// re-display the stored length in the current unit
	void syncCircleState();			// checkbox + width-field-enabled from the tool's type

	Ui::WBQtRulerPanel *m_ui;	// owns the static widget tree (WBQtRulerPanel.ui)

	QCheckBox      *m_circle;
	QDoubleSpinBox *m_width;
	QLabel         *m_widthLabel;
	QCheckBox      *m_useMeters;
	QCheckBox      *m_showGrid;

	bool m_updating;	// re-entrancy guard, mirrors MFC RulerOptions::m_updating

	static WBQtRulerPanel *s_instance;
};

#endif // WB_QT_RULER_PANEL_H
