// WBQtRoadPanel.h -- Qt replacement for the MFC RoadOptions dialog (the road-type browser).
//
// A QTreeWidget of every road + bridge type (grouped "Roads" / "Bridges" then leaf, mirroring
// the MFC tree), a search box + reset, three mutually-exclusive corner-type radio buttons
// (Broad curve / Tight curve / Angled -- the MFC IDC_BROAD_CURVE / IDC_TIGHT_CURVE / IDC_ANGLED),
// a "Join to different road type" checkbox, an "Apply Road" button (retype the selected road
// segments), a road-snap-distance field, and a selected-road-name label. Selecting a tree leaf
// records the current road type (via the bridge) so RoadTool places it and Apply retypes with it;
// the corner/join buttons drive the same command handlers the MFC panel does. It reacts to the
// map selection like the MFC updateSelection(): RoadTool / PointerTool re-run updateSelection when
// a road selection changes, which fires WBQtRoad_PushRefresh() to re-seed this panel's checkboxes
// + name. The MFC RoadOptions stays as the toggle-OFF fallback; this is the RTS_HAS_QT path. A
// top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_ROAD_PANEL_H
#define WB_QT_ROAD_PANEL_H

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtRoadPanel; }	// generated from WBQtRoadPanel.ui

class WBQtRoadPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtRoadPanel(QWidget *owner);
	virtual ~WBQtRoadPanel();

	// Re-seed corner/join checkboxes + selected-road name from the current MFC selection
	// (WBQtRoad_PushRefresh).
	void pushRefresh();

	static WBQtRoadPanel *instance() { return s_instance; }

private slots:
	void onTreeSelectionChanged();
	void onCornerTypeChanged();
	void onJoinToggled();
	void onApplyRoad();
	void onSnapDistanceChanged(double v);
	void onSearch();
	void onReset();

private:
	void rebuildTree(const QString &filter);	// filter empty => full list
	void refreshSelectionState();				// re-display corner/join/name from the tool
	QTreeWidgetItem *findOrAddChild(QTreeWidgetItem *parent, const QString &label);

	Ui::WBQtRoadPanel *m_ui;	// owns the static widget tree (WBQtRoadPanel.ui)

	QTreeWidget    *m_tree;
	QLabel         *m_nameLabel;
	QRadioButton   *m_broad;
	QRadioButton   *m_tight;
	QRadioButton   *m_angled;
	QButtonGroup   *m_cornerGroup;
	QCheckBox      *m_join;
	QDoubleSpinBox *m_snap;
	QLineEdit      *m_search;

	bool m_updating;	// re-entrancy guard, mirrors MFC RoadOptions::m_updating

	static WBQtRoadPanel *s_instance;
};

#endif // WB_QT_ROAD_PANEL_H
