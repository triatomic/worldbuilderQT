// WBQtTracingOverlayWindow.h -- Qt replacement for the TracingOverlayOptions modeless
// dialog ("Tracing Overlay Settings"): an opacity slider (0..100, applied live while
// dragging, persisted when the drag ends) and a resize-interpolation combo
// (Default / Nearest, applied + persisted on change). OK / Esc / close just hide the
// cached singleton; the values were already applied live (== the MFC dialog).
#ifndef WB_QT_TRACING_OVERLAY_WINDOW_H
#define WB_QT_TRACING_OVERLAY_WINDOW_H

#include <QWidget>

class QComboBox;
class QLabel;
class QSlider;

namespace Ui { class WBQtTracingOverlayWindow; }	// generated from WBQtTracingOverlayWindow.ui

class WBQtTracingOverlayWindow : public QWidget
{
	Q_OBJECT
public:
	explicit WBQtTracingOverlayWindow(QWidget *owner);
	virtual ~WBQtTracingOverlayWindow();

	static WBQtTracingOverlayWindow *instance() { return s_instance; }

	// Re-read the persisted settings into the controls (on every open).
	void seedFromSettings();

	// Persist the current values once (commit point) -- used when the window hides.
	void commitCurrent();

protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual void keyPressEvent(QKeyEvent *event);

private slots:
	void onOpacityChanged(int value);
	void onOpacityReleased();
	void onFilterChanged(int index);
	void onOkClicked();

private:
	void updateOpacityLabel(int pct);

	Ui::WBQtTracingOverlayWindow *m_ui;	// owns the static widget tree (WBQtTracingOverlayWindow.ui)

	QSlider   *m_opacitySlider;
	QLabel    *m_opacityLabel;
	QComboBox *m_filterCombo;
	bool       m_updating;

	static WBQtTracingOverlayWindow *s_instance;
};

#endif // WB_QT_TRACING_OVERLAY_WINDOW_H
