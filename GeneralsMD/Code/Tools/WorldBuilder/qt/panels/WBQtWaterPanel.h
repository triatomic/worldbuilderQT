// WBQtWaterPanel.h -- Qt replacement for the MFC WaterOptions dialog.
//
// Mirrors the MFC dialog: a "Water Polygon" toggle + a point-spacing edit (both global tool
// state), and -- when a single water-area PolygonTrigger is selected -- the trigger's name, a
// water-height slider+spinbox row (driving a MovePolygonUndoable, same as the MFC popup slider
// + edit), and a "Make River" toggle. WaterOptions::update() re-seeds it via
// WBQtWater_PushRefresh() whenever the selection changes.
//
// The MFC WaterOptions stays as the toggle-OFF fallback; this is the RTS_HAS_QT path. A
// top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_WATER_PANEL_H
#define WB_QT_WATER_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QSlider;
class QSpinBox;

namespace Ui { class WBQtWaterPanel; }	// generated from WBQtWaterPanel.ui

class WBQtWaterPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtWaterPanel(QWidget *owner);
	virtual ~WBQtWaterPanel();

	// Re-seed every control from the current MFC selection (WBQtWater_PushRefresh).
	void pushRefresh();

	static WBQtWaterPanel *instance() { return s_instance; }

private slots:
	void onWaterPolygonToggled();
	void onSpacingChanged(int v);
	void onNameChanged();
	void onHeightChanged(int v);
	void onHeightSliderPressed();		// begin a slider drag (one undoable across it)
	void onHeightSliderReleased();		// end the slider drag
	void onMakeRiverToggled();
	void onOpenWaveEditor();			// activate the in-editor Wave Editor tool

private:
	void setHeightRow(int v);	// keep the height slider + spinbox in lockstep (caller guards)

	Ui::WBQtWaterPanel *m_ui;	// owns the static widget tree (WBQtWaterPanel.ui)

	QCheckBox *m_waterPolygon;
	QSpinBox  *m_spacing;

	QComboBox *m_name;
	QLabel    *m_heightLabel;
	QSlider   *m_heightSlider;
	QSpinBox  *m_heightSpin;
	QCheckBox *m_makeRiver;

	bool m_updating;	// re-entrancy guard, mirrors MFC WaterOptions::m_updating
	bool m_heightDragging;	// true between the slider's press and release (drag-step vs one-shot)

	static WBQtWaterPanel *s_instance;
};

#endif // WB_QT_WATER_PANEL_H
