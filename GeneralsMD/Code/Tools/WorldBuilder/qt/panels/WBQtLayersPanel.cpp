// WBQtLayersPanel.cpp -- see WBQtLayersPanel.h.
#include "WBQtLayersPanel.h"
#include "ui_WBQtLayersPanel.h"
#include "WBQtLayersBridge.h"
#include "WBQtTreeStyle.h"
#include "WBQtWindowPos.h"
#include "qwinwidget.h"

#include <QFont>
#include <QMenu>
#include <QTreeWidget>

#include <qt_windows.h>

WBQtLayersPanel *WBQtLayersPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)

	// The pre-rename layer name is stashed in this item-data role so onItemChanged can hand
	// old + new to the rename call.
	const int kLayerNameRole = Qt::UserRole + 1;
}

WBQtLayersPanel::WBQtLayersPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtLayersPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtLayersPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "LayersList");

	m_tree = m_ui->tree;
	WBQtTreeStyle::applyTreeLines(m_tree);

	pushRefresh();

	connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		this, SLOT(onItemDoubleClicked(QTreeWidgetItem *, int)));
	connect(m_tree, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
		this, SLOT(onItemChanged(QTreeWidgetItem *, int)));
	connect(m_tree, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(onContextMenu(const QPoint &)));

	s_instance = this;
}

WBQtLayersPanel::~WBQtLayersPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

bool WBQtLayersPanel::isLayerItem(QTreeWidgetItem *item) const
{
	return item != NULL && item->parent() == NULL;
}

QString WBQtLayersPanel::selectedLayerName() const
{
	QTreeWidgetItem *item = m_tree->currentItem();
	if (item == NULL)
	{
		return QString();
	}
	if (item->parent() != NULL)
	{
		item = item->parent();
	}
	return item->data(0, kLayerNameRole).toString();
}

void WBQtLayersPanel::pushRefresh()
{
	m_updating = true;

	// Remember which layers were expanded (keyed by name) so a rebuild doesn't collapse them.
	QStringList expanded;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem *layer = m_tree->topLevelItem(i);
		if (layer->isExpanded())
		{
			expanded.append(layer->data(0, kLayerNameRole).toString());
		}
	}

	m_tree->clear();

	const int cap = 256;
	char buf[cap];
	QFont activeFont = m_tree->font();
	activeFont.setBold(true);

	int layerCount = WBQtLayers_GetLayerCount();
	for (int i = 0; i < layerCount; ++i)
	{
		if (!WBQtLayers_GetLayerName(i, buf, cap))
		{
			continue;
		}
		QString name = QString::fromLatin1(buf);
		int state = WBQtLayers_GetLayerState(i);

		QTreeWidgetItem *layerItem = new QTreeWidgetItem(m_tree);
		layerItem->setData(0, kLayerNameRole, name);
		layerItem->setFlags(layerItem->flags() | Qt::ItemIsEditable);
		if (state == 2)
		{
			// The active layer: bold + suffix (new objects land here).
			layerItem->setText(0, name + "  (active)");
			layerItem->setFont(0, activeFont);
		}
		else if (state == 1)
		{
			layerItem->setText(0, name + "  (hidden)");
			layerItem->setForeground(0, m_tree->palette().brush(QPalette::Disabled, QPalette::Text));
		}
		else
		{
			layerItem->setText(0, name);
		}

		int itemCount = WBQtLayers_GetItemCount(i);
		for (int j = 0; j < itemCount; ++j)
		{
			if (WBQtLayers_GetItemLabel(i, j, buf, cap))
			{
				QTreeWidgetItem *child = new QTreeWidgetItem(layerItem);
				child->setText(0, QString::fromLatin1(buf));
				if (state == 1)
				{
					child->setForeground(0, m_tree->palette().brush(QPalette::Disabled, QPalette::Text));
				}
			}
		}

		if (expanded.contains(name))
		{
			layerItem->setExpanded(true);
		}
	}

	m_updating = false;
}

void WBQtLayersPanel::onNewLayer()
{
	const int cap = 256;
	char buf[cap];
	if (!WBQtLayers_NewLayer(buf, cap))
	{
		return;
	}
	// The model push has already rebuilt the tree; find the new layer and start the rename.
	QString name = QString::fromLatin1(buf);
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem *layer = m_tree->topLevelItem(i);
		if (layer->data(0, kLayerNameRole).toString() == name)
		{
			m_tree->setCurrentItem(layer);
			// Show the bare name while editing (no state suffix).
			m_updating = true;
			layer->setText(0, name);
			m_updating = false;
			m_tree->editItem(layer, 0);
			break;
		}
	}
}

void WBQtLayersPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
	if (item == NULL)
	{
		return;
	}
	// Double-click selects on the map: a layer selects everything in it; a child selects the
	// object/trigger (mirrors the MFC select-layer-object command).
	QString label = isLayerItem(item)
		? item->data(0, kLayerNameRole).toString() : item->text(0);
	WBQtLayers_SelectItem(label.toLatin1().constData());
}

