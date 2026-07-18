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
#include "Common/MultiplayerSettings.h"
#include "GameClient/GameText.h"
#include "GameLogic/SidesList.h"
#include "qt/panels/WBQtPlayerListBridge.h"
#include <vector>
#include <algorithm>

#ifdef RTS_HAS_QT

static const char *NEUTRAL_NAME_STR = "(neutral)";	// == the file-static in playerlistdlg.cpp

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

// ================= De-bridged (windowless) view model -- branch qt-debridge =================
// The dialog window is never Create()d; these bridge-static vectors mirror exactly what
// updateTheUI (playerlistdlg.cpp) put in each hidden control, and qtMRebuild() refills them
// from m_sides. The qt* members below read/write this model instead of GetDlgItem controls.
// Rows in s_allyOther / its parallel sel arrays are the SORTED "other player" UI names (== the
// LBS_SORT allies/enemies listboxes), so the '0'/'1' relation mask lines up 1:1.

namespace
{
	// Copies of the file-static helpers from playerlistdlg.cpp (they are file-local there).
	Bool qtmIsLegalNameChar(char c) { return ::isalnum(c) || c == '_'; }

	void qtmEnsureValidPlayerName(Dict *d)
	{
		char buf[1024];
		strcpy(buf, d->getAsciiString(TheKey_playerName).str());
		for (char *p = buf; *p; ++p)
		{
			if (!qtmIsLegalNameChar(*p))
			{
				*p = '_';
			}
		}
		d->setAsciiString(TheKey_playerName, AsciiString(buf));
	}

	void qtmFixDefaultTeamName(SidesList &sides, AsciiString oldpname, AsciiString newpname)
	{
		AsciiString tname;
		tname.set("team");
		tname.concat(oldpname);
		TeamsInfo *ti = sides.findTeamInfo(tname);
		if (ti)
		{
			tname.set("team");
			tname.concat(newpname);
			ti->getDict()->setAsciiString(TheKey_teamName, tname);
			ti->getDict()->setAsciiString(TheKey_teamOwner, newpname);
		}
	}

	void qtmUpdateAllTeams(SidesList &sides, AsciiString oldpname, AsciiString newpname)
	{
		Int numTeams = sides.getNumTeams();
		for (Int i = 0; i < numTeams; ++i)
		{
			TeamsInfo *teamInfo = sides.getTeamInfo(i);
			if (teamInfo)
			{
				Bool exists;
				Dict *dict = teamInfo->getDict();
				AsciiString teamOwner = dict->getAsciiString(TheKey_teamOwner, &exists);
				if (exists && teamOwner.compare(oldpname) == 0)
				{
					dict->setAsciiString(TheKey_teamOwner, newpname);
				}
			}
		}
	}

	AsciiString qtmPlayerNameForUI(SidesList &sides, int i)
	{
		AsciiString b = sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
		if (b.isEmpty())
		{
			b = NEUTRAL_NAME_STR;
		}
		return b;
	}

	AsciiString qtmUIToInternal(SidesList &sides, const AsciiString &n)
	{
		for (Int i = 0; i < sides.getNumSides(); i++)
		{
			if (qtmPlayerNameForUI(sides, i) == n)
			{
				return sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
			}
		}
		return AsciiString::TheEmptyString;
	}

	Bool qtmContainsToken(const AsciiString &cur, const AsciiString &tokenIn)
	{
		AsciiString name = cur, token;
		while (name.nextToken(&token))
		{
			if (token == tokenIn)
			{
				return true;
			}
		}
		return false;
	}

	AsciiString qtmRemoveDupsFromEnemies(const AsciiString &allies, const AsciiString &enemies)
	{
		AsciiString newEnemies, tmp = enemies, token;
		while (tmp.nextToken(&token))
		{
			if (qtmContainsToken(allies, token))
			{
				continue;
			}
			if (!newEnemies.isEmpty())
			{
				newEnemies.concat(" ");
			}
			newEnemies.concat(token);
		}
		return newEnemies;
	}

	bool qtmNameLess(const AsciiString &a, const AsciiString &b)
	{
		return _stricmp(a.str(), b.str()) < 0;	// == the LBS_SORT listbox order
	}

