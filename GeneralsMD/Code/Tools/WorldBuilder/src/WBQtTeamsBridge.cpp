// WBQtTeamsBridge.cpp -- MFC side of the Qt Teams-dialog seam (Tier 3b-2). Plain MFC TU (no Qt
// include). Defines CTeamsDialog's qt* member functions here (member functions may be defined
// in any TU): the dialog is created HIDDEN as the model owner and the Qt dialog drives its real
// controls / real handlers, so the working-copy m_sides, the team surgery, the MFC team
// property sheet and the fix-owner validation are reused verbatim. Whole body guarded by
// RTS_HAS_QT so the OFF build compiles it to an empty object.
#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "CUndoable.h"
#include "teamsdialog.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "GameLogic/AI.h"
#include "GameLogic/Scripts.h"
#include "qt/panels/WBQtPickUnitBridge.h"
#include "Common/WellKnownKeys.h"
#include "Common/DataChunk.h"
#include "Common/FileSystem.h"
#include "GameLogic/SidesList.h"
#include "qt/panels/WBQtTeamsBridge.h"
#include <vector>

#ifdef RTS_HAS_QT

static CTeamsDialog *s_qtDlg = NULL;
static Int s_qtPrevCurTeam = 0;

// ================= De-bridged (windowless) view model -- branch qt-debridge =================
// The Teams dialog window is never Create()d; these statics mirror what updateUI /
// UpdateTeamsList seeded into the hidden player listbox, teams CListCtrl and command buttons,
// rebuilt from m_sides by qtMRefresh(). Copies of the teamsdialog.cpp file-static helpers.

static const char *QTM_NEUTRAL_NAME_STR = "(neutral)";
static const Int QTM_K_LOCAL_TEAMS_VERSION_1 = 1;

namespace
{
	AsciiString qtmPlayerNameForUI(SidesList &sides, int i)
	{
		AsciiString b = sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
		if (b.isEmpty())
		{
			b = QTM_NEUTRAL_NAME_STR;
		}
		return b;
	}

	AsciiString qtmTeamNameForUI(SidesList &sides, int i)
	{
		TeamsInfo *ti = sides.getTeamInfo(i);
		if (sides.isPlayerDefaultTeam(ti))
		{
			AsciiString n;
			n.format("(default team)");
			return n;
		}
		return ti->getDict()->getAsciiString(TheKey_teamName);
	}

	Bool qtmIsPlayerDefaultTeamIndex(SidesList &sides, Int i)
	{
		return sides.isPlayerDefaultTeam(sides.getTeamInfo(i));
	}

	// == the file-local stream wrapper teamsdialog.cpp uses for the export chunk writer.
	class QtmMFCFileOutputStream : public OutputStream
	{
	protected:
		CFile *m_file;
	public:
		QtmMFCFileOutputStream(CFile *pFile) : m_file(pFile) {}
		virtual Int write(const void *pData, Int numBytes)
		{
			Int numBytesWritten = 0;
			try {
				m_file->Write(pData, numBytes);
				numBytesWritten = numBytes;
			} catch(...) {
			}
			return numBytesWritten;
		}
	};
}

struct QtmTeamRow { AsciiString cols[6]; int teamIndex; int selected; };
static int s_qtCurPlayer = -1;	// == the hidden IDC_PLAYER_LIST GetCurSel (starts unselected)
static std::vector<AsciiString> s_qtPlayerNames;
static std::vector<QtmTeamRow> s_qtTeamRows;
static Bool s_qtNewEnabled = false, s_qtDeleteEnabled = false, s_qtCopyEnabled = false, s_qtMoveEnabled = false;

// == updateUI minus the controls: validate + clamp, optionally rebuild the row model
// (REBUILD_TEAMS == UpdateTeamsList's derivation), then the derived button enables.
void CTeamsDialog::qtMRefresh(int rebuildRows)
{
	m_sides.validateSides();

	if (m_curTeam < 0)
	{
		m_curTeam = 0;
	}
	if (m_curTeam >= m_sides.getNumTeams())
	{
		m_curTeam = m_sides.getNumTeams() - 1;
	}

	if (rebuildRows)
	{
		s_qtPlayerNames.clear();
		Int p;
		for (p = 0; p < m_sides.getNumSides(); p++)
		{
			s_qtPlayerNames.push_back(qtmPlayerNameForUI(m_sides, p));
		}

		s_qtTeamRows.clear();
		if (s_qtCurPlayer >= 0 && s_qtCurPlayer < m_sides.getNumSides())
		{
			AsciiString playerName = qtmPlayerNameForUI(m_sides, s_qtCurPlayer);
			Bool selected = false;
			Int i;
			for (i = 0; i < m_sides.getNumTeams(); i++)
			{
				TeamsInfo *ti = m_sides.getTeamInfo(i);
				if (ti->getDict()->getAsciiString(TheKey_teamOwner) == playerName.str())
				{
					QtmTeamRow row;
					Bool exists;
					row.cols[0] = qtmTeamNameForUI(m_sides, i);
					row.cols[1] = ti->getDict()->getAsciiString(TheKey_teamOnCreateScript, &exists);
					row.cols[2] = ti->getDict()->getAsciiString(TheKey_teamProductionCondition, &exists);
					row.cols[3].format("%d", ti->getDict()->getInt(TheKey_teamProductionPriority, &exists));
					row.cols[4] = ti->getDict()->getAsciiString(TheKey_teamHome, &exists);
					row.cols[5].format("%d", i);
					row.teamIndex = i;
					row.selected = 0;
					if (m_curTeam == i)
					{
						selected = true;
						row.selected = 1;
					}
					s_qtTeamRows.push_back(row);
				}
			}
			if (!selected)
			{
				m_curTeam = -1;
				if (!s_qtTeamRows.empty())
				{
					m_curTeam = s_qtTeamRows[0].teamIndex;
					s_qtTeamRows[0].selected = 1;
				}
			}
		}
	}

	// == updateUI's button-enable pass (derived; there are no controls to disable).
	Bool isDefault = true;
	if (m_curTeam >= 0)
	{
		isDefault = qtmIsPlayerDefaultTeamIndex(m_sides, m_curTeam);
	}
	s_qtDeleteEnabled = !isDefault;
	s_qtCopyEnabled = !isDefault;
	s_qtMoveEnabled = !isDefault;
	s_qtNewEnabled = (s_qtCurPlayer > 0);
}	// remembers the selected team across sessions (== thePrevCurTeam)

static void copyOut(const char *str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str ? str : "", cap - 1);
	buf[cap - 1] = 0;
}

// ================= CTeamsDialog qt* member functions =================

CTeamsDialog *CTeamsDialog::qtInstance(void)
{
	return s_qtDlg;
}

CTeamsDialog *CTeamsDialog::qtOpen(void)
{
	if (s_qtDlg == NULL)
	{
		s_qtDlg = new CTeamsDialog();
		// De-bridged (qt-debridge): never Create() the window. Seed the model like
		// OnInitDialog (m_sides copy, fix-owner validation -- which may pop the Qt-seamed
		// modals) and build the view model directly. The player list starts unselected,
		// exactly like the hidden listbox did.
		s_qtDlg->m_updating = 0;
		s_qtDlg->m_sides = *TheSidesList;
		s_qtDlg->m_curTeam = s_qtPrevCurTeam;
		s_qtDlg->m_expanded = TRUE;
		s_qtCurPlayer = -1;
		s_qtDlg->validateTeamOwners();
		s_qtDlg->qtMRefresh(1);
	}
	return s_qtDlg;
}

void CTeamsDialog::qtClose(int accepted)
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

