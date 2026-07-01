// WBQtBuildListPanel.h -- Qt replacement for the MFC BuildList option panel.
//
// A front-end over the still-created-but-hidden MFC BuildList: the MFC side stays the
// singleton (m_staticThis) so the Build List tool's map-click callbacks (addBuilding /
// setSelectedBuildList) and BuildList::update() (tool activate + undo/redo) keep driving the
// model in TheSidesList. This widget reads side/build-list/attribute state + the live Power
// Used meter through the seam, drives the commands, and re-reads on WBQtBuildList_PushRefresh.
// The MFC BuildList stays as the toggle-OFF fallback; this is the RTS_HAS_QT path.
#ifndef WB_QT_BUILDLIST_PANEL_H
#define WB_QT_BUILDLIST_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QProgressBar;
class QPushButton;

class WBQtBuildListPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtBuildListPanel(QWidget *owner);

	void refresh();		// re-read everything from the seam (also the WBQtBuildList_PushRefresh target)

	static WBQtBuildListPanel *instance() { return s_instance; }

private slots:
	void onSideChanged(int index);
	void onBuildSelChanged();
	void onBuildDoubleClicked();
	void onMoveUp();
	void onMoveDown();
	void onAdd();
	void onDelete();
	void onExport();
	void onImport();
	void onAngleChanged(double v);
	void onZChanged(double v);
	void onAlreadyBuiltToggled();
	void onRebuildsChanged();
	void onForcedShowToggled();

private:
	void refreshAttributes();		// the per-building attribute controls + power + button enables

	QComboBox      *m_side;
	QListWidget    *m_buildList;
	QPushButton    *m_up;
	QPushButton    *m_down;
	QPushButton    *m_add;
	QPushButton    *m_delete;
	QPushButton    *m_export;
	QPushButton    *m_import;
	QDoubleSpinBox *m_angle;
	QDoubleSpinBox *m_z;
	QCheckBox      *m_alreadyBuilt;
	QComboBox      *m_rebuilds;
	QProgressBar   *m_power;
	QCheckBox      *m_forcedShow;

	bool m_updating;	// re-entrancy guard while seeding controls

	static WBQtBuildListPanel *s_instance;
};

#endif // WB_QT_BUILDLIST_PANEL_H
