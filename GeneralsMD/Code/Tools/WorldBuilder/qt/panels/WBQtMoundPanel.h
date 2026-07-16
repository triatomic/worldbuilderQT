// WBQtMoundPanel.h -- Qt replacement for the MFC MoundOptions (Mound/Dig) dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and
// gets the dark title bar automatically. Its controls drive MoundTool via the reverse
// callbacks in WBQtPanelBridge.h; MoundTool pushes display updates back through the
// WBQtMound_Push* functions (defined in the .cpp). The MFC MoundOptions stays as the
// toggle-OFF fallback; this is the RTS_HAS_QT path. Structural twin of WBQtBrushPanel
// (three editable width/feather/height rows + four mirror checkboxes), different ranges.
#ifndef WB_QT_MOUND_PANEL_H
#define WB_QT_MOUND_PANEL_H

#include <QWidget>

class QSlider;
class QSpinBox;
class QLabel;
class QCheckBox;

namespace Ui { class WBQtMoundPanel; }	// generated from WBQtMoundPanel.ui

class WBQtMoundPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtMoundPanel(QWidget *owner);
	virtual ~WBQtMoundPanel();

	// Tool -> widget display push (called by the free WBQtMound_Push* functions).
	void pushWidth(int v);
	void pushFeather(int v);
	void pushHeight(int v);

	static WBQtMoundPanel *instance() { return s_instance; }

private slots:
	void onWidthChanged(int v);
	void onFeatherChanged(int v);
	void onHeightChanged(int v);
	void onMirror();
	void onMirrorX();
	void onMirrorY();
	void onMirrorXY();

private:
	void setRow(QSlider *slider, QSpinBox *spin, int v);	// set both without re-entry

	Ui::WBQtMoundPanel *m_ui;	// owns the static widget tree (WBQtMoundPanel.ui)

	QSlider   *m_widthSlider;
	QSpinBox  *m_widthSpin;
	QLabel    *m_widthLabel;
	QSlider   *m_featherSlider;
	QSpinBox  *m_featherSpin;
	QLabel    *m_featherLabel;
	QSlider   *m_heightSlider;
	QSpinBox  *m_heightSpin;
	QLabel    *m_heightLabel;
	QCheckBox *m_mirror;
	QCheckBox *m_mirrorX;
	QCheckBox *m_mirrorY;
	QCheckBox *m_mirrorXY;

	bool m_updating;	// re-entrancy guard, mirrors MFC MoundOptions::m_updating

	static WBQtMoundPanel *s_instance;
};

#endif // WB_QT_MOUND_PANEL_H