void CTeamsDialog::qtCommit(void)
{
	// == OnOK minus the modal close: preserve the map selection around the one-undoable commit.
	std::vector<Coord3D> selectedPositions;
	for (MapObject* pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (pObj->isSelected())
			selectedPositions.push_back(*pObj->getLocation());
	}

	Bool modified = m_sides.validateSides();
	(void)modified;
	DEBUG_ASSERTLOG(!modified,("had to clean up sides in CTeamsDialog::qtCommit"));

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	SidesListUndoable *pUndo = new SidesListUndoable(m_sides, pDoc);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.

	s_qtPrevCurTeam = m_curTeam;

	for (MapObject* pObjb = MapObject::getFirstMapObject(); pObjb; pObjb = pObjb->getNext()) {
		pObjb->setSelected(false);
		for (std::vector<Coord3D>::size_type i = 0; i < selectedPositions.size(); ++i) {
			if (*pObjb->getLocation() == selectedPositions[i]) {
				pObjb->setSelected(true);
				break; // no need to check the rest
			}
		}
	}
}

int CTeamsDialog::qtPlayerCount(void)
{
	return (int)s_qtPlayerNames.size();
}

void CTeamsDialog::qtPlayerName(int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (i >= 0 && i < (int)s_qtPlayerNames.size())
	{
		copyOut(s_qtPlayerNames[i].str(), buf, cap);
	}
}

int CTeamsDialog::qtPlayerCurSel(void)
{
	return s_qtCurPlayer;
}

void CTeamsDialog::qtSelectPlayer(int i)
{
	// == OnSelchangePlayerList minus the listbox: refilter the team rows for this player.
	s_qtCurPlayer = i;
	qtMRefresh(1);
}

int CTeamsDialog::qtTeamRowCount(void)
{
	return (int)s_qtTeamRows.size();
}

void CTeamsDialog::qtTeamRowText(int row, int col, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (row >= 0 && row < (int)s_qtTeamRows.size() && col >= 0 && col < 6)
	{
		copyOut(s_qtTeamRows[row].cols[col].str(), buf, cap);
	}
}

int CTeamsDialog::qtTeamRowSelected(int row)
{
	if (row >= 0 && row < (int)s_qtTeamRows.size())
	{
		return s_qtTeamRows[row].selected;
	}
	return 0;
}

void CTeamsDialog::qtSelectTeamRow(int row)
{
	if (row < 0 || row >= (int)s_qtTeamRows.size())
	{
		return;
	}
	// == a click on the row: only the selection + button enables change (the per-click
	// light path from the Teams-lag fix, now fully model-side).
	for (size_t i = 0; i < s_qtTeamRows.size(); i++)
	{
		s_qtTeamRows[i].selected = 0;
	}
	s_qtTeamRows[row].selected = 1;
	m_curTeam = s_qtTeamRows[row].teamIndex;
	qtMRefresh(0);
}

int CTeamsDialog::qtIsCtrlEnabled(int ctrlId)
{
	switch (ctrlId)
	{
		case IDC_NEWTEAM:      return s_qtNewEnabled ? 1 : 0;
		case IDC_DELETETEAM:   return s_qtDeleteEnabled ? 1 : 0;
		case IDC_COPYTEAM:     return s_qtCopyEnabled ? 1 : 0;
		case IDC_MOVEUPTEAM:
		case IDC_MOVEDOWNTEAM: return s_qtMoveEnabled ? 1 : 0;
		default:               return 1;
	}
}

void CTeamsDialog::qtNewTeam(void)
{
	// == OnNewteam minus the OnEditTemplate pop: the Qt side opens the Qt team sheet on the
	// new team after this returns.
	Int num = 1;
	AsciiString tname;
	do
	{
		tname.format("team%04d",num++);
	}
	while (m_sides.findTeamInfo(tname));

	AsciiString oname = m_sides.getTeamInfo(m_curTeam)->getDict()->getAsciiString(TheKey_teamOwner);

	Dict d;
	d.setAsciiString(TheKey_teamName, tname);
	d.setAsciiString(TheKey_teamOwner, oname);	// owned by the parent of whatever is selected.
	d.setBool(TheKey_teamIsSingleton, false);

	m_sides.addTeam(&d);
	Int i;
	if (m_sides.findTeamInfo(tname, &i)) {
		m_curTeam = i;
	}
	qtMRefresh(1);
}

void *CTeamsDialog::qtCurTeamDict(void)
{
	if (m_curTeam >= 0 && m_curTeam < m_sides.getNumTeams())
	{
		return m_sides.getTeamInfo(m_curTeam)->getDict();
	}
	return NULL;
}

void *CTeamsDialog::qtSides(void)
{
	return &m_sides;
}

int CTeamsDialog::qtCurTeamIsDefault(void)
{
	if (m_curTeam >= 0 && m_curTeam < m_sides.getNumTeams())
	{
		return m_sides.isPlayerDefaultTeam(m_sides.getTeamInfo(m_curTeam)) ? 1 : 0;
	}
	return 1;	// nothing selected -> treat as un-editable
}

void CTeamsDialog::qtDeleteTeam(void)
{
	// == OnDeleteteam minus the updateUI tail.
	if (m_curTeam < 0)
	{
		return;
	}
	if (qtmIsPlayerDefaultTeamIndex(m_sides, m_curTeam))
	{
		return;	// should not be allowed (button disabled)
	}
	AsciiString tname = m_sides.getTeamInfo(m_curTeam)->getDict()->getAsciiString(TheKey_teamName);
	Int count = MapObject::countMapObjectsWithOwner(tname);
	if (count > 0)
	{
		CString msg;
		msg.Format(IDS_REMOVING_INUSE_TEAM, count);
		if (::AfxMessageBox(msg, MB_YESNO) == IDNO)
		{
			return;
		}
	}
	m_sides.removeTeam(m_curTeam);
	qtMRefresh(1);
}

void CTeamsDialog::qtCopyTeam(void)
{
	// == OnCopyteam minus the updateUI tail.
	Dict d = *m_sides.getTeamInfo(m_curTeam)->getDict();
	AsciiString origName = d.getAsciiString(TheKey_teamName);
	Int num = 1;
	AsciiString tname;
	do
	{
		tname.format("%s.%2d", origName.str(), num++);
	}
	while (m_sides.findTeamInfo(tname));
	d.setAsciiString(TheKey_teamName, tname);
	m_sides.addTeam(&d);
	qtMRefresh(1);
}

void CTeamsDialog::qtEditTeam(void)
{
	// De-bridged: the Qt team sheet (WBQtTeamSheet_Open) IS the editor now; the MFC
	// property-sheet path would need the window. This facade entry is unused by the Qt
	// dialog and intentionally does nothing.
}

void CTeamsDialog::qtSelectTeamMembers(void)
{
	OnSelectTeamMembers();	// pure m_sides + map walk + info box; no controls
}

void CTeamsDialog::qtMoveUpTeam(void)
{
	// == OnMoveUpTeam with findPrevTeamIndex resolved against the view rows instead of the
	// hidden list's col-5 walk (the rows carry the same real team indexes).
	if (m_curTeam <= 0)
	{
		return;
	}
	int prevIndex = -1;
	for (size_t i = 1; i < s_qtTeamRows.size(); i++)
	{
		if (s_qtTeamRows[i].teamIndex == m_curTeam)
		{
			prevIndex = s_qtTeamRows[i - 1].teamIndex;
			break;
		}
	}
	if (prevIndex < 0)
	{
		return;
	}
	Dict *currentTeam = m_sides.getTeamInfo(m_curTeam)->getDict();
	Dict *prevTeam = m_sides.getTeamInfo(prevIndex)->getDict();
	std::swap(*currentTeam, *prevTeam);
	m_curTeam = prevIndex;
	qtMRefresh(1);
}

