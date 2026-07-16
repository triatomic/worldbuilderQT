// WBQtRoadPanel.cpp -- see WBQtRoadPanel.h.
#include "WBQtRoadPanel.h"
#include "ui_WBQtRoadPanel.h"
#include "WBQtRoadBridge.h"
#include "WBQtTreeStyle.h"

// NewSearch toggle (WBQtObjectBridge.cpp): live-filter search when on.
extern "C" int WBQtConfig_GetNewSearch(void);

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QTreeWidget>

WBQtRoadPanel *WBQtRoadPanel::s_instance = NULL;

// The road/bridge index a tree leaf represents is stored in this item-data role (>=0 for
// leaves, -1 for the "Roads"/"Bridges" grouping nodes). Mirrors the MFC tree's lParam.
static const int kListIndexRole = Qt::UserRole + 1;

WBQtRoadPanel::WBQtRoadPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtRoadPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtRoadPanel.ui; bind the members the logic below
	// uses, then wire what Designer can't express. Layout notes preserved in the .ui:
	// the road/bridge tree is the primary control -- it gets a large minimum + all the
	// slack (Expanding, stretch 3, no trailing stretch) so the boxes below don't squeeze
	// it into a few rows and there's no wasted space below the controls.
	m_ui->setupUi(this);

	m_search = m_ui->search;
	m_tree = m_ui->tree;
	// Current road type (the selected road name), matching the MFC "Current road type:" box.
	m_nameLabel = m_ui->nameLabel;
	// Corner-type radios (Broad / Tight / Angled -- mutually exclusive like the MFC panel).
	m_broad = m_ui->broad;
	m_tight = m_ui->tight;
	m_angled = m_ui->angled;
	// Join checkbox (matches the MFC IDC_JOIN label).
	m_join = m_ui->join;
	// Tool Option: road snap distance.
	m_snap = m_ui->snap;

	WBQtTreeStyle::applyTreeLines(m_tree);

	m_cornerGroup = new QButtonGroup(this);
	m_cornerGroup->addButton(m_broad, WBQT_ROAD_CORNER_BROAD);
	m_cornerGroup->addButton(m_tight, WBQT_ROAD_CORNER_TIGHT);
	m_cornerGroup->addButton(m_angled, WBQT_ROAD_CORNER_ANGLED);
	m_cornerGroup->setExclusive(true);

	// Seed everything under the guard so nothing echoes back while we populate.
	m_updating = true;
	rebuildTree(QString());
	m_snap->setValue(WBQtRoad_GetSnapDistance());
	refreshSelectionState();
	m_updating = false;

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	// clicked() fires only for the button the user actually clicks (once, when it becomes the
	// checked one), so programmatic seeding + the paired unchecked-button toggle don't re-apply.
	connect(m_broad, SIGNAL(clicked()), this, SLOT(onCornerTypeChanged()));
	connect(m_tight, SIGNAL(clicked()), this, SLOT(onCornerTypeChanged()));
	connect(m_angled, SIGNAL(clicked()), this, SLOT(onCornerTypeChanged()));
	connect(m_join, SIGNAL(clicked()), this, SLOT(onJoinToggled()));
	// Road Replacer: applies the current road type to the selected road points, flood-fill
	// style. IDC_APPLY_ROAD in the MFC dialog.
	connect(m_ui->applyBtn, SIGNAL(clicked()), this, SLOT(onApplyRoad()));
	connect(m_snap, SIGNAL(valueChanged(double)), this, SLOT(onSnapDistanceChanged(double)));
	connect(m_ui->searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onSearch()));
	if (WBQtConfig_GetNewSearch() != 0)
	{
		// NewSearch: filter live as the user types (the Search button still works).
		connect(m_search, SIGNAL(textChanged(QString)), this, SLOT(onSearch()));
	}

	s_instance = this;
}

WBQtRoadPanel::~WBQtRoadPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

QTreeWidgetItem *WBQtRoadPanel::findOrAddChild(QTreeWidgetItem *parent, const QString &label)
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