	const char *qtmRelationStr(SidesList &sides, int t1, int t2)
	{
		AsciiString t2name = sides.getSideInfo(t2)->getDict()->getAsciiString(TheKey_playerName);
		if (qtmContainsToken(sides.getSideInfo(t1)->getDict()->getAsciiString(TheKey_playerAllies), t2name))
		{
			return "Ally";
		}
		if (qtmContainsToken(sides.getSideInfo(t1)->getDict()->getAsciiString(TheKey_playerEnemies), t2name))
		{
			return "Enemy";
		}
		return "Neutral";
	}
}

// The view model (mirrors the hidden controls updateTheUI seeded).
struct QtmOther { AsciiString name; int allySel; int enemySel; };
static std::vector<AsciiString> s_playerLabels;	// IDC_PLAYERS rows (side order)
static AsciiString s_curName, s_curDisplay, s_curFaction;
static Bool s_curEditable = false, s_curIsComputer = false, s_relationsEnabled = false;
static int s_curColorRGB = 0;
static std::vector<AsciiString> s_factionNames; static int s_factionIdx = -1;
static std::vector<AsciiString> s_colorNames; static int s_colorIdx = -1;
static std::vector<QtmOther> s_others;	// sorted "other player" rows (allies/enemies share)
static std::vector<AsciiString> s_regardOut, s_regardIn;
static Bool s_canAdd = false, s_removeEnabled = false;