void CTeamsDialog::qtMoveDownTeam(void)
{
	// == OnMoveDownTeam, view-row resolved.
	if (m_curTeam >= m_sides.getNumTeams() - 1)
	{
		return;
	}
	int nextIndex = -1;
	for (size_t i = 0; i + 1 < s_qtTeamRows.size(); i++)
	{
		if (s_qtTeamRows[i].teamIndex == m_curTeam)
		{
			nextIndex = s_qtTeamRows[i + 1].teamIndex;
			break;
		}
	}
	if (nextIndex < 0 || nextIndex >= m_sides.getNumTeams())
	{
		return;
	}
	Dict *currentTeam = m_sides.getTeamInfo(m_curTeam)->getDict();
	Dict *nextTeam = m_sides.getTeamInfo(nextIndex)->getDict();
	std::swap(*currentTeam, *nextTeam);
	m_curTeam = nextIndex;
	qtMRefresh(1);
}

void CTeamsDialog::qtExportTeams(void)
{
	// == OnExportTeams minus the player-listbox read (the view model holds the selection).
	Int selectedPlayer = s_qtCurPlayer;
	if (selectedPlayer < 1)
	{
		AfxMessageBox("Please select a valid player first!", MB_OK | MB_ICONWARNING);
		return;
	}
	AsciiString selectedPlayerName = qtmPlayerNameForUI(m_sides, selectedPlayer);

	CFileDialog dlg(FALSE, ".teams", "exportedteams.teams",
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Team Export (*.teams)|*.teams||");
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}
	CString path = dlg.GetPathName();
	try {
		CFile f(path,
			CFile::modeCreate | CFile::modeWrite |
			CFile::shareDenyWrite | CFile::typeBinary);
		QtmMFCFileOutputStream stream(&f);
		DataChunkOutput out(&stream);
		out.openDataChunk("ScriptTeams", QTM_K_LOCAL_TEAMS_VERSION_1);
		int exportedCount = 0;
		for (int i = 0; i < m_sides.getNumTeams(); i++)
		{
			TeamsInfo *ti = m_sides.getTeamInfo(i);
			if (!ti) { continue; }
			if (m_sides.isPlayerDefaultTeam(ti)) { continue; }
			Dict *d = ti->getDict();
			if (!d) { continue; }
			AsciiString teamOwner = d->getAsciiString(TheKey_teamOwner);
			if (teamOwner == selectedPlayerName.str())
			{
				out.writeDict(*d);
				exportedCount++;
			}
		}
		out.closeDataChunk();
		if (exportedCount == 0)
		{
			AfxMessageBox("No teams found for the selected player!", MB_OK | MB_ICONWARNING);
		}
		else
		{
			CString msg;
			msg.Format("Successfully exported %d team(s) from player '%s' to:\n%s",
				exportedCount, selectedPlayerName.str(), path);
			AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
		}
	} catch(...) {
		AfxMessageBox("Error writing team export file!", MB_OK | MB_ICONERROR);
	}
}

void CTeamsDialog::qtImportTeams(void)
{
	// == OnImportTeams minus the player-listbox read and the updateUI tail.
	Int selectedPlayer = s_qtCurPlayer;
	if (selectedPlayer < 1)
	{
		AfxMessageBox("Please select a valid player first!", MB_OK | MB_ICONWARNING);
		return;
	}
	m_importTargetPlayer = qtmPlayerNameForUI(m_sides, selectedPlayer);

	CFileDialog dlg(TRUE, ".teams", NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		"Team Export (*.teams)|*.teams||");
	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}
	CString path = dlg.GetPathName();
	try {
		CachedFileInputStream in;
		if (!in.open(AsciiString(path)))
		{
			AfxMessageBox("Could not open team file!", MB_OK | MB_ICONERROR);
			return;
		}
		DataChunkInput reader(&in);
		reader.registerParser(
			AsciiString("ScriptTeams"),
			AsciiString::TheEmptyString,
			ParseTeamsDataChunk
		);
		if (!reader.parse(this))
		{
			AfxMessageBox("Error parsing team export file!\nFile may be corrupt or incompatible.",
				MB_OK | MB_ICONERROR);
			return;
		}
		validateTeamOwners();
		qtMRefresh(1);
		AfxMessageBox("Teams imported successfully!", MB_OK | MB_ICONINFORMATION);
	} catch(...) {
		AfxMessageBox("Exception occurred while importing teams!", MB_OK | MB_ICONERROR);
	}
}

// ================= the C facade =================

extern "C" void WBQtTeamsData_Open(void)
{
	CTeamsDialog::qtOpen();
}

extern "C" void WBQtTeamsData_Close(int accepted)
{
	CTeamsDialog::qtClose(accepted);
}

extern "C" int WBQtTeamsData_GetPlayerCount(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtPlayerCount() : 0;
}

extern "C" void WBQtTeamsData_GetPlayerName(int i, char *buf, int cap)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtPlayerName(i, buf, cap);
	}
}

extern "C" int WBQtTeamsData_GetPlayerIndex(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtPlayerCurSel() : -1;
}

extern "C" void WBQtTeams_SelectPlayer(int i)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSelectPlayer(i);
	}
}

extern "C" int WBQtTeamsData_GetTeamRowCount(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtTeamRowCount() : 0;
}

extern "C" void WBQtTeamsData_GetTeamRowText(int row, int col, char *buf, int cap)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtTeamRowText(row, col, buf, cap);
	}
}

extern "C" int WBQtTeamsData_GetTeamRowSelected(int row)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtTeamRowSelected(row) : 0;
}

extern "C" void WBQtTeams_SelectTeamRow(int row)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSelectTeamRow(row);
	}
}

extern "C" int WBQtTeamsData_GetNewEnabled(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_NEWTEAM) : 0;
}

extern "C" int WBQtTeamsData_GetDeleteEnabled(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_DELETETEAM) : 0;
}

extern "C" int WBQtTeamsData_GetCopyEnabled(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_COPYTEAM) : 0;
}

extern "C" int WBQtTeamsData_GetMoveEnabled(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtIsCtrlEnabled(IDC_MOVEUPTEAM) : 0;
}

extern "C" void WBQtTeams_NewTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtNewTeam();
	}
}

extern "C" void WBQtTeams_DeleteTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtDeleteTeam();
	}
}

extern "C" void WBQtTeams_CopyTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtCopyTeam();
	}
}

extern "C" void WBQtTeams_EditTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtEditTeam();
	}
}

extern "C" void WBQtTeams_SelectTeamMembers(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSelectTeamMembers();
	}
}

extern "C" void WBQtTeams_MoveUpTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMoveUpTeam();
	}
}

extern "C" void WBQtTeams_MoveDownTeam(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMoveDownTeam();
	}
}

extern "C" void WBQtTeams_ExportTeams(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtExportTeams();
	}
}

extern "C" void WBQtTeams_ImportTeams(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtImportTeams();
	}
}

// ================= the Qt team property sheet (Tier 3b-3) =================
// Four HIDDEN Team* pages (they are CDialogs) bound to the current team's dict in the hidden
// CTeamsDialog's working copy. Their OnInitDialog seeds the controls and every handler writes
// the dict live, so the Qt sheet drives them generically: write the control, then send the
// real WM_COMMAND notification -- the page's own message map dispatches to the right handler.
// The unit-pick "..." buttons still pop the MFC PickUnitDialog (owner = the active Qt sheet).

//----------------------------------------------------------------------------------------
// The team property sheet, de-bridged (T3) -- branch qt-debridge. The four hidden Team*
// page windows are GONE: WBQtTeamSheet_Open captures the current team's working-copy Dict
// (+ the sides list for rename validation), builds the combo catalogs model-side (the same
// enumerations the pages' OnInitDialog ran through EditParameter::load*), and the
// WBQtTeamPage_* facade below is a table-driven dict layer keyed by ctrlId: gets derive
// from the dict exactly like the pages' seeds, sets replicate the handlers' dict writes.
// The Qt sheet (WBQtTeamSheetDialog.cpp) is unchanged. Deliberate deviations from MFC
// bugs: selecting a "[???] x" placeholder re-writes the ORIGINAL value (the MFC handlers
// stored the prefixed text), and picking <none> for a unit type clears the key (MFC stored
// the literal "<none>").
//----------------------------------------------------------------------------------------

