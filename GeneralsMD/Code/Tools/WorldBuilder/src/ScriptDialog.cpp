/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ScriptDialog.cpp : implementation file
//

#include "StdAfx.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "CUndoable.h"
#include "ScriptDialog.h"
#include "GameLogic/ScriptEngine.h"
#include "ScriptProperties.h"
#include "ScriptConditions.h"
#include "ScriptActionsTrue.h"
#include "ScriptActionsFalse.h"
#include "CFixTeamOwnerDialog.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/WellKnownKeys.h"
#include "Common/DataChunk.h"
#include "Common/FileSystem.h"
#include "EditGroup.h"
#include "EditParameter.h"
#include "ExportScriptsOptions.h"
#include "Common/ThingFactory.h"
#include "WaypointOptions.h"
#include "Common/UnicodeString.h"
#include "MainFrm.h"
#ifdef RTS_HAS_QT
#include "qt/panels/WBQtScriptEditBridge.h"
#endif

#include "Common/GlobalData.h"

// This is used to allow sounds to be played via PlaySound
#include <mmsystem.h>

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

// static Bool g_didScriptWarningsUpdate = false;

static const Int K_LOCAL_TEAMS_VERSION_1 = 1;


static const char* NEUTRAL_NAME_STR = "(neutral)";
ScriptDialog *ScriptDialog::m_staticThis = NULL;

static AsciiString formatScriptLabel(Script *pScr, Bool cleanNames) {
	int burstSeconds = pScr->getDelayEvalSeconds();
	AsciiString fmt;
	if (pScr->isSubroutine()) {
		fmt.concat("[S "); 
	} else {
		fmt.concat("[   "); 
	}
	if (pScr->isActive()) {
		fmt.concat("A "); 
	} else {
		fmt.concat("   "); 
	}
	if (pScr->isOneShot()) {
		fmt.concat("D] "); 
	} else {
		fmt.concat("   ] "); 
	}

	// Difficulty markers
	bool easy   = pScr->isEasy();
	bool normal = pScr->isNormal();
	bool hard   = pScr->isHard();

	// only show difficulty if not all 3 present OR if cleanNames == false
	if (!cleanNames || !(easy && normal && hard)) {
		// Build difficulty string dynamically without trailing spaces
		AsciiString diff;
		diff.concat("[");
		bool first = true;
		if (easy) {
			diff.concat("E");
			first = false;
		}
		if (normal) {
			if (!first) diff.concat(" ");
			diff.concat("N");
			first = false;
		}
		if (hard) {
			if (!first) diff.concat(" ");
			diff.concat("H");
		}
		diff.concat("] ");
		fmt.concat(diff);
}
	fmt.concat(pScr->getName().str());

	if (burstSeconds > 0) {
        AsciiString burstFmt;
        burstFmt.format(" <%ds>", burstSeconds);
        fmt.concat(burstFmt);
    }

	return fmt;
}

static AsciiString formatScriptLabel(ScriptGroup *pScrGrp) {
	AsciiString fmt;
	if (pScrGrp->isSubroutine())
	{
		fmt.concat("[S "); 
	}
	else
	{
		fmt.concat("[   "); 
	}
	if (pScrGrp->isActive())
	{
		fmt.concat("A]"); 
	}
	else
	{
		fmt.concat("   ]"); 
	}
	fmt.concat(pScrGrp->getName().str());
	return fmt;
}

// CSDTreeCtrl Implementation /////////////////////////////////////////////////////////////////////

/** This function reacts to a right button push on a
		script or group to create a drop down menu */
void CSDTreeCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	/// first, if there's something under the mouse, select it.
	HTREEITEM item = HitTest(point);
	SelectItem(item);

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_SCRIPTDIALOGPOPUP));
	CMenu* pPopup = menu.GetSubMenu(0);
	if (!pPopup)
	{
		return;
	}
	ClientToScreen(&point);

	/// Display a check mark to signify status of active-ness
	if (item)
	{
		ScriptDialog *sd = (ScriptDialog*) GetParent();
		if (sd->friend_getCurScript() != NULL)
		{
			Bool active = sd->friend_getCurScript()->isActive();
			pPopup->CheckMenuItem(ID_SCRIPTACTIVATE, MF_BYCOMMAND | (active ? MF_CHECKED : MF_UNCHECKED));
		}
		else if (sd->friend_getCurGroup() != NULL)
		{
			Bool active = sd->friend_getCurGroup()->isActive();
			pPopup->CheckMenuItem(ID_SCRIPTACTIVATE, MF_BYCOMMAND | (active ? MF_CHECKED : MF_UNCHECKED));
		}

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, GetParent());
	}
}

BEGIN_MESSAGE_MAP(CSDTreeCtrl, CTreeCtrl)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ScriptDialog dialog


ScriptDialog::ScriptDialog(CWnd* pParent /*=NULL*/)
	: CDialog(ScriptDialog::IDD, pParent)
{
	m_draggingTreeView = false;
	m_autoUpdateWarnings = true;
	m_pOldFont = NULL; 
	m_bCompressed = false;
	m_updating = true;
	m_bNewIcons = false;
	m_bSmartCopyEnabled = false;
	m_bAutoMergeScripts = false;
	//{{AFX_DATA_INIT(ScriptDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

ScriptDialog::~ScriptDialog()
{
	EditParameter::setCurSidesList(NULL);
	m_staticThis=NULL;
}


void ScriptDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ScriptDialog)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ScriptDialog, CDialog)
	//{{AFX_MSG_MAP(ScriptDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SCRIPT_TREE, OnSelchangedScriptTree)
	ON_BN_CLICKED(IDC_NEW_FOLDER, OnNewFolder)
	ON_BN_CLICKED(IDC_NEW_SCRIPT, OnNewScript)
	ON_BN_CLICKED(IDC_EDIT_SCRIPT, OnEditScript)
	ON_BN_CLICKED(IDC_COPY_SCRIPT, OnCopyScript)
	ON_BN_CLICKED(IDC_ADD_DEBUG, OnAddDebug)
	ON_BN_CLICKED(IDC_REMOVE_DEBUG, OnRemoveDebug)
	ON_BN_CLICKED(IDC_SCRIPT_MERGE, OnAutoMergeScripts)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_VERIFY, OnVerify)
	ON_BN_CLICKED(IDC_VERIFYALL, OnVerifyAll)
	ON_BN_CLICKED(IDC_PATCH_GC, OnPatchGC)
	ON_BN_CLICKED(IDC_AUTO_VERIFY, OnAutoVerify)
	ON_BN_CLICKED(IDC_COMPRESS, OnCompress)
	ON_BN_CLICKED(IDC_NEWICONS, OnNewIcons)
	ON_BN_CLICKED(IDC_DEEPSCAN, OnDisableDeepScan)
	ON_BN_CLICKED(IDC_REFRENCEMODE1, OnCheckByParameterForReference)
	ON_BN_CLICKED(IDC_DISABLEREFERENCE, OnDisableReferencesEntirely)
	ON_BN_CLICKED(IDC_CLEANSCRIPTNAME, OnCleanScriptName)
	ON_BN_CLICKED(IDC_FIND_NEXT, OnFindNext)
	ON_BN_CLICKED(IDC_SMART_COPY, OnSmartCopy)
	ON_BN_CLICKED(IDC_SAVE_ACTUAL, OnSaveActual)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_CLICKED(IDC_LOAD, OnLoad)
	ON_NOTIFY(NM_DBLCLK, IDC_SCRIPT_TREE, OnDblclkScriptTree)
	ON_NOTIFY(TVN_BEGINDRAG, IDC_SCRIPT_TREE, OnBegindragScriptTree)
	ON_COMMAND(ID_SCRIPTACTIVATE, OnScriptActivate)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_MOVE()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ScriptDialog message handlers

inline bool asciiStringContains(const AsciiString& haystack, const char* needle)
{
	if (!needle || !haystack.str()) return false;
	return strstr(haystack.str(), needle) != NULL;
}

AsciiString parseLineBreaks(const AsciiString& input)
{
	AsciiString output = input;
	const char* p = output.str();
	AsciiString result;

	while (*p) {
		if (*p == '\n') {
			result.concat("\r\n");
		} else {
			char temp[2] = {*p, '\0'};
			result.concat(temp);
		}
		++p;
	}
	return result;
}

bool alreadyListed(const AsciiString& usedByTag, const AsciiString& scriptName)
{
	const char* tagStr = usedByTag.str();
	const char* nameStr = scriptName.str();

	// simple substring search with comma or bracket after
	AsciiString pattern(", ");
	pattern.concat(scriptName);

	if (strstr(tagStr, pattern.str()) != NULL)
		return true;

	// also check start-of-line case
	pattern = "[Referenced in:";
	pattern.concat(scriptName);
	if (strstr(tagStr, pattern.str()) != NULL)
		return true;

	return false;
}

void ScriptDialog::OnSelchangedScriptTree(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	if (pNMTreeView->itemNew.hItem==NULL) {
		m_curSelection.IntToList(0);
		m_curSelection.m_objType = ListType::PLAYER_TYPE;
	} else {
		m_curSelection.IntToList(pNMTreeView->itemNew.lParam);
		DEBUG_ASSERTCRASH(m_curSelection.m_playerIndex <m_sides.getNumSides(),("")); 
		DEBUG_ASSERTCRASH(m_curSelection.m_objType != ListType::BOGUS_TYPE, ("")); 
	}
	if (!this->m_draggingTreeView) {
		pTree->SelectDropTarget(pNMTreeView->itemNew.hItem); 
	}
	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();

	CWnd *pWnd = GetDlgItem(IDC_EDIT_SCRIPT);
	pWnd->EnableWindow(pScript!=NULL || pGroup!=NULL);
	
	pWnd = GetDlgItem(IDC_COPY_SCRIPT);
	pWnd->EnableWindow(pScript!=NULL || pGroup!=NULL);

	pWnd = GetDlgItem(IDC_ADD_DEBUG);
	pWnd->EnableWindow(pScript!=NULL);

	pWnd = GetDlgItem(IDC_REMOVE_DEBUG);
	pWnd->EnableWindow(pScript!=NULL);

	pWnd = GetDlgItem(IDC_DELETE);
	pWnd->EnableWindow(m_curSelection.m_objType != ListType::PLAYER_TYPE);

	AsciiString scriptBurst;
	AsciiString scriptComment;
	AsciiString actionComment;
	AsciiString conditionComment;
	AsciiString scriptText;

	if (pScript) {
		actionComment = pScript->getActionComment();
		conditionComment = pScript->getConditionComment();
		scriptComment = pScript->getComment();
		scriptText = pScript->getUiText();

		Int burstSeconds = pScript->getDelayEvalSeconds();
		scriptBurst.format("%d", burstSeconds);  

		AsciiString usedByTag;
		AsciiString targetScriptName = pScript->getName();
		Bool foundUse = false;

		// 🔀 Toggle: choose between parameter-based or text-based reference detection
		bool checkByParameter = m_bCheckByParameter; 
		bool disableReference = m_bDisableReferences; 

		if(!disableReference){
			for (int i = 0; i < m_sides.getNumSides(); ++i) {
				ScriptList* pSL = m_sides.getSideInfo(i)->getScriptList();
				if (!pSL) continue;

				if (checkByParameter) {
					// --- Parameter-based search ---
					// Non-grouped scripts
					for (Script* s = pSL->getScript(); s; s = s->getNext()) {
						if (s == pScript) continue;
						bool referenced = false;

						// Conditions
						for (OrCondition* pOr = s->getOrCondition(); pOr && !referenced; pOr = pOr->getNextOrCondition()) {
							for (Condition* c = pOr->getFirstAndCondition(); c && !referenced; c = c->getNext()) {
								for (int p = 0; p < c->getNumParameters(); ++p) {
									Parameter* param = c->getParameter(p);
									if (param && (param->getParameterType() == Parameter::SCRIPT || param->getParameterType() == Parameter::SCRIPT_SUBROUTINE)  &&
										param->getString() == targetScriptName) {
										referenced = true;
										break;
									}
								}
							}
						}

						// Actions
						for (ScriptAction* a = s->getAction(); a && !referenced; a = a->getNext()) {
							for (int p = 0; p < a->getNumParameters(); ++p) {
								Parameter* param = a->getParameter(p);
								if (param && (param->getParameterType() == Parameter::SCRIPT || param->getParameterType() == Parameter::SCRIPT_SUBROUTINE)  &&
									param->getString() == targetScriptName) {
									referenced = true;
									break;
								}
							}
						}

						if (referenced && !alreadyListed(usedByTag, s->getName())) {
							if (foundUse) usedByTag.concat(", ");
							else foundUse = true;
							usedByTag.concat(s->getName());
						}
					}

					// Grouped scripts
					for (ScriptGroup* g = pSL->getScriptGroup(); g; g = g->getNext()) {
						for (Script* s = g->getScript(); s; s = s->getNext()) {
							if (s == pScript) continue;
							bool referenced = false;

							// Conditions
							for (OrCondition* pOr = s->getOrCondition(); pOr && !referenced; pOr = pOr->getNextOrCondition()) {
								for (Condition* c = pOr->getFirstAndCondition(); c && !referenced; c = c->getNext()) {
									for (int p = 0; p < c->getNumParameters(); ++p) {
										Parameter* param = c->getParameter(p);
										if (param && (param->getParameterType() == Parameter::SCRIPT || param->getParameterType() == Parameter::SCRIPT_SUBROUTINE)  &&
											param->getString() == targetScriptName) {
											referenced = true;
											break;
										}
									}
								}
							}

							// Actions
							for (ScriptAction* a = s->getAction(); a && !referenced; a = a->getNext()) {
								for (int p = 0; p < a->getNumParameters(); ++p) {
									Parameter* param = a->getParameter(p);
									if (param && (param->getParameterType() == Parameter::SCRIPT || param->getParameterType() == Parameter::SCRIPT_SUBROUTINE) &&
										param->getString() == targetScriptName) {
										referenced = true;
										break;
									}
								}
							}

							if (referenced && !alreadyListed(usedByTag, s->getName())) {
								if (foundUse) usedByTag.concat(", ");
								else foundUse = true;
								usedByTag.concat(s->getName());
							}
						}
					}
				} 
				else {
					// --- Text-based search (existing behavior) ---
					for (Script* s = pSL->getScript(); s; s = s->getNext()) {
						if (s == pScript) continue;
						AsciiString allText;
						allText.concat(s->getUiText());
						allText.concat(s->getComment());
						allText.concat(s->getActionComment());
						allText.concat(s->getConditionComment());

						CString content = allText.str();
						CString search = targetScriptName.str();

						if (content.Find(search) != -1 && !alreadyListed(usedByTag, s->getName())) {
							if (foundUse) usedByTag.concat(", ");
							else foundUse = true;
							usedByTag.concat(s->getName());
						}
					}

					for (ScriptGroup* g = pSL->getScriptGroup(); g; g = g->getNext()) {
						for (Script* s = g->getScript(); s; s = s->getNext()) {
							if (s == pScript) continue;
							AsciiString allText;
							allText.concat(s->getUiText());
							allText.concat(s->getComment());
							allText.concat(s->getActionComment());
							allText.concat(s->getConditionComment());

							CString content = allText.str();
							CString search = targetScriptName.str();

							if (content.Find(search) != -1 && !alreadyListed(usedByTag, s->getName())) {
								if (foundUse) usedByTag.concat(", ");
								else foundUse = true;
								usedByTag.concat(s->getName());
							}
						}
					}
				}
			}
		}

		if (foundUse) {
			AsciiString temp;
			temp.concat("[Referenced in] : ");
			temp.concat(usedByTag);
			usedByTag = temp;
		}

		if (!scriptComment.isEmpty()) {
			scriptComment.concat("\n\n");
		}

		if (!conditionComment.isEmpty()) {
			scriptComment.concat("[Condition Comment] : ");
			scriptComment.concat(conditionComment);
			scriptComment.concat("\n\n");
		}

		if (!actionComment.isEmpty()) {
			scriptComment.concat("[Action Comment] : ");
			scriptComment.concat(actionComment);
			scriptComment.concat("\n\n");
		}

		scriptComment.concat(usedByTag);
		scriptComment = parseLineBreaks(scriptComment);
	}

	pWnd = GetDlgItem(IDC_SCRIPT_COMMENT);
	pWnd->SetWindowText(scriptComment.str());

	pWnd = GetDlgItem(IDC_SCRIPT_DESCRIPTION);
	pWnd->SetWindowText(scriptText.str());

	*pResult = 0;
}


/* The purpose of these two functions is to allow
the inner class CSDTreeCtrl the ability to check
what Script and ScriptGroup belong to the cursor location */
Script *ScriptDialog::friend_getCurScript(void)
{
	return getCurScript();
}

ScriptGroup *ScriptDialog::friend_getCurGroup(void)
{
	return getCurGroup();
}

Script *ScriptDialog::getCurScript(void)
{
	if (m_curSelection.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE || m_curSelection.m_objType == ListType::SCRIPT_IN_GROUP_TYPE) {
		ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
		if (pSL) {
			Script *pScr=NULL;
			if (m_curSelection.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE) {
				pScr = pSL->getScript();
			}	else {
				Int groupNdx;
				ScriptGroup *pGroup = pSL->getScriptGroup();
				for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
					if (groupNdx == m_curSelection.m_groupIndex) {
						pScr = pGroup->getScript();
						break;
					}
				}
			}
			Int scriptNdx;
			for (scriptNdx = 0; pScr; scriptNdx++,pScr=pScr->getNext()) {
				if (scriptNdx == m_curSelection.m_scriptIndex) {
					return pScr;
				}
			}
		}
	} 
	return NULL;
}

ScriptGroup *ScriptDialog::getCurGroup(void)
{
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (m_curSelection.m_objType == ListType::PLAYER_TYPE) {
		return NULL;
	}
	if (m_curSelection.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE) {
		return NULL;
	}
	if (pSL) {
		Int groupNdx;
		ScriptGroup *pGroup = pSL->getScriptGroup();
		for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
			if (groupNdx == m_curSelection.m_groupIndex) {
				return pGroup;
			}
		}
	}
	return NULL;
}

