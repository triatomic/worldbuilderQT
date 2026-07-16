// WBQtScorchPanel.h -- Qt replacement for the MFC ScorchOptions dialog (Scorch Options).
//
// A top-level Qt::Tool window owned by the shared QWinWidget bridge (see
// WBQtOptionsPanels.cpp) so it floats over the MFC main window with correct stacking and gets
// the dark title bar automatically. It edits the currently selected scorch MapObject via the
// reverse callbacks in WBQtScorchBridge.h (which drive the MFC ScorchOptions statics +
// changeScorch()/changeSize()); ScorchOptions::update() pushes display refreshes back through
// WBQtScorch_PushRefresh (defined in the .cpp). Two controls, mirroring the MFC dialog: a
// scorch-type QComboBox and a scorch-size QDoubleSpinBox (the size popup-slider + edit collapse
// into one spin box in Qt). The MFC ScorchOptions stays as the toggle-OFF fallback; this is the
// RTS_HAS_QT path.
#ifndef WB_QT_SCORCH_PANEL_H
#define WB_QT_SCORCH_PANEL_H

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;

namespace Ui { class WBQtScorchPanel; }	// generated from WBQtScorchPanel.ui

class WBQtScorchPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtScorchPanel(QWidget *owner);
	virtual ~WBQtScorchPanel();

	// Re-seed the type combo + size field from the current MFC statics (WBQtScorch_PushRefresh).
	void pushRefresh();

	static WBQtScorchPanel *instance() { return s_instance; }

private slots:
	void onTypeChanged(int index);
	void onSizeChanged(double v);

private:
	Ui::WBQtScorchPanel *m_ui;	// owns the static widget tree (WBQtScorchPanel.ui)

	QComboBox      *m_type;
	QDoubleSpinBox *m_size;

	bool m_updating;	// re-entrancy guard, mirrors MFC ScorchOptions::m_updating

	static WBQtScorchPanel *s_instance;
};

#endif // WB_QT_SCORCH_PANEL_H
