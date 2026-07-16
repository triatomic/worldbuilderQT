// WBQtFencePanel.h -- Qt replacement for the MFC FenceOptions dialog (the fence object browser).
//
// A front for the object browser, restricted to fence objects: a QTreeWidget of the fence
// templates (grouped side / editor-sorting / leaf, mirroring the MFC tree), a rendered preview
// thumbnail, a "Show all object types" toggle (the MFC "fence only" checkbox inverted in
// label), an editable fence-spacing field, a read-only fence-offset display, and a search box.
// It edits none of its own state -- selecting a tree leaf drives the MFC FenceOptions selection
// (via the bridge), which itself calls ObjectOptions::selectObject so FenceTool keeps placing.
// The MFC FenceOptions stays as the toggle-OFF fallback and owns the template list; this is the
// RTS_HAS_QT path. A top-level Qt::Tool window owned by the shared QWinWidget bridge.
#ifndef WB_QT_FENCE_PANEL_H
#define WB_QT_FENCE_PANEL_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtFencePanel; }	// generated from WBQtFencePanel.ui

class WBQtFencePanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtFencePanel(QWidget *owner);
	virtual ~WBQtFencePanel();

	// Re-seed spacing/offset/preview from the current MFC selection (WBQtFence_PushRefresh).
	void pushRefresh();

	static WBQtFencePanel *instance() { return s_instance; }

private slots:
	void onTreeSelectionChanged();
	void onSpacingChanged(double v);
	void onShowAllToggled();
	void onSearch();
	void onReset();

private:
	void rebuildTree(const QString &filter);	// filter empty => full (filtered) list
	void refreshPreview();
	void refreshSpacingOffset();				// re-display spacing + offset from the tool
	QTreeWidgetItem *findOrAddChild(QTreeWidgetItem *parent, const QString &label);

	Ui::WBQtFencePanel *m_ui;	// owns the static widget tree (WBQtFencePanel.ui)

	QTreeWidget    *m_tree;
	QLabel         *m_preview;
	QLabel         *m_nameLabel;
	QCheckBox      *m_showAll;
	QDoubleSpinBox *m_spacing;
	QLabel         *m_offsetLabel;
	QLineEdit      *m_search;

	bool m_updating;	// re-entrancy guard while seeding controls

	static WBQtFencePanel *s_instance;
};

#endif // WB_QT_FENCE_PANEL_H