/** Updates the warning flags in a script, & script conditions & actions. */
void ScriptDialog::updateScriptWarning(Script *pScript)
{
	pScript->setWarnings(false);
	OrCondition *pOr;
	for (pOr= pScript->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
		Condition *pCondition;
		for (pCondition = pOr->getFirstAndCondition(); pCondition; pCondition = pCondition->getNext()) {
			pCondition->setWarnings(false);
			Int i;
			for (i=0; i<pCondition->getNumParameters(); i++) {
				AsciiString warning;
				warning = EditParameter::getWarningText(pCondition->getParameter(i), FALSE);
				if (!warning.isEmpty()) {
					pScript->setWarnings(true);
					pCondition->setWarnings(true);
				}	
			}
		}
	}
	ScriptAction *pAction;
	for (pAction = pScript->getAction(); pAction; pAction = pAction->getNext()) {
		pAction->setWarnings(false);
		Int i;
		for (i=0; i<pAction->getNumParameters(); i++) {
			AsciiString warning;
			warning = EditParameter::getWarningText(pAction->getParameter(i), TRUE);
			if (!warning.isEmpty()) {
				pScript->setWarnings(true);
				pAction->setWarnings(true);
			}	
		}
	}
}

void ScriptDialog::OnPatchGC()
{
	int result = AfxMessageBox(
		"This will remove the 'GC_' prefix from object names "
		"(for example, GC_Chem_GLAInfantryRebel to Chem_GLAInfantryRebel).\n\n"
		"Use this if your scripts reference old GC_ objects and you want them "
		"to automatically point to their normal counterparts.\n\n"
		"Do you want to continue?",
		MB_YESNO | MB_ICONQUESTION
	);

	if (result == IDYES)
	{
		checkParametersForGC();
		updateIcons(TVI_ROOT);
	}
/*  //Put up a dialog asking for search/replace parameters instead of hard-coded GC_ prefix.
	ReplaceParameter editDlg();
	if (IDOK==editDlg.DoModal())
	{	

	}*/
}
void ScriptDialog::OnFindNext()
{
    UpdateData(TRUE);

    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_SCRIPT_SEARCH);
    CString searchText;
    pEdit->GetWindowText(searchText);
    searchText.MakeLower();

    CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
    HTREEITEM hItem = m_lastFoundItem ? 
        pTree->GetNextItem(m_lastFoundItem, TVGN_NEXT) : 
        pTree->GetRootItem();

    while (hItem)
    {
        CString text = pTree->GetItemText(hItem);
        CString lowerText = text; lowerText.MakeLower();

        bool match = (lowerText.Find(searchText) != -1);

        if (!match)
        {
            // --- Deep search inside the script ---
            ListType lt;
            lt.IntToList(pTree->GetItemData(hItem));

            if (lt.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE ||
                lt.m_objType == ListType::SCRIPT_IN_GROUP_TYPE)
            {
                m_curSelection = lt; // temporarily set selection to use getCurScript
                Script* pScr = getCurScript();
                if (pScr)
                {
                    AsciiString content = pScr->getComment();
                    content.concat(pScr->getUiText());
                    CString lowerContent = content.str();
                    lowerContent.MakeLower();

                    if (lowerContent.Find(searchText) != -1)
                        match = true;

                    // Optionally scan conditions & actions too:
                    for (OrCondition* pOr = pScr->getOrCondition(); pOr && !match; pOr = pOr->getNextOrCondition())
                    {
                        for (Condition* c = pOr->getFirstAndCondition(); c && !match; c = c->getNext())
                        {
                            for (int p = 0; p < c->getNumParameters(); ++p)
                            {
                                Parameter* param = c->getParameter(p);
                                CString paramStr = param->getString().str();
                                paramStr.MakeLower();
                                if (paramStr.Find(searchText) != -1) {
                                    match = true;
                                    break;
                                }
                            }
                        }
                    }
                    for (ScriptAction* a = pScr->getAction(); a && !match; a = a->getNext())
                    {
                        for (int p = 0; p < a->getNumParameters(); ++p)
                        {
                            Parameter* param = a->getParameter(p);
                            CString paramStr = param->getString().str();
                            paramStr.MakeLower();
                            if (paramStr.Find(searchText) != -1) {
                                match = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (match)
        {
            m_lastFoundItem = hItem;
            pTree->SelectItem(hItem);
            pTree->EnsureVisible(hItem);
            return;
        }

        // Traverse to next item
        HTREEITEM hChild = pTree->GetChildItem(hItem);
        if (hChild)
            hItem = hChild;
        else {
            while (hItem && !pTree->GetNextSiblingItem(hItem))
                hItem = pTree->GetParentItem(hItem);
            if (hItem)
                hItem = pTree->GetNextSiblingItem(hItem);
        }
    }

    // MessageBox("No more matches found.", "Search", MB_OK | MB_ICONINFORMATION);
    MessageBeep(MB_ICONWARNING);
    m_lastFoundItem = NULL;
}

/**Force a pass over all the scripts to make sure no warnings.  I moved this
to user control because this function is VERY slow. 7-15-03 -MW*/
void ScriptDialog::OnVerify()
{
	// Flag to indicate if cache was successfully loaded
	Bool loadedFromCache = false;

	if(m_autoUpdateWarnings){
		// Try loading cached warning state first
		if (LoadScriptWarningsState()) {
			DEBUG_LOG(("ScriptDialog: Loaded script warning state from cache.\n"));
			loadedFromCache = true;
		} else {
			DEBUG_LOG(("ScriptDialog: No valid cache found. Will update warnings normally.\n"));
		}

		// If cache didn't load, fall back to the expensive update
		// if (!g_didScriptWarningsUpdate) {
			if (!loadedFromCache) {
				updateWarnings(true);
				DEBUG_LOG(("ScriptDialog: Ran updateWarnings(true) due to missing cache.\n"));
			}
			// g_didScriptWarningsUpdate = true;
		// }
	}

	updateIcons(TVI_ROOT);
}

void ScriptDialog::OnVerifyAll()
{
	updateWarnings(true);
	updateIcons(TVI_ROOT);

	// for (int i = 0; i < m_sides.getNumSides(); ++i) {
    //     ScriptList* pSL = m_sides.getSideInfo(i)->getScriptList();
    //     if (pSL)
    //         reloadPlayer(i, pSL); // rebuild branch, re-applies labels
    // }
}

void ScriptDialog::OnSmartCopy()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_SMART_COPY);
	m_bSmartCopyEnabled = (pButton->GetCheck() == 1);
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopy", m_bSmartCopyEnabled ? 1 : 0);


	if (m_bSmartCopyEnabled)
	{
		AfxMessageBox(
			"This feature will auto increment values on your copied script's parameters\n\n"
			"Example:   Add  1  to counter 'Counter01' -> click copy ->   Add  1  to counter 'Counter02'\n\n"
			"Note: This does not support all parameters. Contact Adriane if you want other parameters to be supported adios.",
			MB_OK | MB_ICONINFORMATION
		);
	}

}

void ScriptDialog::OnAutoMergeScripts()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_SCRIPT_MERGE);
	m_bAutoMergeScripts = (pButton->GetCheck() == 1);
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "AutoMergeScripts", m_bAutoMergeScripts ? 1 : 0);

	if (m_bAutoMergeScripts)
	{
		AfxMessageBox(
			"Auto-Merge allows you to combine items using drag and drop.\n\n"
			"> To merge scripts: hold CTRL, then drag one script onto another.\n"
			"> To merge script folders: hold CTRL, then drag one folder onto another.\n\n"
			"Without holding CTRL, drag and drop will work normally (move/reorder only).",
			MB_OK | MB_ICONINFORMATION
		);
	}
}

void ScriptDialog::OnAutoVerify()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_AUTO_VERIFY);
	m_autoUpdateWarnings=(pButton->GetCheck()==1);
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "AutoVerifyScripts", m_autoUpdateWarnings?1:0);
	//if user wants to check warnings manually, enable the verify button
	CWnd *pWnd = GetDlgItem(IDC_VERIFY);
	pWnd->EnableWindow(!m_autoUpdateWarnings);
}

void ScriptDialog::OnNewIcons()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_NEWICONS);
	m_bNewIcons = (pButton->GetCheck() == 1);
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "NewIcons", m_bNewIcons ? 1 : 0);

	CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	if (pTree)
	{
		// Delete old image list if it exists
		if (m_imageList.GetSafeHandle())
		{
			m_imageList.DeleteImageList();
		}

		if (m_bNewIcons)
			m_imageList.Create(IDB_FOLDERSCRIPTB, 16, 2, ILC_COLOR4); // new icons
		else
			m_imageList.Create(IDB_FOLDERSCRIPT, 16, 2, ILC_COLOR4); // default icons

		pTree->SetImageList(&m_imageList, TVSIL_STATE);
	}
}

void ScriptDialog::OnCleanScriptName()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_CLEANSCRIPTNAME);
	m_bCleanScriptName = (pButton->GetCheck() == 1);
    ::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "CleanStringName", m_bCleanScriptName ? 1 : 0);

	if (m_bCleanScriptName && !m_updating)
	{
		AfxMessageBox(
			"This feature will simplify script names to reduce clutter.\n\n"
			"If a script exists in all three difficulties (Easy, Normal, and Hard), "
			"the difficulty tags will be hidden since it's the same across all.\n\n"
			"However, if a script only exists in one or two difficulties, "
			"the difficulty tags will still be shown to make that clear.",
			MB_OK | MB_ICONINFORMATION
		);
	}

	// Rebuilds the tree
	CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
    if (!pTree) return;

    pTree->SetRedraw(FALSE); // avoid flicker

    for (int i = 0; i < m_sides.getNumSides(); ++i)
    {
        ScriptList* pSL = m_sides.getSideInfo(i)->getScriptList();
        if (pSL)
            reloadPlayer(i, pSL);
    }

    pTree->SetRedraw(TRUE);
    pTree->Invalidate();
    pTree->UpdateWindow();
}

void ScriptDialog::OnDisableDeepScan()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_DEEPSCAN);
	m_bDisableDeepScan = (pButton->GetCheck() == 1);
    ::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "DisableDeepScan", m_bDisableDeepScan ? 1 : 0);
}

void ScriptDialog::OnCheckByParameterForReference()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_REFRENCEMODE1);
	m_bCheckByParameter = (pButton->GetCheck() == 1);
    ::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "ReferenceCheckByParameter", m_bCheckByParameter ? 1 : 0);
}

void ScriptDialog::OnDisableReferencesEntirely()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_DISABLEREFERENCE);
	m_bDisableReferences = (pButton->GetCheck() == 1);
    ::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "DisableReferences", m_bDisableReferences ? 1 : 0);
}

void ScriptDialog::OnCompress()
{
    CButton *pButton = (CButton*)GetDlgItem(IDC_COMPRESS);
    m_bCompressed = (pButton->GetCheck() == 1);
    ::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", m_bCompressed ? 1 : 0);

    CTreeCtrl* pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
    CEdit* pComment = (CEdit*)GetDlgItem(IDC_SCRIPT_COMMENT);
    CEdit* pDescription = (CEdit*)GetDlgItem(IDC_SCRIPT_DESCRIPTION);

    if (!pTree || !pComment || !pDescription)
        return;

    // Always create the small font (used permanently on comments & description)
    if (m_treeFont.GetSafeHandle())
        m_treeFont.DeleteObject();

    m_treeFont.CreateFont(
        14, 0, 0, 0,
        FW_MEDIUM,
        FALSE, FALSE, 0,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        _T("Segoe UI")
    );

    // Comment + Description always use new font
    pComment->SetFont(&m_treeFont);
    pDescription->SetFont(&m_treeFont);

    // Tree uses font only when compressed
    if (m_bCompressed)
    {
        pTree->SetFont(&m_treeFont);
    }
    else if (m_pOldFont) // only tree reverts if desired, safe keep
    {
        pTree->SetFont(m_pOldFont);
    }

    pTree->Invalidate();   pTree->UpdateWindow();
    pComment->Invalidate(); pComment->UpdateWindow();
    pDescription->Invalidate(); pDescription->UpdateWindow();
}

// Load em cached status baby Adriane [Deathscythe]
Bool ScriptDialog::LoadScriptWarningsState()
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) {
		DEBUG_LOG(("LoadScriptWarningsState: No active doc!\n"));
		return false;
	}

	DEBUG_LOG(("LoadScriptWarningsState: Map Path = %s\n", pDoc->getMapPath()));

	CString path = pDoc->getMapPath();
	if (path.IsEmpty()) {
		DEBUG_LOG(("LoadScriptWarningsState: Empty map path.\n"));
		return false;
	}

	int lastSlash = path.ReverseFind('\\');
	if (lastSlash != -1)
		path = path.Left(lastSlash + 1);
	else {
		DEBUG_LOG(("LoadScriptWarningsState: Malformed path - no backslash found.\n"));
		return false;
	}

	CString cacheFile = path + "AdrianeScriptWarningsCache.txt";
	DEBUG_LOG(("LoadScriptWarningsState: Final cache file = %s\n", cacheFile));

	CStdioFile file;
	if (!file.Open(cacheFile, CFile::modeRead | CFile::typeText)) {
		DEBUG_LOG(("LoadScriptWarningsState: Failed to open file for reading.\n"));
		return false;
	}

	CString line;
	std::map<std::string, Bool> warningMap;

	while (file.ReadString(line))
	{
		int delim = line.Find(',');
		if (delim > 0)
		{
			CString key = line.Left(delim);
			CString value = line.Mid(delim + 1);
			value.TrimLeft(); value.TrimRight();

			warningMap[(const char*)key] = (value == "1");
		}
		else {
			DEBUG_LOG(("LoadScriptWarningsState: Malformed line: %s\n", line));
		}
	}
	file.Close();

	SidesList* sidesListP = TheSidesList;
	if (m_staticThis) sidesListP = &m_staticThis->m_sides;

	for (int i = 0; i < sidesListP->getNumSides(); ++i)
	{
		ScriptList* pSL = sidesListP->getSideInfo(i)->getScriptList();
		if (!pSL) continue;

		char sideIndexStr[16];
		sprintf(sideIndexStr, "%d", i);

		Script* pScr;
		for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext())
		{
			std::string key = std::string(sideIndexStr) + "|" + pScr->getName().str();
			if (warningMap.find(key) != warningMap.end())
			{
				pScr->setWarnings(warningMap[key]);
				pScr->setDirty(false);

				if(m_staticThis && m_staticThis->m_bDisableDeepScan != 1){
					appendWarningHintLazy(pScr);
				}

				// Expensive 
				// if (pScr->hasWarnings()) {
				// // 	updateScriptWarning(pScr);  // Force regen of warning messages like [???]
				// }
			}
		}

		ScriptGroup* pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext())
		{
			for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext())
			{
				std::string key = std::string(sideIndexStr) + "|" + pScr->getName().str();
				if (warningMap.find(key) != warningMap.end())
				{
					pScr->setWarnings(warningMap[key]);
					pScr->setDirty(false);

					if(m_staticThis && m_staticThis->m_bDisableDeepScan != 1){
						appendWarningHintLazy(pScr);
					}

					// Expensive
					// if (pScr->hasWarnings()) {
					// 	// updateScriptWarning(pScr);  // Force regen of warning messages like [???]
					// }
				}
			}
		}
	}

	DEBUG_LOG(("LoadScriptWarningsState: Finished loading.\n"));
	return true;
}


void ScriptDialog::appendWarningHintLazy(Script* pScript)
{
	if (!pScript || !pScript->hasWarnings()) return;

	for (OrCondition* pOr = pScript->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
		for (Condition* pCond = pOr->getFirstAndCondition(); pCond; pCond = pCond->getNext()) {
			for (int i = 0; i < pCond->getNumParameters(); ++i) {
				Parameter* param = pCond->getParameter(i);
				AsciiString warn = EditParameter::getWarningText(param, FALSE);
				if (!warn.isEmpty()) {
					pCond->setWarnings(true);
					break; // one warning is enough
				}
			}
		}
	}

	for (ScriptAction* pAct = pScript->getAction(); pAct; pAct = pAct->getNext()) {
		for (int i = 0; i < pAct->getNumParameters(); ++i) {
			Parameter* param = pAct->getParameter(i);
			AsciiString warn = EditParameter::getWarningText(param, TRUE);
			if (!warn.isEmpty()) {
				pAct->setWarnings(true);
				break;
			}
		}
	}
}

void ScriptDialog::SaveScriptWarningsState()
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;

	DEBUG_LOG(("Map Path %s\n", pDoc->getMapPath()));

	CString path = pDoc->getMapPath();
	if (path.IsEmpty()) return;

	int lastSlash = path.ReverseFind('\\');
	if (lastSlash != -1)
		path = path.Left(lastSlash + 1);
	else
		return;

	CString cacheFile = path + "AdrianeScriptWarningsCache.txt";

	CStdioFile file;
	if (!file.Open(cacheFile, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
		return;

	SidesList* sidesListP = TheSidesList;
	if (m_staticThis) sidesListP = &m_staticThis->m_sides;

	for (int i = 0; i < sidesListP->getNumSides(); ++i)
	{
		ScriptList* pSL = sidesListP->getSideInfo(i)->getScriptList();
		if (!pSL) continue;

		char sideIndexStr[16];
		sprintf(sideIndexStr, "%d", i);

		Script* pScr;
		for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext())
		{
			CString line;
			line.Format("%s|%s,%d\n", sideIndexStr, pScr->getName().str(), pScr->hasWarnings() ? 1 : 0);
			file.WriteString(line);
		}

		ScriptGroup* pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext())
		{
			for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext())
			{
				CString line;
				line.Format("%s|%s,%d\n", sideIndexStr, pScr->getName().str(), pScr->hasWarnings() ? 1 : 0);
				file.WriteString(line);
			}
		}
	}

	file.Close();
}


/** Updates the warning flags in the scripts, script groups & script conditions & actions. */
void ScriptDialog::updateWarnings(Bool forceUpdate)
{
    // Only skip if auto-update is disabled AND we're NOT forcing the update
    if (m_staticThis && !m_staticThis->m_autoUpdateWarnings && !forceUpdate)
        return;

	SidesList *sidesListP = TheSidesList;
	Int i;
	if (m_staticThis) sidesListP = &m_staticThis->m_sides;
	for (i=0; i<sidesListP->getNumSides(); i++) {
		ScriptList *pSL = sidesListP->getSideInfo(i)->getScriptList();
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr=pScr->getNext()) {
			if (pScr->isDirty() || forceUpdate) {
				updateScriptWarning(pScr);
				pScr->setDirty(false);
			}
		}
		ScriptGroup *pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup=pGroup->getNext()) {
			pGroup->setWarnings(false);
			for (pScr = pGroup->getScript(); pScr; pScr=pScr->getNext()) {
				if (pScr->isDirty() || forceUpdate) {
					updateScriptWarning(pScr);
					pScr->setDirty(false);
				}
				if (pScr->hasWarnings()) {
					pGroup->setWarnings(true);
				}
			}
		}
	}	

    PlaySound("data\\editor\\audio\\finished.wav", NULL, SND_FILENAME | SND_ASYNC);
}

