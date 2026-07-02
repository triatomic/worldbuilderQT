// WBQtPlayerListBridge.cpp -- MFC side of the Qt Player List seam (Tier 3b-1). Plain MFC TU
// (no Qt include). Defines PlayerListDlg's qt* member functions here (member functions may be
// defined in any TU): the dialog is created HIDDEN as the model owner, the Qt dialog writes
// its real controls and calls its real handlers, so the working-copy m_sides model, the
// rename/team fixups, the ally-enemy dedup and the SidesListUndoable commit are reused
// verbatim. Whole body guarded by RTS_HAS_QT so the OFF build compiles it to an empty object.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "CUndoable.h"
#include "playerlistdlg.h"
#include "Common/WellKnownKeys.h"
#include "Common/PlayerTemplate.h"
#include "Common/UnicodeString.h"
#include "GameLogic/SidesList.h"
#include "qt/panels/WBQtPlayerListBridge.h"

#ifdef RTS_HAS_QT

static PlayerListDlg *s_qtDlg = NULL;
static Int s_qtPrevCurPlyr = 0;	// remembers the selected player across sessions (== thePrevCurPlyr)

static void copyOut(const char *str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str ? str : "", cap - 1);
	buf[cap - 1] = 0;
}

// ================= PlayerListDlg qt* member functions =================

PlayerListDlg *PlayerListDlg::qtInstance(void)
{
	return s_qtDlg;
}

PlayerListDlg *PlayerListDlg::qtOpen(void)
{
	if (s_qtDlg == NULL)
	{
		s_qtDlg = new PlayerListDlg();
		// Create() runs OnInitDialog (m_sides = *TheSidesList, control setup); the template
		// has no WS_VISIBLE, so the dialog stays hidden.
		s_qtDlg->Create(PlayerListDlg::IDD, AfxGetMainWnd());
		// Restore the last-selected player (the MFC static that does this is file-local to
		// playerlistdlg.cpp, so the Qt path keeps its own).
		s_qtDlg->m_curPlayerIdx = s_qtPrevCurPlyr;
		s_qtDlg->updateTheUI();
	}
	return s_qtDlg;
}

void PlayerListDlg::qtClose(int accepted)
{
	if (s_qtDlg != NULL)
	{
		if (accepted != 0)
		{
			s_qtDlg->qtCommit();
		}
		s_qtDlg->DestroyWindow();
		delete s_qtDlg;
		s_qtDlg = NULL;
	}
}

void PlayerListDlg::qtCommit(void)
{
	// == OnOK minus the modal close: validate + commit the working copy as one undoable.
	Bool modified = m_sides.validateSides();
	(void)modified;
	DEBUG_ASSERTLOG(!modified,("had to clean up sides in PlayerListDlg::qtCommit"));

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	SidesListUndoable *pUndo = new SidesListUndoable(m_sides, pDoc);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.

	s_qtPrevCurPlyr = m_curPlayerIdx;
}

int PlayerListDlg::qtListCount(int ctrlId)
{
	CListBox *list = (CListBox *)GetDlgItem(ctrlId);
	return (list != NULL) ? list->GetCount() : 0;
}

void PlayerListDlg::qtListText(int ctrlId, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	CListBox *list = (CListBox *)GetDlgItem(ctrlId);
	if (list != NULL && i >= 0 && i < list->GetCount())
	{
		CString text;
		list->GetText(i, text);
		copyOut((LPCTSTR)text, buf, cap);
	}
}

int PlayerListDlg::qtListCurSel(int ctrlId)
{
	CListBox *list = (CListBox *)GetDlgItem(ctrlId);
	return (list != NULL) ? list->GetCurSel() : -1;
}

int PlayerListDlg::qtListGetSel(int ctrlId, int i)
{
	CListBox *list = (CListBox *)GetDlgItem(ctrlId);
	return (list != NULL && list->GetSel(i) > 0) ? 1 : 0;
}

int PlayerListDlg::qtComboCount(int ctrlId)
{
	CComboBox *combo = (CComboBox *)GetDlgItem(ctrlId);
	return (combo != NULL) ? combo->GetCount() : 0;
}

