// WBQtTeamsDialog.cpp -- see WBQtTeamsDialog.h. Layout mirrors IDD_TEAMS_DIALOG ("Team
// Builder"): players list left, the teams table right, the command row on top. The dialog
// re-reads its whole state from the hidden MFC dialog after every action (all bridge calls
// are synchronous). The Expand/Shrink button is dropped -- the Qt dialog resizes natively.
#include "WBQtTeamsDialog.h"
#include "ui_WBQtTeamsDialog.h"
#include "WBQtTeamsBridge.h"
#include "WBQtTeamSheetDialog.h"

#include <QHeaderView>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>

#include <qt_windows.h>

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kTextCap = 1024;
	const int kTeamColumns = 5;	// name / script / trigger / priority / origin
}

WBQtTeamsDialog::WBQtTeamsDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtTeamsDialog),
	m_updating(false)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// The static widget tree lives in WBQtTeamsDialog.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);

	m_players = m_ui->players;
	m_teams = m_ui->teams;
	m_newButton = m_ui->newButton;
	m_copyButton = m_ui->copyButton;
	m_deleteButton = m_ui->deleteButton;
	m_moveUpButton = m_ui->moveUpButton;
	m_moveDownButton = m_ui->moveDownButton;

	m_teams->header()->resizeSection(0, 200);
	m_teams->header()->resizeSection(1, 200);
	m_teams->header()->resizeSection(2, 200);
	m_teams->header()->resizeSection(3, 60);

	connect(m_players, SIGNAL(currentRowChanged(int)), this, SLOT(onPlayerRowChanged(int)));
	connect(m_teams, SIGNAL(itemSelectionChanged()), this, SLOT(onTeamRowChanged()));
	connect(m_teams, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onTeamDoubleClicked(QTreeWidgetItem*,int)));
	connect(m_ui->membersButton, SIGNAL(clicked()), this, SLOT(onSelectTeamMembers()));
	connect(m_newButton, SIGNAL(clicked()), this, SLOT(onNewTeam()));
	connect(m_copyButton, SIGNAL(clicked()), this, SLOT(onCopyTeam()));
	connect(m_deleteButton, SIGNAL(clicked()), this, SLOT(onDeleteTeam()));
	connect(m_moveUpButton, SIGNAL(clicked()), this, SLOT(onMoveUpTeam()));
	connect(m_moveDownButton, SIGNAL(clicked()), this, SLOT(onMoveDownTeam()));
	connect(m_ui->exportButton, SIGNAL(clicked()), this, SLOT(onExportTeams()));
	connect(m_ui->importButton, SIGNAL(clicked()), this, SLOT(onImportTeams()));
	connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(m_ui->cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

	refreshAll();
}

WBQtTeamsDialog::~WBQtTeamsDialog()
{
	delete m_ui;
}

void WBQtTeamsDialog::refreshAll()
{
	refreshPlayers();
	refreshTeamsTable();
	refreshButtons();
}

void WBQtTeamsDialog::refreshPlayers()
{
	m_updating = true;

	// players (rows == side indices)
	int curPlayer = WBQtTeamsData_GetPlayerIndex();
	m_players->clear();
	char buf[kTextCap];
	int playerCount = WBQtTeamsData_GetPlayerCount();
	for (int i = 0; i < playerCount; i++)
	{
		buf[0] = 0;
		WBQtTeamsData_GetPlayerName(i, buf, sizeof(buf));
		new QListWidgetItem(QString::fromLocal8Bit(buf), m_players);
	}
	if (curPlayer >= 0 && curPlayer < playerCount)
	{
		m_players->setCurrentRow(curPlayer);
	}

	m_updating = false;
}

void WBQtTeamsDialog::refreshTeamsTable()
{
	m_updating = true;

	// the teams table (rows mirror the hidden list control)
	char buf[kTextCap];
	m_teams->clear();
	int rowCount = WBQtTeamsData_GetTeamRowCount();
	QTreeWidgetItem *selected = NULL;
	for (int row = 0; row < rowCount; row++)
	{
		QTreeWidgetItem *item = new QTreeWidgetItem(m_teams);
		for (int col = 0; col < kTeamColumns; col++)
		{
			buf[0] = 0;
			WBQtTeamsData_GetTeamRowText(row, col, buf, sizeof(buf));
			item->setText(col, QString::fromLocal8Bit(buf));
		}
		if (WBQtTeamsData_GetTeamRowSelected(row) != 0)
		{
			selected = item;
		}
	}
	if (selected != NULL)
	{
		m_teams->setCurrentItem(selected);
		m_teams->scrollToItem(selected);
	}

	m_updating = false;
}