extern AsciiString ConvertToNonGCName(AsciiString name, Bool checkTemplate=true);

void ScriptDialog::patchScriptParametersForGC(Script *pScript)
{
	AsciiString swapString;
	pScript->setWarnings(false);
	OrCondition *pOr;
	for (pOr= pScript->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
		Condition *pCondition;
		for (pCondition = pOr->getFirstAndCondition(); pCondition; pCondition = pCondition->getNext()) {
			pCondition->setWarnings(false);
			Int i;
			for (i=0; i<pCondition->getNumParameters(); i++) {
				AsciiString warning;
				Parameter *pParm = pCondition->getParameter(i);
				warning = EditParameter::getWarningText(pParm, FALSE);
				if (!warning.isEmpty()) {
					if (pParm->getParameterType() == Parameter::OBJECT_TYPE)
					{	//see if removing the GC prefix fixes this warning:
						AsciiString uiString = pParm->getString();
						if (uiString.isEmpty()) 
							uiString = "???";
						if (uiString.startsWith("GC_"))
						{	swapString = ConvertToNonGCName(uiString, false);
							pParm->friend_setString(swapString);
							warning = EditParameter::getWarningText(pParm, FALSE);
							if (!warning.isEmpty())
							{	//Removing GC prefix didn't help, so restore original
								pParm->friend_setString(uiString);
							}
							else
								continue;	//warning was fixed so leave swapped parameter.
						}
					}
					pScript->setWarnings(true);
					pCondition->setWarnings(true);
				}	
			}
		}
	}
	ScriptAction *pAction;
	for (pAction = pScript->getAction(); pAction; pAction = pAction->getNext()) {
		pAction->setWarnings(false);
		Int i;
		for (i=0; i<pAction->getNumParameters(); i++) {
			AsciiString warning;
			Parameter *pParm=pAction->getParameter(i);
			warning = EditParameter::getWarningText(pParm, TRUE);
			if (!warning.isEmpty()) {
				if (pParm->getParameterType() == Parameter::OBJECT_TYPE)
				{	//see if removing the GC prefix fixes this warning:
					AsciiString uiString = pParm->getString();
					if (uiString.isEmpty()) 
						uiString = "???";
					if (uiString.startsWith("GC_"))
					{	swapString = ConvertToNonGCName(uiString,false);
						pParm->friend_setString(swapString);
						warning = EditParameter::getWarningText(pParm, FALSE);
						if (!warning.isEmpty())
						{	//Removing GC prefix didn't help, so restore original
							pParm->friend_setString(uiString);
						}
						else
							continue;	//warning was fixed so leave swapped parameter.
					}
				}
				pScript->setWarnings(true);
				pAction->setWarnings(true);
			}	
		}
	}
}

/*Checks all script parameters for obsolete values (example: mission disk using GC_ templates)*/
void ScriptDialog::checkParametersForGC(void)
{
	SidesList *sidesListP = TheSidesList;
	Int i;
	if (m_staticThis) sidesListP = &m_staticThis->m_sides;
	for (i=0; i<sidesListP->getNumSides(); i++) {
		ScriptList *pSL = sidesListP->getSideInfo(i)->getScriptList();
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr=pScr->getNext()) {
			updateScriptWarning(pScr);
			if (pScr->hasWarnings())
			{	//check if this is using invalid GC parameters
				patchScriptParametersForGC(pScr);
			}
		}
		ScriptGroup *pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup=pGroup->getNext()) {
			pGroup->setWarnings(false);
			for (pScr = pGroup->getScript(); pScr; pScr=pScr->getNext()) {
				updateScriptWarning(pScr);
				if (pScr->hasWarnings()) {
					//check if this is using invalid GC parameters.
					patchScriptParametersForGC(pScr);
					if (pScr->hasWarnings())	//patching may have removed warning
						pGroup->setWarnings(true);
				}
			}
		}
	}	
}

BOOL ScriptDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_bSmartCopyEnabled=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopy", 0);

	CButton *pButton = (CButton*)GetDlgItem(IDC_SMART_COPY);
	pButton->SetCheck(m_bSmartCopyEnabled ? 1:0);

	m_bAutoMergeScripts=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "AutoMergeScripts", 0);

	pButton = (CButton*)GetDlgItem(IDC_SCRIPT_MERGE);
	pButton->SetCheck(m_bAutoMergeScripts ? 1:0);

	m_autoUpdateWarnings=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "AutoVerifyScripts", 1);

	pButton = (CButton*)GetDlgItem(IDC_AUTO_VERIFY);
	pButton->SetCheck(m_autoUpdateWarnings ? 1:0);

	//if user wants to check warnings manually, enable the verify button
	CWnd *pWnd = GetDlgItem(IDC_VERIFY);
	pWnd->EnableWindow(!m_autoUpdateWarnings);

	m_staticThis = this;

	m_bDisableReferences=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "DisableReferences", 0);
	pButton = (CButton*)GetDlgItem(IDC_DISABLEREFERENCE);
	pButton->SetCheck(m_bDisableReferences ? 1:0);
	OnDisableReferencesEntirely();

	m_bCheckByParameter=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "ReferenceCheckByParameter", 1);
	pButton = (CButton*)GetDlgItem(IDC_REFRENCEMODE1);
	pButton->SetCheck(m_bCheckByParameter ? 1:0);
	OnCheckByParameterForReference();

	m_bDisableDeepScan=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "DisableDeepScan", 0);
	pButton = (CButton*)GetDlgItem(IDC_DEEPSCAN);
	pButton->SetCheck(m_bDisableDeepScan ? 1:0);
	OnDisableDeepScan();

	m_bCleanScriptName=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CleanStringName", 1);
	pButton = (CButton*)GetDlgItem(IDC_CLEANSCRIPTNAME);
	pButton->SetCheck(m_bCleanScriptName ? 1:0);
	OnCleanScriptName();

	m_updating = false;

	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);

	// replace current Tree Dialog with CSDTreeCtrl

	CRect rect;
	mTree = new CSDTreeCtrl;
	pTree->GetWindowRect(&rect);
	ScreenToClient(&rect);

	DWORD style = pTree->GetStyle();
	mTree->Create(style, rect, this, IDC_SCRIPT_TREE);
	pTree->DestroyWindow();

	pTree = (CTreeCtrl*) GetDlgItem(IDC_SCRIPT_TREE);
	// pTree should == mTree now.

	Bool didSelect = false;
	ScriptList::updateDefaults();
	m_sides = *TheSidesList;
	EditParameter::setCurSidesList(&m_sides);
	Int i;

	// Flag to indicate if cache was successfully loaded
	Bool loadedFromCache = false;

	if(m_autoUpdateWarnings){
		// Try loading cached warning state first
		if (LoadScriptWarningsState()) {
			DEBUG_LOG(("ScriptDialog: Loaded script warning state from cache.\n"));
			loadedFromCache = true;
			updateWarnings(true);
			updateIcons(TVI_ROOT);
		} else {
			DEBUG_LOG(("ScriptDialog: No valid cache found. Will update warnings normally.\n"));
		}

		// If cache didn't load, fall back to the expensive update
		// if (!g_didScriptWarningsUpdate) {
			if (!loadedFromCache) {
				updateWarnings(true);
				DEBUG_LOG(("ScriptDialog: Ran updateWarnings(true) due to missing cache.\n"));
			}
			// g_didScriptWarningsUpdate = true;
		// }
	}

	// ============================================================================
	// FORCE RENAME UNNAMED SCRIPTS AND GROUPS
	// ============================================================================

	Int totalRenamedScripts = 0;
	Int totalRenamedGroups = 0;
	Int unnamedScriptCounter = 1;
	Int unnamedGroupCounter = 1;

	for (i = 0; i < m_sides.getNumSides(); i++) {
		ScriptList *pSL = m_sides.getSideInfo(i)->getScriptList();
		if (!pSL) continue;
		
		Dict *d = m_sides.getSideInfo(i)->getDict();
		AsciiString playerName = d->getAsciiString(TheKey_playerName);
		if (playerName.isEmpty()) playerName = NEUTRAL_NAME_STR;
		
		// Fix ungrouped scripts
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext()) {
			if (pScr->getName().isEmpty()) {
				AsciiString newName;
				newName.format("[UNNAMED_SCRIPT_%d]", unnamedScriptCounter);
				pScr->setName(newName);
				totalRenamedScripts++;
				unnamedScriptCounter++;
				
				DEBUG_LOG(("Renamed unnamed script in %s to: %s\n", playerName.str(), newName.str()));
			}
		}
		
		// Fix grouped scripts
		ScriptGroup *pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext()) {
			
			// Fix unnamed group
			if (pGroup->getName().isEmpty()) {
				AsciiString newName;
				newName.format("[UNNAMED_GROUP_%d]", unnamedGroupCounter);
				pGroup->setName(newName);
				totalRenamedGroups++;
				unnamedGroupCounter++;
				
				DEBUG_LOG(("Renamed unnamed group in %s to: %s\n", playerName.str(), newName.str()));
			}
			
			// Fix scripts within the group
			for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext()) {
				if (pScr->getName().isEmpty()) {
					AsciiString newName;
					newName.format("[UNNAMED_SCRIPT_%d]", unnamedScriptCounter);
					pScr->setName(newName);
					totalRenamedScripts++;
					unnamedScriptCounter++;
					
					AsciiString groupName = pGroup->getName();
					DEBUG_LOG(("Renamed unnamed script in group '%s' (%s) to: %s\n", 
							groupName.str(), playerName.str(), newName.str()));
				}
			}
		}
	}

	// Show notification if any were renamed
	if (totalRenamedScripts > 0 || totalRenamedGroups > 0) {
		CString msg;
		msg.Format("Auto-renamed %d unnamed script(s) and %d unnamed group(s).\n\n"
				"These items are now visible in the tree view with [UNNAMED_*] prefix.\n\n"
				"You can now delete them or rename them properly.\n\n"
				"Check the debug output for details.",
				totalRenamedScripts, totalRenamedGroups);
		AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
		
		DEBUG_LOG(("\n=== AUTO-RENAME SUMMARY ===\n"));
		DEBUG_LOG(("Total Scripts Renamed: %d\n", totalRenamedScripts));
		DEBUG_LOG(("Total Groups Renamed: %d\n", totalRenamedGroups));
		DEBUG_LOG(("===========================\n\n"));
	}


	if (pTree) {
		m_imageList.Create(IDB_FOLDERSCRIPT, 16, 2, ILC_COLOR4);
		pTree->SetImageList(&m_imageList, TVSIL_STATE);
		for (i=0; i<m_sides.getNumSides(); i++) {
			HTREEITEM hItem = addPlayer(i);
			if (!didSelect && hItem != NULL) {
				pTree->SelectItem(hItem);
				didSelect = true;
			}
		}
		pTree->SetFocus();
	}

	CRect top;
	GetWindowRect(&top);
	top.top = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "Top", top.top);
	top.left =::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "Left", top.left);
	SetWindowPos(NULL, top.left, top.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	

	m_bCompressed=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", 1);
	pButton = (CButton*)GetDlgItem(IDC_COMPRESS);
	pButton->SetCheck(m_bCompressed ? 1:0);
	OnCompress();

	m_bNewIcons=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "NewIcons", 1);
	pButton = (CButton*)GetDlgItem(IDC_NEWICONS);
	pButton->SetCheck(m_bNewIcons ? 1:0);
	OnNewIcons();


#if 0 // Debug: Report unnamed scripts
	AsciiString unnamedReport;
	Int totalUnnamedScripts = 0;
	Int totalUnnamedGroups = 0;

	for (i = 0; i < m_sides.getNumSides(); i++) {
		ScriptList *pSL = m_sides.getSideInfo(i)->getScriptList();
		if (!pSL) continue;
		
		Dict *d = m_sides.getSideInfo(i)->getDict();
		AsciiString playerName = d->getAsciiString(TheKey_playerName);
		if (playerName.isEmpty()) playerName = NEUTRAL_NAME_STR;
		
		Bool playerHasUnnamed = false;
		AsciiString playerReport;
		
		// Check ungrouped scripts
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext()) {
			if (pScr->getName().isEmpty()) {
				if (!playerHasUnnamed) {
					playerReport.format("\n[%s]\n", playerName.str());
					playerHasUnnamed = true;
				}
				
				totalUnnamedScripts++;
				
				// Build script details
				AsciiString scriptInfo;
				scriptInfo.format("  - Unnamed Script (ungrouped)\n");
				
				// Count conditions
				Int conditionCount = 0;
				for (OrCondition *pOr = pScr->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
					for (Condition *c = pOr->getFirstAndCondition(); c; c = c->getNext()) {
						conditionCount++;
					}
				}
				
				// Count true actions
				Int trueActionCount = 0;
				ScriptAction *pTrueAction = pScr->getAction();
				while (pTrueAction) {
					trueActionCount++;
					pTrueAction = pTrueAction->getNext();
				}
				
				// Count false actions
				Int falseActionCount = 0;
				ScriptAction *pFalseAction = pScr->getFalseAction();
				while (pFalseAction) {
					falseActionCount++;
					pFalseAction = pFalseAction->getNext();
				}
				
				AsciiString details;
				details.format("    Conditions: %d, True Actions: %d, False Actions: %d\n", 
							conditionCount, trueActionCount, falseActionCount);
				scriptInfo.concat(details);
				
				// Add comment if available
				if (!pScr->getComment().isEmpty()) {
					AsciiString comment = "    Comment: ";
					AsciiString fullComment = pScr->getComment();
					
					// Truncate if too long (manually)
					if (fullComment.getLength() > 80) {
						const char* commentStr = fullComment.str();
						char truncated[81];
						strncpy(truncated, commentStr, 77);
						truncated[77] = '\0';
						comment.concat(truncated);
						comment.concat("...");
					} else {
						comment.concat(fullComment);
					}
					comment.concat("\n");
					scriptInfo.concat(comment);
				}

				playerReport.concat(scriptInfo);
			}
		}
		
		// Check grouped scripts
		ScriptGroup *pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext()) {
			
			// Check if group itself is unnamed
			if (pGroup->getName().isEmpty()) {
				if (!playerHasUnnamed) {
					playerReport.format("\n[%s]\n", playerName.str());
					playerHasUnnamed = true;
				}
				totalUnnamedGroups++;
				playerReport.concat("  - Unnamed Script Group\n");
			}
			
			// Check scripts within the group
			for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext()) {
				if (pScr->getName().isEmpty()) {
					if (!playerHasUnnamed) {
						playerReport.format("\n[%s]\n", playerName.str());
						playerHasUnnamed = true;
					}
					
					totalUnnamedScripts++;
					
					// Build script details
					AsciiString scriptInfo;
					AsciiString groupName = pGroup->getName().isEmpty() ? "Unnamed Group" : pGroup->getName();
					scriptInfo.format("  - Unnamed Script (in group: %s)\n", groupName.str());
					
					// Count conditions
					Int conditionCount = 0;
					for (OrCondition *pOr = pScr->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
						for (Condition *c = pOr->getFirstAndCondition(); c; c = c->getNext()) {
							conditionCount++;
						}
					}
					
					// Count true actions
					Int trueActionCount = 0;
					ScriptAction *pTrueAction = pScr->getAction();
					while (pTrueAction) {
						trueActionCount++;
						pTrueAction = pTrueAction->getNext();
					}
					
					// Count false actions
					Int falseActionCount = 0;
					ScriptAction *pFalseAction = pScr->getFalseAction();
					while (pFalseAction) {
						falseActionCount++;
						pFalseAction = pFalseAction->getNext();
					}
					
					AsciiString details;
					details.format("    Conditions: %d, True Actions: %d, False Actions: %d\n", 
								conditionCount, trueActionCount, falseActionCount);
					scriptInfo.concat(details);
					
					// Add comment if available
					if (!pScr->getComment().isEmpty()) {
						AsciiString comment = "    Comment: ";
						AsciiString fullComment = pScr->getComment();
						
						// Truncate if too long (manually)
						if (fullComment.getLength() > 80) {
							const char* commentStr = fullComment.str();
							char truncated[81];
							strncpy(truncated, commentStr, 77);
							truncated[77] = '\0';
							comment.concat(truncated);
							comment.concat("...");
						} else {
							comment.concat(fullComment);
						}
						comment.concat("\n");
						scriptInfo.concat(comment);
					}
					
					playerReport.concat(scriptInfo);
				}
			}
		}
		
		if (playerHasUnnamed) {
			unnamedReport.concat(playerReport);
		}
	}

	// Display report if any unnamed scripts/groups found
	if (totalUnnamedScripts > 0 || totalUnnamedGroups > 0) {
		AsciiString header;
		header.format("=== UNNAMED SCRIPTS REPORT ===\n");
		header.concat("Found unnamed scripts/groups that may cause issues:\n");
		header.concat("---------------------------------------------\n");
		
		AsciiString summary;
		summary.format("\nTotal Unnamed Scripts: %d\n", totalUnnamedScripts);
		summary.concat("Total Unnamed Groups: ");
		char buf[32];
		sprintf(buf, "%d", totalUnnamedGroups);
		summary.concat(buf);
		summary.concat("\n\n");
		summary.concat("These scripts will not appear in the tree view and cannot be referenced.\n");
		summary.concat("Please assign names to them or delete them if they are not needed.");
		
		AsciiString fullReport = header;
		fullReport.concat(unnamedReport);
		fullReport.concat(summary);
		
		// Log to debug output
		DEBUG_LOG(("%s\n", fullReport.str()));
		
		// Show message box with option to see full report
		CString msg;
		msg.Format("Found %d unnamed script(s) and %d unnamed group(s).\n\n"
				"These items will not appear in the tree view.\n\n"
				"Check the WorldBuilder debug output window for details.",
				totalUnnamedScripts, totalUnnamedGroups);
		AfxMessageBox(msg, MB_OK | MB_ICONWARNING);
	}
#endif
	
	return FALSE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

