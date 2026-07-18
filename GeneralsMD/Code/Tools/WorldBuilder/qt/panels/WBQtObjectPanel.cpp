// WBQtObjectPanel.cpp -- see WBQtObjectPanel.h.
#include "WBQtObjectPanel.h"
#include "ui_WBQtObjectPanel.h"
#include "WBQtComboStyle.h"
#include "WBQtPanelBridge.h"
#include "WBQtPreviewImage.h"
#include "WBQtTreeStyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

WBQtObjectPanel *WBQtObjectPanel::s_instance = NULL;

// The list index a tree leaf represents is stored in this item-data role (>=0 for leaves,
// absent/-1 for grouping nodes).
static const int kListIndexRole = Qt::UserRole + 1;

WBQtObjectPanel::WBQtObjectPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtObjectPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtObjectPanel.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_search = m_ui->search;
	m_tree = m_ui->tree;
	m_nameLabel = m_ui->nameLabel;
	m_preview = m_ui->preview;
	m_team = m_ui->team;
	m_height = m_ui->height;
	m_placeAll = m_ui->placeAll;
	m_placeAllYSpacing = m_ui->placeAllYSpacing;
	m_previewSound = m_ui->previewSound;
	m_previewBuildZone = m_ui->previewBuildZone;
	m_useWaterHeight = m_ui->useWaterHeight;

	// MFC's combos are WS_VSCROLL: give every drop-down here a scrolling popup.
	WBQtComboStyle::applyPopupScrollRecursive(this);

	WBQtTreeStyle::applyTreeLines(m_tree);

	m_height->setValue(WBQtObject_GetHeight());

	// Seed everything under the guard so nothing echoes back while we populate.
	m_updating = true;
	rebuildTree(QString());
	m_previewSound->setChecked(WBQtObject_GetPreviewSound() != 0);
	m_previewBuildZone->setChecked(WBQtObject_GetPreviewBuildZone() != 0);
	m_useWaterHeight->setChecked(WBQtObject_GetUseWaterHeight() != 0);
	m_placeAll->setChecked(WBQtObject_GetPlaceAll() != 0);
	m_placeAllYSpacing->setValue(WBQtObject_GetPlaceAllYSpacing());
	refreshTeamCombo();
	m_updating = false;

	connect(m_tree, SIGNAL(itemSelectionChanged()), this, SLOT(onTreeSelectionChanged()));
	connect(m_team, SIGNAL(currentIndexChanged(int)), this, SLOT(onTeamChanged(int)));
	connect(m_height, SIGNAL(valueChanged(int)), this, SLOT(onHeightChanged(int)));
	connect(m_ui->searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
	connect(m_ui->resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
	connect(m_search, SIGNAL(returnPressed()), this, SLOT(onSearch()));
	if (WBQtConfig_GetNewSearch() != 0)
	{
		// NewSearch: filter live as the user types (the Search button still works).
		connect(m_search, SIGNAL(textChanged(QString)), this, SLOT(onSearch()));
	}
	connect(m_previewSound, SIGNAL(clicked()), this, SLOT(onPreviewSoundToggled()));
	connect(m_previewBuildZone, SIGNAL(clicked()), this, SLOT(onPreviewBuildZoneToggled()));
	connect(m_useWaterHeight, SIGNAL(clicked()), this, SLOT(onUseWaterHeightToggled()));
	connect(m_placeAll, SIGNAL(clicked()), this, SLOT(onPlaceAllToggled()));
	connect(m_placeAllYSpacing, SIGNAL(valueChanged(int)), this, SLOT(onPlaceAllYSpacingChanged(int)));

	s_instance = this;
}

WBQtObjectPanel::~WBQtObjectPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
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
	char preBuf[cap];
	char sideBuf[cap];
	char sortBuf[cap];
	char leafBuf[cap];
	char nameBuf[cap];

	int count = WBQtObject_GetCount();
	QString lowered = filter.toLower();
	bool expandAll = !lowered.isEmpty();

	for (int i = 0; i < count; ++i)
	{
		if (!WBQtObject_GetEntry(i, preBuf, sideBuf, sortBuf, leafBuf, cap))
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

		// Optional pre-side bucket (ES_TEST -> top-level "TEST"), then side, mirroring the MFC
		// addObject() tier order: [pre-side] / side / sorting / leaf.
		QTreeWidgetItem *parent = NULL;
		QString pre = QString::fromLatin1(preBuf);
		if (!pre.isEmpty())
		{
			parent = findOrAddChild(NULL, pre);
		}
		parent = findOrAddChild(parent, QString::fromLatin1(sideBuf));
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

	// == ObjectOptions' selection block: play the template's ambient sound when the toggle is
	// on (the MFC gate was window-open, always false in the Qt build, so it never ran here).
	WBQtObject_PreviewAmbient();
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

void WBQtObjectPanel::onPlaceAllYSpacingChanged(int v)
{
	if (m_updating)
	{
		return;
	}
	WBQtObject_SetPlaceAllYSpacing(v);
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