void WBQtLayersPanel::onItemChanged(QTreeWidgetItem *item, int column)
{
	if (m_updating || item == NULL || !isLayerItem(item))
	{
		return;
	}
	// The inline rename editor closed: hand old + new to the model. Strip any state suffix the
	// user may have left in place. A refused rename (trigger layer) just re-seeds.
	QString oldName = item->data(0, kLayerNameRole).toString();
	QString newName = item->text(0);
	newName.remove("  (active)");
	newName.remove("  (hidden)");
	newName = newName.trimmed();
	if (newName.isEmpty() || newName == oldName)
	{
		pushRefresh();
		return;
	}
	WBQtLayers_RenameLayer(oldName.toLatin1().constData(), newName.toLatin1().constData());
	// The rename path calls updateUIFromList -> push, so the tree is already rebuilt on success;
	// re-seed anyway to restore the decorated text on failure.
	pushRefresh();
}

void WBQtLayersPanel::onContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_tree->itemAt(pos);

	const int cap = 256;
	char buf[cap];
	int layerCount = WBQtLayers_GetLayerCount();

	QMenu menu(this);

	if (item == NULL)
	{
		// Right-click on empty space: just Insert New Layer, like the MFC tree menu.
		QAction *newAct = menu.addAction("Insert New Layer");
		if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == newAct)
		{
			onNewLayer();
		}
		return;
	}
	m_tree->setCurrentItem(item);

	if (isLayerItem(item))
	{
		QString layerName = item->data(0, kLayerNameRole).toString();
		QAction *newAct = menu.addAction("Insert New Layer");
		menu.addSeparator();
		QAction *selectAct = menu.addAction("Select On Map");
		QAction *activeAct = menu.addAction("Set/Unset Active");
		QAction *hideAct = menu.addAction("Hide/Show");
		QAction *renameAct = menu.addAction("Rename");
		QMenu *mergeMenu = menu.addMenu("Merge Into");
		for (int i = 0; i < layerCount; ++i)
		{
			if (WBQtLayers_GetLayerName(i, buf, cap))
			{
				QString other = QString::fromLatin1(buf);
				if (other != layerName)
				{
					QAction *a = mergeMenu->addAction(other);
					a->setData(other);
				}
			}
		}
		QAction *moveSelAct = menu.addAction("Move View Selection Here");
		menu.addSeparator();
		QAction *deleteAct = menu.addAction("Delete");

		QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
		if (chosen == NULL)
		{
			return;
		}
		if (chosen == newAct)
		{
			onNewLayer();
		}
		else if (chosen == selectAct)
		{
			WBQtLayers_SelectItem(layerName.toLatin1().constData());
		}
		else if (chosen == activeAct)
		{
			WBQtLayers_ToggleActiveLayer(layerName.toLatin1().constData());
			pushRefresh();
		}
		else if (chosen == hideAct)
		{
			WBQtLayers_ToggleHideLayer(layerName.toLatin1().constData());
			pushRefresh();
		}
		else if (chosen == renameAct)
		{
			m_updating = true;
			item->setText(0, layerName);
			m_updating = false;
			m_tree->editItem(item, 0);
		}
		else if (chosen == moveSelAct)
		{
			WBQtLayers_MoveViewSelectionToLayer(layerName.toLatin1().constData());
		}
		else if (chosen == deleteAct)
		{
			WBQtLayers_DeleteLayer(layerName.toLatin1().constData());
		}
		else if (!chosen->data().isNull())
		{
			// Only the Merge Into sub-actions carry data (the target layer name).
			WBQtLayers_MergeLayerInto(layerName.toLatin1().constData(),
				chosen->data().toString().toLatin1().constData());
		}
	}
	else
	{
		QString label = item->text(0);
		QAction *selectAct = menu.addAction("Select On Map");
		QMenu *moveMenu = menu.addMenu("Move To Layer");
		for (int i = 0; i < layerCount; ++i)
		{
			if (WBQtLayers_GetLayerName(i, buf, cap))
			{
				QAction *a = moveMenu->addAction(QString::fromLatin1(buf));
				a->setData(QString::fromLatin1(buf));
			}
		}

		QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
		if (chosen == NULL)
		{
			return;
		}
		if (chosen == selectAct)
		{
			WBQtLayers_SelectItem(label.toLatin1().constData());
		}
		else if (!chosen->data().isNull())
		{
			WBQtLayers_MoveObjectToLayer(label.toLatin1().constData(),
				chosen->data().toString().toLatin1().constData());
		}
	}
}

// --- Open / close / push / focus hooks (the Qt side of WBQtLayersBridge.h) --------------------

extern "C" void WBQtLayers_PushRefresh(void)
{
	if (WBQtLayersPanel::instance() != NULL && WBQtLayersPanel::instance()->isVisible())
	{
		WBQtLayersPanel::instance()->pushRefresh();
	}
}

extern "C" void WBQtLayers_Open(void *frameHwnd)
{
	if (frameHwnd == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtLayersPanel *panel = WBQtLayersPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtLayersPanel(s_owner);
	}
	// Show WITHOUT activating (== the MFC SW_SHOW of a tool window) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
	panel->pushRefresh();
}

extern "C" void WBQtLayers_Close(void)
{
	if (WBQtLayersPanel::instance() != NULL)
	{
		WBQtLayersPanel::instance()->hide();
	}
}