HTREEITEM ScriptDialog::addPlayer(Int playerIndx)
{

	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	TVINSERTSTRUCT ins;
	Dict *d = m_sides.getSideInfo(playerIndx)->getDict();
	AsciiString name = d->getAsciiString(TheKey_playerName);
	UnicodeString uni = d->getUnicodeString(TheKey_playerDisplayName);
	AsciiString fmt;
	if (name.isEmpty())
		fmt.format("%s", NEUTRAL_NAME_STR);
	else
		fmt.format("%s",name.str());
	::memset(&ins, 0, sizeof(ins));
	ListType lt;
	lt.m_objType=ListType::PLAYER_TYPE;
	lt.m_playerIndex = playerIndx;
	ins.hParent = TVI_ROOT;
	ins.hInsertAfter = TVI_LAST;
	ins.item.mask = TVIF_PARAM|TVIF_TEXT|TVIF_STATE;
	ins.item.state = INDEXTOSTATEIMAGEMASK(1);
	ins.item.stateMask = TVIS_STATEIMAGEMASK ;
	ins.item.lParam = lt.ListToInt();
	ins.item.pszText = (char *)fmt.str();
	ins.item.cchTextMax = 0;				
	HTREEITEM hItem = pTree->InsertItem(&ins);
	ScriptList *pSL = m_sides.getSideInfo(playerIndx)->getScriptList();
	if (pSL) {
		addScriptList(hItem, playerIndx, pSL);
	}
	updateIcons(TVI_ROOT);
	return hItem;
}		

void ScriptDialog::setIconGroup(HTREEITEM item)
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	int iconIndex = 0;

	if (getCurGroup()->hasWarnings() && getCurGroup()->isActive() == FALSE )
		iconIndex = 7;
	else if (getCurGroup()->hasWarnings())
		iconIndex = 3;
	else if (getCurGroup()->isActive())
		iconIndex = 1;
	else
		iconIndex = 5;

	pTree->SetItemState(item, INDEXTOSTATEIMAGEMASK(iconIndex), TVIS_STATEIMAGEMASK);
}

void ScriptDialog::setIconScript(HTREEITEM item)
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	int iconIndex = 0;

	if (getCurScript()->hasWarnings() && getCurScript()->isActive() == FALSE)
		iconIndex = 8;
	else if (getCurScript()->hasWarnings())
		iconIndex = 4;
	else if (getCurScript()->isActive())
		iconIndex = 2;
	else
		iconIndex = 6;

	pTree->SetItemState(item, INDEXTOSTATEIMAGEMASK(iconIndex), TVIS_STATEIMAGEMASK);
}

void ScriptDialog::SetItemIconIfDifferent(CTreeCtrl* pTree, HTREEITEM hItem, int desiredIndex)
{
	int currentState = (pTree->GetItemState(hItem, TVIS_STATEIMAGEMASK) & TVIS_STATEIMAGEMASK) >> 12;
	if (currentState != desiredIndex) {
		pTree->SetItemState(hItem, INDEXTOSTATEIMAGEMASK(desiredIndex), TVIS_STATEIMAGEMASK);
	}
}

Bool ScriptDialog::updateIcons(HTREEITEM hItem)
{
	const ListType saveList = m_curSelection;
	Bool warnings = false;
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	HTREEITEM child = pTree->GetChildItem(hItem);

	while (child != NULL) {
		ListType lt;
		lt.IntToList(pTree->GetItemData(child));

		/// player type
		if (lt.m_objType == ListType::PLAYER_TYPE)
		{
			if (updateIcons(child)) {
				// pTree->SetItemState(child, INDEXTOSTATEIMAGEMASK(3), TVIS_STATEIMAGEMASK);
				SetItemIconIfDifferent(pTree, child, 3);
			} else {
				// pTree->SetItemState(child, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
				SetItemIconIfDifferent(pTree, child, 1);
			}
		}

		/// script group
		else if (lt.m_objType == ListType::GROUP_TYPE)
		{
			m_curSelection = lt;
			if (updateIcons(child)) {
				// pTree->SetItemState(child, INDEXTOSTATEIMAGEMASK(3), TVIS_STATEIMAGEMASK);
				// SetItemIconIfDifferent(pTree, child, 3);
				warnings = true;
			}
			setIconGroup(child);
		}

		/// script
		else
		{
			m_curSelection = lt;
			Script *pScr = getCurScript();
			DEBUG_ASSERTCRASH(pScr, ("Unexpected."));
			if (pScr) {
				if (pScr->hasWarnings()) {
					// pTree->SetItemState(child, INDEXTOSTATEIMAGEMASK(4), TVIS_STATEIMAGEMASK);
					// SetItemIconIfDifferent(pTree, child, 4);
					warnings = true;
				}
				setIconScript(child);
			}
		}

		child = pTree->GetNextSiblingItem(child);
	}
	m_curSelection = saveList;
	return warnings;
}

void ScriptDialog::addScriptList(HTREEITEM hPlayer, Int playerIndex, ScriptList *pSL)
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	TVINSERTSTRUCT ins;
	Int groupNdx;
	ScriptGroup *pGroup = pSL->getScriptGroup();
	Bool warnings = false;
	for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
		AsciiString fmt;
		if (pGroup->getName().isEmpty())
			continue;
		else
			fmt = formatScriptLabel(pGroup);
		::memset(&ins, 0, sizeof(ins));
		ListType lt;
		lt.m_objType=ListType::GROUP_TYPE;
		lt.m_playerIndex = playerIndex;
		lt.m_groupIndex = groupNdx;
		ins.hParent = hPlayer;
		ins.hInsertAfter = TVI_LAST;
		ins.item.mask = TVIF_PARAM|TVIF_TEXT|TVIF_STATE;
		ins.item.lParam = lt.ListToInt();
		ins.item.pszText = (char *)fmt.str();
		ins.item.cchTextMax = 0;				
		ins.item.state = INDEXTOSTATEIMAGEMASK(1);
		if (pGroup->hasWarnings()) {
			ins.item.state = INDEXTOSTATEIMAGEMASK(3);
			warnings = true;
		}
		ins.item.stateMask = TVIS_STATEIMAGEMASK ;
		HTREEITEM hItem = pTree->InsertItem(&ins);
		Script *pScr = pGroup->getScript();
		if (pScr) {
			Int scriptNdx;
			for (scriptNdx = 0; pScr; scriptNdx++,pScr=pScr->getNext()) {
				AsciiString fmt;
				if (pScr->getName().isEmpty())
					continue;
				fmt = formatScriptLabel(pScr, m_bCleanScriptName);
				::memset(&ins, 0, sizeof(ins));
				ListType lt;
				lt.m_objType=ListType::SCRIPT_IN_GROUP_TYPE;
				lt.m_playerIndex = playerIndex;
				lt.m_groupIndex = groupNdx;
				lt.m_scriptIndex = scriptNdx;
				ins.hParent = hItem;
				ins.hInsertAfter = TVI_LAST;
				ins.item.mask = TVIF_PARAM|TVIF_TEXT|TVIF_STATE;
				ins.item.state = INDEXTOSTATEIMAGEMASK(2);
				if (pScr->hasWarnings()) {
					ins.item.state = INDEXTOSTATEIMAGEMASK(4);
					warnings = true;
				}
				ins.item.stateMask = TVIS_STATEIMAGEMASK ;
				ins.item.lParam = lt.ListToInt();
				ins.item.pszText = (char *)fmt.str();
				ins.item.cchTextMax = 0;				
				/*HTREEITEM hItem =*/ pTree->InsertItem(&ins);
			}
		}
	}
	Script *pScr = pSL->getScript();
	if (pScr) {
		Int scriptNdx;
		for (scriptNdx = 0; pScr; scriptNdx++,pScr=pScr->getNext()) {
			AsciiString fmt;
			if (pScr->getName().isEmpty())
				continue;
			fmt = formatScriptLabel(pScr, m_bCleanScriptName);
			::memset(&ins, 0, sizeof(ins));
			ListType lt;
			lt.m_objType=ListType::SCRIPT_IN_PLAYER_TYPE;
			lt.m_playerIndex = playerIndex;
			lt.m_groupIndex = 0;
			lt.m_scriptIndex = scriptNdx;
			ins.hParent = hPlayer;
			ins.hInsertAfter = TVI_LAST;
			ins.item.mask = TVIF_PARAM|TVIF_TEXT|TVIF_STATE;
			ins.item.state = INDEXTOSTATEIMAGEMASK(2);
			if (pScr->hasWarnings()) {
				ins.item.state = INDEXTOSTATEIMAGEMASK(4);
				warnings = true;
			}
			ins.item.stateMask = TVIS_STATEIMAGEMASK ;
			ins.item.lParam = lt.ListToInt();
			ins.item.pszText = (char *)fmt.str();
			ins.item.cchTextMax = 0;				
			/*HTREEITEM hItem =*/ pTree->InsertItem(&ins);
		}
	}
	if (warnings) {
		pTree->SetItemState(hPlayer, INDEXTOSTATEIMAGEMASK(3), TVIS_STATEIMAGEMASK);
	} else {
		pTree->SetItemState(hPlayer, INDEXTOSTATEIMAGEMASK(1), TVIS_STATEIMAGEMASK);
	}
}

void ScriptDialog::reloadPlayer(Int playerIndex, ScriptList *pSL)
{
	updateWarnings();
	
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);

	// Disable redraw to prevent flickering
	pTree->SetRedraw(FALSE);

	HTREEITEM player = pTree->GetChildItem(TVI_ROOT);
	while (player != NULL) {
		TVITEM item;
		::memset(&item, 0, sizeof(item));
		item.mask = TVIF_HANDLE|TVIF_PARAM;
		item.hItem = player;
		pTree->GetItem(&item);
		ListType lt;
		lt.IntToList(item.lParam);
		if (lt.m_playerIndex==playerIndex) {
			break;
		}
		player = pTree->GetNextSiblingItem(player);
	}
	DEBUG_ASSERTCRASH(player, ("Couldn't find player."));
	if (!player) {
		pTree->SetRedraw(TRUE);
		return;
	}

	ListType currentSel = m_curSelection;
	if (currentSel.m_objType == ListType::SCRIPT_IN_GROUP_TYPE && currentSel.m_scriptIndex > 0) {
		--currentSel.m_scriptIndex;
	}

	// Delete all children under the player item
	HTREEITEM child;
	do {
		child = pTree->GetChildItem(player);
		if (child) pTree->DeleteItem(child);
	} while (child);

	// Restore selection and add scripts
	m_curSelection = currentSel;
	addScriptList(player, playerIndex, pSL);

	// Re-enable redraw
	pTree->SetRedraw(TRUE);
	pTree->Invalidate();
	pTree->UpdateWindow();
}

void ScriptDialog::updateSelection(ListType sel)
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	HTREEITEM item = findItem(sel, TRUE);
	if (item) {
		pTree->SelectItem(item);
	}
}

HTREEITEM ScriptDialog::findItem(ListType sel, Bool failSafe)
{
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	HTREEITEM player = pTree->GetChildItem(TVI_ROOT);
	TVITEM item;
	while (player != NULL) {
		::memset(&item, 0, sizeof(item));
		item.mask = TVIF_HANDLE|TVIF_PARAM;
		item.hItem = player;
		pTree->GetItem(&item);
		ListType lt;
		lt.IntToList(item.lParam);
		if (lt.m_playerIndex==sel.m_playerIndex) {
			break;
		}
		player = pTree->GetNextSiblingItem(player);
	}
	DEBUG_ASSERTCRASH(player, ("Couldn't find player."));
	if (!player) return NULL;
	if (sel.m_objType == ListType::PLAYER_TYPE) {
		return player;
	}

	HTREEITEM group;
	if (sel.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE) {
		group = player; // top level scripts are grouped under player.
	} else {
		group = pTree->GetChildItem(player);
		while (group != NULL) {
			::memset(&item, 0, sizeof(item));
			item.mask = TVIF_HANDLE|TVIF_PARAM;
			item.hItem = group;
			pTree->GetItem(&item);
			ListType lt;
			lt.IntToList(item.lParam);
			if (lt.m_groupIndex==sel.m_groupIndex) {
				break;
			}
			DEBUG_ASSERTCRASH(lt.m_objType == ListType::GROUP_TYPE, ("Not group"));
			group = pTree->GetNextSiblingItem(group);
		}
	} 
	DEBUG_ASSERTCRASH(group, ("Couldn't find group."));
	if (!group) return NULL;
	if (sel.m_objType == ListType::GROUP_TYPE) {
		return group;
	}

	HTREEITEM script;
	for (script = pTree->GetChildItem(group); script != NULL; script = pTree->GetNextSiblingItem(script)) {
		::memset(&item, 0, sizeof(item));
		item.mask = TVIF_HANDLE|TVIF_PARAM;
		item.hItem = script;
		pTree->GetItem(&item);
		ListType lt;
		lt.IntToList(item.lParam);
		if (sel.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE && lt.m_objType == ListType::GROUP_TYPE) {
			continue;
		}
		DEBUG_ASSERTCRASH(lt.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE || lt.m_objType == ListType::SCRIPT_IN_GROUP_TYPE, ("Not script"));
		if (lt.m_scriptIndex==sel.m_scriptIndex) {
			break;
		}
	}

	if (script || !failSafe) {
		DEBUG_ASSERTCRASH(script, ("Couldn't find script.")); 
		return script;
	}

	// at least select the group if possible.
	return group;
}

void ScriptDialog::OnNewFolder() 
{
	Int ndx;
	if (m_curSelection.m_objType == ListType::PLAYER_TYPE) {
		ndx = 0;
	} else {
		ndx = m_curSelection.m_groupIndex+1;
	}
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL) {
		ListType savSel = m_curSelection;
		ScriptGroup *pNewGroup = newInstance( ScriptGroup);
		EditGroup editDlg(pNewGroup);
		if (IDOK==editDlg.DoModal()) {

			AsciiString name = pNewGroup->getName();
			name.trim();

			if (name.isEmpty()) {
				AfxMessageBox(
					"Error: Script folder name cannot be empty.\n\n"
					"Please enter a valid name.",
					MB_OK | MB_ICONERROR
				);

				pNewGroup->deleteInstance();
				return;
			}

			pSL->addGroup(pNewGroup, ndx);
			reloadPlayer(savSel.m_playerIndex, pSL);
			savSel.m_groupIndex = ndx;
			savSel.m_objType = ListType::GROUP_TYPE;
			updateSelection(savSel);
		} else {
			pNewGroup->deleteInstance();
		}
	}
	updateIcons(TVI_ROOT);
}

void ScriptDialog::OnNewScript() 
{
	Script *pNewScript = newInstance( Script);

	Int id = ScriptList::getNextID();
	AsciiString name;
	name.format("Script %d", id);
	pNewScript->setName(name);

	Condition *pFalse1 = newInstance( Condition)(Condition::CONDITION_TRUE);
	OrCondition *pOr = newInstance( OrCondition);
	pOr->setFirstAndCondition(pFalse1);
	pNewScript->setOrCondition(pOr);

	ScriptAction *action = newInstance( ScriptAction)(ScriptAction::NO_OP);
	pNewScript->setAction(action);

#ifdef RTS_HAS_QT
	// Qt mode: run the native Qt tabbed editor (WBQtScriptEditDialog) instead of the MFC
	// property sheet; same accept/reject handling as the sheet path below.
	{
		if (WBQtScriptEdit_Run(pNewScript, ::AfxGetMainWnd()->GetSafeHwnd()) != 0)
		{
			AsciiString name = pNewScript->getName();
			name.trim();

			if (name.isEmpty())
			{
				AfxMessageBox(
					"Error: Script name cannot be empty.\n\n"
					"Please enter a valid name.",
					MB_OK | MB_ICONERROR
				);

				pNewScript->deleteInstance();
				return;
			}

			insertScript(pNewScript);
		}
		else
		{
			pNewScript->deleteInstance();
		}
		updateIcons(TVI_ROOT);
		return;
	}
#endif

	CPropertySheet editDialog;
	editDialog.Construct(name.str());
	ScriptProperties sp;
	sp.setScript(pNewScript);
	ScriptConditionsDlg sc;
	sc.setScript(pNewScript);
	ScriptActionsTrue st;
	st.setScript(pNewScript);
	ScriptActionsFalse sf;
	sf.setScript(pNewScript);
	editDialog.AddPage(&sp);
	editDialog.AddPage(&sc);
	editDialog.AddPage(&st);
	editDialog.AddPage(&sf);

	if (IDOK == editDialog.DoModal()) {

		AsciiString name = pNewScript->getName();
		name.trim();

		if (name.isEmpty()) {
			AfxMessageBox(
				"Error: Script name cannot be empty.\n\n"
				"Please enter a valid name.",
				MB_OK | MB_ICONERROR
			);

			pNewScript->deleteInstance();
			return;
		}

		insertScript(pNewScript);
	}	else {
		pNewScript->deleteInstance();
	}
	updateIcons(TVI_ROOT);
}		

void ScriptDialog::insertScript(Script *pNewScript)
{
	Int ndx;
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL) {
		ListType savSel = m_curSelection;
		Bool inGroup = savSel.m_objType == ListType::GROUP_TYPE	||
			savSel.m_objType == ListType::SCRIPT_IN_GROUP_TYPE;
		if (inGroup) {
			if (savSel.m_objType == ListType::GROUP_TYPE ) {
				ndx = 0;
			} else {
				ndx = savSel.m_scriptIndex+1;
			}
			Int groupNdx;
			ScriptGroup *pGroup = pSL->getScriptGroup();
			for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
				if (groupNdx == savSel.m_groupIndex) {
					pGroup->addScript(pNewScript, ndx);
					savSel.m_objType = ListType::SCRIPT_IN_GROUP_TYPE;
					savSel.m_scriptIndex = ndx;
					break;
				}
			}
		} else {
			if (m_curSelection.m_objType == ListType::PLAYER_TYPE ) {
				ndx = 0;
			} else {
				ndx = m_curSelection.m_scriptIndex+1;
			}
			pSL->addScript(pNewScript, ndx);
			savSel.m_objType = ListType::SCRIPT_IN_PLAYER_TYPE;
			savSel.m_scriptIndex = ndx;
		}

		reloadPlayer(savSel.m_playerIndex, pSL);
		updateSelection(savSel);
	}
	updateIcons(TVI_ROOT);
}

