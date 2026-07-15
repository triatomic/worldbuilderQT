// WBQtFencePanel.cpp -- see WBQtFencePanel.h.
#include "WBQtFencePanel.h"
#include "WBQtFenceBridge.h"
#include "WBQtPreviewImage.h"
#include "WBQtTreeStyle.h"

// NewSearch toggle (WBQtObjectBridge.cpp): live-filter search when on.
extern "C" int WBQtConfig_GetNewSearch(void);

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

WBQtFencePanel *WBQtFencePanel::s_instance = NULL;

// The filtered index a tree leaf represents is stored in this item-data role (>=0 for leaves,
// absent/-1 for grouping nodes). Mirrors the MFC tree's lParam.
static const int kListIndexRole = Qt::UserRole + 1;

WBQtFencePanel::WBQtFencePanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Fence Options");
	resize(320, 620);

	QVBoxLayout *root = new QVBoxLayout(this);

	// "Show all object types" -- the MFC "fence only" checkbox. Checked == show every template,
	// unchecked == only fence-width templates (the default).
	m_showAll = new QCheckBox("Show all object types", this);
	root->addWidget(m_showAll);

	// Search row. Only meaningful when showing all types (matches the MFC enable/disable);
	// searching within the fence-only list is left as-is (the MFC search box is disabled then).
	QHBoxLayout *searchRow = new QHBoxLayout();
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText("Search objects...");
	QPushButton *searchBtn = new QPushButton("Search", this);
	QPushButton *resetBtn = new QPushButton("Reset", this);
	searchRow->addWidget(m_search, 1);
	searchRow->addWidget(searchBtn);
	searchRow->addWidget(resetBtn);
	root->addLayout(searchRow);

	// The object tree.
	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	m_tree->setColumnCount(1);
	m_tree->setMinimumHeight(260);
	m_tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	WBQtTreeStyle::applyTreeLines(m_tree);
	root->addWidget(m_tree, 3);

	// Selected-object name + preview thumbnail.
	m_nameLabel = new QLabel("No Selection", this);
	root->addWidget(m_nameLabel);

	m_preview = new QLabel(this);
	m_preview->setFixedSize(128, 128);
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setFrameShape(QFrame::Box);
	root->addWidget(m_preview, 0, Qt::AlignHCenter);

	// Fence spacing (editable) + offset (read-only, derived from the template on selection).
	QGroupBox *fenceBox = new QGroupBox("Fence", this);
	QVBoxLayout *fenceLay = new QVBoxLayout(fenceBox);

	QHBoxLayout *spacingRow = new QHBoxLayout();
	spacingRow->addWidget(new QLabel("Spacing:", fenceBox));
	m_spacing = new QDoubleSpinBox(fenceBox);
	m_spacing->setDecimals(2);
	m_spacing->setRange(0.0, 1000000.0);
	spacingRow->addWidget(m_spacing, 1);
	fenceLay->addLayout(spacingRow);

	QHBoxLayout *offsetRow = new QHBoxLayout();
	offsetRow->addWidget(new QLabel("Offset:", fenceBox));
	m_offsetLabel = new QLabel("0", fenceBox);
	offsetRow->addWidget(m_offsetLabel, 1);
	fenceLay->addLayout(offsetRow);
	root->addWidget(fenceBox);

	// Seed everything under the guard so nothing echoes back while we populate.
	m_updating = true;
	m_showAll->setChecked(WBQtFence_GetShowAll() != 0);
	m_search->setEnabled(m_showAll->isChecked());
	rebuildTree(QString());
	refreshSpacingOffset();
	m_updating = false;

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_spacing, SIGNAL(valueChanged(double)), this, SLOT(onSpacingChanged(double)));
	connect(m_showAll, SIGNAL(clicked()), this, SLOT(onShowAllToggled()));
	connect(searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onSearch()));
	if (WBQtConfig_GetNewSearch() != 0)
	{
		// NewSearch: filter live as the user types (only enabled in show-all mode, like
		// the box itself; the Search button still works).
		connect(m_search, SIGNAL(textChanged(QString)), this, SLOT(onSearch()));
	}

	s_instance = this;
}

QTreeWidgetItem *WBQtFencePanel::findOrAddChild(QTreeWidgetItem *parent, const QString &label)
{
	// Grouping nodes are matched by their text; parent == NULL means a top-level node.
	if (parent == NULL)
	{
		for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
		{
			QTreeWidgetItem *child = m_tree->topLevelItem(i);
			if (child->data(0, kListIndexRole).toInt() < 0 && child->text(0) == label)
			{
				return child;
			}
		}
		QTreeWidgetItem *node = new QTreeWidgetItem(m_tree);
		node->setText(0, label);
		node->setData(0, kListIndexRole, -1);
		return node;
	}

	for (int i = 0; i < parent->childCount(); ++i)
	{
		QTreeWidgetItem *child = parent->child(i);
		if (child->data(0, kListIndexRole).toInt() < 0 && child->text(0) == label)
		{
			return child;
		}
	}
	QTreeWidgetItem *node = new QTreeWidgetItem(parent);
	node->setText(0, label);
	node->setData(0, kListIndexRole, -1);
	return node;
}