// == updateTheUI, windowless: rebuild the view model from m_sides. (No control writes.)
void PlayerListDlg::qtMRebuild(void)
{
	m_sides.validateSides();
	if (m_curPlayerIdx < 0)
	{
		m_curPlayerIdx = 0;
	}
	if (m_curPlayerIdx >= m_sides.getNumSides())
	{
		m_curPlayerIdx = m_sides.getNumSides() - 1;
	}

	// player list labels (side order): 'name="display"' or NEUTRAL_NAME_STR.
	s_playerLabels.clear();
	Int len = m_sides.getNumSides();
	for (Int i = 0; i < len; i++)
	{
		Dict *d = m_sides.getSideInfo(i)->getDict();
		AsciiString name = d->getAsciiString(TheKey_playerName);
		UnicodeString uni = d->getUnicodeString(TheKey_playerDisplayName);
		AsciiString fmt;
		if (name.isEmpty())
		{
			fmt = NEUTRAL_NAME_STR;
		}
		else
		{
			fmt.format("%s=\"%ls\"", name.str(), uni.str());
		}
		s_playerLabels.push_back(fmt);
	}

	Dict *pdict = m_sides.getSideInfo(m_curPlayerIdx)->getDict();
	AsciiString curName = pdict->getAsciiString(TheKey_playerName);
	Bool isNeutral = curName.isEmpty();
	s_curEditable = !isNeutral;
	s_relationsEnabled = !isNeutral;

	s_curName = curName;
	UnicodeString cur_pdname = pdict->getUnicodeString(TheKey_playerDisplayName);
	s_curDisplay.translate(cur_pdname);
	s_curIsComputer = !pdict->getBool(TheKey_playerIsHuman);

	// color RGB: dict value, else the faction template's preferred color.
	{
		RGBColor rgb;
		rgb.red = rgb.green = rgb.blue = 0;
		Bool hasColor = false;
		Int color = pdict->getInt(TheKey_playerColor, &hasColor);
		if (hasColor)
		{
			rgb.setFromInt(color);
		}
		else
		{
			AsciiString tmplname = pdict->getAsciiString(TheKey_playerFaction);
			const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
			if (pt)
			{
				rgb = *pt->getPreferredColor();
			}
		}
		s_curColorRGB = rgb.getAsInt();
	}

	// faction combo (SORTED CBS_SORT) + current index.
	s_factionNames.clear();
	if (ThePlayerTemplateStore)
	{
		for (Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); i++)
		{
			s_factionNames.push_back(ThePlayerTemplateStore->getNthPlayerTemplate(i)->getName());
		}
	}
	std::sort(s_factionNames.begin(), s_factionNames.end(), qtmNameLess);
	s_factionIdx = -1;
	{
		AsciiString curFaction = pdict->getAsciiString(TheKey_playerFaction);
		// Cache the stored faction so the Qt panel can show a free-typed faction that matches no
		// list entry (s_factionIdx stays -1), mirroring MFC's SetCurSel(-1) leaving the combo's
		// edit-control text intact.
		s_curFaction = curFaction;
		for (size_t i = 0; i < s_factionNames.size(); i++)
		{
			if (s_factionNames[i] == curFaction) { s_factionIdx = (int)i; break; }
		}
	}

	// color combo (NOT sorted -- multiplayer color order) + current index by RGB match.
	s_colorNames.clear();
	s_colorIdx = -1;
	{
		Int numColors = TheMultiplayerSettings->getNumColors();
		for (Int c = 0; c < numColors; ++c)
		{
			MultiplayerColorDefinition *def = TheMultiplayerSettings->getColor(c);
			if (!def) { continue; }
			UnicodeString colorName = TheGameText->fetch(def->getTooltipName().str());
			AsciiString str;
			str.translate(colorName);
			s_colorNames.push_back(str);
			if (s_colorIdx < 0 && s_curColorRGB == def->getRGBValue().getAsInt())
			{
				s_colorIdx = (int)s_colorNames.size() - 1;
			}
		}
	}

	// allies/enemies "other" rows (SORTED, excluding self + neutral) with their sel flags.
	s_others.clear();
	AsciiString curAllies = pdict->getAsciiString(TheKey_playerAllies);
	AsciiString curEnemies = pdict->getAsciiString(TheKey_playerEnemies);
	std::vector<AsciiString> otherNames;
	for (Int i = 0; i < m_sides.getNumSides(); i++)
	{
		AsciiString nm = m_sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
		if (nm == curName || nm.isEmpty()) { continue; }
		otherNames.push_back(qtmPlayerNameForUI(m_sides, i));
	}
	std::sort(otherNames.begin(), otherNames.end(), qtmNameLess);
	for (size_t i = 0; i < otherNames.size(); i++)
	{
		QtmOther o;
		o.name = otherNames[i];
		AsciiString internalNm = qtmUIToInternal(m_sides, otherNames[i]);
		o.allySel = qtmContainsToken(curAllies, internalNm) ? 1 : 0;
		o.enemySel = qtmContainsToken(curEnemies, internalNm) ? 1 : 0;
		s_others.push_back(o);
	}

	// regard summaries (SORTED, like the LBS_SORT attitude listboxes).
	s_regardOut.clear();
	s_regardIn.clear();
	for (Int i = 0; i < m_sides.getNumSides(); i++)
	{
		AsciiString pname = m_sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
		if (pname.isEmpty() || pname == curName) { continue; }
		AsciiString uiName = qtmPlayerNameForUI(m_sides, i);
		char buf[1024];
		sprintf(buf, "%s: %s", uiName.str(), qtmRelationStr(m_sides, m_curPlayerIdx, i));
		s_regardOut.push_back(AsciiString(buf));
		sprintf(buf, "%s: %s", uiName.str(), qtmRelationStr(m_sides, i, m_curPlayerIdx));
		s_regardIn.push_back(AsciiString(buf));
	}
	std::sort(s_regardOut.begin(), s_regardOut.end(), qtmNameLess);
	std::sort(s_regardIn.begin(), s_regardIn.end(), qtmNameLess);

	s_canAdd = (m_sides.getNumSides() < MAX_PLAYER_COUNT);
	s_removeEnabled = (m_sides.getNumSides() > 1 && !isNeutral);
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
		// De-bridged (qt-debridge): never Create() the window. Seed the model like
		// OnInitDialog (m_sides = *TheSidesList) and build the view model directly.
		s_qtDlg->m_updating = 0;
		s_qtDlg->m_sides = *TheSidesList;
		s_qtDlg->m_curPlayerIdx = s_qtPrevCurPlyr;
		s_qtDlg->qtMRebuild();
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
		s_qtDlg->DestroyWindow();	// harmless no-op: the window was never Create()d
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

// The ctrlId-keyed getters now read the view model (the ctrlId selects which list).
int PlayerListDlg::qtListCount(int ctrlId)
{
	switch (ctrlId)
	{
		case IDC_PLAYERS:             return (int)s_playerLabels.size();
		case IDC_ALLIESLIST:
		case IDC_ENEMIESLIST:         return (int)s_others.size();
		case IDC_PLAYER_ATTITUDE_OUT: return (int)s_regardOut.size();
		case IDC_PLAYER_ATTITUDE_IN:  return (int)s_regardIn.size();
		default:                      return 0;
	}
}

void PlayerListDlg::qtListText(int ctrlId, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	const std::vector<AsciiString> *v = NULL;
	switch (ctrlId)
	{
		case IDC_PLAYERS:             v = &s_playerLabels; break;
		case IDC_PLAYER_ATTITUDE_OUT: v = &s_regardOut; break;
		case IDC_PLAYER_ATTITUDE_IN:  v = &s_regardIn; break;
		case IDC_ALLIESLIST:
		case IDC_ENEMIESLIST:
			if (i >= 0 && i < (int)s_others.size())
			{
				copyOut(s_others[i].name.str(), buf, cap);
			}
			return;
		default: return;
	}
	if (v != NULL && i >= 0 && i < (int)v->size())
	{
		copyOut((*v)[i].str(), buf, cap);
	}
}

