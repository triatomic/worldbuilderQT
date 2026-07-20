// WBQtGrovePanel.cpp -- see WBQtGrovePanel.h.
#include "WBQtGrovePanel.h"
#include "ui_WBQtGrovePanel.h"
#include "WBQtComboStyle.h"
#include "WBQtGroveBridge.h"
#include "WBQtPreviewImage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>

WBQtGrovePanel *WBQtGrovePanel::s_instance = NULL;

// The 1..11 tree slot each signalling child widget belongs to is stored in this dynamic
// property, so a shared slot can recover which row fired without a pointer-compare loop.
static const char *kRowProperty = "wbqtGroveRow";

WBQtGrovePanel::WBQtGrovePanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtGrovePanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtGrovePanel.ui; bind the members the logic
	// below uses, then add the dynamic tree-type rows.
	m_ui->setupUi(this);
	resize(360, 640);

	m_setName = m_ui->setName;
	m_total = m_ui->total;
	m_numTrees = m_ui->numTrees;
	m_allowWater = m_ui->allowWater;
	m_allowCliff = m_ui->allowCliff;
	m_usePropsOnly = m_ui->usePropsOnly;
	m_preview = m_ui->preview;	// object preview thumbnail (of the last-touched tree combo)

	// The 11 tree rows, laid out like the MFC dialog: the narrow % edit LEFT of the type
	// combo, rows 1-10 under the "% / Tree Type" header, and row 11 inside the
	// "Other Props:" group (IDC_Grove_Type11/Per11 live there in the .rc).
	for (int t = 1; t <= TREES_PER_SET; ++t)
	{
		int i = t - 1;
		m_treeType[i] = new QComboBox(this);
		m_treeType[i]->setProperty(kRowProperty, t);
		m_weight[i] = new QSpinBox(this);
		m_weight[i]->setRange(0, 1000000);
		m_weight[i]->setProperty(kRowProperty, t);
		m_weight[i]->setMaximumWidth(48);	// == the 20-DLU MFC percent edit
		if (t < TREES_PER_SET)
		{
			m_ui->treesGrid->addWidget(m_weight[i], t, 0);
			m_ui->treesGrid->addWidget(m_treeType[i], t, 1);
		}
		else
		{
			m_ui->otherGrid->addWidget(m_weight[i], 0, 0);
			m_ui->otherGrid->addWidget(m_treeType[i], 0, 1);
		}
	}

	// MFC's combos are WS_VSCROLL: give every drop-down here a scrolling popup.
	WBQtComboStyle::applyPopupScrollRecursive(this);

	// The tree-type lists are long (every tree template in the game), so let the user type to
	// narrow them. MFC's IDC_Grove_Type* were CBS_DROPDOWNLIST (pick-only) -- we deliberately
	// go further here; NoInsert + the completer mean typing can still only ever land on a real
	// entry, so the index onTreeTypeChanged() hands to the MFC combo stays valid.
	for (int i = 0; i < TREES_PER_SET; ++i)
	{
		WBQtComboStyle::applyTypeToFilter(m_treeType[i]);
	}
	WBQtComboStyle::applyTypeToFilter(m_setName);

	// Seed everything from the hidden MFC panel under the guard so nothing echoes back.
	m_updating = true;
	seedFromMfc();
	m_updating = false;

	connect(m_setName, SIGNAL(currentIndexChanged(int)), this, SLOT(onSetNameChanged(int)));
	for (int i = 0; i < TREES_PER_SET; ++i)
	{
		connect(m_treeType[i], SIGNAL(currentIndexChanged(int)), this, SLOT(onTreeTypeChanged(int)));
		connect(m_weight[i], SIGNAL(valueChanged(int)), this, SLOT(onWeightChanged(int)));
	}
	connect(m_numTrees, SIGNAL(valueChanged(int)), this, SLOT(onNumTreesChanged(int)));
	connect(m_allowWater, SIGNAL(clicked()), this, SLOT(onAllowWaterToggled()));
	connect(m_allowCliff, SIGNAL(clicked()), this, SLOT(onAllowCliffToggled()));
	connect(m_usePropsOnly, SIGNAL(clicked()), this, SLOT(onUsePropsOnlyToggled()));
	// No Save Set button: the MFC dialog ships it NOT WS_VISIBLE | WS_DISABLED (dead code).
	connect(m_ui->settingsBtn, SIGNAL(clicked()), this, SLOT(onOpenSettings()));

	s_instance = this;
}

WBQtGrovePanel::~WBQtGrovePanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

int WBQtGrovePanel::rowOfSender(QObject *sender)
{
	if (sender == NULL)
	{
		return 0;
	}
	bool ok = false;
	int t = sender->property(kRowProperty).toInt(&ok);
	if (!ok || t < 1 || t > TREES_PER_SET)
	{
		return 0;
	}
	return t;
}

void WBQtGrovePanel::fillTreeCombo(int type)
{
	// Caller has set m_updating (repopulation must not fire onTreeTypeChanged).
	if (type < 1 || type > TREES_PER_SET)
	{
		return;
	}
	QComboBox *combo = m_treeType[type - 1];
	combo->clear();
	const int cap = 256;
	char nameBuf[cap];
	int count = WBQtGrove_GetTreeTypeCount(type);
	for (int i = 0; i < count; ++i)
	{
		if (WBQtGrove_GetTreeTypeName(type, i, nameBuf, cap))
		{
			combo->addItem(QString::fromLatin1(nameBuf));
		}
		else
		{
			combo->addItem(QString());
		}
	}
	int sel = WBQtGrove_GetTreeTypeSel(type);
	if (sel >= 0 && sel < combo->count())
	{
		combo->setCurrentIndex(sel);
	}
}