static Dict *s_qtSheetDict = NULL;
static SidesList *s_qtSheetSides = NULL;
static Bool s_qtSheetDeployBy = false;	// the reinforcement transport gate

static std::vector<AsciiString> s_qtCatWaypointsId;	// Identity home-waypoint combo
static std::vector<AsciiString> s_qtCatWaypointsRe;	// Reinforcement combo ("Waypoint" RC pre-item)
static std::vector<AsciiString> s_qtCatScripts;		// subroutine scripts + <none>, sorted
static std::vector<AsciiString> s_qtCatScriptsGen;	// TeamGeneric: <none> FIRST, then sorted
static std::vector<AsciiString> s_qtCatOwners;		// player display names, sorted
static std::vector<AsciiString> s_qtCatUnits;		// every template + <none>, sorted
static std::vector<AsciiString> s_qtCatTransports;	// "Helicopter" RC pre-item + transports + <none>
static std::vector<AsciiString> s_qtCatVeterancy;	// fixed RC items
static std::vector<AsciiString> s_qtCatInteractions;	// fixed RC items

namespace
{
	bool qtmStrLess(const AsciiString &a, const AsciiString &b)
	{
		return _stricmp(a.str(), b.str()) < 0;	// == the CBS_SORT order
	}

	int qtmCatFind(const std::vector<AsciiString> &v, const AsciiString &s)
	{
		for (size_t i = 0; i < v.size(); i++)
		{
			if (v[i] == s)
			{
				return (int)i;
			}
		}
		return -1;
	}