int PlayerListDlg::qtListCurSel(int ctrlId)
{
	return (ctrlId == IDC_PLAYERS) ? m_curPlayerIdx : -1;
}

int PlayerListDlg::qtListGetSel(int ctrlId, int i)
{
	if (i < 0 || i >= (int)s_others.size())
	{
		return 0;
	}
	return (ctrlId == IDC_ENEMIESLIST) ? s_others[i].enemySel : s_others[i].allySel;
}

int PlayerListDlg::qtComboCount(int ctrlId)
{
	if (ctrlId == IDC_PLAYERFACTION)     return (int)s_factionNames.size();
	if (ctrlId == IDC_PlayerColorCombo)  return (int)s_colorNames.size();
	return 0;
}

void PlayerListDlg::qtComboText(int ctrlId, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	const std::vector<AsciiString> *v = (ctrlId == IDC_PLAYERFACTION) ? &s_factionNames
		: (ctrlId == IDC_PlayerColorCombo) ? &s_colorNames : NULL;
	if (v != NULL && i >= 0 && i < (int)v->size())
	{
		copyOut((*v)[i].str(), buf, cap);
	}
}

int PlayerListDlg::qtComboCurSel(int ctrlId)
{
	if (ctrlId == IDC_PLAYERFACTION)     return s_factionIdx;
	if (ctrlId == IDC_PlayerColorCombo)  return s_colorIdx;
	return -1;
}

void PlayerListDlg::qtGetEditText(int ctrlId, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	if (ctrlId == IDC_PLAYERNAME)
	{
		copyOut(s_curName.str(), buf, cap);
	}
	else if (ctrlId == IDC_PLAYERDISPLAYNAME)
	{
		copyOut(s_curDisplay.str(), buf, cap);
	}
	else if (ctrlId == IDC_PLAYERFACTION)
	{
		copyOut(s_curFaction.str(), buf, cap);
	}
}

int PlayerListDlg::qtGetCheck(int ctrlId)
{
	return (ctrlId == IDC_PLAYERISCOMPUTER && s_curIsComputer) ? 1 : 0;
}

int PlayerListDlg::qtIsCtrlEnabled(int ctrlId)
{
	switch (ctrlId)
	{
		case IDC_PLAYERNAME:
		case IDC_PLAYERDISPLAYNAME:
		case IDC_PLAYERISCOMPUTER: return s_curEditable ? 1 : 0;
		case IDC_ALLIESLIST:
		case IDC_ENEMIESLIST:      return s_relationsEnabled ? 1 : 0;
		case IDC_REMOVEPLAYER:     return s_removeEnabled ? 1 : 0;
		default:                   return 1;
	}
}

int PlayerListDlg::qtGetColorRGB(void)
{
	return s_curColorRGB;
}

int PlayerListDlg::qtCanAddPlayer(void)
{
	// == OnNewplayer's guard (keeps room for an observer).
	return (m_sides.getNumSides() < MAX_PLAYER_COUNT - 1) ? 1 : 0;
}

void PlayerListDlg::qtSelectPlayer(int i)
{
	// == OnSelchangePlayers minus the listbox read.
	m_curPlayerIdx = i;
	qtMRebuild();
}

void PlayerListDlg::qtSetName(const char *text)
{
	// == OnChangePlayername minus the edit read: validate + rename fixups on m_sides.
	Dict *pdict = m_sides.getSideInfo(m_curPlayerIdx)->getDict();
	AsciiString pnamenew(text ? text : "");
	AsciiString pnameold = pdict->getAsciiString(TheKey_playerName);
	if (pnameold == pnamenew)
	{
		return;
	}
	if (m_sides.findSideInfo(pnamenew))
	{
		::AfxMessageBox(IDS_NAME_IN_USE);
	}
	else
	{
		pdict->setAsciiString(TheKey_playerName, pnamenew);
		qtmEnsureValidPlayerName(pdict);
		qtmUpdateAllTeams(m_sides, pnameold, pnamenew);
		qtmFixDefaultTeamName(m_sides, pnameold, pnamenew);
	}
	qtMRebuild();
}

