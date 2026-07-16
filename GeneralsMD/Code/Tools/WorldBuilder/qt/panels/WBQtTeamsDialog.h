// WBQtTeamsDialog.h -- the native Qt Teams dialog ("Team Builder", Tier 3b-2). The hidden MFC
// CTeamsDialog stays the model owner (working-copy SidesList, team surgery, the MFC team
// property sheet, fix-owner validation, import/export); this dialog reads state back from it
// and pushes actions through the C facade in WBQtTeamsBridge.h after every change.
#ifndef WB_QT_TEAMS_DIALOG_H
#define WB_QT_TEAMS_DIALOG_H

#include <QDialog>

class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui { class WBQtTeamsDialog; }	// generated from WBQtTeamsDialog.ui

class WBQtTeamsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamsDialog(QWidget *parent = 0);
	virtual ~WBQtTeamsDialog();

private slots:
	void onPlayerRowChanged(int row);
	void onTeamRowChanged();
	void onTeamDoubleClicked(QTreeWidgetItem *item, int column);
	void onNewTeam();
	void onCopyTeam();
	void onDeleteTeam();
	void onSelectTeamMembers();
	void onMoveUpTeam();
	void onMoveDownTeam();
	void onExportTeams();
	void onImportTeams();

private:
	void refreshAll();
	void refreshPlayers();
	void refreshTeamsTable();
	void refreshButtons();
	void runTeamSheet();

	Ui::WBQtTeamsDialog *m_ui;	// owns the static widget tree (WBQtTeamsDialog.ui)

	bool m_updating;
	QListWidget *m_players;
	QTreeWidget *m_teams;
	QPushButton *m_newButton;
	QPushButton *m_copyButton;
	QPushButton *m_deleteButton;
	QPushButton *m_moveUpButton;
	QPushButton *m_moveDownButton;
};

#endif // WB_QT_TEAMS_DIALOG_H