void ScriptDialog::OnEditScript() 
{
	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();
	DEBUG_ASSERTCRASH(pScript || pGroup, ("Null script."));
	if (pScript == NULL) {
		CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
		HTREEITEM item = findItem(m_curSelection);
		if (pGroup) {
			EditGroup editDlg(pGroup);
			if (IDOK==editDlg.DoModal()) {
				if (item) {
					pTree->SetItemText(item, pGroup->getName().str());
					pTree->SelectItem(NULL);
					updateWarnings();
					pTree->SelectItem(item);
				}
			}
		}
		updateIcons(TVI_ROOT);
		pTree->SetItemText(item, formatScriptLabel(pGroup).str());
		return;
	}

	Script *pDup = pScript->duplicate();

#ifdef RTS_HAS_QT
	// Qt mode: edit the duplicate in the native Qt tabbed editor; on OK commit exactly
	// like the MFC sheet path below (updateFrom + tree label + warning refresh).
	{
		if (WBQtScriptEdit_Run(pDup, ::AfxGetMainWnd()->GetSafeHwnd()) != 0)
		{
			pScript->updateFrom(pDup);
			CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
			HTREEITEM item = findItem(m_curSelection);
			if (item)
			{
				pTree->SetItemText(item, formatScriptLabel(pScript, m_bCleanScriptName).str());
				pTree->SelectItem(NULL);
				pScript->setDirty(true);
				updateWarnings();
				pTree->SelectItem(item); // Updates the comment field & text field.
			}
		}
		updateIcons(TVI_ROOT);
		pDup->deleteInstance();
		return;
	}
#endif

	CPropertySheet editDialog;
	editDialog.Construct(pScript->getName().str());
	ScriptProperties sp;
	sp.setScript(pDup);
	ScriptConditionsDlg sc;
	sc.setScript(pDup);
	ScriptActionsTrue st;
	st.setScript(pDup);
	ScriptActionsFalse sf;
	sf.setScript(pDup);
	editDialog.AddPage(&sp);
	editDialog.AddPage(&sc);
	editDialog.AddPage(&st);
	editDialog.AddPage(&sf);

	if (IDOK == editDialog.DoModal()) {
		pScript->updateFrom(pDup);
		CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
		HTREEITEM item = findItem(m_curSelection);
		if (item) {
			pTree->SetItemText(item, formatScriptLabel(pScript, m_bCleanScriptName).str());
			pTree->SelectItem(NULL);
			pScript->setDirty(true);
			updateWarnings();
			pTree->SelectItem(item); // Updates the comment field & text field.
		}
	}
	updateIcons(TVI_ROOT);
	pDup->deleteInstance();
}

void ScriptDialog::applySmartCopyIncrement(Script* pScr)
{
    if (!pScr) return;

    // Increment script name if it ends with a number
    pScr->setName(incrementStringNumber(pScr->getName()));

    // Increment parameters inside conditions
    for (OrCondition* pOr = pScr->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
        for (Condition* c = pOr->getFirstAndCondition(); c; c = c->getNext()) {
            for (int i = 0; i < c->getNumParameters(); ++i) {
                Parameter* param = c->getParameter(i);
                if (!param) continue;
                if (
                    param->getParameterType() == Parameter::TEXT_STRING ||
                    param->getParameterType() == Parameter::TEAM ||
                    param->getParameterType() == Parameter::WAYPOINT ||
                    param->getParameterType() == Parameter::SCRIPT ||
                    param->getParameterType() == Parameter::UNIT ||
                    param->getParameterType() == Parameter::REVEALNAME ||
					param->getParameterType() == Parameter::COUNTER ||
					param->getParameterType() == Parameter::FLAG ||
					param->getParameterType() == Parameter::SIDE
                )
                {
                    AsciiString newVal = incrementStringNumber(param->getString());
                    param->friend_setString(newVal);
                }
            }
        }
    }

    // --- Increment parameters inside TRUE actions ---
    for (ScriptAction* a = pScr->getAction(); a; a = a->getNext()) {
        for (int i = 0; i < a->getNumParameters(); ++i) {
            Parameter* param = a->getParameter(i);
            if (!param) continue;
            if (
                param->getParameterType() == Parameter::TEXT_STRING ||
                param->getParameterType() == Parameter::TEAM ||
                param->getParameterType() == Parameter::WAYPOINT ||
                param->getParameterType() == Parameter::SCRIPT ||
				param->getParameterType() == Parameter::SCRIPT_SUBROUTINE ||
                param->getParameterType() == Parameter::UNIT ||
                param->getParameterType() == Parameter::REVEALNAME ||
				param->getParameterType() == Parameter::COUNTER ||
				param->getParameterType() == Parameter::FLAG ||
				param->getParameterType() == Parameter::SIDE
            )
            {
                AsciiString newVal = incrementStringNumber(param->getString());
                param->friend_setString(newVal);
            }
        }
    }

    // --- Increment parameters inside FALSE actions ---
    for (ScriptAction* b = pScr->getFalseAction(); b; b = b->getNext()) {
        for (int z = 0; z < b->getNumParameters(); ++z) {
            Parameter* param = b->getParameter(z);
            if (!param) continue;
            if (
                param->getParameterType() == Parameter::TEXT_STRING ||
                param->getParameterType() == Parameter::TEAM ||
                param->getParameterType() == Parameter::WAYPOINT ||
                param->getParameterType() == Parameter::SCRIPT ||
				param->getParameterType() == Parameter::SCRIPT_SUBROUTINE ||
                param->getParameterType() == Parameter::UNIT ||
                param->getParameterType() == Parameter::REVEALNAME ||
				param->getParameterType() == Parameter::COUNTER ||
				param->getParameterType() == Parameter::FLAG ||
				param->getParameterType() == Parameter::SIDE
            )
            {
                AsciiString newVal = incrementStringNumber(param->getString());
                param->friend_setString(newVal);
            }
        }
    }
}

AsciiString ScriptDialog::incrementStringNumber(const AsciiString& input)
{
    const char* str = input.str();
    int len = strlen(str);

    // Find trailing number
    int pos = len - 1;
    while (pos >= 0 && isdigit(str[pos])) pos--;

    if (pos == len - 1) {
        // No number at end, return unchanged
        return input;
    }

    CString prefix(str, pos + 1); // text before number
    CString numberStr(str + pos + 1);
    int number = atoi(numberStr);
    number++;

    CString result;
    result.Format("%s%0*d", prefix, numberStr.GetLength(), number);
    return AsciiString(result);
}

void ScriptDialog::OnCopyScript() 
{
    Script *pScript = getCurScript();
    ScriptGroup *pGroup = getCurGroup();

    if (pScript) {
        Script *pDup = pScript->duplicate();

        // Smart copy logic
        if (m_bSmartCopyEnabled)
            applySmartCopyIncrement(pDup);

        AsciiString newName = pDup->getName();
		if(!m_bSmartCopyEnabled){
			// If smart copy is disabled, just append " C" to the name
			newName.concat(" C");
		}
        pDup->setName(newName);

        insertScript(pDup);
        updateIcons(TVI_ROOT);
        return;
    }

    if (pGroup && m_curSelection.m_objType == ListType::GROUP_TYPE) {
        ScriptGroup* pNewGroup = newInstance(ScriptGroup);
        AsciiString newGroupName = pGroup->getName();
        newGroupName.concat(" Copy");
        pNewGroup->setName(newGroupName);
        pNewGroup->setActive(pGroup->isActive());
        pNewGroup->setSubroutine(pGroup->isSubroutine());

        Int scriptIndex = 0;
        for (Script* pScr = pGroup->getScript(); pScr; pScr = pScr->getNext(), ++scriptIndex) {
            Script* pDup = pScr->duplicate();

            if (m_bSmartCopyEnabled)
                applySmartCopyIncrement(pDup);

            AsciiString scriptName = pDup->getName();
            // scriptName.concat(" C");
            pDup->setName(scriptName);

            pNewGroup->addScript(pDup, scriptIndex);
        }

        ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
        if (pSL) {
            Int insertIndex = m_curSelection.m_groupIndex + 1;
            pSL->addGroup(pNewGroup, insertIndex);
            reloadPlayer(m_curSelection.m_playerIndex, pSL);
        }
        updateIcons(TVI_ROOT);
    }
}

void ScriptDialog::OnDelete() 
{
	Script *pScript = getCurScript();
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL) {
		Bool inGroup = m_curSelection.m_objType != ListType::SCRIPT_IN_PLAYER_TYPE;
		if (inGroup) {
			Int groupNdx;
			ScriptGroup *pGroup = pSL->getScriptGroup();
			for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext()) {
				if (groupNdx == m_curSelection.m_groupIndex) {
					if (m_curSelection.m_objType == ListType::GROUP_TYPE) {
						pSL->deleteGroup(pGroup);
						m_curSelection.m_objType = ListType::PLAYER_TYPE;
					} else {
						pGroup->deleteScript(pScript);
						if (pGroup->getScript()==NULL) {
							m_curSelection.m_objType = ListType::GROUP_TYPE;
						}
					}
					break;
				}
			}
		} else {
			pSL->deleteScript(pScript);
			if (pSL->getScript()==NULL) {
				m_curSelection.m_objType = ListType::PLAYER_TYPE;
			}
		}
 		reloadPlayer(m_curSelection.m_playerIndex, pSL);
		updateSelection(m_curSelection);
	}
	updateIcons(TVI_ROOT);
}

void ScriptDialog::OnAddDebug() 
{
    Script *pScript = getCurScript();
    if (!pScript) {
        AfxMessageBox("No script selected. Please select a script first.", MB_OK | MB_ICONWARNING);
        return;
    }

    // Create the debug action (SHOW_MILITARY_CAPTION)
    ScriptAction *debugAction = newInstance(ScriptAction)(ScriptAction::SHOW_MILITARY_CAPTION);
    
    // Set first parameter: script name + " called"
    AsciiString debugText = "[Debug] "; 
	debugText.concat(pScript->getName());
    debugText.concat(" called");
    debugAction->getParameter(0)->friend_setString(debugText);
    
    // ---- Duration based on text length ----
    const int msPerChar = 400;
    int textLength = debugText.getLength();

    int durationMs = textLength * msPerChar;

    // Clamp to sane limits
    if (durationMs < 2000)  durationMs = 2000;   // minimum 2s
    if (durationMs > 15000) durationMs = 15000;  // maximum 15s

    debugAction->getParameter(1)->friend_setInt(durationMs);
    // --------------------------------------
    
    // Add the action at the END of the existing TRUE actions
    ScriptAction *lastAction = pScript->getAction();
    if (lastAction) {
        // Find the last action in the chain
        while (lastAction->getNext()) {
            lastAction = lastAction->getNext();
        }
        lastAction->setNextAction(debugAction);
    } else {
        // No actions exist yet, set as first action
        pScript->setAction(debugAction);
    }
    
    // Mark script as dirty to trigger warning updates
    pScript->setDirty(true);
    
    // Update the tree view
    CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
    HTREEITEM item = findItem(m_curSelection);
    if (item) {
        pTree->SetItemText(item, formatScriptLabel(pScript, m_bCleanScriptName).str());
        pTree->SelectItem(NULL);
        updateWarnings();
        pTree->SelectItem(item);
    }
    updateIcons(TVI_ROOT);
    
    MessageBeep(MB_ICONWARNING);
}

void ScriptDialog::OnRemoveDebug()
{
    Script *pScript = getCurScript();
    if (!pScript) {
        AfxMessageBox("No script selected. Please select a script first.", MB_OK | MB_ICONWARNING);
        return;
    }

    ScriptAction *currentAction = pScript->getAction();
    ScriptAction *prevAction = NULL;
    int removedCount = 0;

    while (currentAction) {
        bool shouldRemove = false;

        // Check if this is a SHOW_MILITARY_CAPTION action with "[Debug]" prefix
        if (currentAction->getActionType() == ScriptAction::SHOW_MILITARY_CAPTION) {
            Parameter *param = currentAction->getParameter(0);
            if (param && param->getParameterType() == Parameter::TEXT_STRING) {
                AsciiString text = param->getString();
                if (text.startsWith("[Debug] ")) {
                    shouldRemove = true;
                }
            }
        }

        if (shouldRemove) {
            ScriptAction *toDelete = currentAction;
            currentAction = currentAction->getNext();

            // Unlink from chain
            if (prevAction) {
                prevAction->setNextAction(currentAction);
            } else {
                // Removing first action
                pScript->setAction(currentAction);
            }

            // Delete the action
            toDelete->setNextAction(NULL);
            toDelete->deleteInstance();
            
            removedCount++;
        } else {
            // Move to next action
            prevAction = currentAction;
            currentAction = currentAction->getNext();
        }
    }

    if (removedCount > 0) {
        // Mark script as dirty
        pScript->setDirty(true);

        // Update the tree view
        CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
        HTREEITEM item = findItem(m_curSelection);
        if (item) {
            pTree->SetItemText(item, formatScriptLabel(pScript, m_bCleanScriptName).str());
            pTree->SelectItem(NULL);
            updateWarnings();
            pTree->SelectItem(item);
        }
        updateIcons(TVI_ROOT);

        CString msg;
        msg.Format("Removed %d debug action(s).", removedCount);
        AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
    } else {
        AfxMessageBox("No debug actions found in this script.", MB_OK | MB_ICONINFORMATION);
    }
}


class LocalMFCFileOutputStream : public OutputStream
{
protected:
	CFile *m_file;
public:
	LocalMFCFileOutputStream(CFile *pFile):m_file(pFile) {};
	virtual Int write(const void *pData, Int numBytes) {
		Int numBytesWritten = 0;
		try {
			m_file->Write(pData, numBytes);
			numBytesWritten = numBytes;
		} catch(...) {
			DEBUG_CRASH(("threw exception in LocalMFCFileOutputStream"));
		}
		return(numBytesWritten);
	};
};

void ScriptDialog::markWaypoint(MapObject *pObj)
{
	Bool exists;
	if (!pObj) return;
	if (pObj->isWaypoint() && !pObj->getProperties()->getBool(TheKey_exportWithScript, &exists)) {
		pObj->getProperties()->setBool(TheKey_exportWithScript, true);
		CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
		Int curID = pObj->getWaypointID();
		Int i;
		for (i = 0; i<pDoc->getNumWaypointLinks(); i++) {
//			Bool gotLocation=false;
			Int waypointID1, waypointID2;
			pDoc->getWaypointLink(i, &waypointID1, &waypointID2);
			if (curID == waypointID1) {
				markWaypoint(pDoc->getWaypointByID(waypointID2));
			}
			if (curID == waypointID2) {
				markWaypoint(pDoc->getWaypointByID(waypointID1));
			}
		}
	}
}

/** Looks for referenced waypoints & teams. */
void ScriptDialog::scanParmForWaypointsAndTeams(Parameter *pParm, Bool doUnits, Bool doWaypoints, Bool doTriggers, Bool doTeams)
{
	if (pParm->getParameterType() == Parameter::WAYPOINT && doWaypoints) {
		AsciiString waypointName  = pParm->getString();
		MapObject *pObj;
		for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
			if (pObj->isWaypoint() && pObj->getWaypointName()==waypointName) {
				markWaypoint(pObj);
			}
		}
	}
	if (pParm->getParameterType() == Parameter::WAYPOINT_PATH && doWaypoints) {
		AsciiString waypointPathLabel = pParm->getString();
		MapObject *pObj;
		for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
			if (pObj->isWaypoint() ) {
				Bool exists;
				if (waypointPathLabel == pObj->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists)) {
					markWaypoint(pObj);
				}
				if (waypointPathLabel == pObj->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists)) {
					markWaypoint(pObj);
				}
				if (waypointPathLabel == pObj->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists)) {
					markWaypoint(pObj);
				}
			}
		}
	}
	if (pParm->getParameterType() == Parameter::TEAM) {
		AsciiString teamName  = pParm->getString();
		TeamsInfo * pInfo = m_sides.findTeamInfo(teamName);
		if (pInfo && doTeams) {
			pInfo->getDict()->setBool(TheKey_exportWithScript, true);
		}	 
		if (doUnits) {
			MapObject *pObj;
			for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				Bool exists;
				AsciiString objsTeamName = pObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
				if (objsTeamName==teamName) {
					pObj->getProperties()->setBool(TheKey_exportWithScript, true);
				}
			}
		}
	}
	if (pParm->getParameterType() == Parameter::UNIT) {
		AsciiString unitName  = pParm->getString();
		if (doUnits) {
			MapObject *pObj;
			for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				Bool exists;
				AsciiString objsTeamName = pObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
				AsciiString objsUnitName = pObj->getProperties()->getAsciiString(TheKey_objectName, &exists);
				if (objsUnitName==unitName) {
					pObj->getProperties()->setBool(TheKey_exportWithScript, true);
					TeamsInfo * pInfo = m_sides.findTeamInfo(objsTeamName);
					if (pInfo) {
						pInfo->getDict()->setBool(TheKey_exportWithScript, true);
					}	 
				}
			}
		}
	}
	if (pParm->getParameterType() == Parameter::TRIGGER_AREA && doTriggers) {
		PolygonTrigger *pTrig;
		for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
			if (pTrig->getTriggerName() == pParm->getString()) {
				pTrig->setDoExportWithScripts(true);
			}
		}
	}
}

/** Looks for referenced waypoints & teams. */
void ScriptDialog::scanForWaypointsAndTeams(Script *pScript, Bool doUnits, Bool doWaypoints, Bool doTriggers, Bool doTeams)
{
	pScript->setWarnings(false);
	OrCondition *pOr;
	for (pOr= pScript->getOrCondition(); pOr; pOr = pOr->getNextOrCondition()) {
		Condition *pCondition;
		for (pCondition = pOr->getFirstAndCondition(); pCondition; pCondition = pCondition->getNext()) {
			Int i;
			for (i=0; i<pCondition->getNumParameters(); i++) {
				scanParmForWaypointsAndTeams(pCondition->getParameter(i), doUnits, doWaypoints, doTriggers, doTeams);
			}
		}
	}
	ScriptAction *pAction;
	for (pAction = pScript->getAction(); pAction; pAction = pAction->getNext()) {
		pAction->setWarnings(false);
		Int i;
		for (i=0; i<pAction->getNumParameters(); i++) {
			scanParmForWaypointsAndTeams(pAction->getParameter(i), doUnits, doWaypoints, doTriggers, doTeams);
		}
	}
}

#define K_PLAYERS_NAMES_FOR_SCRIPTS_VERSION_1 1
#define K_PLAYERS_NAMES_FOR_SCRIPTS_VERSION_2 2

