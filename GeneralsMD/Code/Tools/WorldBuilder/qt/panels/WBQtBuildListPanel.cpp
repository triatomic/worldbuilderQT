// WBQtBuildListPanel.cpp -- see WBQtBuildListPanel.h.
#include "WBQtBuildListPanel.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

WBQtBuildListPanel *WBQtBuildListPanel::s_instance = NULL;

WBQtBuildListPanel::WBQtBuildListPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_updating(false)
{
	setWindowTitle("Build List");
	resize(240, 460);

	QVBoxLayout *root = new QVBoxLayout(this);

	// Side.
	QGroupBox *sideBox = new QGroupBox("Side", this);
	QVBoxLayout *sideLay = new QVBoxLayout(sideBox);
	m_side = new QComboBox(sideBox);
	sideLay->addWidget(m_side);
	root->addWidget(sideBox);

	// Build list.
	QGroupBox *listBox = new QGroupBox("Build List", this);
	QVBoxLayout *listLay = new QVBoxLayout(listBox);
	m_buildList = new QListWidget(listBox);
	listLay->addWidget(m_buildList, 1);

	// Move + edit buttons.
	QHBoxLayout *btnRow = new QHBoxLayout();
	m_up = new QPushButton("Up", listBox);
	m_down = new QPushButton("Down", listBox);
	m_add = new QPushButton("Add Building", listBox);
	m_delete = new QPushButton("Delete", listBox);
	btnRow->addWidget(m_up);
	btnRow->addWidget(m_down);
	btnRow->addWidget(m_add);
	btnRow->addWidget(m_delete);
	listLay->addLayout(btnRow);

	QHBoxLayout *ioRow = new QHBoxLayout();
	m_export = new QPushButton("Export", listBox);
	m_import = new QPushButton("Import", listBox);
	m_alreadyBuilt = new QCheckBox("Structure Already Built", listBox);
	ioRow->addWidget(m_export);
	ioRow->addWidget(m_import);
	ioRow->addStretch(1);
	listLay->addLayout(ioRow);
	listLay->addWidget(m_alreadyBuilt);
	root->addWidget(listBox, 1);

	// Per-building attributes.
	QGroupBox *attrBox = new QGroupBox("Selected Building", this);
	QVBoxLayout *attrLay = new QVBoxLayout(attrBox);

	QHBoxLayout *azRow = new QHBoxLayout();
	azRow->addWidget(new QLabel("Z:", attrBox));
	m_z = new QDoubleSpinBox(attrBox);
	m_z->setRange(-100000.0, 100000.0);
	m_z->setDecimals(2);
	azRow->addWidget(m_z, 1);
	azRow->addWidget(new QLabel("Angle:", attrBox));
	m_angle = new QDoubleSpinBox(attrBox);
	m_angle->setRange(-360.0, 360.0);
	m_angle->setDecimals(2);
	azRow->addWidget(m_angle, 1);
	attrLay->addLayout(azRow);

	QHBoxLayout *rbRow = new QHBoxLayout();
	rbRow->addWidget(new QLabel("Rebuilds:", attrBox));
	m_rebuilds = new QComboBox(attrBox);
	m_rebuilds->setEditable(true);
	m_rebuilds->addItem("0");
	m_rebuilds->addItem("1");
	m_rebuilds->addItem("2");
	m_rebuilds->addItem("3");
	m_rebuilds->addItem("4");
	m_rebuilds->addItem("5");
	m_rebuilds->addItem("Unlimited");
	rbRow->addWidget(m_rebuilds, 1);
	attrLay->addLayout(rbRow);
	root->addWidget(attrBox);

	// Power meter.
	QGroupBox *powerBox = new QGroupBox("Power Used:", this);
	QVBoxLayout *powerLay = new QVBoxLayout(powerBox);
	m_power = new QProgressBar(powerBox);
	m_power->setRange(0, 100);
	m_power->setTextVisible(false);
	powerLay->addWidget(m_power);
	root->addWidget(powerBox);

	m_forcedShow = new QCheckBox("Don't Hide Objects When Tool Is Closed", this);
	root->addWidget(m_forcedShow);

	refresh();

	connect(m_side, SIGNAL(currentIndexChanged(int)), this, SLOT(onSideChanged(int)));
	connect(m_buildList, SIGNAL(itemSelectionChanged()), this, SLOT(onBuildSelChanged()));
	connect(m_buildList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(onBuildDoubleClicked()));
	connect(m_up, SIGNAL(clicked()), this, SLOT(onMoveUp()));
	connect(m_down, SIGNAL(clicked()), this, SLOT(onMoveDown()));
	connect(m_add, SIGNAL(clicked()), this, SLOT(onAdd()));
	connect(m_delete, SIGNAL(clicked()), this, SLOT(onDelete()));
	connect(m_export, SIGNAL(clicked()), this, SLOT(onExport()));
	connect(m_import, SIGNAL(clicked()), this, SLOT(onImport()));
	connect(m_angle, SIGNAL(valueChanged(double)), this, SLOT(onAngleChanged(double)));
	connect(m_z, SIGNAL(valueChanged(double)), this, SLOT(onZChanged(double)));
	connect(m_alreadyBuilt, SIGNAL(clicked()), this, SLOT(onAlreadyBuiltToggled()));
	connect(m_rebuilds, SIGNAL(currentIndexChanged(int)), this, SLOT(onRebuildsChanged()));
	connect(m_forcedShow, SIGNAL(clicked()), this, SLOT(onForcedShowToggled()));

	s_instance = this;
}

