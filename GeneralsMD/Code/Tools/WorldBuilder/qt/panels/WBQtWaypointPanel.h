// WBQtWaypointPanel.h -- Qt replacement for the MFC WaypointOptions dialog.
//
// Edits the single selected waypoint MapObject and/or the single selected PolygonTrigger --
// there is no state of its own. It is shown by WaypointTool, PolygonTool and PointerTool
// whenever a waypoint or a trigger area is selected; WaypointOptions::update() re-seeds it via
// WBQtWaypoint_PushRefresh(). The controls mirror the MFC dialog: a dual-purpose name combo
// (waypoint name OR trigger-area name, with a preset drop-down that depends on the kind), a
// waypoint location X/Y pair, three path-label edits + a bi-directional checkbox (shown only
// for a LINKED waypoint), and the two blocks of read-only helper text (AI perimeter labels /
// train waypoint labels) that the MFC dialog hides when a trigger is selected.
//
// The MFC WaypointOptions stays as the toggle-OFF fallback; this is the RTS_HAS_QT path. A
// top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_WAYPOINT_PANEL_H
#define WB_QT_WAYPOINT_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;

class WBQtWaypointPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtWaypointPanel(QWidget *owner);

	// Re-seed every control from the current MFC selection (WBQtWaypoint_PushRefresh).
	void pushRefresh();

	static WBQtWaypointPanel *instance() { return s_instance; }

private slots:
	void onNameChanged();
	void onLocationXChanged(double v);
	void onLocationYChanged(double v);
	void onLabel1Changed();
	void onLabel2Changed();
	void onLabel3Changed();
	void onBiDirectionalToggled();

private:
	void rebuildNamePresets(int kind);	// repopulate the combo drop-down for the current kind
	void applyLabel(int labelIndex, QLineEdit *edit);

	QGroupBox      *m_nameBox;
	QComboBox      *m_name;
	QGroupBox      *m_locationBox;
	QDoubleSpinBox *m_locX;
	QDoubleSpinBox *m_locY;
	QGroupBox      *m_labelsBox;
	QLineEdit      *m_label1;
	QLineEdit      *m_label2;
	QLineEdit      *m_label3;
	QCheckBox      *m_biDirectional;
	QGroupBox      *m_helpBox;	// the read-only AI / train helper text (waypoint only)

	bool m_updating;	// re-entrancy guard, mirrors MFC WaypointOptions::m_updating

	static WBQtWaypointPanel *s_instance;
};

#endif // WB_QT_WAYPOINT_PANEL_H