/** Write out selected scripts, and possibly waypoints, trigger areas & teams. */
void ScriptDialog::OnSave() 
{
	Bool doWaypoints = true;
	Bool doTriggerAreas = true;
	Bool doUnits = true;
	Bool doAllScripts = true;
	Bool doSides = true;
	Bool doTeams = true;
	Int	 i;

	ExportScriptsOptions optionsDlg;
	if (IDCANCEL == optionsDlg.DoModal()) {
		return;
	}
	doWaypoints = optionsDlg.getDoWaypoints();
	doUnits = optionsDlg.getDoUnits();
	doTeams = optionsDlg.getDoTeams(); // you'll implement this getter
	doTriggerAreas = optionsDlg.getDoTriggers();
	doAllScripts = optionsDlg.getDoAllScripts();
	doSides = optionsDlg.getDoSides();

	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();

	ScriptList *scripts[MAX_PLAYER_COUNT];
	for (i=0; i<MAX_PLAYER_COUNT; i++) {
		scripts[i] = NULL;
	}

	CFileDialog fileDlg(false, ".scb", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, 
		"Script files (.scb)|*.scb||", this);

	Int result = fileDlg.DoModal();

	// Open document dialog may change working directory, 
	// change it back.
	char buf[_MAX_PATH];
	::GetModuleFileName(NULL, buf, sizeof(buf));
	char *pEnd = buf + strlen(buf);
	while (pEnd != buf) {
		if (*pEnd == '\\') {
			*pEnd = 0;
			break;
		}
		pEnd--;
	}
	::SetCurrentDirectory(buf);
	if (IDCANCEL==result) {
		return;
	}

	MapObject *pObj;
	for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		pObj->getProperties()->setBool(TheKey_exportWithScript, false);
		if (pObj->isWaypoint() && (doWaypoints&&doAllScripts)) {
			pObj->getProperties()->setBool(TheKey_exportWithScript, true);
		}
	}

	PolygonTrigger *pTrig;
	for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
		pTrig->setDoExportWithScripts(doAllScripts && doTriggerAreas);
		if (pTrig->isWaterArea()) {
			pTrig->setDoExportWithScripts(false); // don't export water.
		}
	}

	// DEBUG_LOG(("doTeams %s", doTeams ? "true" : "false"));
	// DEBUG_LOG(("doAllScripts %s", doAllScripts ? "true" : "false"));

	for (i = 0; i < m_sides.getNumTeams(); i++) {
		m_sides.getTeamInfo(i)->getDict()->setBool(TheKey_exportWithScript, doAllScripts);
	}
	Int numScriptLists = 0;
	if (doAllScripts) {
		numScriptLists = m_sides.getNumSides();
		for (i=0; i<numScriptLists; i++) {
			scripts[i] = m_sides.getSideInfo(i)->getScriptList();
		}
	} else {
		numScriptLists = 1;
		if (m_curSelection.m_objType == ListType::PLAYER_TYPE) {
			scripts[0] = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
			scripts[0] = scripts[0]->duplicate();
		}	else if (pGroup) {
			pGroup = pGroup->duplicate();
			scripts[0] = newInstance( ScriptList);
			scripts[0]->addGroup(pGroup, 0);
		} else if (pScript) {
			pScript = pScript->duplicate();
			scripts[0] = newInstance( ScriptList);
			scripts[0]->addScript(pScript, 0);
		}
		if (scripts[0] == NULL) {
			::AfxMessageBox("No scripts selected - aborting export.", MB_OK);
			return;
		}
	}
	for (i=0; i<numScriptLists; i++) {
		ScriptList *pSL = scripts[i];
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr=pScr->getNext()) {
			scanForWaypointsAndTeams(pScr, doUnits, doWaypoints, doTriggerAreas, doTeams);
		}
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup=pGroup->getNext()) {
			for (pScr = pGroup->getScript(); pScr; pScr=pScr->getNext()) {
				scanForWaypointsAndTeams(pScr, doUnits, doWaypoints, doTriggerAreas, doTeams);
			}
		}
	}


	CString path = fileDlg.GetPathName();

	CFile theFile(path, CFile::modeCreate|CFile::modeWrite|CFile::shareDenyWrite|CFile::typeBinary);
	try {
		LocalMFCFileOutputStream theStream(&theFile);
		DataChunkOutput chunkWriter(&theStream);
		ScriptList::WriteScriptsDataChunk(chunkWriter, scripts, numScriptLists);

		/***************Players DATA ***************/
		chunkWriter.openDataChunk("ScriptsPlayers", 	K_PLAYERS_NAMES_FOR_SCRIPTS_VERSION_2);
		chunkWriter.writeInt(doSides);
		if (doAllScripts || doSides) {
			chunkWriter.writeInt(m_sides.getNumSides());
			for (i=0; i<m_sides.getNumSides(); i++) {
				AsciiString name = m_sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
				chunkWriter.writeAsciiString(name);

				if (doSides) {
					// The user has requested that the sides get exported.
					chunkWriter.writeDict(*m_sides.getSideInfo(i)->getDict());
				}

			}
		} else  {
			chunkWriter.writeInt(1);
			chunkWriter.writeAsciiString("**SELECTION**");
		}
		chunkWriter.closeDataChunk();

		/***************OBJECTS DATA ***************/
		chunkWriter.openDataChunk("ObjectsList", 	K_OBJECTS_VERSION_3);
			
		for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) 
		{
			if (!pObj->getProperties()->getBool(TheKey_exportWithScript)) {
				continue;
			};
			chunkWriter.openDataChunk("Object", 	K_OBJECTS_VERSION_3);
				Coord3D loc = *pObj->getLocation();
				chunkWriter.writeReal( loc.x);
				chunkWriter.writeReal( loc.y);
				chunkWriter.writeReal( loc.z);
				chunkWriter.writeReal( pObj->getAngle());
				chunkWriter.writeInt(pObj->getFlags()); 
				chunkWriter.writeAsciiString(pObj->getName());	

				chunkWriter.writeDict(*pObj->getProperties());	

			chunkWriter.closeDataChunk();
		}
		chunkWriter.closeDataChunk();

		/***************POLYGON TRIGGERS DATA ***************/
		chunkWriter.openDataChunk("PolygonTriggers", 	K_TRIGGERS_VERSION_3);
			
			PolygonTrigger *pTrig;
			Int count = 0;
			for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
				if (pTrig->doExportWithScripts()) {
					count++;
				}
			}
			chunkWriter.writeInt(count); 
			for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
				if (!pTrig->doExportWithScripts()) continue;
				chunkWriter.writeAsciiString(pTrig->getTriggerName());	
				chunkWriter.writeInt(pTrig->getID()); 
				chunkWriter.writeByte(pTrig->isWaterArea());
				chunkWriter.writeByte(pTrig->isRiver());
				chunkWriter.writeInt(pTrig->getRiverStart());
				chunkWriter.writeInt(pTrig->getNumPoints()); 
				Int i;
				for (i=0; i<pTrig->getNumPoints(); i++) {
					ICoord3D loc = *pTrig->getPoint(i);
					chunkWriter.writeInt( loc.x);
					chunkWriter.writeInt( loc.y);
					chunkWriter.writeInt( loc.z);
				}
			}
		chunkWriter.closeDataChunk();
 		/***************TEAMS DATA ***************/
		chunkWriter.openDataChunk("ScriptTeams", 	K_LOCAL_TEAMS_VERSION_1);
			for (i = 0; i < m_sides.getNumTeams(); i++)
			{
				if (m_sides.getTeamInfo(i)->getDict()->getBool(TheKey_exportWithScript)) {
					chunkWriter.writeDict(*m_sides.getTeamInfo(i)->getDict());
				}
			}

			
		chunkWriter.closeDataChunk();
 		/***************WAYPOINTS DATA ***************/
			CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
			Int i;
			count = 0;
			for (i = 0; i<pDoc->getNumWaypointLinks(); i++) {
					Int waypointID1, waypointID2;
				MapObject *pWay1, *pWay2;
				pDoc->getWaypointLink(i, &waypointID1, &waypointID2);
				pWay1 = pDoc->getWaypointByID(waypointID1);
				pWay2 = pDoc->getWaypointByID(waypointID2);
				if (pWay1 && pWay2) {
					if (!pWay1->getProperties()->getBool(TheKey_exportWithScript)) {
						continue;
					};
					if (!pWay1->getProperties()->getBool(TheKey_exportWithScript)) {
						continue;
					};
					count++;
				}
			}

		chunkWriter.openDataChunk("WaypointsList", 	K_WAYPOINTS_VERSION_1);
			chunkWriter.writeInt(count);
			for (i = 0; i<pDoc->getNumWaypointLinks(); i++) {
					Int waypointID1, waypointID2;
				MapObject *pWay1, *pWay2;
				pDoc->getWaypointLink(i, &waypointID1, &waypointID2);
				pWay1 = pDoc->getWaypointByID(waypointID1);
				pWay2 = pDoc->getWaypointByID(waypointID2);
				if (pWay1 && pWay2) {
					if (!pWay1->getProperties()->getBool(TheKey_exportWithScript)) {
						continue;
					};
					if (!pWay1->getProperties()->getBool(TheKey_exportWithScript)) {
						continue;
					};
					chunkWriter.writeInt(waypointID1);
					chunkWriter.writeInt(waypointID2);
				}
			}
		chunkWriter.closeDataChunk();

	} catch(...) {
			DEBUG_CRASH(("threw exception in ScriptDialog::OnSave"));
	}
	if (!doAllScripts) {
		scripts[0]->deleteInstance();
	}
	theFile.Close();
}

void ScriptDialog::OnLoad() 
{
	CFileDialog fileDlg(true, ".scb", NULL, 0, 
		"Script files (.scb)|*.scb||", this);

	Int result = fileDlg.DoModal();

	// Open document dialog may change working directory, 
	// change it back.
	char buf[_MAX_PATH];
	::GetModuleFileName(NULL, buf, sizeof(buf));
	char *pEnd = buf + strlen(buf);
	while (pEnd != buf) {
		if (*pEnd == '\\') {
			*pEnd = 0;
			break;
		}
		pEnd--;
	}
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	::SetCurrentDirectory(buf);
	if (IDCANCEL==result) {
		return;
	}

	CString path = fileDlg.GetPathName();

	CachedFileInputStream theInputStream;
	if (theInputStream.open(AsciiString(path))) 
	try {
		ChunkInputStream *pStrm = &theInputStream;
		DataChunkInput file( pStrm );
		m_firstReadObject = NULL;
		m_firstTrigger = NULL;
		m_waypointBase = pDoc->getNextWaypointID();
		m_maxWaypoint = m_waypointBase;
		file.registerParser( AsciiString("PlayerScriptsList"), AsciiString::TheEmptyString, ScriptList::ParseScriptsDataChunk );
		file.registerParser( AsciiString("ObjectsList"), AsciiString::TheEmptyString, ParseObjectsDataChunk );
		file.registerParser( AsciiString("PolygonTriggers"), AsciiString::TheEmptyString, ParsePolygonTriggersDataChunk );
		file.registerParser( AsciiString("WaypointsList"), AsciiString::TheEmptyString, ParseWaypointDataChunk );
		file.registerParser( AsciiString("ScriptTeams"), AsciiString::TheEmptyString, ParseTeamsDataChunk );
		file.registerParser( AsciiString("ScriptsPlayers"), AsciiString::TheEmptyString, ParsePlayersDataChunk );
		if (!file.parse(this)) {
			throw(ERROR_CORRUPT_FILE_FORMAT);
		}
		pDoc->setNextWaypointID(m_maxWaypoint);

		CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
		SidesListUndoable *pUndo = new SidesListUndoable(m_sides, pDoc);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.		
		m_sides = *TheSidesList;

		if (m_firstReadObject) {
			AddObjectUndoable *pUndo = new AddObjectUndoable(pDoc, m_firstReadObject);
			pDoc->AddAndDoUndoable(pUndo);
			REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
			m_firstReadObject = NULL; // undoable owns it now.
		}
		PolygonTrigger *pTrig;
		PolygonTrigger *pNextTrig;
		for (pTrig=m_firstTrigger; pTrig; pTrig = pNextTrig) {
			pNextTrig = pTrig->getNext();
			pTrig->setNextPoly(NULL);
			PolygonTrigger::addPolygonTrigger(pTrig);
		}

		ScriptList *scripts[MAX_PLAYER_COUNT];
		Int count = ScriptList::getReadScripts(scripts);
		Int i;
		for (i=0; i<count; i++) {
			if (scripts[i]->getScript() == NULL && scripts[i]->getScriptGroup()==NULL) continue;
			Int curSide = -1;
			if (count==1) {
				curSide = m_curSelection.m_playerIndex;
			} else {
				Int j;
				for (j=0; j<m_sides.getNumSides(); j++) {
					// Using i as an index assumes that i < m_sides.getNumSides.  Is that safe???
 					AsciiString name = m_sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
					if (name == m_readPlayerNames[j]) {
						curSide = j;
						break;
					}
				}
				if (curSide == -1) {
					CString msg = "Could not find player";
					msg += m_readPlayerNames[i].str();
					msg += ", discarding scripts for this player.";
					::AfxMessageBox(msg);
					continue;
				}
			}
			if (curSide>= m_sides.getNumSides()) {
				curSide = 0;
				::AfxMessageBox("Imported scripts came from more players than exist in this map.  Additional scripts moved to Neutral player.");
			}
			ScriptList *pSL = m_sides.getSideInfo(curSide)->getScriptList();

			if (pSL) {
				Script *pScr;
				Script *pNextScr;
				Int j=0;
				for (pScr = scripts[i]->getScript(); pScr; pScr=pNextScr) {
					pNextScr=pScr->getNext();
					pScr->setNextScript(NULL);
					pSL->addScript(pScr, j); //unlink it and add.
					j++;
				}
				j=0;
				ScriptGroup *pGroup;
				ScriptGroup *pNextGroup;
				for (pGroup = scripts[i]->getScriptGroup(); pGroup; pGroup=pNextGroup) {
					pNextGroup=pGroup->getNext();
					pGroup->setNextGroup(NULL);
					pSL->addGroup(pGroup, j);
					j++;
				}
				scripts[i]->discard(); /* Frees the script list, but none of it's children, as they have been
															copied into the current scripts. */
				scripts[i] = NULL;
				//reloadPlayer(curSide, pSL);
			}

		}

		for (i = 0; i < m_sides.getNumSides(); i++) {
			// Make sure that the dialog tree is updated.
			ScriptList *pSL = m_sides.getSideInfo(i)->getScriptList();
			reloadPlayer(i, pSL);
			updateIcons(TVI_ROOT);
		}


	} catch(...) {
   	  	DEBUG_CRASH(("threw exception in ScriptDialog::OnLoad"));
	}
}

/**
* ScriptDialog::ParseObjectsDataChunk - read an objects chunk.
* Format is the newer CHUNKY format.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	file.m_currentObject = NULL;
	file.registerParser( AsciiString("Object"), info->label, ParseObjectDataChunk );
	return (file.parse(userData));
}

/**
* WorldHeightMap::ParseObjectData - read a object info chunk.
* Format is the newer CHUNKY format.
*	See WHeightMapEdit.cpp for the writer.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	ScriptDialog *pThis = (ScriptDialog *)userData;
	MapObject *pPrevious = (MapObject *)file.m_currentObject;

	Coord3D loc;
	loc.x = file.readReal();
	loc.y = file.readReal();
	loc.z = file.readReal();
	Real angle = file.readReal();
	Int flags = file.readInt(); 
	AsciiString name = file.readAsciiString();
	Dict d;
	d = file.readDict();
	MapObject *pThisOne;

	// create the map object
	pThisOne = newInstance( MapObject)( loc, name, angle, flags, &d, 
														TheThingFactory->findTemplate( name ) );

	if (pThisOne->getProperties()->getType(TheKey_waypointID) == Dict::DICT_INT) {
		pThisOne->setIsWaypoint();
		pThisOne->setWaypointID(pThisOne->getWaypointID()+pThis->m_waypointBase);
		if (pThis->m_maxWaypoint < pThisOne->getWaypointID()) pThis->m_maxWaypoint = pThisOne->getWaypointID();
	}

	DEBUG_LOG(("Adding object %s (%s)\n", name.str(), pThisOne->getProperties()->getAsciiString(TheKey_originalOwner).str()));
	// Check for duplicates.

	MapObject *pObj;
	Bool duplicate = false;
	for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		Coord3D curLoc;
		curLoc = *pObj->getLocation();
		Bool locsMatch = (loc.x==curLoc.x&&loc.y==curLoc.y);
		// If the locations match, and they are both waypoints or both not waypoints, and the names match, 
		// They're duplicate.
		if (locsMatch && (pObj->isWaypoint() == pThisOne->isWaypoint()) && (pObj->getName() == pThisOne->getName())) {
			duplicate = true;
		}
		if (pThisOne->isWaypoint() && pObj->isWaypoint() ) {
			// If both waypoints, and the names match, are dupes.
			if (!duplicate && (pThisOne->getWaypointName()==pObj->getWaypointName())) {
				AsciiString warning;
				warning.format("Duplicate named waypoints '%s', renaming imported waypoint.", pThisOne->getWaypointName().str());
				::AfxMessageBox(warning.str(), MB_OK);
 				AsciiString name = WaypointOptions::GenerateUniqueName(pThisOne->getWaypointID());
				name.concat("-imp");
			}
		}	
		if (duplicate) break;
	}
	if (duplicate) {
		pThisOne->deleteInstance();
		return true;
	}

	if (pPrevious) {
		DEBUG_ASSERTCRASH(pThis->m_firstReadObject != NULL && pPrevious->getNext() == NULL, ("Bad linkage."));
		pPrevious->setNextMap(pThisOne);
	}	else {
		DEBUG_ASSERTCRASH(pThis->m_firstReadObject == NULL, ("Bad linkage."));
		pThis->m_firstReadObject = pThisOne;
	}
	file.m_currentObject = pThisOne;
	return true;
}

/**
* ScriptDialog::ParseWaypointData - read waypoint data chunk.
* Format is the newer CHUNKY format.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Int count = file.readInt();
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	ScriptDialog *pThis = (ScriptDialog *)userData;
	Int i;
	for (i=0; i<count; i++) {
		Int waypoint1 = file.readInt();
		Int waypoint2 = file.readInt();
		pDoc->addWaypointLink(waypoint1+pThis->m_waypointBase, waypoint2+pThis->m_waypointBase);
		if (pThis->m_maxWaypoint < waypoint1+pThis->m_waypointBase) pThis->m_maxWaypoint = waypoint1+pThis->m_waypointBase;
		if (pThis->m_maxWaypoint < waypoint2+pThis->m_waypointBase) pThis->m_maxWaypoint = waypoint1+pThis->m_waypointBase;
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* ScriptDialog::ParseTeamsDataChunk - read teams data chunk.
* Format is the newer CHUNKY format.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParseTeamsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	ScriptDialog *pThis = (ScriptDialog *)userData;
	while (!file.atEndOfChunk()) {
		Dict teamDict = file.readDict();
		AsciiString teamName = teamDict.getAsciiString(TheKey_teamName);
		if (pThis->m_sides.findTeamInfo(teamName)) {
			continue;
		}
		DEBUG_LOG(("Adding team %s\n", teamName.str()));
		AsciiString player = teamDict.getAsciiString(TheKey_teamOwner);
		if (pThis->m_sides.findSideInfo(player)) {
			// player exists, so just add it.
			pThis->m_sides.addTeam(&teamDict);
		} else {
			AsciiString warning;
			warning.format("Importing team %s of player %s.  Player %s doesn't exist, Select player..", 
				teamName.str(), player.str(), player.str());

			::AfxMessageBox(warning.str(), MB_OK);	
			TeamsInfo ti;	 
			ti.init(&teamDict);
			CFixTeamOwnerDialog fix(&ti, &pThis->m_sides);
			bool nameSet = false;
			if (fix.DoModal() == IDOK) {
				if (fix.pickedValidTeam()) {
					teamDict.setAsciiString(TheKey_teamOwner, fix.getSelectedOwner());
					nameSet = true;
				}
			}
						
			if (nameSet == false) {
				AsciiString neutralPlayerName; // neutral player name is empty string
				// player doesn't exist, so add it to the neutral player.
				teamDict.setAsciiString(TheKey_teamOwner, neutralPlayerName);
			}
			pThis->m_sides.addTeam(&teamDict);
		}
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* ScriptDialog::ParsePlayersDataChunk - read players names data chunk.
* Format is the newer CHUNKY format.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParsePlayersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	ScriptDialog *pThis = (ScriptDialog *)userData;
	Int readDicts = 0;
	if (info->version >= K_PLAYERS_NAMES_FOR_SCRIPTS_VERSION_2) {
		readDicts = file.readInt();
	}
	Int numNames = file.readInt();
	Int i;
	for (i=0; i<numNames; i++) {
		if (i>=MAX_PLAYER_COUNT) break;
		pThis->m_readPlayerNames[i] = file.readAsciiString();
		if (readDicts) {
			Dict sideDict = file.readDict();
			bool nameFound = false;
			for (Int j=0; j < pThis->m_sides.getNumSides(); j++) {
				AsciiString name = pThis->m_sides.getSideInfo(j)->getDict()->getAsciiString(TheKey_playerName);

				if (name == pThis->m_readPlayerNames[i]) {
					// The side already exists so don't add it or overwrite the old data.
					nameFound = true;
					break;
				}
			}
			if (nameFound == false) {
				// This side doesn't currently exist, so add it.
				pThis->m_sides.addSide(&sideDict);
				ScriptList* pList = newInstance(ScriptList);
				SidesInfo* sides = pThis->m_sides.findSideInfo(pThis->m_readPlayerNames[i]);
				// A script list must be created.
				sides->setScriptList(pList);
				// Update the dialog.
				pThis->addPlayer(i);
			}
		}
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Unexpected data left over."));
	return true;
}

/**
* ScriptDialog::ParsePolygonTriggersDataChunk - read a polygon triggers chunk.
* Format is the newer CHUNKY format.
*	See PolygonTrigger::WritePolygonTriggersDataChunk for the writer.
*	Input: DataChunkInput 
*		
*/
Bool ScriptDialog::ParsePolygonTriggersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData)
{
	Int count;
	Int numPoints;
	Int triggerID;
//	Int maxTriggerId = 0;
	AsciiString triggerName;
	// Remove any existing polygon triggers, if any.
	ScriptDialog *pThis = (ScriptDialog *)userData;
	pThis->m_firstTrigger = NULL;
	PolygonTrigger *pPrevTrig = NULL;
	count = file.readInt(); 
	Bool isRiver;
	Int riverStart;
	while (count>0) {
		count--;
		Bool isWater = false;
		triggerName = file.readAsciiString();
		triggerID = file.readInt();
		if (info->version >= K_TRIGGERS_VERSION_2) {
			isWater = file.readByte();
		}
		isRiver = false;
		riverStart = 0;
		if (info->version >= K_TRIGGERS_VERSION_3) {
			isRiver = file.readByte();
			riverStart = file.readInt();
		}
		numPoints = file.readInt(); 
		PolygonTrigger *pTrig = newInstance(PolygonTrigger)(numPoints+1);
		pTrig->setTriggerName(triggerName);
		pTrig->setWaterArea(isWater);
		pTrig->setRiver(isRiver);
		pTrig->setRiverStart(riverStart);
		Int i;
		for (i=0; i<numPoints; i++) {
			ICoord3D loc;
			loc.x = file.readInt();
			loc.y = file.readInt();
			loc.z = file.readInt();
			pTrig->addPoint(loc);
		}
		// Check for duplicates. 
		Bool duplicate = false;
		PolygonTrigger *pCurrentTrigger;
		for (pCurrentTrigger=PolygonTrigger::getFirstPolygonTrigger(); pCurrentTrigger; pCurrentTrigger = pCurrentTrigger->getNext()) {
			if (triggerName == pCurrentTrigger->getTriggerName()) {
				duplicate = true;
				AsciiString warning;
				warning.format("Duplicated trigger named '%s' discarded.", triggerName.str());
				::AfxMessageBox(warning.str(), MB_OK);
				break;
			}
		}
		if (duplicate ) {
			pTrig->deleteInstance();
		} else {
			if (pPrevTrig) {
				pPrevTrig->setNextPoly(pTrig);
			} else {
				pThis->m_firstTrigger = pTrig;
			}
			pPrevTrig = pTrig;
		}
	}
	DEBUG_ASSERTCRASH(file.atEndOfChunk(), ("Incorrect data file length."));
	return true;
}


