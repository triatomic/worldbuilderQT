// WBQtObjectPanel.cpp -- see WBQtObjectPanel.h.
#include "WBQtObjectPanel.h"
#include "WBQtPanelBridge.h"
#include "WBQtPreviewImage.h"
#include "WBQtTreeStyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

WBQtObjectPanel *WBQtObjectPanel::s_instance = NULL;

// The list index a tree leaf represents is stored in this item-data role (>=0 for leaves,
// absent/-1 for grouping nodes).
static const int kListIndexRole = Qt::UserRole + 1;

WBQtObjectPanel::WBQtObjectPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Object Options");
	resize(320, 620);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Search row.
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

	// Owning team + placement height.
	QGroupBox *placeBox = new QGroupBox("Placement", this);
	QVBoxLayout *placeLay = new QVBoxLayout(placeBox);

	QHBoxLayout *teamRow = new QHBoxLayout();
	teamRow->addWidget(new QLabel("Owning team:", placeBox));
	m_team = new QComboBox(placeBox);
	teamRow->addWidget(m_team, 1);
	placeLay->addLayout(teamRow);

	QHBoxLayout *heightRow = new QHBoxLayout();
	heightRow->addWidget(new QLabel("Height:", placeBox));
	m_height = new QSpinBox(placeBox);
	m_height->setRange(-1000000, 1000000);
	m_height->setValue(WBQtObject_GetHeight());
	heightRow->addWidget(m_height, 1);
	placeLay->addLayout(heightRow);

	m_placeAll = new QCheckBox("Place all objects in category", placeBox);
	m_placeAll->setToolTip("One click places every object from the selected object's\n"
		"tree category (e.g. GLA > VEHICLE) in a grid. A single Undo\n"
		"removes the whole batch.");
	placeLay->addWidget(m_placeAll);
	root->addWidget(placeBox);

	// Preview toggles.
	QGroupBox *optBox = new QGroupBox("Preview", this);
	QVBoxLayout *optLay = new QVBoxLayout(optBox);
	m_previewSound = new QCheckBox("Preview ambient sound", optBox);
	m_previewBuildZone = new QCheckBox("Preview build zone", optBox);
	m_useWaterHeight = new QCheckBox("Use water height", optBox);
	optLay->addWidget(m_previewSound);
	optLay->addWidget(m_previewBuildZone);
	optLay->addWidget(m_useWaterHeight);
	root->addWidget(optBox);

	// Seed everything under the guard so nothing echoes back while we populate.
	m_updating = true;
	rebuildTree(QString());
	m_previewSound->setChecked(WBQtObject_GetPreviewSound() != 0);
	m_previewBuildZone->setChecked(WBQtObject_GetPreviewBuildZone() != 0);
	m_useWaterHeight->setChecked(WBQtObject_GetUseWaterHeight() != 0);
	m_placeAll->setChecked(WBQtObject_GetPlaceAll() != 0);
	refreshTeamCombo();
	m_updating = false;

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_team, SIGNAL(currentIndexChanged(int)), this, SLOT(onTeamChanged(int)));
	connect(m_height, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onSearch()));
	connect(m_previewSound, SIGNAL(clicked()), this, SLOT(onPreviewSoundToggled()));
	connect(m_previewBuildZone, SIGNAL(clicked()), this, SLOT(onPreviewBuildZoneToggled()));
	connect(m_useWaterHeight, SIGNAL(clicked()), this, SLOT(onUseWaterHeightToggled()));
	connect(m_placeAll, SIGNAL(clicked()), this, SLOT(onPlaceAllToggled()));

	s_instance = this;
}

QTreeWidgetItem *WBQtObjectPanel::findOrAddChild(QTreeWidgetItem *parent, const QString &label)
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