	// == EditParameter::loadWaypoints' walk (the loader fills a CComboBox).
	void qtmCollectWaypoints(std::vector<AsciiString> &out)
	{
		for (MapObject *pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
		{
			if (pMapObj->isWaypoint())
			{
				AsciiString wayName = pMapObj->getWaypointName();
				if (qtmCatFind(out, wayName) < 0)
				{
					out.push_back(wayName);
				}
			}
		}
	}

	// == EditParameter::loadScripts(pCombo, true)'s walk (subroutines only; the sheet opens
	// with the script editor closed, so the source is TheSidesList like the loader's default).
	void qtmCollectSubroutineScripts(std::vector<AsciiString> &out)
	{
		SidesList *sides = TheSidesList;
		for (Int i = 0; i < sides->getNumSides(); i++)
		{
			ScriptList *pSL = sides->getSideInfo(i)->getScriptList();
			if (pSL == NULL)
			{
				continue;
			}
			Script *pScr;
			for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext())
			{
				if (!pScr->isSubroutine())
				{
					continue;
				}
				out.push_back(pScr->getName());
			}
			ScriptGroup *pGroup;
			for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext())
			{
				if (pGroup->isSubroutine())
				{
					out.push_back(pGroup->getName());
				}
				for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext())
				{
					if (!pScr->isSubroutine())
					{
						continue;
					}
					out.push_back(pScr->getName());
				}
			}
		}
	}

	// The ctrlId -> dict key tables.
	// NOTE: the RC reuses control-id VALUES across the four page templates (e.g.
	// IDC_TEAM_NAME == IDC_BASE_DEFENSE), so every lookup dispatches on the PAGE first.
	NameKeyType qtmCheckKey(int page, int ctrlId)
	{
		if (page == WB_QT_TEAMPAGE_IDENTITY)
		{
			switch (ctrlId)
			{
				case IDC_AUTO_REINFORCE:            return TheKey_teamAutoReinforce;
				case IDC_AI_RECRUITABLE:            return TheKey_teamIsAIRecruitable;
				case IDC_TEAM_SINGLETON:            return TheKey_teamIsSingleton;
				case IDC_PRODUCTION_EXECUTEACTIONS: return TheKey_teamExecutesActionsOnCreate;
				default:                            break;
			}
		}
		else if (page == WB_QT_TEAMPAGE_REINFORCEMENT)
		{
			switch (ctrlId)
			{
				case IDC_TEAM_STARTS_FULL: return TheKey_teamStartsFull;
				case IDC_TRANSPORTS_EXIT:  return TheKey_teamTransportsExit;
				default:                   break;
			}
		}
		else if (page == WB_QT_TEAMPAGE_BEHAVIOR)
		{
			switch (ctrlId)
			{
				case IDC_TRANSPORTS_RETURN:    return TheKey_teamTransportsReturn;
				case IDC_AVOID_THREATS:        return TheKey_teamAvoidThreats;
				case IDC_PERIMETER_DEFENSE:    return TheKey_teamIsPerimeterDefense;
				case IDC_BASE_DEFENSE:         return TheKey_teamIsBaseDefense;
				case IDC_ATTACK_COMMON_TARGET: return TheKey_teamAttackCommonTarget;
				default:                       break;
			}
		}
		return NAMEKEY_INVALID;
	}

	// string combos whose <none> row maps to an EMPTY dict value (== the page handlers).
	NameKeyType qtmNoneComboKey(int page, int ctrlId)
	{
		if (page == WB_QT_TEAMPAGE_IDENTITY)
		{
			switch (ctrlId)
			{
				case IDC_HOME_WAYPOINT:        return TheKey_teamHome;
				case IDC_PRODUCTION_CONDITION: return TheKey_teamProductionCondition;
				default:                       break;
			}
		}
		else if (page == WB_QT_TEAMPAGE_REINFORCEMENT)
		{
			switch (ctrlId)
			{
				case IDC_TRANSPORT_COMBO: return TheKey_teamTransport;
				case IDC_WAYPOINT_COMBO:  return TheKey_teamReinforcementOrigin;
				default:                  break;
			}
		}
		else if (page == WB_QT_TEAMPAGE_BEHAVIOR)
		{
			switch (ctrlId)
			{
				case IDC_ON_CREATE_SCRIPT:         return TheKey_teamOnCreateScript;
				case IDC_ON_IDLE_SCRIPT:           return TheKey_teamOnIdleScript;
				case IDC_ON_ENEMY_SIGHTED:         return TheKey_teamEnemySightedScript;
				case IDC_ON_DESTROYED:             return TheKey_teamOnDestroyedScript;
				case IDC_ON_ALL_CLEAR:             return TheKey_teamAllClearScript;
				case IDC_ON_UNIT_DESTROYED_SCRIPT: return TheKey_teamOnUnitDestroyedScript;
				default:                           break;
			}
		}
		return NAMEKEY_INVALID;
	}

	int qtmUnitSlot(int ctrlId)	// unit-type combo -> slot 0..6, else -1
	{
		switch (ctrlId)
		{
			case IDC_UNIT_TYPE1: return 0;
			case IDC_UNIT_TYPE2: return 1;
			case IDC_UNIT_TYPE3: return 2;
			case IDC_UNIT_TYPE4: return 3;
			case IDC_UNIT_TYPE5: return 4;
			case IDC_UNIT_TYPE6: return 5;
			case IDC_UNIT_TYPE7: return 6;
			default:             return -1;
		}
	}

	int qtmUnitButtonSlot(int ctrlId)
	{
		switch (ctrlId)
		{
			case IDC_UNIT_TYPE1_BUTTON: return 0;
			case IDC_UNIT_TYPE2_BUTTON: return 1;
			case IDC_UNIT_TYPE3_BUTTON: return 2;
			case IDC_UNIT_TYPE4_BUTTON: return 3;
			case IDC_UNIT_TYPE5_BUTTON: return 4;
			case IDC_UNIT_TYPE6_BUTTON: return 5;
			case IDC_UNIT_TYPE7_BUTTON: return 6;
			default:                    return -1;
		}
	}

	NameKeyType qtmUnitTypeKey(int slot)
	{
		switch (slot)
		{
			case 0:  return TheKey_teamUnitType1;
			case 1:  return TheKey_teamUnitType2;
			case 2:  return TheKey_teamUnitType3;
			case 3:  return TheKey_teamUnitType4;
			case 4:  return TheKey_teamUnitType5;
			case 5:  return TheKey_teamUnitType6;
			default: return TheKey_teamUnitType7;
		}
	}

	int qtmCountSlot(int ctrlId, NameKeyType *keyOut)	// min/max unit-count edits
	{
		switch (ctrlId)
		{
			case IDC_MIN_UNIT1: *keyOut = TheKey_teamUnitMinCount1; return 1;
			case IDC_MAX_UNIT1: *keyOut = TheKey_teamUnitMaxCount1; return 1;
			case IDC_MIN_UNIT2: *keyOut = TheKey_teamUnitMinCount2; return 1;
			case IDC_MAX_UNIT2: *keyOut = TheKey_teamUnitMaxCount2; return 1;
			case IDC_MIN_UNIT3: *keyOut = TheKey_teamUnitMinCount3; return 1;
			case IDC_MAX_UNIT3: *keyOut = TheKey_teamUnitMaxCount3; return 1;
			case IDC_MIN_UNIT4: *keyOut = TheKey_teamUnitMinCount4; return 1;
			case IDC_MAX_UNIT4: *keyOut = TheKey_teamUnitMaxCount4; return 1;
			case IDC_MIN_UNIT5: *keyOut = TheKey_teamUnitMinCount5; return 1;
			case IDC_MAX_UNIT5: *keyOut = TheKey_teamUnitMaxCount5; return 1;
			case IDC_MIN_UNIT6: *keyOut = TheKey_teamUnitMinCount6; return 1;
			case IDC_MAX_UNIT6: *keyOut = TheKey_teamUnitMaxCount6; return 1;
			case IDC_MIN_UNIT7: *keyOut = TheKey_teamUnitMinCount7; return 1;
			case IDC_MAX_UNIT7: *keyOut = TheKey_teamUnitMaxCount7; return 1;
			default: return 0;
		}
	}

	int qtmGenericSlot(int ctrlId)	// TeamGeneric script combo -> slot 0..15, else -1
	{
		static const int ids[16] = {
			IDC_TeamGeneric_Script1, IDC_TeamGeneric_Script2, IDC_TeamGeneric_Script3,
			IDC_TeamGeneric_Script4, IDC_TeamGeneric_Script5, IDC_TeamGeneric_Script6,
			IDC_TeamGeneric_Script7, IDC_TeamGeneric_Script8, IDC_TeamGeneric_Script9,
			IDC_TeamGeneric_Script10, IDC_TeamGeneric_Script11, IDC_TeamGeneric_Script12,
			IDC_TeamGeneric_Script13, IDC_TeamGeneric_Script14, IDC_TeamGeneric_Script15,
			IDC_TeamGeneric_Script16 };
		for (int i = 0; i < 16; i++)
		{
			if (ids[i] == ctrlId)
			{
				return i;
			}
		}
		return -1;
	}

	AsciiString qtmGenericHookKeyName(int slot)
	{
		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_teamGenericScriptHook).str(), slot);
		return keyName;
	}

	// TeamGeneric writes rebuild the whole compacted hook chain (== _scriptsToDict): keep the
	// 16 slot values here, seeded at sheet-open.
	AsciiString s_qtGenericSlots[16];

	void qtmGenericRebuildDict(void)
	{
		// == _scriptsToDict: pack the non-empty slots into a contiguous hook chain. Compact the
		// slot buffer IN PLACE too (so index i is display row i), matching how _dictToScripts
		// reads them back -- clearing slot 2 of 3 then slides slot 3 up into slot 2.
		AsciiString compact[16];
		int scriptNum = 0;
		int i;
		for (i = 0; i < 16; i++)
		{
			if (!s_qtGenericSlots[i].isEmpty())
			{
				compact[scriptNum] = s_qtGenericSlots[i];
				++scriptNum;
			}
		}
		for (i = 0; i < 16; i++)
		{
			s_qtGenericSlots[i] = (i < scriptNum) ? compact[i] : AsciiString::TheEmptyString;
		}

		for (i = 0; i < scriptNum; i++)
		{
			s_qtSheetDict->setAsciiString(NAMEKEY(qtmGenericHookKeyName(i)), s_qtGenericSlots[i]);
		}
		for ( ; i < 16; i++)
		{
			AsciiString keyName = qtmGenericHookKeyName(i);
			if (s_qtSheetDict->known(NAMEKEY(keyName), Dict::DICT_ASCIISTRING))
			{
				s_qtSheetDict->remove(NAMEKEY(keyName));
			}
		}
	}

	// The displayed text for a none-mapped string combo (== what the seeded hidden combo's
	// selection read back): dict value; empty/absent -> <none>; not in catalog -> "[???] v".
	void qtmComboDisplay(const std::vector<AsciiString> &cat, const AsciiString &value, char *buf, int cap)
	{
		if (value.isEmpty())
		{
			copyOut(NONE_STRING, buf, cap);
			return;
		}
		if (qtmCatFind(cat, value) >= 0)
		{
			copyOut(value.str(), buf, cap);
			return;
		}
		AsciiString missing;
		missing.format("[???] %s", value.str());
		copyOut(missing.str(), buf, cap);
	}

	// Like qtmComboDisplay, but a value that isn't in the catalog shows as <none> instead of the
	// "[???] value" placeholder. == TeamReinforcement::OnInitDialog's transport combo, which (unlike
	// the other combos) never adds a [???] row: a stored transport not found by FindStringExact just
	// selects <none>.
	void qtmComboDisplayNoPlaceholder(const std::vector<AsciiString> &cat, const AsciiString &value, char *buf, int cap)
	{
		if (value.isEmpty() || qtmCatFind(cat, value) < 0)
		{
			copyOut(NONE_STRING, buf, cap);
			return;
		}
		copyOut(value.str(), buf, cap);
	}

	// Resolve an incoming combo selection back to the raw value ("<none>" -> empty,
	// "[???] x" -> x).
	AsciiString qtmComboValueFromText(const char *text)
	{
		AsciiString v(text ? text : "");
		if (v == NONE_STRING)
		{
			v.clear();
		}
		else if (strncmp(v.str(), "[???] ", 6) == 0)
		{
			v = AsciiString(v.str() + 6);
		}
		return v;
	}
}