void ScriptDialog::OnDblclkScriptTree(NMHDR* pNMHDR, LRESULT* pResult) 
{
	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();
	if (pScript == NULL && pGroup == NULL) return;
	OnEditScript();
	*pResult = 0;
}

void ScriptDialog::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == 'S' && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OnSaveActual();
        return;
    }
    CDialog::OnKeyDown(nChar, nRepCnt, nFlags);
}

BOOL ScriptDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            OnSaveActual();
            return TRUE; // consumed, don't pass further
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

// This will fire the save without closing the script dialog screen
void ScriptDialog::OnSaveActual() 
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	SidesListUndoable *pUndo = new SidesListUndoable(m_sides, pDoc);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.

	SaveScriptWarningsState();

	PlaySound("data\\editor\\audio\\finished.wav", NULL, SND_FILENAME | SND_ASYNC);

    // Berate the user if they haven't saved the map yet
    if (pDoc->GetPathName().IsEmpty()) {
        int result = AfxMessageBox(
            "You haven't even saved this map yet, monsieur.\n\n"
            "Save the map file first before trying to use Save Now you doodoo.\n\n"
            "Want to save the map now?",
            MB_YESNO | MB_ICONWARNING
        );

        if (result == IDYES) {
            if (!pDoc->DoFileSave())
                return; // user cancelled or save failed
        } else {
            return; // user said no, bail out
        }
    } else {
		// Save the map now
		if (!pDoc->DoFileSave())
			return; // user cancelled or save failed
	}
}

void ScriptDialog::OnOK() 
{
    // Check if the IDC_OBJECT_SEARCH_EDIT control is focused
    if (GetFocus() == GetDlgItem(IDC_SCRIPT_SEARCH))
    {
        OnFindNext();  // Trigger search on "Enter" key press
    }
    else
    {
		OnSaveActual();

		// Clear focus in scripting mode in main frame
		if (CMainFrame::GetMainFrame()) {
			CMainFrame::GetMainFrame()->setFocusInScripting(false);
		}

        CDialog::OnOK();  // Call the default OK behavior if the search box isn't focused
	}
}

void ScriptDialog::OnCancel() 
{
	// Clear focus in scripting mode in main frame
	if (CMainFrame::GetMainFrame()) {
		CMainFrame::GetMainFrame()->setFocusInScripting(false);
	}
	CDialog::OnCancel();
}

#ifdef RTS_HAS_QT
//----------------------------------------------------------------------------------------
// Qt Script-editor front-end support. The Qt window (WBQtScriptWindow) drives this hidden
// MFC dialog: it reads the flat tree model (qtGetNodeCount/qtGetNode, mirroring the order
// addPlayer/addScriptList build), pushes selection into m_curSelection (qtSetSelection),
// and invokes the same command handlers (qtDo*). m_sides / the sub-editors / commit stay
// here. formatScriptLabel is file-static in this TU, so the label building lives here.
//----------------------------------------------------------------------------------------

// Walk the working model in the same pre-order the tree uses, invoking a callback per node.
// depth: 0 player, 1 group or ungrouped-script, 2 script-in-group.
namespace {
	// flags per node: bit0 active, bit1 hasWarnings, bit2 subroutine (0 for player nodes).
	struct QtNodeVisitor { virtual void visit(int depth, int listTypeInt, int flags, const AsciiString &label) = 0; };
}

namespace {
	enum { QT_NODE_ACTIVE = 1, QT_NODE_WARNINGS = 2, QT_NODE_SUBROUTINE = 4 };
	int qtScriptFlags(Script *pScr)
	{
		int f = 0;
		if (pScr->isActive()) { f |= QT_NODE_ACTIVE; }
		if (pScr->hasWarnings()) { f |= QT_NODE_WARNINGS; }
		if (pScr->isSubroutine()) { f |= QT_NODE_SUBROUTINE; }
		return f;
	}
	int qtGroupFlags(ScriptGroup *pGroup)
	{
		int f = 0;
		if (pGroup->isActive()) { f |= QT_NODE_ACTIVE; }
		if (pGroup->hasWarnings()) { f |= QT_NODE_WARNINGS; }
		if (pGroup->isSubroutine()) { f |= QT_NODE_SUBROUTINE; }
		return f;
	}
}

static void qtWalkModel(SidesList &sides, Bool cleanNames, QtNodeVisitor &v)
{
	for (Int p = 0; p < sides.getNumSides(); p++)
	{
		Dict *dd = sides.getSideInfo(p)->getDict();
		AsciiString pname = dd->getAsciiString(TheKey_playerName);
		AsciiString plabel;
		if (pname.isEmpty())
		{
			plabel = NEUTRAL_NAME_STR;
		}
		else
		{
			plabel = pname;
		}
		ListType lt;
		lt.m_objType = ListType::PLAYER_TYPE;
		lt.m_playerIndex = p;
		v.visit(0, lt.ListToInt(), 0, plabel);

		ScriptList *pSL = sides.getSideInfo(p)->getScriptList();
		if (pSL == NULL)
		{
			continue;
		}

		// Pass A: groups (folders) and their scripts.
		Int groupNdx = 0;
		for (ScriptGroup *pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext(), groupNdx++)
		{
			if (pGroup->getName().isEmpty())
			{
				continue;
			}
			ListType glt;
			glt.m_objType = ListType::GROUP_TYPE;
			glt.m_playerIndex = p;
			glt.m_groupIndex = groupNdx;
			v.visit(1, glt.ListToInt(), qtGroupFlags(pGroup), formatScriptLabel(pGroup));

			Int scriptNdx = 0;
			for (Script *pScr = pGroup->getScript(); pScr; pScr = pScr->getNext(), scriptNdx++)
			{
				if (pScr->getName().isEmpty())
				{
					continue;
				}
				ListType slt;
				slt.m_objType = ListType::SCRIPT_IN_GROUP_TYPE;
				slt.m_playerIndex = p;
				slt.m_groupIndex = groupNdx;
				slt.m_scriptIndex = scriptNdx;
				v.visit(2, slt.ListToInt(), qtScriptFlags(pScr), formatScriptLabel(pScr, cleanNames));
			}
		}

		// Pass B: ungrouped scripts (direct children of the player).
		Int scriptNdx = 0;
		for (Script *pScr = pSL->getScript(); pScr; pScr = pScr->getNext(), scriptNdx++)
		{
			if (pScr->getName().isEmpty())
			{
				continue;
			}
			ListType slt;
			slt.m_objType = ListType::SCRIPT_IN_PLAYER_TYPE;
			slt.m_playerIndex = p;
			slt.m_groupIndex = 0;
			slt.m_scriptIndex = scriptNdx;
			v.visit(1, slt.ListToInt(), qtScriptFlags(pScr), formatScriptLabel(pScr, cleanNames));
		}
	}
}

namespace {
	// Counts nodes.
	struct QtCountVisitor : public QtNodeVisitor {
		int count;
		QtCountVisitor() : count(0) {}
		virtual void visit(int, int, int, const AsciiString &) { count++; }
	};
	// Captures node #target into out-params.
	struct QtPickVisitor : public QtNodeVisitor {
		int target; int cur; int depth; int listType; int flags; AsciiString label; Bool found;
		QtPickVisitor(int t) : target(t), cur(0), depth(0), listType(0), flags(0), found(false) {}
		virtual void visit(int d, int lt, int fl, const AsciiString &l)
		{
			if (cur == target)
			{
				depth = d; listType = lt; flags = fl; label = l; found = true;
			}
			cur++;
		}
	};
}

int ScriptDialog::qtGetNodeCount(void)
{
	QtCountVisitor cv;
	qtWalkModel(m_sides, m_bCleanScriptName, cv);
	return cv.count;
}

int ScriptDialog::qtGetNode(int i, int *depthOut, int *listTypeOut, int *flagsOut, char *labelOut, int cap)
{
	QtPickVisitor pv(i);
	qtWalkModel(m_sides, m_bCleanScriptName, pv);
	if (!pv.found)
	{
		return 0;
	}
	if (depthOut != NULL)
	{
		*depthOut = pv.depth;
	}
	if (listTypeOut != NULL)
	{
		*listTypeOut = pv.listType;
	}
	if (flagsOut != NULL)
	{
		*flagsOut = pv.flags;
	}
	if (labelOut != NULL && cap > 0)
	{
		strncpy(labelOut, pv.label.str(), cap - 1);
		labelOut[cap - 1] = 0;
	}
	return 1;
}

void ScriptDialog::qtSetSelection(int listTypeInt)
{
	// Guard against a stale ListType (e.g. a find cursor from a previous, now-deleted dialog
	// session) whose player index is out of range for THIS session's m_sides -- getCurScript /
	// getCurGroup call getSideInfo(m_playerIndex) unconditionally and would deref garbage.
	ListType lt;
	lt.IntToList(listTypeInt);
	if (lt.m_playerIndex >= m_sides.getNumSides())
	{
		return;	// ignore an out-of-range selection rather than crash downstream
	}
	m_curSelection = lt;
}

int ScriptDialog::qtGetSelection(void)
{
	return m_curSelection.ListToInt();
}

int ScriptDialog::qtHasScript(void)
{
	return (getCurScript() != NULL) ? 1 : 0;
}

Script *ScriptDialog::qtCurScript(void)
{
	return getCurScript();
}

int ScriptDialog::qtHasGroup(void)
{
	return (getCurGroup() != NULL) ? 1 : 0;
}

void ScriptDialog::qtDoNewFolder(void) { OnNewFolder(); }
void ScriptDialog::qtDoNewScript(void) { OnNewScript(); }
void ScriptDialog::qtDoEditScript(void) { OnEditScript(); }
void ScriptDialog::qtDoCopyScript(void) { OnCopyScript(); }
void ScriptDialog::qtDoDelete(void) { OnDelete(); }

void ScriptDialog::qtCommitAndClose(void)
{
	// Commit the working model (m_sides -> TheSidesList via SidesListUndoable + save), like
	// OnOK's non-search path. We do NOT call CDialog::OnOK() (that is the visible-modeless
	// close path) and do NOT destroy the dialog here -- the bridge tears it down AFTER this
	// returns, so `this` stays valid for the rest of the call.
	OnSaveActual();
	if (CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->setFocusInScripting(false);
	}
}

void ScriptDialog::qtCancelAndClose(void)
{
	// Discard the working model (nothing committed). Teardown happens in the bridge after
	// this returns (see qtCommitAndClose).
	if (CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->setFocusInScripting(false);
	}
}

void ScriptDialog::qtDropOn(int dragListType, int targetListType)
{
	// Resolve both nodes on the (hidden) MFC tree and reuse doDropOn unchanged -- it reads
	// the packed ListType off each item, mutates m_sides (reorder / move / Ctrl auto-merge),
	// and refreshes the MFC tree; the Qt window rebuilds afterwards. doDropOn reads the live
	// Ctrl key state itself, so a Ctrl-drag in Qt still triggers the merge path.
	ListType dragLT;
	dragLT.IntToList(dragListType);
	ListType targetLT;
	targetLT.IntToList(targetListType);
	HTREEITEM hDrag = findItem(dragLT);
	HTREEITEM hTarget = findItem(targetLT);
	if (hDrag != NULL && hTarget != NULL)
	{
		doDropOn(hDrag, hTarget);
	}
}

// Case-insensitive "haystack contains needle" that walks the strings in place. Deliberately
// NOT AsciiString::toLower(): that round-trips through a fixed 2K stack buffer with an
// unbounded strcpy, and a big script's comment+uiText blows it (GS stack-cookie crash).
static Bool qtContainsNoCase(const char *haystack, const char *needle)
{
	if (haystack == NULL || needle == NULL || needle[0] == 0)
	{
		return false;
	}
	for (const char *h = haystack; *h; h++)
	{
		const char *a = h;
		const char *b = needle;
		while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
		{
			a++;
			b++;
		}
		if (*b == 0)
		{
			return true;
		}
	}
	return false;
}

// Does the script at the given selection deep-match the search text? Mirrors the deep scan
// in OnFindNext: comment + uiText + every condition/action parameter string.
static Bool qtScriptDeepMatches(Script *pScr, const AsciiString &needle)
{
	if (pScr == NULL)
	{
		return false;
	}
	AsciiString content = pScr->getComment();
	content.concat(pScr->getUiText());
	if (qtContainsNoCase(content.str(), needle.str()))
	{
		return true;
	}
	for (OrCondition *pOr = pScr->getOrCondition(); pOr; pOr = pOr->getNextOrCondition())
	{
		for (Condition *c = pOr->getFirstAndCondition(); c; c = c->getNext())
		{
			for (int p = 0; p < c->getNumParameters(); ++p)
			{
				if (qtContainsNoCase(c->getParameter(p)->getString().str(), needle.str()))
				{
					return true;
				}
			}
		}
	}
	for (ScriptAction *a = pScr->getAction(); a; a = a->getNext())
	{
		for (int p = 0; p < a->getNumParameters(); ++p)
		{
			if (qtContainsNoCase(a->getParameter(p)->getString().str(), needle.str()))
			{
				return true;
			}
		}
	}
	return false;
}