void PlayerListDlg::qtSetDisplayName(const char *text)
{
	// == OnChangePlayerdisplayname minus the edit read.
	AsciiString tmp(text ? text : "");
	UnicodeString pnamenew;
	pnamenew.translate(tmp);
	m_sides.getSideInfo(m_curPlayerIdx)->getDict()->setUnicodeString(TheKey_playerDisplayName, pnamenew);
	qtMRebuild();
}

void PlayerListDlg::qtSetIsComputer(int isComputer)
{
	// == OnPlayeriscomputer minus the checkbox read (check == computer -> not human).
	m_sides.getSideInfo(m_curPlayerIdx)->getDict()->setBool(TheKey_playerIsHuman, isComputer == 0);
	qtMRebuild();
}

void PlayerListDlg::qtSetFaction(const char *name)
{
	// == OnEditchangePlayerfaction minus the combo read.
	m_sides.getSideInfo(m_curPlayerIdx)->getDict()->setAsciiString(TheKey_playerFaction, AsciiString(name ? name : ""));
	qtMRebuild();
}

void PlayerListDlg::qtSetColorIndex(int i)
{
	// == OnSelectPlayerColor minus the combo read: the index IS the multiplayer color index
	// (the color combo rows are the color defs in order).
	Dict *playerDict = m_sides.getSideInfo(m_curPlayerIdx)->getDict();
	if (i >= 0 && i < TheMultiplayerSettings->getNumColors())
	{
		MultiplayerColorDefinition *def = TheMultiplayerSettings->getColor(i);
		if (def)
		{
			playerDict->setInt(TheKey_playerColor, def->getColor());
		}
	}
	qtMRebuild();
}

void PlayerListDlg::qtSetColorRGB(int rgb)
{
	// == OnColorPress minus the CColorDialog (the Qt side picked the color).
	RGBColor color;
	color.setFromInt(rgb);
	m_sides.getSideInfo(m_curPlayerIdx)->getDict()->setInt(TheKey_playerColor, color.getAsInt());
	qtMRebuild();
}

void PlayerListDlg::qtSetRelationSel(int isEnemy, const char *mask)
{
	// == OnSelchangeAllieslist minus the listbox reads: rebuild allies+enemies from BOTH masks
	// (the model keeps the current selection of the other list), dedup, write the dict.
	Dict *pdict = m_sides.getSideInfo(m_curPlayerIdx)->getDict();
	if (pdict->getAsciiString(TheKey_playerName).isEmpty() || mask == NULL)
	{
		return;	// neutral: relations not editable
	}

	// Apply the incoming mask to the changed list's sel flags in the view model.
	int maskLen = (int)strlen(mask);
	for (int i = 0; i < (int)s_others.size() && i < maskLen; i++)
	{
		if (isEnemy)
		{
			s_others[i].enemySel = (mask[i] == '1') ? 1 : 0;
		}
		else
		{
			s_others[i].allySel = (mask[i] == '1') ? 1 : 0;
		}
	}

	// Rebuild the two internal-name token strings from the (parallel) sel flags.
	AsciiString allies, enemies;
	for (size_t i = 0; i < s_others.size(); i++)
	{
		AsciiString internalNm = qtmUIToInternal(m_sides, s_others[i].name);
		if (s_others[i].allySel)
		{
			if (!allies.isEmpty()) { allies.concat(" "); }
			allies.concat(internalNm);
		}
		if (s_others[i].enemySel)
		{
			if (!enemies.isEmpty()) { enemies.concat(" "); }
			enemies.concat(internalNm);
		}
	}
	enemies = qtmRemoveDupsFromEnemies(allies, enemies);

	pdict->setAsciiString(TheKey_playerAllies, allies);
	pdict->setAsciiString(TheKey_playerEnemies, enemies);
	qtMRebuild();
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
	qtMRebuild();
}