extern "C" int WBQtTeamSheet_Open(void)
{
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg == NULL || s_qtSheetDict != NULL)
	{
		return 0;
	}
	if (dlg->qtCurTeamIsDefault() != 0)
	{
		return 0;	// default teams have no editable template (== the double-click guard)
	}
	Dict *teamDict = static_cast<Dict *>(dlg->qtCurTeamDict());
	SidesList *sides = static_cast<SidesList *>(dlg->qtSides());
	if (teamDict == NULL)
	{
		return 0;
	}

	// De-bridged (T3): no page windows -- capture the dict + sides and build the catalogs
	// the pages' OnInitDialog used to load into their combos.
	s_qtSheetDict = teamDict;
	s_qtSheetSides = sides;

	Bool exists;

	s_qtCatWaypointsId.clear();
	qtmCollectWaypoints(s_qtCatWaypointsId);
	s_qtCatWaypointsId.push_back(AsciiString(NONE_STRING));
	std::sort(s_qtCatWaypointsId.begin(), s_qtCatWaypointsId.end(), qtmStrLess);

	s_qtCatWaypointsRe.clear();
	s_qtCatWaypointsRe.push_back(AsciiString("Waypoint"));	// the RC template pre-item
	qtmCollectWaypoints(s_qtCatWaypointsRe);
	s_qtCatWaypointsRe.push_back(AsciiString(NONE_STRING));
	std::sort(s_qtCatWaypointsRe.begin(), s_qtCatWaypointsRe.end(), qtmStrLess);

	s_qtCatScripts.clear();
	qtmCollectSubroutineScripts(s_qtCatScripts);
	s_qtCatScripts.push_back(AsciiString(NONE_STRING));
	std::sort(s_qtCatScripts.begin(), s_qtCatScripts.end(), qtmStrLess);

	// TeamGeneric inserted <none> at row 0 (InsertString bypasses the sort).
	s_qtCatScriptsGen.clear();
	qtmCollectSubroutineScripts(s_qtCatScriptsGen);
	std::sort(s_qtCatScriptsGen.begin(), s_qtCatScriptsGen.end(), qtmStrLess);
	s_qtCatScriptsGen.insert(s_qtCatScriptsGen.begin(), AsciiString(NONE_STRING));

	s_qtCatOwners.clear();
	{
		for (Int i = 0; i < TheSidesList->getNumSides(); i++)
		{
			AsciiString name = TheSidesList->getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
			if (name.isEmpty())
			{
				name = QTM_NEUTRAL_NAME_STR;
			}
			s_qtCatOwners.push_back(name);
		}
		std::sort(s_qtCatOwners.begin(), s_qtCatOwners.end(), qtmStrLess);
	}

	s_qtCatUnits.clear();
	{
		const ThingTemplate *tTemplate;
		for (tTemplate = TheThingFactory->firstTemplate();
				tTemplate;
				tTemplate = tTemplate->friend_getNextTemplate())
		{
			s_qtCatUnits.push_back(tTemplate->getName());
		}
		s_qtCatUnits.push_back(AsciiString(NONE_STRING));
		std::sort(s_qtCatUnits.begin(), s_qtCatUnits.end(), qtmStrLess);
	}

	s_qtCatTransports.clear();
	{
		s_qtCatTransports.push_back(AsciiString("Helicopter"));	// the RC template pre-item
		const ThingTemplate *tTemplate;
		for (tTemplate = TheThingFactory->firstTemplate();
				tTemplate;
				tTemplate = tTemplate->friend_getNextTemplate())
		{
			if (tTemplate->isKindOf(KINDOF_TRANSPORT))
			{
				s_qtCatTransports.push_back(tTemplate->getName());
			}
		}
		s_qtCatTransports.push_back(AsciiString(NONE_STRING));
		std::sort(s_qtCatTransports.begin(), s_qtCatTransports.end(), qtmStrLess);
	}

	s_qtCatVeterancy.clear();
	s_qtCatVeterancy.push_back(AsciiString("Normal"));
	s_qtCatVeterancy.push_back(AsciiString("Veteran"));
	s_qtCatVeterancy.push_back(AsciiString("Elite"));
	s_qtCatVeterancy.push_back(AsciiString("Heroic"));

	s_qtCatInteractions.clear();
	s_qtCatInteractions.push_back(AsciiString("Sleep"));
	s_qtCatInteractions.push_back(AsciiString("Passive"));
	s_qtCatInteractions.push_back(AsciiString("Normal"));
	s_qtCatInteractions.push_back(AsciiString("Alert"));
	s_qtCatInteractions.push_back(AsciiString("Aggressive"));

	// == TeamReinforcement::OnInitDialog's gate seed: the transport combo + exit are enabled and
	// deploy-by starts checked ONLY if the stored transport is actually present in the loaded
	// transport list (MFC's FindStringExact). A stored name that isn't in the catalog gates as
	// <none>/unchecked, exactly like the MFC page.
	{
		AsciiString transport = s_qtSheetDict->getAsciiString(TheKey_teamTransport, &exists);
		s_qtSheetDeployBy = (exists && !transport.isEmpty()
			&& qtmCatFind(s_qtCatTransports, transport) >= 0);
	}

	// == TeamIdentity::OnInitDialog: an absent teamExecutesActionsOnCreate is written false.
	{
		Bool executeActions = s_qtSheetDict->getBool(TheKey_teamExecutesActionsOnCreate, &exists);
		(void)executeActions;
		if (!exists)
		{
			s_qtSheetDict->setBool(TheKey_teamExecutesActionsOnCreate, false);
		}
	}

	// == TeamGeneric::_dictToScripts' seed: the existing hook chain into the slot buffer.
	{
		int i;
		for (i = 0; i < 16; i++)
		{
			s_qtGenericSlots[i].clear();
		}
		for (i = 0; i < 16; i++)
		{
			AsciiString keyName = qtmGenericHookKeyName(i);
			AsciiString v = s_qtSheetDict->getAsciiString(NAMEKEY(keyName), &exists);
			if (!exists)
			{
				break;
			}
			s_qtGenericSlots[i] = v;
		}
	}

	return 1;
}

// ---- Qt window sizes ([QtWindowSize] section, keys prefixed per window, e.g.
// TeamSheetWidth) -- shared home for any Qt dialog that persists its size. ----
extern "C" int WBQtTeamSheet_GetProfileInt(const char *key, int def)
{
	return (key != NULL) ? ::AfxGetApp()->GetProfileInt("QtWindowSize", key, def) : def;
}

extern "C" void WBQtTeamSheet_SetProfileInt(const char *key, int value)
{
	if (key != NULL)
	{
		::AfxGetApp()->WriteProfileInt("QtWindowSize", key, value);
	}
}

extern "C" void WBQtTeamSheet_Close(void)
{
	// Page edits already landed in the working copy live (the MFC sheet behaved the same:
	// its DoModal result was ignored). Drop the captured state; the Teams dialog refreshes
	// its rows after (the Qt side re-pushes the player selection).
	s_qtSheetDict = NULL;
	s_qtSheetSides = NULL;
	CTeamsDialog *dlg = CTeamsDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSelectPlayer(dlg->qtPlayerCurSel());
	}
}