void WBQtBuildListPanel::refresh()
{
	m_updating = true;

	// Side combo.
	m_side->clear();
	const int cap = 256;
	char buf[cap];
	int sides = WBQtBuildList_GetSideCount();
	for (int i = 0; i < sides; ++i)
	{
		if (WBQtBuildList_GetSideName(i, buf, cap))
		{
			m_side->addItem(QString::fromLatin1(buf));
		}
		else
		{
			m_side->addItem(QString());
		}
	}
	int curSide = WBQtBuildList_GetCurSide();
	if (curSide >= 0 && curSide < m_side->count())
	{
		m_side->setCurrentIndex(curSide);
	}

	// Build list.
	m_buildList->clear();
	int builds = WBQtBuildList_GetBuildCount();
	for (int i = 0; i < builds; ++i)
	{
		if (WBQtBuildList_GetBuildName(i, buf, cap))
		{
			m_buildList->addItem(QString::fromLatin1(buf));
		}
		else
		{
			m_buildList->addItem(QString());
		}
	}
	int curBuild = WBQtBuildList_GetCurBuild();
	if (curBuild >= 0 && curBuild < m_buildList->count())
	{
		m_buildList->setCurrentRow(curBuild);
	}

	m_forcedShow->setChecked(WBQtBuildList_GetForcedShow() != 0);

	m_updating = false;
	refreshAttributes();
}

void WBQtBuildListPanel::refreshAttributes()
{
	m_updating = true;

	bool has = (WBQtBuildList_HasCurBuild() != 0);
	m_angle->setEnabled(has);
	m_z->setEnabled(has);
	m_alreadyBuilt->setEnabled(has);
	m_rebuilds->setEnabled(has);
	m_delete->setEnabled(has);

	if (has)
	{
		m_angle->setValue(WBQtBuildList_GetAngle());
		m_z->setValue(WBQtBuildList_GetZ());
		m_alreadyBuilt->setChecked(WBQtBuildList_GetAlreadyBuilt() != 0);
		int nr = WBQtBuildList_GetRebuilds();
		if (nr < 0)
		{
			m_rebuilds->setCurrentIndex(6);	// Unlimited
		}
		else if (nr <= 5)
		{
			m_rebuilds->setCurrentIndex(nr);
		}
		else
		{
			m_rebuilds->setEditText(QString::number(nr));
		}
	}

	int curBuild = WBQtBuildList_GetCurBuild();
	int builds = WBQtBuildList_GetBuildCount();
	m_up->setEnabled(curBuild > 0);
	m_down->setEnabled(curBuild >= 0 && curBuild < builds - 1);

	m_power->setValue(WBQtBuildList_GetPowerPercent());

	m_updating = false;
}

void WBQtBuildListPanel::onSideChanged(int index)
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurSide(index);
	refresh();
}

void WBQtBuildListPanel::onBuildSelChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	refreshAttributes();
}

void WBQtBuildListPanel::onBuildDoubleClicked()
{
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_EditProps();
	refresh();
}

void WBQtBuildListPanel::onMoveUp()
{
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_MoveUp();
	refresh();
}

void WBQtBuildListPanel::onMoveDown()
{
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_MoveDown();
	refresh();
}

void WBQtBuildListPanel::onAdd()
{
	WBQtBuildList_AddBuilding();
	// Placement happens via a map click; the panel refreshes when BuildList::update fires.
}

void WBQtBuildListPanel::onDelete()
{
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_DeleteBuilding();
	refresh();
}

void WBQtBuildListPanel::onExport()
{
	WBQtBuildList_Export();
}

void WBQtBuildListPanel::onImport()
{
	WBQtBuildList_Import();
	refresh();
}

void WBQtBuildListPanel::onAngleChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_SetAngle(v);
}

void WBQtBuildListPanel::onZChanged(double v)
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_SetZ(v);
}

void WBQtBuildListPanel::onAlreadyBuiltToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_SetAlreadyBuilt(m_alreadyBuilt->isChecked() ? 1 : 0);
}

void WBQtBuildListPanel::onRebuildsChanged()
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	int idx = m_rebuilds->currentIndex();
	if (idx == 6)
	{
		WBQtBuildList_SetRebuilds(-1);	// Unlimited
	}
	else if (idx >= 0 && idx <= 5)
	{
		WBQtBuildList_SetRebuilds(idx);
	}
	else
	{
		// A typed value.
		WBQtBuildList_SetRebuilds(m_rebuilds->currentText().toInt());
	}
}

void WBQtBuildListPanel::onForcedShowToggled()
{
	if (m_updating)
	{
		return;
	}
	WBQtBuildList_SetForcedShow(m_forcedShow->isChecked() ? 1 : 0);
}

// --- Forward push (BuildList refresh -> widget), the Qt-side of WBQtPanelBridge.h ---------
extern "C" void WBQtBuildList_PushRefresh(void)
{
	if (WBQtBuildListPanel::instance() != NULL)
	{
		WBQtBuildListPanel::instance()->refresh();
	}
}