void PlayerListDlg::qtRemovePlayer(void)
{
	// == OnRemoveplayer minus the updateTheUI tail (it is pure m_sides surgery + a confirm box).
	Dict *playerDict = m_sides.getSideInfo(m_curPlayerIdx)->getDict();
	AsciiString pname = playerDict->getAsciiString(TheKey_playerName);
	if (pname.isEmpty())
	{
		return;	// neutral can't be removed
	}
	Int i, count = 0;
	for (i = 0; i < m_sides.getNumTeams(); i++)
	{
		Dict *tdict = m_sides.getTeamInfo(i)->getDict();
		if (tdict->getAsciiString(TheKey_teamOwner) == pname)
		{
			count += MapObject::countMapObjectsWithOwner(tdict->getAsciiString(TheKey_teamName));
		}
	}
	if (count > 0)
	{
		CString msg;
		msg.Format(IDS_REMOVING_INUSE_TEAM, count);
		if (::AfxMessageBox(msg, MB_YESNO) == IDNO)
		{
			return;
		}
	}
	if (m_sides.getNumSides() <= 1)
	{
		return;
	}
	m_sides.removeSide(m_curPlayerIdx);
try_again:
	for (i = 0; i < m_sides.getNumTeams(); i++)
	{
		Dict *tdict = m_sides.getTeamInfo(i)->getDict();
		if (tdict->getAsciiString(TheKey_teamOwner) == pname)
		{
			m_sides.removeTeam(i);
			goto try_again;
		}
	}
	m_sides.validateSides();
	qtMRebuild();
}

void PlayerListDlg::qtAddSkirmishPlayers(void)
{
	// == OnAddskirmishplayers minus the updateTheUI tail. The static addSide helper lives in
	// playerlistdlg.cpp; reproduce it here (pure m_sides surgery).
	static const struct { const char *faction; const char *name; const wchar_t *uname; } skirmishers[] =
	{
		{ "FactionCivilian", "PlyrCivilian", L"PlyrCivilian" },
		{ "FactionAmerica", "SkirmishAmerica", L"SkirmishAmerica" },
		{ "FactionChina", "SkirmishChina", L"SkirmishChina" },
		{ "FactionGLA", "SkirmishGLA", L"SkirmishGLA" },
		{ "FactionAmericaAirForceGeneral", "SkirmishAmericaAirForceGeneral", L"SkirmishAmericaAirForceGeneral" },
		{ "FactionAmericaLaserGeneral", "SkirmishAmericaLaserGeneral", L"SkirmishAmericaLaserGeneral" },
		{ "FactionAmericaSuperWeaponGeneral", "SkirmishAmericaSuperWeaponGeneral", L"SkirmishAmericaSuperWeaponGeneral" },
		{ "FactionChinaTankGeneral", "SkirmishChinaTankGeneral", L"SkirmishChinaTankGeneral" },
		{ "FactionChinaNukeGeneral", "SkirmishChinaNukeGeneral", L"SkirmishChinaNukeGeneral" },
		{ "FactionChinaInfantryGeneral", "SkirmishChinaInfantryGeneral", L"SkirmishChinaInfantryGeneral" },
		{ "FactionGLADemolitionGeneral", "SkirmishGLADemolitionGeneral", L"SkirmishGLADemolitionGeneral" },
		{ "FactionGLAToxinGeneral", "SkirmishGLAToxinGeneral", L"SkirmishGLAToxinGeneral" },
		{ "FactionGLAStealthGeneral", "SkirmishGLAStealthGeneral", L"SkirmishGLAStealthGeneral" }
	};
	for (size_t s = 0; s < sizeof(skirmishers) / sizeof(skirmishers[0]); s++)
	{
		if (!m_sides.findSideInfo(AsciiString(skirmishers[s].name)))
		{
			Dict newPlayerDict;
			UnicodeString playerUStr;
			playerUStr.set(skirmishers[s].uname);
			newPlayerDict.setAsciiString(TheKey_playerName, AsciiString(skirmishers[s].name));
			newPlayerDict.setBool(TheKey_playerIsHuman, false);
			newPlayerDict.setUnicodeString(TheKey_playerDisplayName, playerUStr);
			newPlayerDict.setAsciiString(TheKey_playerFaction, AsciiString(skirmishers[s].faction));
			newPlayerDict.setAsciiString(TheKey_playerEnemies, AsciiString(""));
			newPlayerDict.setAsciiString(TheKey_playerAllies, AsciiString(""));
			qtmEnsureValidPlayerName(&newPlayerDict);
			m_sides.addSide(&newPlayerDict);
			m_sides.validateSides();
		}
	}
	qtMRebuild();
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

extern "C" void WBQtPlayerListData_GetFactionText(char *buf, int cap)
{
	PlayerListDlg *dlg = PlayerListDlg::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtGetEditText(IDC_PLAYERFACTION, buf, cap);
	}
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

extern "C" void WBQtAddPlayerData_GetTemplateSide(int i, char *buf, int cap)
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
			copyOut(pt->getSide().str(), buf, cap);
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
