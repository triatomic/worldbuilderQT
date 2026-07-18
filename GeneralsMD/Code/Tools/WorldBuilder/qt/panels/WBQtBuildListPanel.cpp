// WBQtBuildListPanel.cpp -- see WBQtBuildListPanel.h.
#include "WBQtBuildListPanel.h"
#include "ui_WBQtBuildListPanel.h"
#include "WBQtComboStyle.h"
#include "WBQtPanelBridge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>

WBQtBuildListPanel *WBQtBuildListPanel::s_instance = NULL;

WBQtBuildListPanel::WBQtBuildListPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtBuildListPanel),
	  m_updating(false)
{
	// The static widget tree lives in WBQtBuildListPanel.ui; bind the members the
	// logic below uses.
	m_ui->setupUi(this);

	m_side = m_ui->side;
	m_buildList = m_ui->buildList;
	m_up = m_ui->up;
	m_down = m_ui->down;
	m_add = m_ui->add;
	m_delete = m_ui->deleteBtn;
	m_export = m_ui->exportBtn;
	m_import = m_ui->importBtn;
	m_angle = m_ui->angle;
	m_z = m_ui->z;
	m_alreadyBuilt = m_ui->alreadyBuilt;
	m_rebuilds = m_ui->rebuilds;
	m_power = m_ui->power;
	m_forcedShow = m_ui->forcedShow;

	// == the MFC IDC_REBUILDS (CBS_DROPDOWN): type to narrow the list. The rest of the panel's
	// combos were CBS_DROPDOWNLIST, so they stay pick-only -- but all of them scroll (WS_VSCROLL).
	WBQtComboStyle::applyTypeToFilter(m_rebuilds);
	WBQtComboStyle::applyPopupScrollRecursive(this);

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
	// == MFC's ON_CBN_EDITCHANGE(IDC_REBUILDS): a hand-typed count (e.g. 12) matches no list
	// entry, so currentIndexChanged never fires; commit it when the edit finishes instead.
	if (m_rebuilds->lineEdit() != NULL)
	{
		connect(m_rebuilds->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onRebuildsTextCommitted()));
	}
	connect(m_forcedShow, SIGNAL(clicked()), this, SLOT(onForcedShowToggled()));

	s_instance = this;
}

WBQtBuildListPanel::~WBQtBuildListPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
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

// A hand-typed rebuild count that matches no list entry (7+). currentIndexChanged never fires
// for it, so this commits on edit-finish -- == MFC's OnEditchangeRebuilds (GetCurSel()==-1).
void WBQtBuildListPanel::onRebuildsTextCommitted()
{
	if (m_updating)
	{
		return;
	}
	// Only handle genuine free text; a real list pick is already covered by onRebuildsChanged.
	if (m_rebuilds->currentIndex() >= 0)
	{
		return;
	}
	bool ok = false;
	int nr = m_rebuilds->currentText().toInt(&ok);
	if (!ok)
	{
		return;	// non-numeric -- ignore, like sscanf failing
	}
	WBQtBuildList_SetCurBuild(m_buildList->currentRow());
	WBQtBuildList_SetRebuilds(nr);
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