void PlayerListDlg::qtComboText(int ctrlId, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	CComboBox *combo = (CComboBox *)GetDlgItem(ctrlId);
	if (combo != NULL && i >= 0 && i < combo->GetCount())
	{
		CString text;
		combo->GetLBText(i, text);
		copyOut((LPCTSTR)text, buf, cap);
	}
}

int PlayerListDlg::qtComboCurSel(int ctrlId)
{
	CComboBox *combo = (CComboBox *)GetDlgItem(ctrlId);
	return (combo != NULL) ? combo->GetCurSel() : -1;
}

void PlayerListDlg::qtGetEditText(int ctrlId, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	CWnd *ctrl = GetDlgItem(ctrlId);
	if (ctrl != NULL)
	{
		CString text;
		ctrl->GetWindowText(text);
		copyOut((LPCTSTR)text, buf, cap);
	}
}

int PlayerListDlg::qtGetCheck(int ctrlId)
{
	CButton *button = (CButton *)GetDlgItem(ctrlId);
	return (button != NULL && button->GetCheck() == 1) ? 1 : 0;
}

int PlayerListDlg::qtIsCtrlEnabled(int ctrlId)
{
	CWnd *ctrl = GetDlgItem(ctrlId);
	return (ctrl != NULL && ctrl->IsWindowEnabled()) ? 1 : 0;
}

int PlayerListDlg::qtGetColorRGB(void)
{
	return m_colorButton.getColor().getAsInt();
}

int PlayerListDlg::qtCanAddPlayer(void)
{
	// == OnNewplayer's guard (keeps room for an observer).
	return (m_sides.getNumSides() < MAX_PLAYER_COUNT - 1) ? 1 : 0;
}

void PlayerListDlg::qtSelectPlayer(int i)
{
	CListBox *list = (CListBox *)GetDlgItem(IDC_PLAYERS);
	if (list != NULL)
	{
		list->SetCurSel(i);
	}
	OnSelchangePlayers();
}

void PlayerListDlg::qtSetName(const char *text)
{
	CWnd *ctrl = GetDlgItem(IDC_PLAYERNAME);
	if (ctrl != NULL)
	{
		ctrl->SetWindowText(text ? text : "");
	}
	OnChangePlayername();
}

void PlayerListDlg::qtSetDisplayName(const char *text)
{
	CWnd *ctrl = GetDlgItem(IDC_PLAYERDISPLAYNAME);
	if (ctrl != NULL)
	{
		ctrl->SetWindowText(text ? text : "");
	}
	OnChangePlayerdisplayname();
}

void PlayerListDlg::qtSetIsComputer(int isComputer)
{
	CButton *button = (CButton *)GetDlgItem(IDC_PLAYERISCOMPUTER);
	if (button != NULL)
	{
		button->SetCheck(isComputer ? 1 : 0);
	}
	OnPlayeriscomputer();
}

void PlayerListDlg::qtSetFaction(const char *name)
{
	CComboBox *combo = (CComboBox *)GetDlgItem(IDC_PLAYERFACTION);
	if (combo != NULL)
	{
		combo->SelectString(-1, name ? name : "");
	}
	OnEditchangePlayerfaction();
}

void PlayerListDlg::qtSetColorIndex(int i)
{
	CComboBox *combo = (CComboBox *)GetDlgItem(IDC_PlayerColorCombo);
	if (combo != NULL)
	{
		combo->SetCurSel(i);
	}
	OnSelectPlayerColor();
}

void PlayerListDlg::qtSetColorRGB(int rgb)
{
	// == OnColorPress minus the CColorDialog (the Qt side picked the color).
	m_colorButton.setColor(rgb);
	RGBColor color = m_colorButton.getColor();
	m_sides.getSideInfo(m_curPlayerIdx)->getDict()->setInt(TheKey_playerColor, color.getAsInt());
	updateTheUI();
}

void PlayerListDlg::qtSetRelationSel(int isEnemy, const char *mask)
{
	// Push the whole multi-selection state into the hidden listbox, then run the real handler
	// ONCE (it recomputes both dict strings from both listboxes and dedups).
	CListBox *list = (CListBox *)GetDlgItem(isEnemy ? IDC_ENEMIESLIST : IDC_ALLIESLIST);
	if (list == NULL || mask == NULL)
	{
		return;
	}
	int count = list->GetCount();
	int maskLen = (int)strlen(mask);
	for (int i = 0; i < count && i < maskLen; i++)
	{
		list->SetSel(i, mask[i] == '1');
	}
	OnSelchangeAllieslist();
}

