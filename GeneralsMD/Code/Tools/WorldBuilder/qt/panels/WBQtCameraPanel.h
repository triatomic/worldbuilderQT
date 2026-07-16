// WBQtCameraPanel.h -- Qt replacement for the MFC CameraOptions dialog.
//
// The modeless Camera Options window (View > Camera Options): a pitch scrub-spinbox (replacing
// the MFC popup slider), Restore To Default, the read-only camera info (height above ground,
// zoom, position, target -- live-updated per camera move via WBQtCamera_PushRefresh), and the
// Drop Waypoint / Center Camera On Selected Object buttons. All calls go through the
// WBQtCamera_* facade; the hidden MFC CameraOptions stays as the OFF fallback.
#ifndef WB_QT_CAMERA_PANEL_H
#define WB_QT_CAMERA_PANEL_H

#include <QWidget>

class QLabel;
class WBQtScrubSpinBox;

namespace Ui { class WBQtCameraPanel; }	// generated from WBQtCameraPanel.ui

class WBQtCameraPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtCameraPanel(QWidget *owner);
	virtual ~WBQtCameraPanel();

	// Re-seed the pitch + info readouts from the view. Called per camera move while visible.
	void pushRefresh();

	static WBQtCameraPanel *instance() { return s_instance; }

protected:
	virtual void showEvent(QShowEvent *event);	// refresh on show, like the MFC OnShowWindow

private slots:
	void onReset();
	void onPitchChanged();
	void onDropWaypoint();
	void onCenterOnSelected();

private:
	Ui::WBQtCameraPanel *m_ui;	// owns the static widget tree (WBQtCameraPanel.ui)

	WBQtScrubSpinBox *m_pitch;
	QLabel *m_heightText;
	QLabel *m_zoomText;
	QLabel *m_posText;
	QLabel *m_targetText;

	bool m_updating;

	static WBQtCameraPanel *s_instance;
};

#endif // WB_QT_CAMERA_PANEL_H