namespace {
	// Finds the first node (in tree pre-order) strictly AFTER a 'from' node whose label or
	// script content matches. from==0 means start at the top.
	struct QtFindVisitor : public QtNodeVisitor {
		ScriptDialog *dlg; AsciiString needle; int fromInt; Bool passedFrom; Bool found; int result;
		QtFindVisitor(ScriptDialog *d, const AsciiString &n, int f)
			: dlg(d), needle(n), fromInt(f), passedFrom(f == 0), found(false), result(0) {}
		virtual void visit(int, int listTypeInt, int, const AsciiString &label)
		{
			if (found)
			{
				return;
			}
			if (!passedFrom)
			{
				if (listTypeInt == fromInt)
				{
					passedFrom = true;
				}
				return;
			}
			Bool match = qtContainsNoCase(label.str(), needle.str());
			if (!match)
			{
				ListType lt;
				lt.IntToList(listTypeInt);
				if (lt.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE ||
					lt.m_objType == ListType::SCRIPT_IN_GROUP_TYPE)
				{
					int saved = dlg->qtGetSelection();
					dlg->qtSetSelection(listTypeInt);
					match = qtScriptDeepMatches(dlg->qtCurScript(), needle);
					dlg->qtSetSelection(saved);
				}
			}
			if (match)
			{
				found = true;
				result = listTypeInt;
			}
		}
	};
}

int ScriptDialog::qtFindNext(const char *text, int fromListType, int *outListType)
{
	if (text == NULL || text[0] == 0)
	{
		return 0;
	}
	AsciiString needle = text;
	QtFindVisitor fv(this, needle, fromListType);
	qtWalkModel(m_sides, m_bCleanScriptName, fv);
	if (!fv.found)
	{
		return 0;
	}
	if (outListType != NULL)
	{
		*outListType = fv.result;
	}
	return 1;
}

void ScriptDialog::qtVerify(void)
{
	// == OnVerifyAll: recompute all script/group warning flags. The Qt window rebuilds after,
	// so the fresh flags reach the tree; updateIcons keeps the (hidden) MFC tree consistent.
	updateWarnings(true);
	updateIcons(TVI_ROOT);
}

void ScriptDialog::qtToggleActive(void)
{
	// == OnScriptActivate: flip the current script/group's active flag (uses m_curSelection,
	// which the Qt window pushed before calling this).
	OnScriptActivate();
}

void ScriptDialog::qtGetDetail(int listTypeInt, char *descOut, int descCap, char *commentOut, int commentCap)
{
	if (descOut != NULL && descCap > 0)
	{
		descOut[0] = 0;
	}
	if (commentOut != NULL && commentCap > 0)
	{
		commentOut[0] = 0;
	}

	// Resolve the node without disturbing the real selection.
	ListType saved = m_curSelection;
	ListType lt;
	lt.IntToList(listTypeInt);
	if (lt.m_playerIndex >= m_sides.getNumSides())
	{
		return;
	}
	m_curSelection = lt;
	Script *pScript = getCurScript();

	if (pScript != NULL)
	{
		// Description = the script's readable breakdown (== IDC_SCRIPT_DESCRIPTION).
		if (descOut != NULL && descCap > 0)
		{
			strncpy(descOut, pScript->getUiText().str(), descCap - 1);
			descOut[descCap - 1] = 0;
		}

		// Comment = comment + condition/action comments, exactly like IDC_SCRIPT_COMMENT but
		// WITHOUT the expensive cross-script "[Referenced in]" scan (the MFC 'Disable
		// references' fast path). parseLineBreaks is file-static in this TU.
		AsciiString scriptComment = pScript->getComment();
		AsciiString conditionComment = pScript->getConditionComment();
		AsciiString actionComment = pScript->getActionComment();
		if (!scriptComment.isEmpty())
		{
			scriptComment.concat("\n\n");
		}
		if (!conditionComment.isEmpty())
		{
			scriptComment.concat("[Condition Comment] : ");
			scriptComment.concat(conditionComment);
			scriptComment.concat("\n\n");
		}
		if (!actionComment.isEmpty())
		{
			scriptComment.concat("[Action Comment] : ");
			scriptComment.concat(actionComment);
			scriptComment.concat("\n\n");
		}
		scriptComment = parseLineBreaks(scriptComment);
		if (commentOut != NULL && commentCap > 0)
		{
			strncpy(commentOut, scriptComment.str(), commentCap - 1);
			commentOut[commentCap - 1] = 0;
		}
	}

	m_curSelection = saved;
}

// 9d checkboxes. Ids mirror WBQtPanelBridge.h's WBQT_SCK_*. Get reads the backing member;
// Set writes the hidden MFC checkbox control then calls the real On* handler so the member,
// registry persistence, and side effects (font/icon/tree rebuild) all match the MFC path.
namespace {
	enum { SCK_COMPRESS = 0, SCK_NEWICONS, SCK_CLEANNAME, SCK_AUTOVERIFY, SCK_SMARTCOPY,
		SCK_FASTLOAD, SCK_SCRIPTMERGE, SCK_REFBYPARAM, SCK_DISABLEREF };
	int sckControlId(int which)
	{
		switch (which)
		{
			case SCK_COMPRESS:    return IDC_COMPRESS;
			case SCK_NEWICONS:    return IDC_NEWICONS;
			case SCK_CLEANNAME:   return IDC_CLEANSCRIPTNAME;
			case SCK_AUTOVERIFY:  return IDC_AUTO_VERIFY;
			case SCK_SMARTCOPY:   return IDC_SMART_COPY;
			case SCK_FASTLOAD:    return IDC_DEEPSCAN;
			case SCK_SCRIPTMERGE: return IDC_SCRIPT_MERGE;
			case SCK_REFBYPARAM:  return IDC_REFRENCEMODE1;
			case SCK_DISABLEREF:  return IDC_DISABLEREFERENCE;
			default: return 0;
		}
	}
}

int ScriptDialog::qtGetCheckbox(int which)
{
	switch (which)
	{
		case SCK_COMPRESS:    return m_bCompressed ? 1 : 0;
		case SCK_NEWICONS:    return m_bNewIcons ? 1 : 0;
		case SCK_CLEANNAME:   return m_bCleanScriptName ? 1 : 0;
		case SCK_AUTOVERIFY:  return m_autoUpdateWarnings ? 1 : 0;
		case SCK_SMARTCOPY:   return m_bSmartCopyEnabled ? 1 : 0;
		case SCK_FASTLOAD:    return m_bDisableDeepScan ? 1 : 0;
		case SCK_SCRIPTMERGE: return m_bAutoMergeScripts ? 1 : 0;
		case SCK_REFBYPARAM:  return m_bCheckByParameter ? 1 : 0;
		case SCK_DISABLEREF:  return m_bDisableReferences ? 1 : 0;
		default: return 0;
	}
}

void ScriptDialog::qtSetCheckbox(int which, int checked)
{
	int idc = sckControlId(which);
	if (idc == 0)
	{
		return;
	}
	CButton *pButton = (CButton *)GetDlgItem(idc);
	if (pButton != NULL)
	{
		pButton->SetCheck(checked ? 1 : 0);
	}
	// Drive the real handler (reads the checkbox back, sets the member, persists, side FX).
	switch (which)
	{
		case SCK_COMPRESS:    OnCompress(); break;
		case SCK_NEWICONS:    OnNewIcons(); break;
		case SCK_CLEANNAME:   OnCleanScriptName(); break;
		case SCK_AUTOVERIFY:  OnAutoVerify(); break;
		case SCK_SMARTCOPY:   OnSmartCopy(); break;
		case SCK_FASTLOAD:    OnDisableDeepScan(); break;
		case SCK_SCRIPTMERGE: OnAutoMergeScripts(); break;
		case SCK_REFBYPARAM:  OnCheckByParameterForReference(); break;
		case SCK_DISABLEREF:  OnDisableReferencesEntirely(); break;
		default: break;
	}
}

void ScriptDialog::qtAddDebug(void)      { OnAddDebug(); }
void ScriptDialog::qtRemoveDebug(void)   { OnRemoveDebug(); }
void ScriptDialog::qtPatchGC(void)       { OnPatchGC(); }
void ScriptDialog::qtExportScripts(void) { OnSave(); }
void ScriptDialog::qtImportScripts(void) { OnLoad(); }
void ScriptDialog::qtSaveNow(void)       { OnSaveActual(); }
#endif

void ScriptDialog::OnBegindragScriptTree(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);

	m_curSelection.IntToList(pNMTreeView->itemNew.lParam);
	if (m_curSelection.m_objType != ListType::PLAYER_TYPE) {
		m_dragItem = pNMTreeView->itemNew.hItem;
    pTree->SelectItem(m_dragItem); 
		m_draggingTreeView = true;
 		SetCapture();
	}
	*pResult = 0;
}

void ScriptDialog::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (m_draggingTreeView) {
		CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
 
    HTREEITEM htiTarget;  // handle to target item 
    TVHITTESTINFO tvht;  // hit test information 

// Adjust the drag point to align with the center of the tree item.
		const Int CENTER_OFFSET = 50;
		point.y -= CENTER_OFFSET;
    tvht.pt = point; 
    if ((htiTarget = pTree->HitTest( &tvht)) != NULL) {
			pTree->SelectDropTarget(htiTarget); 
    } 
  }
	
	CDialog::OnMouseMove(nFlags, point);
}

void ScriptDialog::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (m_draggingTreeView) {
		CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
		m_draggingTreeView = false;

		ReleaseCapture();
    HTREEITEM htiTarget;  // handle to target item 
    TVHITTESTINFO tvht;  // hit test information 

// Adjust the drag point to align with the center of the tree item.
		const Int CENTER_OFFSET = 50;
		point.y -= CENTER_OFFSET;
    tvht.pt = point; 
    if ((htiTarget = pTree->HitTest( &tvht)) != NULL) { 
      pTree->SelectItem(htiTarget); 
			pTree->SelectDropTarget(htiTarget);
			doDropOn(m_dragItem, htiTarget);
    } 
	}
	CDialog::OnLButtonUp(nFlags, point);
}

void ScriptDialog::doDropOn(HTREEITEM hDrag, HTREEITEM hTarget) 
{
	if (hDrag == hTarget) return;
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	ListType drag;
	drag.IntToList(pTree->GetItemData(hDrag));
	ListType target;
	target.IntToList(pTree->GetItemData(hTarget));			

	Script *dragScript = NULL;
	ScriptGroup *dragGroup = NULL;
	m_curSelection = drag;
	Script *pScript = getCurScript();
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	ScriptGroup *pGroup = getCurGroup();
	bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (pSL == NULL) return;
	
	if (pScript) {
		// NEW CODE: Check if dropping a script onto another script with auto-merge enabled
		if (m_bAutoMergeScripts && isCtrlDown &&
		    (target.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE || 
		     target.m_objType == ListType::SCRIPT_IN_GROUP_TYPE)) {
			
			// Get the target script
			m_curSelection = target;
			Script *targetScript = getCurScript();
			
			if (targetScript && pScript != targetScript) {
				// MERGE OPERATION
				Script *sourceScript = pScript;
				
				// Count actions being merged
				Int trueActionCount = 0;
				Int falseActionCount = 0;
				
				// Copy all true actions from source to target
				ScriptAction *pAction = sourceScript->getAction();
				
				// Find the end of target's true action chain
				ScriptAction *pTargetLastTrue = targetScript->getAction();
				if (pTargetLastTrue) {
					while (pTargetLastTrue->getNext()) {
						pTargetLastTrue = pTargetLastTrue->getNext();
					}
				}
				
				// Append source true actions
				while (pAction) {
					trueActionCount++;
					ScriptAction *pDup = pAction->duplicate();
					if (pTargetLastTrue) {
						pTargetLastTrue->setNextAction(pDup);
						pTargetLastTrue = pDup;
					} else {
						targetScript->setAction(pDup);
						pTargetLastTrue = pDup;
					}
					pAction = pAction->getNext();
				}
				
				// Copy all false actions from source to target
				pAction = sourceScript->getFalseAction();
				
				// Find the end of target's false action chain
				ScriptAction *pTargetLastFalse = targetScript->getFalseAction();
				if (pTargetLastFalse) {
					while (pTargetLastFalse->getNext()) {
						pTargetLastFalse = pTargetLastFalse->getNext();
					}
				}
				
				// Append source false actions
				while (pAction) {
					falseActionCount++;
					ScriptAction *pDup = pAction->duplicate();
					if (pTargetLastFalse) {
						pTargetLastFalse->setNextAction(pDup);
						pTargetLastFalse = pDup;
					} else {
						targetScript->setFalseAction(pDup);
						pTargetLastFalse = pDup;
					}
					pAction = pAction->getNext();
				}
				
				// Delete the source script
				m_curSelection = drag;
				pGroup = getCurGroup();
				pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
				
				if (pGroup) {
					pGroup->deleteScript(sourceScript);
				} else {
					pSL->deleteScript(sourceScript);
				}
				
				// Mark target script as dirty and update warnings
				targetScript->setDirty(true);
				updateScriptWarning(targetScript);
				
				// Reload and update
				pTree->DeleteItem(hDrag);
				reloadPlayer(target.m_playerIndex, m_sides.getSideInfo(target.m_playerIndex)->getScriptList());
				updateSelection(target);
				updateIcons(TVI_ROOT);
				
				// Update the tree item text to refresh the display
				HTREEITEM targetItem = findItem(target);
				if (targetItem) {
					pTree->SetItemText(targetItem, formatScriptLabel(targetScript, m_bCleanScriptName).str());
				}
				
				CString msg;
				msg.Format("Successfully merged script:\n%d true action(s)\n%d false action(s)", 
				           trueActionCount, falseActionCount);
				AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
				return;
			}
		}
		
		// ORIGINAL CODE: Normal script moving/reordering
		dragScript = pScript->duplicate();
		if (pGroup) {
			pGroup->deleteScript(pScript);
		} else {
			pSL->deleteScript(pScript);
		}
		if (drag.m_objType == target.m_objType &&
				drag.m_playerIndex == target.m_playerIndex &&
				drag.m_groupIndex == target.m_groupIndex &&
				drag.m_scriptIndex < target.m_scriptIndex) {
				target.m_scriptIndex--;
		}
	}	else if (drag.m_objType == ListType::GROUP_TYPE) {
		// NEW CODE: Check if dropping a group onto another group with auto-merge enabled
		if (m_bAutoMergeScripts && isCtrlDown && target.m_objType == ListType::GROUP_TYPE) {
			// MERGE OPERATION for groups
			ScriptGroup *sourceGroup = pGroup;
			
			// Get the target group
			m_curSelection = target;
			ScriptGroup *targetGroup = getCurGroup();
			
			if (!targetGroup || !sourceGroup) {
				AfxMessageBox("Error: Could not find groups for merging.", MB_OK | MB_ICONERROR);
				return;
			}
			
			// Copy all scripts from source to target
			Int scriptCount = 0;
			Script *pScr = sourceGroup->getScript();
			while (pScr) {
				scriptCount++;
				pScr = pScr->getNext();
			}
			
			// Add scripts to target group at the end
			for (Script *pScrn = sourceGroup->getScript(); pScrn; pScrn = pScrn->getNext()) {
				Script *pDup = pScrn->duplicate();
				// Find the end position in target group
				Int endPos = 0;
				Script *pTargetScr = targetGroup->getScript();
				while (pTargetScr) {
					endPos++;
					pTargetScr = pTargetScr->getNext();
				}
				targetGroup->addScript(pDup, endPos);
			}
			
			// Delete the source group
			m_curSelection = drag;
			pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
			pSL->deleteGroup(sourceGroup);
			
			// Reload and update
			pTree->DeleteItem(hDrag);
			reloadPlayer(drag.m_playerIndex, pSL);
			updateSelection(target);
			updateIcons(TVI_ROOT);
			
			CString msg;
			msg.Format("Successfully merged %d script(s) into the target group.", scriptCount);
			AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
			return;
		}
		
		// ORIGINAL CODE (without popup): Normal group reordering
		dragGroup = pGroup->duplicate();
		pSL->deleteGroup(pGroup);
		if (drag.m_objType != ListType::SCRIPT_IN_PLAYER_TYPE &&
				drag.m_playerIndex == target.m_playerIndex &&
				drag.m_groupIndex < target.m_groupIndex) {
				target.m_groupIndex--;
		}
	}
	pTree->DeleteItem(hDrag);
	m_curSelection = target;
	pScript = getCurScript();
	pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	pGroup = getCurGroup();
	DEBUG_ASSERTCRASH((pSL), ("Hmm - bad data. jba."));
	if (pSL == NULL) return;

	// If we are dragging a group onto a script, adjust the group index so we add after.
	if (drag.m_objType == ListType::GROUP_TYPE) {
		if (target.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE) {
			target.m_groupIndex = 9999;
		}
		if (target.m_objType == ListType::SCRIPT_IN_GROUP_TYPE) {
			target.m_groupIndex++;
		}
		target.m_objType = ListType::GROUP_TYPE;
	}

	if (dragScript) {
		if (pGroup) { 
				pGroup->addScript(dragScript, target.m_scriptIndex);
		}	else {
			pSL->addScript(dragScript, target.m_scriptIndex);
		}
	} else if (dragGroup) {
		pSL->addGroup(dragGroup, target.m_groupIndex);
		Int count = 0;
		ScriptGroup *pGroup = pSL->getScriptGroup();
		while (pGroup->getNext()) {
			if (pGroup==dragGroup) break;
			count++;
			pGroup = pGroup->getNext();
		}
		target.m_groupIndex = count;
	}
	if (target.m_playerIndex != drag.m_playerIndex) {
		reloadPlayer(drag.m_playerIndex, m_sides.getSideInfo(drag.m_playerIndex)->getScriptList());
	}
	reloadPlayer(target.m_playerIndex, pSL);
	updateSelection(target);
	updateIcons(TVI_ROOT);
}

void ScriptDialog::OnMove(int x, int y) 
{
	CDialog::OnMove(x, y);
	
	if (this->IsWindowVisible() && !this->IsIconic()) {
		CRect frameRect;
		GetWindowRect(&frameRect);
		::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "Top", frameRect.top);
		::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "Left", frameRect.left);
	}
	
}

/** This function reacts to the selection of "active" from
    the right click drop down menu */
void ScriptDialog::OnScriptActivate()
{
	AsciiString newName;
	Bool active;
	CTreeCtrl *pTree = (CTreeCtrl*)GetDlgItem(IDC_SCRIPT_TREE);
	HTREEITEM item = findItem(m_curSelection);

	if (getCurScript() != NULL)
	{
		// Toggle active state
		active = getCurScript()->isActive();
		getCurScript()->setActive(!active);

		// Update label
		Script *pScript = getCurScript();
		pTree->SetItemText(item, formatScriptLabel(pScript, m_bCleanScriptName).str());

		// Set icon using centralized logic
		setIconScript(item);
	}
	else if (getCurGroup() != NULL)
	{
		// Toggle active state
		active = getCurGroup()->isActive();
		getCurGroup()->setActive(!active);

		// Update label
		ScriptGroup *pScriptGroup = getCurGroup();
		pTree->SetItemText(item, formatScriptLabel(pScriptGroup).str());

		// Set icon using centralized logic
		setIconGroup(item);
	}
}