void PlayerListDlg::qtNewPlayer(const char *factionTemplate)
{
	// == OnNewplayer minus the AddPlayerDialog pop (the Qt side ran its own picker and
	// committed it; factionTemplate is the picked template's name).
	if (m_sides.getNumSides() >= MAX_PLAYER_COUNT - 1) ///Added -1 so we can always have an observer even for Single player games.
		return;

	AsciiString addedPTName(factionTemplate ? factionTemplate : "");

	AsciiString pname;
	UnicodeString pnameu;
	Int num = 1;
	do {
		pname.format("player%04d",num);
		pnameu.format(L"Player %04d's Display Name",num);
		num++;
	} while (m_sides.findSideInfo(pname));

	Dict newPlayerDict;
	newPlayerDict.setAsciiString(TheKey_playerName, pname);
	newPlayerDict.setBool(TheKey_playerIsHuman, true);
	newPlayerDict.setUnicodeString(TheKey_playerDisplayName, pnameu);
	newPlayerDict.setAsciiString(TheKey_playerFaction, addedPTName);
	newPlayerDict.setAsciiString(TheKey_playerEnemies, AsciiString(""));
	newPlayerDict.setAsciiString(TheKey_playerAllies, AsciiString(""));

	// (the illegal-char scrub lives in a file-static in playerlistdlg.cpp; the generated
	// "playerNNNN" name is always legal, so it is not needed here)
	m_sides.addSide(&newPlayerDict);

	Bool modified = m_sides.validateSides();
	(void)modified;
	DEBUG_ASSERTLOG(!modified,("had to clean up sides in PlayerListDlg::qtNewPlayer"));
	m_curPlayerIdx = m_sides.getNumSides()-1;
	updateTheUI();
}

void PlayerListDlg::qtRemovePlayer(void)
{
	OnRemoveplayer();
}

void PlayerListDlg::qtAddSkirmishPlayers(void)
{
	OnAddskirmishplayers();
}

// ================= the C facade =================

extern "C" void WBQtPlayerListData_Open(void)
{
	PlayerListDlg::qtOpen();
}

extern "C" void WBQtPlayerListData_Close(int accepted)
{
	PlayerListDlg::qtClose(accepted);
}

extern "C" int WBQtPlayerListData_GetPlayerCount(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListCount(IDC_PLAYERS) : 0;
}

extern "C" void WBQtPlayerListData_GetPlayerLabel(int i, char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtListText(IDC_PLAYERS, i, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_GetCurPlayer(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListCurSel(IDC_PLAYERS) : -1;
}

extern "C" void WBQtPlayerList_SelectPlayer(int i)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSelectPlayer(i);
	}
}

extern "C" void WBQtPlayerListData_GetName(char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtGetEditText(IDC_PLAYERNAME, buf, cap);
	}
}

