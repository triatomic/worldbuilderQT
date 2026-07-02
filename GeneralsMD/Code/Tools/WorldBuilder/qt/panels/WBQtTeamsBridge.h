// WBQtTeamsBridge.h -- opaque facade for the Qt Teams dialog (Tier 3b-2).
//
// Model: the MFC CTeamsDialog is created HIDDEN and stays the model owner -- its working-copy
// m_sides, the new/copy/delete/move-team logic, the team property sheet (still the MFC
// CPropertySheet in this stage), the fix-owner validation and the import/export file flow are
// all reused verbatim. The Qt dialog reads the hidden list-control/listbox contents back
// (updateUI keeps them canonical) and pushes actions through the real handlers, via qt* member
// functions defined in src/WBQtTeamsBridge.cpp. Plain C surface (int / const char* only).
#ifndef WB_QT_TEAMS_BRIDGE_H
#define WB_QT_TEAMS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ================= MFC -> Qt (implemented in qt/panels/WBQtTeamsDialog.cpp) =================

// Run the modal Qt Teams dialog over the frame (disabled for the modal's lifetime). Returns 1
// on OK (committed as one undoable), 0 on cancel. NOTE: opening may pop fix-team-owner modals
// first (== the MFC OnInitDialog's validateTeamOwners), before the Qt window shows.
int WBQtTeams_Run(void *frameHwnd);

// ================= Qt -> MFC (implemented in src/WBQtTeamsBridge.cpp) =================

void WBQtTeamsData_Open(void);
void WBQtTeamsData_Close(int accepted);

// players (rows == side indices, unsorted)
int  WBQtTeamsData_GetPlayerCount(void);
void WBQtTeamsData_GetPlayerName(int i, char *buf, int cap);
int  WBQtTeamsData_GetPlayerIndex(void);
void WBQtTeams_SelectPlayer(int i);

// the teams table for the selected player (rows == the hidden CListCtrl rows;
// columns: 0 name, 1 script, 2 trigger, 3 priority, 4 origin)
int  WBQtTeamsData_GetTeamRowCount(void);
void WBQtTeamsData_GetTeamRowText(int row, int col, char *buf, int cap);
int  WBQtTeamsData_GetTeamRowSelected(int row);
void WBQtTeams_SelectTeamRow(int row);

// button enables (maintained by updateUI on the hidden buttons)
int  WBQtTeamsData_GetNewEnabled(void);
int  WBQtTeamsData_GetDeleteEnabled(void);
int  WBQtTeamsData_GetCopyEnabled(void);
int  WBQtTeamsData_GetMoveEnabled(void);

// actions (each runs the real handler; New/Edit pop the MFC team property sheet,
// Delete may pop the in-use confirmation, Export/Import pop MFC file dialogs)
void WBQtTeams_NewTeam(void);
void WBQtTeams_DeleteTeam(void);
void WBQtTeams_CopyTeam(void);
void WBQtTeams_EditTeam(void);
void WBQtTeams_SelectTeamMembers(void);
void WBQtTeams_MoveUpTeam(void);
void WBQtTeams_MoveDownTeam(void);
void WBQtTeams_ExportTeams(void);
void WBQtTeams_ImportTeams(void);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_TEAMS_BRIDGE_H