void WBQtRoadPanel::rebuildTree(const QString &filter)
{
	m_tree->clear();

	const int cap = 256;
	char groupBuf[cap];
	char leafBuf[cap];
	char nameBuf[cap];

	int count = WBQtRoad_GetCount();
	QString lowered = filter.toLower();
	bool expandAll = !lowered.isEmpty();

	for (int i = 0; i < count; ++i)
	{
		if (!WBQtRoad_GetEntry(i, groupBuf, leafBuf, cap))
		{
			continue;
		}

		if (!lowered.isEmpty())
		{
			// Search matches against the full (unique) name, like the MFC OnSearch.
			if (!WBQtRoad_GetFullName(i, nameBuf, cap))
			{
				continue;
			}
			QString full = QString::fromLatin1(nameBuf).toLower();
			if (!full.contains(lowered))
			{
				continue;
			}
		}

		QTreeWidgetItem *parent = findOrAddChild(NULL, QString::fromLatin1(groupBuf));
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

void WBQtRoadPanel::onTreeSelectionChanged()
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
		return;	// a "Roads"/"Bridges" grouping node, not a road type
	}

	// Record the current road type (drives RoadTool placement + Apply retype).
	WBQtRoad_SelectIndex(listIndex);
	m_nameLabel->setText(sel.first()->text(0));
}

void WBQtRoadPanel::refreshSelectionState()
{
	// Caller has set m_updating (setting the radios/checkbox must not fire their slots).
	const int cap = 256;
	char nameBuf[cap];
	int cornerType = WBQT_ROAD_CORNER_BROAD;
	int join = 0;
	int mixed = 0;
	WBQtRoad_GetSelectionState(&cornerType, &join, &mixed, nameBuf, cap);

	if (mixed)
	{
		// Mixed selection: the MFC path clears all corner checks. A QButtonGroup is exclusive, so
		// drop the exclusivity briefly to allow "none checked", then restore it.
		m_cornerGroup->setExclusive(false);
		m_broad->setChecked(false);
		m_tight->setChecked(false);
		m_angled->setChecked(false);
		m_cornerGroup->setExclusive(true);
	}
	else
	{
		m_broad->setChecked(cornerType == WBQT_ROAD_CORNER_BROAD);
		m_tight->setChecked(cornerType == WBQT_ROAD_CORNER_TIGHT);
		m_angled->setChecked(cornerType == WBQT_ROAD_CORNER_ANGLED);
	}
	m_join->setChecked(join != 0);

	QString name = QString::fromLatin1(nameBuf);
	if (!name.isEmpty())
	{
		m_nameLabel->setText(name);
	}
}

void WBQtRoadPanel::onCornerTypeChanged()
{
	if (m_updating)
	{
		return;
	}
	// Read whichever radio is now checked and apply that corner type to the selection (mirrors
	// OnBroadCurve / OnTightCurve / OnAngled).
	int cornerType;
	if (m_angled->isChecked())
	{
		cornerType = WBQT_ROAD_CORNER_ANGLED;
	}
	else if (m_tight->isChecked())
	{
		cornerType = WBQT_ROAD_CORNER_TIGHT;
	}
	else if (m_broad->isChecked())
	{
		cornerType = WBQT_ROAD_CORNER_BROAD;
	}
	else
	{
		return;	// nothing checked (transient during a programmatic clear)
	}
	WBQtRoad_SetCornerType(cornerType);
}

void WBQtRoadPanel::onJoinToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtRoad_SetJoin(m_join->isChecked() ? 1 : 0);
}

void WBQtRoadPanel::onApplyRoad()
{
	WBQtRoad_ApplyRoadType();
}

void WBQtRoadPanel::onSnapDistanceChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtRoad_SetSnapDistance(v);
}

void WBQtRoadPanel::onSearch()
{
	QString text = m_search->text().trimmed();
	m_updating = true;
	rebuildTree(text);
	m_updating = false;
}

void WBQtRoadPanel::onReset()
{
	m_search->clear();
	m_updating = true;
	rebuildTree(QString());
	m_updating = false;
}

void WBQtRoadPanel::pushRefresh()
{
	// The map selection changed; re-seed the corner/join checkboxes + selected-road name from the
	// current selection without re-driving any command handler.
	m_updating = true;
	refreshSelectionState();
	m_updating = false;
}

// --- Forward push functions (MFC selection -> widget), the Qt side of WBQtRoadBridge.h -------
extern "C" void WBQtRoad_PushRefresh(void)
{
	if (WBQtRoadPanel::instance() != NULL)
	{
		WBQtRoadPanel::instance()->pushRefresh();
	}
}
