// WBQtPlayerListBridge.h -- opaque facade for the Qt Player List dialog (Tier 3b-1) and its
// Add-Player sub-dialog.
//
// Model: the MFC PlayerListDlg is created HIDDEN and stays the model owner -- its working copy
// m_sides, the rename/team fixups, ally-enemy dedup, remove-team sweeps and the
// SidesListUndoable commit are all reused verbatim. The Qt dialog reads state back from the
// hidden dialog's real controls (kept canonical by updateTheUI) and pushes changes by writing
// those controls and invoking the real handlers, via qt* member functions defined in
// src/WBQtPlayerListBridge.cpp. Plain C surface (int / const char* only).
#ifndef WB_QT_PLAYERLIST_BRIDGE_H
#define WB_QT_PLAYERLIST_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// ================= MFC -> Qt (implemented in qt/panels/WBQtPlayerListDialog.cpp) =================

// Run the modal Qt Player List over the frame (disabled for the modal's lifetime). Handles the
// hidden-dialog lifecycle internally. Returns 1 on OK (committed as one undoable), 0 on cancel.
int WBQtPlayerList_Run(void *frameHwnd);

// ================= Qt -> MFC (implemented in src/WBQtPlayerListBridge.cpp) =================

// lifecycle: create/destroy the hidden MFC dialog (accepted!=0 commits before destroying)
void WBQtPlayerListData_Open(void);
void WBQtPlayerListData_Close(int accepted);

// the player list
int  WBQtPlayerListData_GetPlayerCount(void);
void WBQtPlayerListData_GetPlayerLabel(int i, char *buf, int cap);
int  WBQtPlayerListData_GetCurPlayer(void);
void WBQtPlayerList_SelectPlayer(int i);

// current player state (reads the hidden controls, which updateTheUI keeps canonical)
void WBQtPlayerListData_GetName(char *buf, int cap);
void WBQtPlayerListData_GetDisplayName(char *buf, int cap);
int  WBQtPlayerListData_IsNameEditable(void);		// false for (neutral)
int  WBQtPlayerListData_IsComputer(void);
int  WBQtPlayerListData_GetColorRGB(void);			// 0x00RRGGBB swatch color

// faction combo
int  WBQtPlayerListData_GetFactionCount(void);
void WBQtPlayerListData_GetFactionName(int i, char *buf, int cap);
int  WBQtPlayerListData_GetFactionIndex(void);

// color combo
int  WBQtPlayerListData_GetColorCount(void);
void WBQtPlayerListData_GetColorName(int i, char *buf, int cap);
int  WBQtPlayerListData_GetColorIndex(void);

// allies/enemies (rows == the hidden sorted listboxes; same row space for both lists)
int  WBQtPlayerListData_GetOtherCount(void);
void WBQtPlayerListData_GetOtherName(int i, char *buf, int cap);
int  WBQtPlayerListData_GetAllySel(int i);
int  WBQtPlayerListData_GetEnemySel(int i);
int  WBQtPlayerListData_RelationsEnabled(void);

// the read-only regard summaries
int  WBQtPlayerListData_GetRegardCount(int incoming);
void WBQtPlayerListData_GetRegardLine(int incoming, int i, char *buf, int cap);

// button enables (maintained by updateTheUI on the hidden buttons)
int  WBQtPlayerListData_GetRemoveEnabled(void);
int  WBQtPlayerListData_CanAddPlayer(void);

// actions (each writes the hidden control(s) then calls the real handler)
void WBQtPlayerList_SetName(const char *text);
void WBQtPlayerList_SetDisplayName(const char *text);
void WBQtPlayerList_SetIsComputer(int isComputer);
void WBQtPlayerList_SetFaction(const char *name);
void WBQtPlayerList_SetColorIndex(int i);
void WBQtPlayerList_SetColorRGB(int rgb);
// mask: one '0'/'1' per ally/enemy row (the whole selection state in one call)
void WBQtPlayerList_SetRelations(int isEnemy, const char *mask);
void WBQtPlayerList_NewPlayer(const char *factionTemplate);
void WBQtPlayerList_RemovePlayer(void);
void WBQtPlayerList_AddSkirmishPlayers(void);

// --- the Add-Player sub-dialog's template catalog + commit (== AddPlayerDialog) ---
int  WBQtAddPlayerData_GetTemplateCount(void);
void WBQtAddPlayerData_GetTemplateName(int i, char *buf, int cap);
// == AddPlayerDialog::OnOK: adds a player by template to the GLOBAL sides list as its own
// undoable (quirk preserved: the Player List's later commit supersedes it). Returns 1 if the
// template exists.
int  WBQtAddPlayer_Commit(const char *templateName);

#ifdef __cplusplus
}
#endif

#endif // WB_QT_PLAYERLIST_BRIDGE_H