void WBQtObjectPanel::rebuildTree(const QString &filter)
{
	m_tree->clear();

	const int cap = 256;
	char sideBuf[cap];
	char sortBuf[cap];
	char leafBuf[cap];
	char nameBuf[cap];

	int count = WBQtObject_GetCount();
	QString lowered = filter.toLower();
	bool expandAll = !lowered.isEmpty();

	for (int i = 0; i < count; ++i)
	{
		if (!WBQtObject_GetEntry(i, sideBuf, sortBuf, leafBuf, cap))
		{
			continue;
		}

		if (!lowered.isEmpty())
		{
			// Search matches against the full (unique) name, like the MFC OnSearch.
			if (!WBQtObject_GetFullName(i, nameBuf, cap))
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

void WBQtObjectPanel::onTreeSelectionChanged()
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
		return;	// a grouping node, not an object
	}

	WBQtObject_SelectIndex(listIndex);
	m_nameLabel->setText(sel.first()->text(0));

	m_updating = true;
	refreshTeamCombo();
	m_updating = false;
	refreshPreview();
}

void WBQtObjectPanel::refreshTeamCombo()
{
	// Caller has set m_updating (combo repopulation must not fire onTeamChanged).
	m_team->clear();
	const int cap = 256;
	char nameBuf[cap];
	int teams = WBQtObject_GetTeamCount();
	for (int i = 0; i < teams; ++i)
	{
		if (WBQtObject_GetTeamName(i, nameBuf, cap))
		{
			m_team->addItem(QString::fromLatin1(nameBuf));
		}
		else
		{
			m_team->addItem(QString());
		}
	}
	int def = WBQtObject_GetDefaultTeamForCurrent();
	if (def >= 0 && def < m_team->count())
	{
		m_team->setCurrentIndex(def);
		WBQtObject_SetTeam(def);
	}
}

void WBQtObjectPanel::refreshPreview()
{
	int w = 128, h = 128;
	WBQtObject_GetPreviewSize(&w, &h);
	QByteArray bgr(w * h * 3, 0);
	if (WBQtObject_RenderPreview(reinterpret_cast<unsigned char*>(bgr.data()), bgr.size()))
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

void WBQtObjectPanel::onTeamChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	WBQtObject_SetTeam(index);
}

void WBQtObjectPanel::onHeightChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	WBQtObject_SetHeight(v);
}

void WBQtObjectPanel::onSearch()
{
	QString text = m_search->text().trimmed();
	m_updating = true;
	rebuildTree(text);
	m_updating = false;
}

void WBQtObjectPanel::onReset()
{
	m_search->clear();
	m_updating = true;
	rebuildTree(QString());
	m_updating = false;
}

void WBQtObjectPanel::onPreviewSoundToggled()
{
	WBQtObject_SetPreviewSound(m_previewSound->isChecked() ? 1 : 0);
}

void WBQtObjectPanel::onPreviewBuildZoneToggled()
{
	WBQtObject_SetPreviewBuildZone(m_previewBuildZone->isChecked() ? 1 : 0);
}

void WBQtObjectPanel::onUseWaterHeightToggled()
{
	WBQtObject_SetUseWaterHeight(m_useWaterHeight->isChecked() ? 1 : 0);
}

void WBQtObjectPanel::onPlaceAllToggled()
{
	WBQtObject_SetPlaceAll(m_placeAll->isChecked() ? 1 : 0);
}

void WBQtObjectPanel::pushFromSelection()
{
	// WB changed the selection; re-seed the label/team/preview from the current object.
	m_updating = true;
	refreshTeamCombo();
	m_height->setValue(WBQtObject_GetHeight());
	m_updating = false;
	refreshPreview();
}

void WBQtObjectPanel::selectListIndex(int listIndex)
{
	if (listIndex < 0)
	{
		return;
	}
	// Find the leaf whose stored index matches and select it (without re-driving the tool).
	m_updating = true;
	for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
	{
		if ((*it)->data(0, kListIndexRole).toInt() == listIndex)
		{
			m_tree->setCurrentItem(*it);
			m_nameLabel->setText((*it)->text(0));
			break;
		}
	}
	refreshTeamCombo();
	m_updating = false;
	refreshPreview();
}

// --- Forward push functions (MFC selection -> widget), the Qt-side of WBQtPanelBridge.h --
extern "C" void WBQtObject_PushFromSelection(void)
{
	if (WBQtObjectPanel::instance() != NULL)
	{
		WBQtObjectPanel::instance()->pushFromSelection();
	}
}

extern "C" void WBQtObject_PushSelectIndex(int listIndex)
{
	if (WBQtObjectPanel::instance() != NULL)
	{
		WBQtObjectPanel::instance()->selectListIndex(listIndex);
	}
}
