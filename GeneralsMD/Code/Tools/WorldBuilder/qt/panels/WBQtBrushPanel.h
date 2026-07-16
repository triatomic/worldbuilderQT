// WBQtBrushPanel.h -- Qt replacement for the MFC BrushOptions (Height Brush) dialog.
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and
// gets the dark title bar automatically. Its controls drive BrushTool via the reverse
// callbacks in WBQtPanelBridge.h; BrushTool pushes display updates back through the
// WBQtBrush_Push* functions (defined in the .cpp). The MFC BrushOptions stays as the
// toggle-OFF fallback; this is the RTS_HAS_QT path. Twin of WBQtFeatherPanel, but with
// three editable rows (width/feather/height) each carrying its own "FEET" label.
#ifndef WB_QT_BRUSH_PANEL_H
#define WB_QT_BRUSH_PANEL_H

#include <QWidget>

class QSlider;
class QSpinBox;
class QLabel;
class QCheckBox;

namespace Ui { class WBQtBrushPanel; }	// generated from WBQtBrushPanel.ui

class WBQtBrushPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtBrushPanel(QWidget *owner);
	virtual ~WBQtBrushPanel();

	// Tool -> widget display push (called by the free WBQtBrush_Push* functions).
	void pushWidth(int v);
	void pushFeather(int v);
	void pushHeight(int v);

	static WBQtBrushPanel *instance() { return s_instance; }

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

	Ui::WBQtBrushPanel *m_ui;	// owns the static widget tree (WBQtBrushPanel.ui)

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

	bool m_updating;	// re-entrancy guard, mirrors MFC BrushOptions::m_updating

	static WBQtBrushPanel *s_instance;
};

#endif // WB_QT_BRUSH_PANEL_H
