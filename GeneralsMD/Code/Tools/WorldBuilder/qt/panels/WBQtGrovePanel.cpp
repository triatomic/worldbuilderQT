// WBQtGrovePanel.cpp -- see WBQtGrovePanel.h.
#include "WBQtGrovePanel.h"
#include "WBQtGroveBridge.h"
#include "WBQtPreviewImage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

WBQtGrovePanel *WBQtGrovePanel::s_instance = NULL;

// The 1..11 tree slot each signalling child widget belongs to is stored in this dynamic
// property, so a shared slot can recover which row fired without a pointer-compare loop.
static const char *kRowProperty = "wbqtGroveRow";

WBQtGrovePanel::WBQtGrovePanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Grove Options");
	resize(360, 640);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Set-name preset combo + the Save / Settings buttons.
	QHBoxLayout *setRow = new QHBoxLayout();
	setRow->addWidget(new QLabel("Set:", this));
	m_setName = new QComboBox(this);
	setRow->addWidget(m_setName, 1);
	QPushButton *saveBtn = new QPushButton("Save", this);
	QPushButton *settingsBtn = new QPushButton("Settings", this);
	setRow->addWidget(saveBtn);
	setRow->addWidget(settingsBtn);
	root->addLayout(setRow);

	// The 11 tree-type rows: a template combo + a weight spinbox each.
	QGroupBox *treesBox = new QGroupBox("Tree types & weights", this);
	QGridLayout *grid = new QGridLayout(treesBox);
	grid->addWidget(new QLabel("Type", treesBox), 0, 0);
	grid->addWidget(new QLabel("Weight", treesBox), 0, 1);
	for (int t = 1; t <= TREES_PER_SET; ++t)
	{
		int i = t - 1;
		m_treeType[i] = new QComboBox(treesBox);
		m_treeType[i]->setProperty(kRowProperty, t);
		m_weight[i] = new QSpinBox(treesBox);
		m_weight[i]->setRange(0, 1000000);
		m_weight[i]->setProperty(kRowProperty, t);
		grid->addWidget(m_treeType[i], t, 0);
		grid->addWidget(m_weight[i], t, 1);
	}
	root->addWidget(treesBox);

	// Running weight total (read-only display).
	QHBoxLayout *totalRow = new QHBoxLayout();
	totalRow->addWidget(new QLabel("Total weight:", this));
	m_total = new QLabel("0", this);
	totalRow->addWidget(m_total, 1);
	root->addLayout(totalRow);

	// Number of trees per grove.
	QHBoxLayout *numRow = new QHBoxLayout();
	numRow->addWidget(new QLabel("Number of trees:", this));
	m_numTrees = new QSpinBox(this);
	m_numTrees->setRange(0, 1000000);
	numRow->addWidget(m_numTrees, 1);
	root->addLayout(numRow);

	// Placement checkboxes.
	QGroupBox *placeBox = new QGroupBox("Placement", this);
	QVBoxLayout *placeLay = new QVBoxLayout(placeBox);
	m_allowWater = new QCheckBox("Allow water placement", placeBox);
	m_allowCliff = new QCheckBox("Allow cliff placement", placeBox);
	m_usePropsOnly = new QCheckBox("Use props only", placeBox);
	placeLay->addWidget(m_allowWater);
	placeLay->addWidget(m_allowCliff);
	placeLay->addWidget(m_usePropsOnly);
	root->addWidget(placeBox);

	// Object preview thumbnail (of the last-touched tree combo).
	m_preview = new QLabel(this);
	m_preview->setFixedSize(128, 128);
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setFrameShape(QFrame::Box);
	root->addWidget(m_preview, 0, Qt::AlignHCenter);

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
	connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSaveSet()));
	connect(settingsBtn, SIGNAL(clicked()), this, SLOT(onOpenSettings()));

	s_instance = this;
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

void WBQtGrovePanel::onSaveSet()
{
	WBQtGrove_SaveSet();
}

void WBQtGrovePanel::onOpenSettings()
{
	WBQtGrove_OpenSettings();
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