void WBQtTeamsDialog::refreshButtons()
{
	m_newButton->setEnabled(WBQtTeamsData_GetNewEnabled() != 0);
	m_copyButton->setEnabled(WBQtTeamsData_GetCopyEnabled() != 0);
	m_deleteButton->setEnabled(WBQtTeamsData_GetDeleteEnabled() != 0);
	m_moveUpButton->setEnabled(WBQtTeamsData_GetMoveEnabled() != 0);
	m_moveDownButton->setEnabled(WBQtTeamsData_GetMoveEnabled() != 0);
}

void WBQtTeamsDialog::onPlayerRowChanged(int row)
{
	if (m_updating || row < 0)
	{
		return;
	}
	WBQtTeams_SelectPlayer(row);
	// The player names don't change from selecting one -- rebuilding m_players mid-click
	// (as refreshAll did) is wasted work; only the teams table content follows the player.
	refreshTeamsTable();
	refreshButtons();
}

void WBQtTeamsDialog::onTeamRowChanged()
{
	if (m_updating)
	{
		return;
	}
	int row = m_teams->indexOfTopLevelItem(m_teams->currentItem());
	if (row >= 0)
	{
		WBQtTeams_SelectTeamRow(row);
		// Selecting a row changes no table content -- refreshAll here rebuilt the whole
		// mirror on every click (O(teams) cell read-backs); only the button enables
		// depend on whether the row is a default team.
		refreshButtons();
	}
}

void WBQtTeamsDialog::runTeamSheet()
{
	// Binds the four hidden Team* pages to the current team; refuses default teams.
	if (WBQtTeamSheet_Open() != 0)
	{
		WBQtTeamSheetDialog dlg(this);
		dlg.exec();
		WBQtTeamSheet_Close();
	}
}

void WBQtTeamsDialog::onTeamDoubleClicked(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);
	int row = m_teams->indexOfTopLevelItem(item);
	if (row >= 0)
	{
		WBQtTeams_SelectTeamRow(row);
		runTeamSheet();		// the Qt team template sheet (== OnEditTemplate)
		refreshAll();
	}
}

void WBQtTeamsDialog::onNewTeam()
{
	WBQtTeams_NewTeam();	// creates + selects the team (no sheet pop)
	runTeamSheet();			// == OnNewteam's OnEditTemplate on the fresh team
	refreshAll();
}

void WBQtTeamsDialog::onCopyTeam()
{
	WBQtTeams_CopyTeam();
	refreshAll();
}

void WBQtTeamsDialog::onDeleteTeam()
{
	WBQtTeams_DeleteTeam();	// may pop the in-use confirmation
	refreshAll();
}

void WBQtTeamsDialog::onSelectTeamMembers()
{
	WBQtTeams_SelectTeamMembers();	// pops the count info box + centers the 3D view
	refreshAll();
}

void WBQtTeamsDialog::onMoveUpTeam()
{
	WBQtTeams_MoveUpTeam();
	refreshAll();
}

void WBQtTeamsDialog::onMoveDownTeam()
{
	WBQtTeams_MoveDownTeam();
	refreshAll();
}

void WBQtTeamsDialog::onExportTeams()
{
	WBQtTeams_ExportTeams();	// MFC file dialog + info boxes
	refreshAll();
}

void WBQtTeamsDialog::onImportTeams()
{
	WBQtTeams_ImportTeams();	// MFC file dialog; may pop fix-owner dialogs
	refreshAll();
}

// ===================== the modal entry point =====================

extern "C" int WBQtTeams_Run(void * /*frameHwnd*/)
{
	// Open may pop fix-team-owner modals (== the MFC OnInitDialog) before the window shows.
	WBQtTeamsData_Open();
	// Stage 1 phase 3: parent to the main window + Qt ApplicationModal (fences the viewport
	// via QWinHost WindowBlocked); the EnableWindow(frame) discipline is gone.
	WBQtTeamsDialog dlg(WBQt_DialogParent());
	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	WBQtTeamsData_Close(rc);
	return rc;
}