void WBQtGrovePanel::fillSetCombo()
{
	// Caller has set m_updating (repopulation must not fire onSetNameChanged).
	// Re-read Grovesets.ini into the hidden MFC combo first (== MFC's ON_CBN_DROPDOWN), so a set
	// renamed via the Settings button shows up here without an app restart.
	WBQtGrove_RefreshSetNames();
	m_setName->clear();
	const int cap = 256;
	char nameBuf[cap];
	int count = WBQtGrove_GetSetCount();
	for (int i = 0; i < count; ++i)
	{
		if (WBQtGrove_GetSetName(i, nameBuf, cap))
		{
			m_setName->addItem(QString::fromLatin1(nameBuf));
		}
		else
		{
			m_setName->addItem(QString());
		}
	}
	int sel = WBQtGrove_GetCurrentSet();
	if (sel >= 0 && sel < m_setName->count())
	{
		m_setName->setCurrentIndex(sel);
	}
}

void WBQtGrovePanel::seedFromMfc()
{
	// Caller has set m_updating.
	fillSetCombo();
	for (int t = 1; t <= TREES_PER_SET; ++t)
	{
		fillTreeCombo(t);
		m_weight[t - 1]->setValue(WBQtGrove_GetWeight(t));
	}
	m_numTrees->setValue(WBQtGrove_GetNumTrees());
	m_allowWater->setChecked(WBQtGrove_GetAllowWater() != 0);
	m_allowCliff->setChecked(WBQtGrove_GetAllowCliff() != 0);
	m_usePropsOnly->setChecked(WBQtGrove_GetUsePropsOnly() != 0);
	refreshTotal();
	refreshPreview();
}

void WBQtGrovePanel::refreshTotal()
{
	m_total->setText(QString::number(WBQtGrove_GetTotalPerc()));
}

void WBQtGrovePanel::refreshPreview()
{
	int w = 128, h = 128;
	WBQtGrove_GetPreviewSize(&w, &h);
	QByteArray bgr(w * h * 3, 0);
	if (WBQtGrove_RenderPreview(reinterpret_cast<unsigned char*>(bgr.data()), bgr.size()))
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

void WBQtGrovePanel::onSetNameChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	// Load the picked set: persists the index + pulls in that set's tree makeup and ratios.
	WBQtGrove_SelectSet(index);
	// The load changed the combos + weights; re-seed the panel to match.
	m_updating = true;
	for (int t = 1; t <= TREES_PER_SET; ++t)
	{
		fillTreeCombo(t);
		m_weight[t - 1]->setValue(WBQtGrove_GetWeight(t));
	}
	refreshTotal();
	m_updating = false;
	refreshPreview();
}

void WBQtGrovePanel::onTreeTypeChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	int type = rowOfSender(sender());
	if (type == 0)
	{
		return;
	}
	// Drive the MFC combo (latches the preview source + re-saves the makeup).
	WBQtGrove_SetTreeTypeSel(type, index);
	refreshPreview();
}

void WBQtGrovePanel::onWeightChanged(int value)
{
	if (m_updating)
	{
		return;
	}
	int type = rowOfSender(sender());
	if (type == 0)
	{
		return;
	}
	WBQtGrove_SetWeight(type, value);
	m_updating = true;
	refreshTotal();
	m_updating = false;
}

void WBQtGrovePanel::onNumTreesChanged(int value)
{
	if (m_updating)
	{
		return;
	}
	WBQtGrove_SetNumTrees(value);
}

void WBQtGrovePanel::onAllowWaterToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtGrove_SetAllowWater(m_allowWater->isChecked() ? 1 : 0);
}

void WBQtGrovePanel::onAllowCliffToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtGrove_SetAllowCliff(m_allowCliff->isChecked() ? 1 : 0);
}

void WBQtGrovePanel::onUsePropsOnlyToggled()
{
	if (m_updating)
	{
		return;
	}
	// Props-only changes what the totals count; re-display the total after the bridge recomputes.
	WBQtGrove_SetUsePropsOnly(m_usePropsOnly->isChecked() ? 1 : 0);
	m_updating = true;
	refreshTotal();
	m_updating = false;
}

void WBQtGrovePanel::onOpenSettings()
{
	WBQtGrove_OpenSettings();
	// The Settings button opens Grovesets.ini in Notepad for renaming sets. ShellExecute returns
	// immediately (Notepad is async), so we can't re-read on its close here -- but the panel's
	// showEvent picks up the renames the next time it is shown, and any set-name drop-down also
	// re-reads via fillSetCombo(). Re-fill now too so a rename saved before this returns is seen.
	m_updating = true;
	fillSetCombo();
	m_updating = false;
}

void WBQtGrovePanel::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	// Re-read Grovesets.ini into the set-name combo (renames may have happened while hidden).
	m_updating = true;
	fillSetCombo();
	m_updating = false;
}

void WBQtGrovePanel::pushRefresh()
{
	// Something changed the MFC state under us; re-seed every control from it.
	m_updating = true;
	seedFromMfc();
	m_updating = false;
}

// --- Forward push functions (MFC state -> widget), the Qt side of WBQtGroveBridge.h ----------
extern "C" void WBQtGrove_PushRefresh(void)
{
	if (WBQtGrovePanel::instance() != NULL)
	{
		WBQtGrovePanel::instance()->pushRefresh();
	}
}