extern "C" void WBQtPlayerListData_GetDisplayName(char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtGetEditText(IDC_PLAYERDISPLAYNAME, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_IsNameEditable(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_PLAYERNAME) : 0;
}

extern "C" int WBQtPlayerListData_IsComputer(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtGetCheck(IDC_PLAYERISCOMPUTER) : 0;
}

extern "C" int WBQtPlayerListData_GetColorRGB(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtGetColorRGB() : 0;
}

extern "C" int WBQtPlayerListData_GetFactionCount(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtComboCount(IDC_PLAYERFACTION) : 0;
}

extern "C" void WBQtPlayerListData_GetFactionName(int i, char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtComboText(IDC_PLAYERFACTION, i, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_GetFactionIndex(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtComboCurSel(IDC_PLAYERFACTION) : -1;
}

extern "C" int WBQtPlayerListData_GetColorCount(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtComboCount(IDC_PlayerColorCombo) : 0;
}

extern "C" void WBQtPlayerListData_GetColorName(int i, char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtComboText(IDC_PlayerColorCombo, i, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_GetColorIndex(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtComboCurSel(IDC_PlayerColorCombo) : -1;
}

extern "C" int WBQtPlayerListData_GetOtherCount(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListCount(IDC_ALLIESLIST) : 0;
}

extern "C" void WBQtPlayerListData_GetOtherName(int i, char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtListText(IDC_ALLIESLIST, i, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_GetAllySel(int i)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListGetSel(IDC_ALLIESLIST, i) : 0;
}

extern "C" int WBQtPlayerListData_GetEnemySel(int i)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListGetSel(IDC_ENEMIESLIST, i) : 0;
}

extern "C" int WBQtPlayerListData_RelationsEnabled(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_ALLIESLIST) : 0;
}

extern "C" int WBQtPlayerListData_GetRegardCount(int incoming)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtListCount(incoming ? IDC_PLAYER_ATTITUDE_IN : IDC_PLAYER_ATTITUDE_OUT) : 0;
}

extern "C" void WBQtPlayerListData_GetRegardLine(int incoming, int i, char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtListText(incoming ? IDC_PLAYER_ATTITUDE_IN : IDC_PLAYER_ATTITUDE_OUT, i, buf, cap);
	}
}

extern "C" int WBQtPlayerListData_GetRemoveEnabled(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_REMOVEPLAYER) : 0;
}

extern "C" int WBQtPlayerListData_CanAddPlayer(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	return (dlg != NULL) ? dlg->qtCanAddPlayer() : 0;
}

extern "C" void WBQtPlayerList_SetName(const char *text)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetName(text);
	}
}

extern "C" void WBQtPlayerList_SetDisplayName(const char *text)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetDisplayName(text);
	}
}

extern "C" void WBQtPlayerList_SetIsComputer(int isComputer)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetIsComputer(isComputer);
	}
}

extern "C" void WBQtPlayerList_SetFaction(const char *name)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetFaction(name);
	}
}

extern "C" void WBQtPlayerList_SetColorIndex(int i)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetColorIndex(i);
	}
}

extern "C" void WBQtPlayerList_SetColorRGB(int rgb)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetColorRGB(rgb);
	}
}

extern "C" void WBQtPlayerList_SetRelations(int isEnemy, const char *mask)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetRelationSel(isEnemy, mask);
	}
}

extern "C" void WBQtPlayerList_NewPlayer(const char *factionTemplate)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtNewPlayer(factionTemplate);
	}
}

extern "C" void WBQtPlayerList_RemovePlayer(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtRemovePlayer();
	}
}

extern "C" void WBQtPlayerList_AddSkirmishPlayers(void)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtAddSkirmishPlayers();
	}
}

// ================= the Add-Player sub-dialog =================
// == AddPlayerDialog with an empty side filter (the Player List always passes "").

extern "C" int WBQtAddPlayerData_GetTemplateCount(void)
{
	return (ThePlayerTemplateStore != NULL) ? ThePlayerTemplateStore->getPlayerTemplateCount() : 0;
}

extern "C" void WBQtAddPlayerData_GetTemplateName(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (ThePlayerTemplateStore != NULL && i >= 0 && i < ThePlayerTemplateStore->getPlayerTemplateCount())
	{
		const PlayerTemplate* pt = ThePlayerTemplateStore->getNthPlayerTemplate(i);
		if (pt != NULL)
		{
			copyOut(pt->getName().str(), buf, cap);
		}
	}
}

extern "C" int WBQtAddPlayer_Commit(const char *templateName)
{
	// == AddPlayerDialog::OnOK.
	AsciiString name(templateName ? templateName : "");
	const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(name));
	if (pt)
	{
		AsciiString addedSide = pt->getName();
		SidesList newSides = *TheSidesList;
		newSides.addPlayerByTemplate(addedSide);
		Bool modified = newSides.validateSides();
		(void)modified;
		DEBUG_ASSERTLOG(!modified,("had to clean up sides in WBQtAddPlayer_Commit"));

		CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
		SidesListUndoable *pUndo = new SidesListUndoable(newSides, pDoc);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
		return 1;
	}
	return 0;
}

#endif // RTS_HAS_QT