extern "C" void WBQtTeamPage_GetText(int page, int ctrlId, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (s_qtSheetDict == NULL)
	{
		return;
	}
	Bool exists;
	AsciiString text;

	if (page == WB_QT_TEAMPAGE_GENERIC)
	{
		int slot = qtmGenericSlot(ctrlId);
		if (slot >= 0)
		{
			qtmComboDisplay(s_qtCatScriptsGen, s_qtGenericSlots[slot], buf, cap);
		}
		return;
	}
	if (page == WB_QT_TEAMPAGE_REINFORCEMENT)
	{
		if (ctrlId == IDC_VETERANCY)
		{
			Int v = s_qtSheetDict->getInt(TheKey_teamVeterancy, &exists);
			if (v >= 0 && v < (int)s_qtCatVeterancy.size())
			{
				copyOut(s_qtCatVeterancy[v].str(), buf, cap);
			}
			return;
		}
		NameKeyType key = qtmNoneComboKey(page, ctrlId);
		if (key != NAMEKEY_INVALID)
		{
			AsciiString value = s_qtSheetDict->getAsciiString(key, &exists);
			if (ctrlId == IDC_TRANSPORT_COMBO)
			{
				// == the MFC transport combo, which never adds a [???] row: an unknown value shows <none>.
				qtmComboDisplayNoPlaceholder(s_qtCatTransports, value, buf, cap);
			}
			else
			{
				qtmComboDisplay(s_qtCatWaypointsRe, value, buf, cap);
			}
		}
		return;
	}
	if (page == WB_QT_TEAMPAGE_BEHAVIOR)
	{
		if (ctrlId == IDC_PERCENT_DESTROYED)
		{
			// == TeamBehavior::OnInitDialog: rounded percent, 0.5 default.
			Real threshold = s_qtSheetDict->getReal(TheKey_teamDestroyedThreshold, &exists);
			if (!exists) { threshold = 0.5f; }
			Int percent = floor((threshold * 100) + 0.5);
			text.format("%d", percent);
			copyOut(text.str(), buf, cap);
			return;
		}
		if (ctrlId == IDC_ENEMY_INTERACTIONS)
		{
			Int v = s_qtSheetDict->getInt(TheKey_teamAggressiveness, &exists) - ATTITUDE_SLEEP;
			if (v >= 0 && v < (int)s_qtCatInteractions.size())
			{
				copyOut(s_qtCatInteractions[v].str(), buf, cap);
			}
			return;
		}
		NameKeyType key = qtmNoneComboKey(page, ctrlId);
		if (key != NAMEKEY_INVALID)
		{
			AsciiString value = s_qtSheetDict->getAsciiString(key, &exists);
			qtmComboDisplay(s_qtCatScripts, value, buf, cap);
		}
		return;
	}

	// --- Identity (seeds == TeamIdentity::OnInitDialog) ---
	switch (ctrlId)
	{
		case IDC_TEAM_NAME:
			copyOut(s_qtSheetDict->getAsciiString(TheKey_teamName, &exists).str(), buf, cap);
			return;
		case IDC_DESCRIPTION:
			copyOut(s_qtSheetDict->getAsciiString(TheKey_teamDescription, &exists).str(), buf, cap);
			return;
		case IDC_MAX:
		{
			Int maxInstances = s_qtSheetDict->getInt(TheKey_teamMaxInstances, &exists);
			if (!exists) { maxInstances = 1; }
			text.format("%d", maxInstances);
			copyOut(text.str(), buf, cap);
			return;
		}
		case IDC_PRODUCTION_PRIORITY:
			text.format("%d", s_qtSheetDict->getInt(TheKey_teamProductionPriority, &exists));
			copyOut(text.str(), buf, cap);
			return;
		case IDC_PRIORITY_INCREASE:
			text.format("%d", s_qtSheetDict->getInt(TheKey_teamProductionPrioritySuccessIncrease, &exists));
			copyOut(text.str(), buf, cap);
			return;
		case IDC_PRIORITY_DECREASE:
			text.format("%d", s_qtSheetDict->getInt(TheKey_teamProductionPriorityFailureDecrease, &exists));
			copyOut(text.str(), buf, cap);
			return;
		case IDC_TEAM_BUILD_FRAMES:
			text.format("%d", s_qtSheetDict->getInt(TheKey_teamInitialIdleFrames, &exists));
			copyOut(text.str(), buf, cap);
			return;
		case IDC_TEAMOWNER:
		{
			AsciiString oname = s_qtSheetDict->getAsciiString(TheKey_teamOwner, &exists);
			if (oname.isEmpty())
			{
				oname = QTM_NEUTRAL_NAME_STR;
			}
			copyOut(oname.str(), buf, cap);
			return;
		}
		default:
			break;
	}

	// --- identity's none-mapped string combos ---
	{
		NameKeyType key = qtmNoneComboKey(page, ctrlId);
		if (key != NAMEKEY_INVALID)
		{
			AsciiString value = s_qtSheetDict->getAsciiString(key, &exists);
			qtmComboDisplay((ctrlId == IDC_HOME_WAYPOINT) ? s_qtCatWaypointsId : s_qtCatScripts, value, buf, cap);
			return;
		}
	}

	// --- unit-type combos ---
	{
		int slot = qtmUnitSlot(ctrlId);
		if (slot >= 0)
		{
			AsciiString value = s_qtSheetDict->getAsciiString(qtmUnitTypeKey(slot), &exists);
			qtmComboDisplay(s_qtCatUnits, value, buf, cap);
			return;
		}
	}

	// --- unit min/max count edits ---
	{
		NameKeyType key;
		if (qtmCountSlot(ctrlId, &key))
		{
			text.format("%d", s_qtSheetDict->getInt(key, &exists));
			copyOut(text.str(), buf, cap);
			return;
		}
	}

}

extern "C" void WBQtTeamPage_SetText(int page, int ctrlId, const char *text, int notify)
{
	(void)notify;
	if (s_qtSheetDict == NULL)
	{
		return;
	}
	AsciiString value(text ? text : "");

	if (page == WB_QT_TEAMPAGE_BEHAVIOR)
	{
		if (ctrlId == IDC_PERCENT_DESTROYED)
		{
			// == TeamBehavior::OnChangePercentDestroyed.
			s_qtSheetDict->setReal(TheKey_teamDestroyedThreshold, atoi(value.str()) / 100.0f);
		}
		return;
	}
	if (page != WB_QT_TEAMPAGE_IDENTITY)
	{
		return;
	}

	switch (ctrlId)
	{
		case IDC_TEAM_NAME:
		{
			// == TeamIdentity::OnKillfocusTeamName: rename validation against the WORKING
			// sides copy + the in-use prompt; a rejected rename leaves the dict unchanged
			// (the Qt sheet reads the name back to detect that).
			AsciiString tnamenew = value;
			AsciiString tnamecur = s_qtSheetDict->getAsciiString(TheKey_teamName);
			Bool set = true;
			if (tnamecur != tnamenew)
			{
				if (s_qtSheetSides->findTeamInfo(tnamenew) || s_qtSheetSides->findSideInfo(tnamenew))
				{
					::AfxMessageBox(IDS_NAME_IN_USE);
					set = false;
				}
				else
				{
					Int count = MapObject::countMapObjectsWithOwner(tnamecur);
					if (count > 0)
					{
						set = false;
						CString msg;
						msg.Format(IDS_RENAMING_INUSE_TEAM, count);
						if (::AfxMessageBox(msg, MB_YESNO) == IDYES)
						{
							set = true;
						}
					}
				}
			}
			if (set)
			{
				s_qtSheetDict->setAsciiString(TheKey_teamName, tnamenew);
			}
			return;
		}
		case IDC_DESCRIPTION:
			s_qtSheetDict->setAsciiString(TheKey_teamDescription, value);
			return;
		case IDC_MAX:
			s_qtSheetDict->setInt(TheKey_teamMaxInstances, atoi(value.str()));
			return;
		case IDC_PRODUCTION_PRIORITY:
			s_qtSheetDict->setInt(TheKey_teamProductionPriority, atoi(value.str()));
			return;
		case IDC_PRIORITY_INCREASE:
			s_qtSheetDict->setInt(TheKey_teamProductionPrioritySuccessIncrease, atoi(value.str()));
			return;
		case IDC_PRIORITY_DECREASE:
			s_qtSheetDict->setInt(TheKey_teamProductionPriorityFailureDecrease, atoi(value.str()));
			return;
		case IDC_TEAM_BUILD_FRAMES:
			s_qtSheetDict->setInt(TheKey_teamInitialIdleFrames, atoi(value.str()));
			return;
		default:
			break;
	}

	// unit min/max count edits (== TeamIdentity::OnCommand EN_CHANGE: sscanf-gated).
	{
		NameKeyType key;
		if (qtmCountSlot(ctrlId, &key))
		{
			Int theInt;
			if (1 == sscanf(value.str(), "%d", &theInt))
			{
				s_qtSheetDict->setInt(key, theInt);
			}
			return;
		}
	}
}

extern "C" int WBQtTeamPage_GetCheck(int page, int ctrlId)
{
	if (s_qtSheetDict == NULL)
	{
		return 0;
	}
	if (page == WB_QT_TEAMPAGE_REINFORCEMENT && ctrlId == IDC_DEPLOY_BY)
	{
		return s_qtSheetDeployBy ? 1 : 0;
	}
	NameKeyType key = qtmCheckKey(page, ctrlId);
	if (key == NAMEKEY_INVALID)
	{
		return 0;
	}
	Bool exists;
	return s_qtSheetDict->getBool(key, &exists) ? 1 : 0;
}

