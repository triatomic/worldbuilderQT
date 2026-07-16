// WBQtLayersPanel.h -- Qt replacement for the MFC LayersList dialog.
//
// The modeless Layers window (View > Layers List): a tree of layers (bold = active layer, dimmed
// = hidden) with their objects/triggers as children, New/Delete/Hide-Show/Set Active buttons,
// inline layer rename, and right-click menus (select on map, merge layer into, move object to
// layer, move the view selection here). The hidden MFC LayersList (TheLayersList) stays the model
// owner; its updateUIFromList() refresh funnel re-seeds this window via WBQtLayers_PushRefresh().
#ifndef WB_QT_LAYERS_PANEL_H
#define WB_QT_LAYERS_PANEL_H

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtLayersPanel; }	// generated from WBQtLayersPanel.ui

class WBQtLayersPanel : public QWidget
{
	Q_OBJECT

public:
	explicit WBQtLayersPanel(QWidget *owner);
	virtual ~WBQtLayersPanel();

	// Rebuild the tree from the hidden MFC model (WBQtLayers_PushRefresh).
	void pushRefresh();

	static WBQtLayersPanel *instance() { return s_instance; }

private slots:
	void onNewLayer();
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onItemChanged(QTreeWidgetItem *item, int column);
	void onContextMenu(const QPoint &pos);

private:
	QString selectedLayerName() const;	// the selected item's layer (itself or its parent)
	bool    isLayerItem(QTreeWidgetItem *item) const;

	Ui::WBQtLayersPanel *m_ui;	// owns the static widget tree (WBQtLayersPanel.ui)

	QTreeWidget *m_tree;

	bool m_updating;	// guard so programmatic tree changes don't re-fire slots

	static WBQtLayersPanel *s_instance;
};

#endif // WB_QT_LAYERS_PANEL_H
