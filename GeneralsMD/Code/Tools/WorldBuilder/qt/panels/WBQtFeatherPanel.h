// WBQtFeatherPanel.h -- Qt replacement for the MFC FeatherOptions dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and
// gets the dark title bar automatically. Its controls drive FeatherTool via the reverse
// callbacks in WBQtPanelBridge.h; FeatherTool pushes display updates back through the
// WBQtFeather_Push* functions (defined in the .cpp). The MFC FeatherOptions stays as the
// toggle-OFF fallback; this is the RTS_HAS_QT path.
#ifndef WB_QT_FEATHER_PANEL_H
#define WB_QT_FEATHER_PANEL_H

#include <QWidget>

class QSlider;
class QSpinBox;
class QLabel;
class QCheckBox;

namespace Ui { class WBQtFeatherPanel; }	// generated from WBQtFeatherPanel.ui

class WBQtFeatherPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtFeatherPanel(QWidget *owner);
	virtual ~WBQtFeatherPanel();

	// Tool -> widget display push (called by the free WBQtFeather_Push* functions).
	void pushFeather(int v);
	void pushRadius(int v);
	void pushRate(int v);

	static WBQtFeatherPanel *instance() { return s_instance; }

private slots:
	void onFeatherChanged(int v);
	void onRadiusChanged(int v);
	void onRateChanged(int v);
	void onMirror();
	void onMirrorX();
	void onMirrorY();
	void onMirrorXY();

private:
	void setRow(QSlider *slider, QSpinBox *spin, int v);	// set both without re-entry

	Ui::WBQtFeatherPanel *m_ui;	// owns the static widget tree (WBQtFeatherPanel.ui)

	QSlider   *m_featherSlider;
	QSpinBox  *m_featherSpin;
	QLabel    *m_feetLabel;
	QSlider   *m_radiusSlider;
	QSpinBox  *m_radiusSpin;
	QSlider   *m_rateSlider;
	QSpinBox  *m_rateSpin;
	QCheckBox *m_mirror;
	QCheckBox *m_mirrorX;
	QCheckBox *m_mirrorY;
	QCheckBox *m_mirrorXY;

	bool m_updating;	// re-entrancy guard, mirrors MFC FeatherOptions::m_updating

	static WBQtFeatherPanel *s_instance;
};

#endif // WB_QT_FEATHER_PANEL_H
