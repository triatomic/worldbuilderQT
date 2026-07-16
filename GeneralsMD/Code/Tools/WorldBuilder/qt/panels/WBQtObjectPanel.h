// WBQtObjectPanel.h -- Qt replacement for the MFC ObjectOptions dialog (the object browser).
//
// The big panel: a QTreeWidget of every object template (grouped side / editor-sorting /
// leaf, mirroring the MFC tree), a rendered preview thumbnail, an owning-team combo, a
// placement-height field, a search box, and the three preview toggles. It edits none of its
// own state -- selecting a tree leaf drives the MFC ObjectOptions selection statics (via the
// bridge) that ObjectTool / FenceTool / etc. read, so placement keeps working. The MFC
// ObjectOptions stays as the toggle-OFF fallback and owns the template list; this is the
// RTS_HAS_QT path. A top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_OBJECT_PANEL_H
#define WB_QT_OBJECT_PANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtObjectPanel; }	// generated from WBQtObjectPanel.ui

class WBQtObjectPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtObjectPanel(QWidget *owner);
	virtual ~WBQtObjectPanel();

	// Re-seed label/team/preview from the current MFC selection (WBQtObject_PushFromSelection).
	void pushFromSelection();
	// Move the tree selection to match a programmatic selectObject (WBQtObject_PushSelectIndex).
	void selectListIndex(int listIndex);

	static WBQtObjectPanel *instance() { return s_instance; }

private slots:
	void onTreeSelectionChanged();
	void onTeamChanged(int index);
	void onHeightChanged(int v);
	void onSearch();
	void onReset();
	void onPreviewSoundToggled();
	void onPreviewBuildZoneToggled();
	void onUseWaterHeightToggled();
	void onPlaceAllToggled();
	void onPlaceAllYSpacingChanged(int v);

private:
	void rebuildTree(const QString &filter);	// filter empty => full list
	void refreshTeamCombo();
	void refreshPreview();
	QTreeWidgetItem *findOrAddChild(QTreeWidgetItem *parent, const QString &label);

	Ui::WBQtObjectPanel *m_ui;	// owns the static widget tree (WBQtObjectPanel.ui)

	QTreeWidget *m_tree;
	QLabel      *m_preview;
	QLabel      *m_nameLabel;
	QComboBox   *m_team;
	QSpinBox    *m_height;
	QLineEdit   *m_search;
	QCheckBox   *m_previewSound;
	QCheckBox   *m_previewBuildZone;
	QCheckBox   *m_useWaterHeight;
	QCheckBox   *m_placeAll;
	QSpinBox    *m_placeAllYSpacing;

	bool m_updating;	// re-entrancy guard while seeding controls

	static WBQtObjectPanel *s_instance;
};

#endif // WB_QT_OBJECT_PANEL_H