extern "C" void WBQtTeamPage_SetCheck(int page, int ctrlId, int check)
{
	if (s_qtSheetDict == NULL)
	{
		return;
	}
	if (page == WB_QT_TEAMPAGE_REINFORCEMENT && ctrlId == IDC_DEPLOY_BY)
	{
		// == TeamReinforcement::OnDeployBy: unchecking clears the transport; checking only
		// enables the transport controls (no dict write until a transport is picked).
		s_qtSheetDeployBy = (check != 0);
		if (check == 0)
		{
			s_qtSheetDict->setAsciiString(TheKey_teamTransport, AsciiString::TheEmptyString);
		}
		return;
	}
	NameKeyType key = qtmCheckKey(page, ctrlId);
	if (key == NAMEKEY_INVALID)
	{
		return;
	}
	s_qtSheetDict->setBool(key, check != 0);
	// == TeamBehavior: base defense and perimeter defense are mutually exclusive.
	if (page == WB_QT_TEAMPAGE_BEHAVIOR && check != 0 && ctrlId == IDC_PERIMETER_DEFENSE)
	{
		s_qtSheetDict->setBool(TheKey_teamIsBaseDefense, false);
	}
	else if (page == WB_QT_TEAMPAGE_BEHAVIOR && check != 0 && ctrlId == IDC_BASE_DEFENSE)
	{
		s_qtSheetDict->setBool(TheKey_teamIsPerimeterDefense, false);
	}
}

extern "C" int WBQtTeamPage_IsEnabled(int page, int ctrlId)
{
	if (s_qtSheetDict == NULL)
	{
		return 0;
	}
	// == TeamReinforcement's gate: transport combo + exit follow the deploy-by state.
	if (page == WB_QT_TEAMPAGE_REINFORCEMENT &&
		(ctrlId == IDC_TRANSPORT_COMBO || ctrlId == IDC_TRANSPORTS_EXIT))
	{
		return s_qtSheetDeployBy ? 1 : 0;
	}
	return 1;
}

static const std::vector<AsciiString> *qtmSheetCatalog(int page, int ctrlId)
{
	if (page == WB_QT_TEAMPAGE_IDENTITY)
	{
		if (ctrlId == IDC_HOME_WAYPOINT)        { return &s_qtCatWaypointsId; }
		if (ctrlId == IDC_PRODUCTION_CONDITION) { return &s_qtCatScripts; }
		if (ctrlId == IDC_TEAMOWNER)            { return &s_qtCatOwners; }
		if (qtmUnitSlot(ctrlId) >= 0)           { return &s_qtCatUnits; }
	}
	else if (page == WB_QT_TEAMPAGE_REINFORCEMENT)
	{
		if (ctrlId == IDC_WAYPOINT_COMBO)  { return &s_qtCatWaypointsRe; }
		if (ctrlId == IDC_TRANSPORT_COMBO) { return &s_qtCatTransports; }
		if (ctrlId == IDC_VETERANCY)       { return &s_qtCatVeterancy; }
	}
	else if (page == WB_QT_TEAMPAGE_BEHAVIOR)
	{
		if (ctrlId == IDC_ENEMY_INTERACTIONS) { return &s_qtCatInteractions; }
		if (qtmNoneComboKey(page, ctrlId) != NAMEKEY_INVALID) { return &s_qtCatScripts; }
	}
	else if (page == WB_QT_TEAMPAGE_GENERIC)
	{
		if (qtmGenericSlot(ctrlId) >= 0) { return &s_qtCatScriptsGen; }
	}
	return NULL;
}

extern "C" int WBQtTeamPage_ComboCount(int page, int ctrlId)
{
	const std::vector<AsciiString> *cat = qtmSheetCatalog(page, ctrlId);
	return (cat != NULL) ? (int)cat->size() : 0;
}

// How many generic-script slots are filled (== the length of the compacted hook chain).
// The sheet shows this many combos and hides the rest (== _dictToScripts's SW_HIDE tail).
extern "C" int WBQtTeamGeneric_FilledCount(void)
{
	int n = 0;
	for (int i = 0; i < 16; i++)
	{
		if (!s_qtGenericSlots[i].isEmpty())
		{
			++n;
		}
	}
	return n;
}

extern "C" void WBQtTeamPage_ComboItem(int page, int ctrlId, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	const std::vector<AsciiString> *cat = qtmSheetCatalog(page, ctrlId);
	if (cat != NULL && i >= 0 && i < (int)cat->size())
	{
		copyOut((*cat)[i].str(), buf, cap);
	}
}

extern "C" void WBQtTeamPage_ComboSelectText(int page, int ctrlId, const char *text, int notify)
{
	(void)notify;
	if (s_qtSheetDict == NULL)
	{
		return;
	}

	// == TeamReinforcement::OnSelchangeVeterancy: the INDEX is the stored value.
	if (page == WB_QT_TEAMPAGE_REINFORCEMENT && ctrlId == IDC_VETERANCY)
	{
		int idx = qtmCatFind(s_qtCatVeterancy, AsciiString(text ? text : ""));
		if (idx >= 0)
		{
			s_qtSheetDict->setInt(TheKey_teamVeterancy, idx);
		}
		return;
	}
	// == TeamBehavior::OnSelchangeEnemyInteractions.
	if (page == WB_QT_TEAMPAGE_BEHAVIOR && ctrlId == IDC_ENEMY_INTERACTIONS)
	{
		int idx = qtmCatFind(s_qtCatInteractions, AsciiString(text ? text : ""));
		if (idx >= 0)
		{
			s_qtSheetDict->setInt(TheKey_teamAggressiveness, idx + ATTITUDE_SLEEP);
		}
		return;
	}
	// == TeamIdentity::OnSelendokTeamowner: the display text is stored verbatim (the
	// teams-list filter matches owners by the same display convention).
	if (page == WB_QT_TEAMPAGE_IDENTITY && ctrlId == IDC_TEAMOWNER)
	{
		s_qtSheetDict->setAsciiString(TheKey_teamOwner, AsciiString(text ? text : ""));
		return;
	}
	// unit-type combos (<none> clears; the MFC handler stored the literal text).
	if (page == WB_QT_TEAMPAGE_IDENTITY)
	{
		int slot = qtmUnitSlot(ctrlId);
		if (slot >= 0)
		{
			s_qtSheetDict->setAsciiString(qtmUnitTypeKey(slot), qtmComboValueFromText(text));
			return;
		}
	}
	// TeamGeneric slots: update the buffer then rebuild the compacted hook chain.
	if (page == WB_QT_TEAMPAGE_GENERIC)
	{
		int slot = qtmGenericSlot(ctrlId);
		if (slot >= 0)
		{
			s_qtGenericSlots[slot] = qtmComboValueFromText(text);
			qtmGenericRebuildDict();
			return;
		}
	}
	// none-mapped string combos.
	{
		NameKeyType key = qtmNoneComboKey(page, ctrlId);
		if (key != NAMEKEY_INVALID)
		{
			s_qtSheetDict->setAsciiString(key, qtmComboValueFromText(text));
			return;
		}
	}
}

extern "C" void WBQtTeamPage_ClickButton(int page, int ctrlId)
{
	if (page != WB_QT_TEAMPAGE_IDENTITY || s_qtSheetDict == NULL)
	{
		return;
	}
	// == TeamIdentity::OnUnitTypeButton: pop the Qt unit picker and store the pick.
	int slot = qtmUnitButtonSlot(ctrlId);
	if (slot < 0)
	{
		return;
	}
	int allowable[4];
	allowable[0] = ES_VEHICLE;
	allowable[1] = ES_INFANTRY;
	allowable[2] = ES_STRUCTURE;
	allowable[3] = ES_SYSTEM;
	char qtPicked[256];
	qtPicked[0] = 0;
	int qtRc = WBQtPickUnit_Run(::AfxGetMainWnd()->GetSafeHwnd(), allowable, 4, false, qtPicked, sizeof(qtPicked));
	if (qtRc == 1)
	{
		s_qtSheetDict->setAsciiString(qtmUnitTypeKey(slot), AsciiString(qtPicked));
	}
}

#endif // RTS_HAS_QT