void WBQtFencePanel::rebuildTree(const QString &filter)
{
	m_tree->clear();

	const int cap = 256;
	char sideBuf[cap];
	char sortBuf[cap];
	char leafBuf[cap];
	char nameBuf[cap];

	// The count + entries reflect the current show-all flag; the bridge enumerates the same
	// filtered list the MFC tree does, so the index roles line up with the tool's lParam.
	int count = WBQtFence_GetCount();
	QString lowered = filter.toLower();
	bool expandAll = !lowered.isEmpty();

	for (int i = 0; i < count; ++i)
	{
		if (!WBQtFence_GetEntry(i, sideBuf, sortBuf, leafBuf, cap))
		{
			continue;
		}

		if (!lowered.isEmpty())
		{
			// Search matches against the full (unique) name, like the MFC OnSearch.
			if (!WBQtFence_GetFullName(i, nameBuf, cap))
			{
				continue;
			}
			QString full = QString::fromLatin1(nameBuf).toLower();
			if (!full.contains(lowered))
			{
				continue;
			}
		}

		QTreeWidgetItem *parent = findOrAddChild(NULL, QString::fromLatin1(sideBuf));
		QString sorting = QString::fromLatin1(sortBuf);
		if (!sorting.isEmpty())
		{
			parent = findOrAddChild(parent, sorting);
		}

		QTreeWidgetItem *leaf = new QTreeWidgetItem(parent);
		leaf->setText(0, QString::fromLatin1(leafBuf));
		leaf->setData(0, kListIndexRole, i);
	}

	m_tree->sortItems(0, Qt::AscendingOrder);
	if (expandAll)
	{
		m_tree->expandAll();
	}
}

void WBQtFencePanel::onTreeSelectionChanged()
{
	if (m_updating)
	{
		return;
	}
	QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
	if (sel.isEmpty())
	{
		return;
	}
	int listIndex = sel.first()->data(0, kListIndexRole).toInt();
	if (listIndex < 0)
	{
		// A grouping node -- clear the current selection like the MFC "item has children" branch.
		WBQtFence_SelectIndex(-1);
		m_nameLabel->setText("No Selection");
		return;
	}

	// Drive the MFC selection: sets m_currentObjectIndex, clears the custom-spacing latch, and
	// runs updateObjectOptions (ObjectOptions::selectObject + recompute spacing/offset + preview).
	WBQtFence_SelectIndex(listIndex);
	m_nameLabel->setText(sel.first()->text(0));

	m_updating = true;
	refreshSpacingOffset();
	m_updating = false;
	refreshPreview();
}

void WBQtFencePanel::refreshSpacingOffset()
{
	// Caller has set m_updating (writing the spinbox must not fire onSpacingChanged).
	m_spacing->setValue(WBQtFence_GetSpacing());
	m_offsetLabel->setText(QString::number(WBQtFence_GetOffset(), 'f', 2));
}

void WBQtFencePanel::refreshPreview()
{
	int w = 128, h = 128;
	WBQtFence_GetPreviewSize(&w, &h);
	QByteArray bgr(w * h * 3, 0);
	if (WBQtFence_RenderPreview(reinterpret_cast<unsigned char*>(bgr.data()), bgr.size()))
	{
		// Flip + convert + the MFC ObjectPreview center-quarter zoom (shared helper).
		QImage img = WBQtPreviewImage::fromBridgeBgr(
			reinterpret_cast<const unsigned char*>(bgr.constData()), w, h);
		m_preview->setPixmap(WBQtPreviewImage::toLabelPixmap(img, m_preview->size()));
	}
	else
	{
		m_preview->setText("(no preview)");
	}
}

void WBQtFencePanel::onSpacingChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	// Mirrors OnChangeFenceSpacingEdit: store the value and latch custom spacing so a later
	// selection won't overwrite it from the template width.
	WBQtFence_SetSpacing(v);
}

void WBQtFencePanel::onShowAllToggled()
{
	// Mirrors OnCheckFenceOnly: flip the flag, toggle the search box's usability, rebuild.
	WBQtFence_SetShowAll(m_showAll->isChecked() ? 1 : 0);
	m_updating = true;
	m_search->setEnabled(m_showAll->isChecked());
	if (!m_showAll->isChecked())
	{
		m_search->clear();
	}
	rebuildTree(QString());
	m_nameLabel->setText("No Selection");
	m_updating = false;
}

void WBQtFencePanel::onSearch()
{
	QString text = m_search->text().trimmed();
	m_updating = true;
	rebuildTree(text);
	m_updating = false;
}

void WBQtFencePanel::onReset()
{
	m_search->clear();
	m_updating = true;
	rebuildTree(QString());
	m_updating = false;
}

void WBQtFencePanel::pushRefresh()
{
	// FenceTool changed / re-selected; re-seed the spacing field + preview from the current
	// object without re-driving the tool.
	m_updating = true;
	refreshSpacingOffset();
	m_updating = false;
	refreshPreview();
}

// --- Forward push functions (MFC selection -> widget), the Qt side of WBQtFenceBridge.h ------
extern "C" void WBQtFence_PushRefresh(void)
{
	if (WBQtFencePanel::instance() != NULL)
	{
		WBQtFencePanel::instance()->pushRefresh();
	}
}
