// WBQtWavePanel.h -- Qt replacement for the MFC WaveEditorOptions dialog.
//
// The wave editor panel: current type + Cycle/Undo/Save/Reload, the four editor-mode toggle
// buttons (Create/Manipulate/Paint/Bucket), the bucket brush-size slider (Bucket mode only), the
// help text, the multi-select wave list (mirrors the tool's selection both ways), Delete
// Selected/All, and the two overlay checkboxes. All state lives on WaveEditorTool / DrawObject
// statics behind the WBQtWave_* facade; the hidden MFC WaveEditorOptions stays as the OFF
// fallback. WaveEditorOptions::refresh() re-seeds this panel via WBQtWave_PushRefresh(), and the
// '['/']' view hotkeys re-seed the brush slider via WBQtWave_PushBrushSize().
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge, like the other panels.
#ifndef WB_QT_WAVE_PANEL_H
#define WB_QT_WAVE_PANEL_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class QTreeWidget;

class WBQtWavePanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtWavePanel(QWidget *owner);

	// Re-seed the type label + wave list from the tool (WBQtWave_PushRefresh).
	void pushRefresh();
	// Re-seed the brush slider + readout (WBQtWave_PushBrushSize -- the '['/']' hotkeys).
	void pushBrushSize();

	static WBQtWavePanel *instance() { return s_instance; }

protected:
	// Panel hidden (tool swap / right-click closes options): drop the wave highlight so a wave
	// doesn't stay selected with no panel to manage it (mirrors MFC OnShowWindow(FALSE)).
	virtual void hideEvent(QHideEvent *event);

private slots:
	void onCycleType();
	void onUndo();
	void onSave();
	void onReload();
	void onModeClicked();
	void onBrushSliderMoved(int v);
	void onDeleteSelected();
	void onDeleteAll();
	void onShowWaveLinesToggled();
	void onShowShorelineToggled();
	void onListSelectionChanged();
	void onListContextMenu(const QPoint &pos);

private:
	void updateTypeLabel();
	void updateBrushLabel();
	void syncModeButtons(int mode);	// exactly one mode button pressed; brush row only in Bucket
	void populateList();
	void syncToolSelectionFromList();

	QLabel      *m_typeLabel;
	QPushButton *m_modeCreate;
	QPushButton *m_modeManipulate;
	QPushButton *m_modePaint;
	QPushButton *m_modeBucket;
	QLabel      *m_brushLabel;
	QSlider     *m_brushSlider;
	QTreeWidget *m_list;
	QCheckBox   *m_showLines;
	QCheckBox   *m_showShoreline;

	bool m_updating;	// guard so programmatic list/control changes don't re-fire slots

	static WBQtWavePanel *s_instance;
};

#endif // WB_QT_WAVE_PANEL_H
