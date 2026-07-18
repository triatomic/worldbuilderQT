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

// actions (each runs the real handler; Delete may pop the in-use confirmation,
// Export/Import pop MFC file dialogs). NewTeam only creates+selects the team -- the Qt side
// opens the Qt team sheet on it afterwards.
void WBQtTeams_NewTeam(void);
void WBQtTeams_DeleteTeam(void);
void WBQtTeams_CopyTeam(void);
void WBQtTeams_EditTeam(void);
void WBQtTeams_SelectTeamMembers(void);
void WBQtTeams_MoveUpTeam(void);
void WBQtTeams_MoveDownTeam(void);
void WBQtTeams_ExportTeams(void);
void WBQtTeams_ImportTeams(void);

// --- the Qt team property sheet (Tier 3b-3): four HIDDEN Team* pages bound to the current
// team; the Qt sheet drives their real controls and sends the real WM_COMMAND notifications,
// so every page handler (live dict writes, rename validation, the PickUnitDialog pops) is
// reused verbatim ---

#define WB_QT_TEAMPAGE_IDENTITY			0
#define WB_QT_TEAMPAGE_REINFORCEMENT	1
#define WB_QT_TEAMPAGE_BEHAVIOR			2
#define WB_QT_TEAMPAGE_GENERIC			3

#define WB_QT_TEAMNOTIFY_NONE		0
#define WB_QT_TEAMNOTIFY_CHANGE		1	// EN_CHANGE (edits)
#define WB_QT_TEAMNOTIFY_KILLFOCUS	2	// EN_KILLFOCUS (the team-name commit)
#define WB_QT_TEAMNOTIFY_SELCHANGE	1	// CBN_SELCHANGE (combos)
#define WB_QT_TEAMNOTIFY_SELENDOK	2	// CBN_SELENDOK (the owner combo)

// Bind the hidden pages to the CURRENT team (1 on success; 0 if none/default team/already open).
int  WBQtTeamSheet_Open(void);
void WBQtTeamSheet_Close(void);

// Window-size persistence ([QtWindowSize] in WorldBuilder.ini; keys prefixed per
// window, e.g. TeamSheetWidth -- shared home for any Qt dialog that saves its size).
int  WBQtTeamSheet_GetProfileInt(const char *key, int def);
void WBQtTeamSheet_SetProfileInt(const char *key, int value);

void WBQtTeamPage_GetText(int page, int ctrlId, char *buf, int cap);
void WBQtTeamPage_SetText(int page, int ctrlId, const char *text, int notify);
int  WBQtTeamPage_GetCheck(int page, int ctrlId);
void WBQtTeamPage_SetCheck(int page, int ctrlId, int check);
int  WBQtTeamPage_IsEnabled(int page, int ctrlId);
int  WBQtTeamPage_ComboCount(int page, int ctrlId);
int  WBQtTeamGeneric_FilledCount(void);		// filled generic-script slots (compacted chain length)
void WBQtTeamPage_ComboItem(int page, int ctrlId, int i, char *buf, int cap);
void WBQtTeamPage_ComboSelectText(int page, int ctrlId, const char *text, int notify);
void WBQtTeamPage_ClickButton(int page, int ctrlId);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_TEAMS_BRIDGE_H
