// WBQtScriptBridge.cpp -- the MFC side of the Qt Script-editor seam (Phase 9a). Plain MFC TU
// (no Qt include); reverse callbacks forward to the still-created-but-hidden MFC ScriptDialog
// via ScriptDialog::qtInstance(). Whole body guarded by RTS_HAS_QT so the OFF build compiles
// it to an empty object. The heavy lifting (model walk, command handlers, commit) lives on
// ScriptDialog itself; this file is just the C seam.
#include "StdAfx.h"
#include "resource.h"				// IDD_ScriptDialog (used in ScriptDialog.h's IDD enum)
#include "Lib/BaseType.h"
#include "WorldBuilderDoc.h"		// MapObject (ScriptDialog.h has a MapObject* member)
#include "GameLogic/PolygonTrigger.h"	// PolygonTrigger (ditto)
#include "ScriptDialog.h"
#include "MainFrm.h"
#include "CUndoable.h"				// SidesListUndoable / AddObjectUndoable (import commit)
#include "EditGroup.h"				// the New Folder name dialog (same one OnNewFolder pops)
#include "EditParameter.h"			// setCurSidesList (the param editors resolve sides from it)
#include "ExportScriptsOptions.h"	// the export options statics (qtM import/export)
#include "GameLogic/ScriptEngine.h"	// Script/Condition/OrCondition/ScriptAction/Parameter
#include "Common/WellKnownKeys.h"	// TheKey_playerName (force-rename pass)
#include "Common/DataChunk.h"		// DataChunkInput (import)
#include "Common/FileSystem.h"		// CachedFileInputStream (import)
#include "qt/WBQtPanelBridge.h"
#include "qt/panels/WBQtScriptEditBridge.h"	// WBQtScriptEdit_Run (new/edit script sheet)
#include "qt/panels/WBQtParamBridge.h"		// WBQtEditGroup_Run (edit folder dialog)

#include <vector>

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// Editor-local undo/redo: snapshots of the working copy (m_sides), taken by each mutating
// qtM* command right before it changes the model. This is the same copy idiom the session
// already uses (open: m_sides = *TheSidesList; commit: one SidesListUndoable) -- undo here
// never touches the document, only the uncommitted working copy. TU-statics are safe: the
// dialog is a singleton (qtInstance) and the stacks are cleared on every qtOpenModelOnly.
namespace
{
	const int kQtScriptUndoDepth = 32;
	std::vector<SidesList *> s_qtScriptUndo;
	std::vector<SidesList *> s_qtScriptRedo;

	void qtFreeSnapshots(std::vector<SidesList *> &stack)
	{
		for (size_t i = 0; i < stack.size(); i++)
		{
			delete stack[i];
		}
		stack.clear();
	}
}

// Exact-name lookup for the detail pane's clickable "[Referenced in]" links. Walks the
// working copy with the same player/group/script indices the Qt tree was built from, so
// the returned packed ListType matches the tree item's stored one.
int ScriptDialog::qtFindScriptByName(const char *name)
{
	if (name == NULL || name[0] == 0)
	{
		return -1;
	}
	AsciiString target(name);
	for (Int i = 0; i < m_sides.getNumSides(); i++)
	{
		ScriptList *pSL = m_sides.getSideInfo(i)->getScriptList();
		if (!pSL)
		{
			continue;
		}
		ListType lt;
		lt.m_playerIndex = (unsigned char)i;
		Int ndx;
		Script *s;
		for (s = pSL->getScript(), ndx = 0; s; s = s->getNext(), ndx++)
		{
			if (s->getName() == target)
			{
				lt.m_objType = ListType::SCRIPT_IN_PLAYER_TYPE;
				lt.m_groupIndex = 0;
				lt.m_scriptIndex = (unsigned short)ndx;
				return lt.ListToInt();
			}
		}
		Int groupNdx;
		ScriptGroup *g;
		for (g = pSL->getScriptGroup(), groupNdx = 0; g; g = g->getNext(), groupNdx++)
		{
			for (s = g->getScript(), ndx = 0; s; s = s->getNext(), ndx++)
			{
				if (s->getName() == target)
				{
					lt.m_objType = ListType::SCRIPT_IN_GROUP_TYPE;
					lt.m_groupIndex = (unsigned short)groupNdx;
					lt.m_scriptIndex = (unsigned short)ndx;
					return lt.ListToInt();
				}
			}
		}
	}
	return -1;
}

void ScriptDialog::qtPushUndoSnapshot(void)
{
	SidesList *snap = new SidesList;
	*snap = m_sides;
	s_qtScriptUndo.push_back(snap);
	if ((int)s_qtScriptUndo.size() > kQtScriptUndoDepth)
	{
		delete s_qtScriptUndo.front();
		s_qtScriptUndo.erase(s_qtScriptUndo.begin());
	}
	// A new edit invalidates the redo trail, like any linear undo stack.
	qtFreeSnapshots(s_qtScriptRedo);
}

void ScriptDialog::qtDropLastUndoSnapshot(void)
{
	if (!s_qtScriptUndo.empty())
	{
		delete s_qtScriptUndo.back();
		s_qtScriptUndo.pop_back();
	}
}

void ScriptDialog::qtClearUndoHistory(void)
{
	qtFreeSnapshots(s_qtScriptUndo);
	qtFreeSnapshots(s_qtScriptRedo);
}

int ScriptDialog::qtMUndo(void)
{
	if (s_qtScriptUndo.empty())
	{
		return 0;
	}
	SidesList *cur = new SidesList;
	*cur = m_sides;
	s_qtScriptRedo.push_back(cur);
	SidesList *snap = s_qtScriptUndo.back();
	s_qtScriptUndo.pop_back();
	m_sides = *snap;
	delete snap;
	// The old selection's indices may not exist in the restored state; fall back to the
	// player row (the Qt window rebuilds its tree and re-pushes selection on the next click).
	m_curSelection.m_objType = ListType::PLAYER_TYPE;
	m_curSelection.m_groupIndex = 0;
	m_curSelection.m_scriptIndex = 0;
	// The SidesList copy does not carry the computed per-script warning flags (same as the
	// session-open copy), so recompute them or every red label/icon goes white after undo.
	if (m_autoUpdateWarnings)
	{
		updateWarnings(true);
	}
	return 1;
}

int ScriptDialog::qtMRedo(void)
{
	if (s_qtScriptRedo.empty())
	{
		return 0;
	}
	SidesList *cur = new SidesList;
	*cur = m_sides;
	s_qtScriptUndo.push_back(cur);
	SidesList *snap = s_qtScriptRedo.back();
	s_qtScriptRedo.pop_back();
	m_sides = *snap;
	delete snap;
	m_curSelection.m_objType = ListType::PLAYER_TYPE;
	m_curSelection.m_groupIndex = 0;
	m_curSelection.m_scriptIndex = 0;
	if (m_autoUpdateWarnings)
	{
		updateWarnings(true);
	}
	return 1;
}

//----------------------------------------------------------------------------------------
// De-bridged (windowless) ScriptDialog members -- branch qt-debridge. In Qt mode the
// dialog window is never Create()d: the ScriptDialog OBJECT is only the model container
// (m_sides + m_curSelection + option members), so there is no hidden tree to populate or
// refresh. The qtM* members replicate the On* handlers' MODEL cores verbatim and drop the
// tree-refresh tails (findItem/reloadPlayer/updateSelection/updateIcons) -- the Qt window
// rebuilds its tree from qtGetNodeCount/qtGetNode after every command. Where the MFC path
// updated m_curSelection through a hidden-tree SelectItem round-trip, these set it
// directly. Defined here (member functions may be defined in any TU) so ScriptDialog.cpp
// stays byte-identical for the Qt-OFF build.
//----------------------------------------------------------------------------------------

// == OnInitDialog minus every control/tree/imagelist/window-position touch.
void ScriptDialog::qtOpenModelOnly(void)
{
	mTree = NULL;	// the ctor leaves it uninitialized; nothing here may ever use it

	qtClearUndoHistory();	// snapshots belong to one editing session

	m_bSmartCopyEnabled = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopy", 0);
	m_bAutoMergeScripts = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "AutoMergeScripts", 0);
	m_autoUpdateWarnings = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "AutoVerifyScripts", 1);
	m_staticThis = this;
	m_bDisableReferences = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "DisableReferences", 0);
	m_bCheckByParameter = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "ReferenceCheckByParameter", 1);
	m_bDisableDeepScan = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "DisableDeepScan", 0);
	m_bCleanScriptName = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CleanStringName", 1);
	m_bCompressed = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", 1);
	m_bNewIcons = ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "NewIcons", 1);
	m_updating = false;

	ScriptList::updateDefaults();
	m_sides = *TheSidesList;
	EditParameter::setCurSidesList(&m_sides);

	// == OnInitDialog's warning init, minus updateIcons (no tree).
	if (m_autoUpdateWarnings)
	{
		Bool loadedFromCache = false;
		if (LoadScriptWarningsState())
		{
			loadedFromCache = true;
			updateWarnings(true);
		}
		if (!loadedFromCache)
		{
			updateWarnings(true);
		}
	}

	// == OnInitDialog's force-rename of unnamed scripts/groups (model-only, kept verbatim).
	Int totalRenamedScripts = 0;
	Int totalRenamedGroups = 0;
	Int unnamedScriptCounter = 1;
	Int unnamedGroupCounter = 1;
	Int i;
	for (i = 0; i < m_sides.getNumSides(); i++)
	{
		ScriptList *pSL = m_sides.getSideInfo(i)->getScriptList();
		if (!pSL)
		{
			continue;
		}
		Script *pScr;
		for (pScr = pSL->getScript(); pScr; pScr = pScr->getNext())
		{
			if (pScr->getName().isEmpty())
			{
				AsciiString newName;
				newName.format("[UNNAMED_SCRIPT_%d]", unnamedScriptCounter);
				pScr->setName(newName);
				totalRenamedScripts++;
				unnamedScriptCounter++;
			}
		}
		ScriptGroup *pGroup;
		for (pGroup = pSL->getScriptGroup(); pGroup; pGroup = pGroup->getNext())
		{
			if (pGroup->getName().isEmpty())
			{
				AsciiString newName;
				newName.format("[UNNAMED_GROUP_%d]", unnamedGroupCounter);
				pGroup->setName(newName);
				totalRenamedGroups++;
				unnamedGroupCounter++;
			}
			for (pScr = pGroup->getScript(); pScr; pScr = pScr->getNext())
			{
				if (pScr->getName().isEmpty())
				{
					AsciiString newName;
					newName.format("[UNNAMED_SCRIPT_%d]", unnamedScriptCounter);
					pScr->setName(newName);
					totalRenamedScripts++;
					unnamedScriptCounter++;
				}
			}
		}
	}
	if (totalRenamedScripts > 0 || totalRenamedGroups > 0)
	{
		CString msg;
		msg.Format("Auto-renamed %d unnamed script(s) and %d unnamed group(s).\n\n"
				"These items are now visible in the tree view with [UNNAMED_*] prefix.\n\n"
				"You can now delete them or rename them properly.\n\n"
				"Check the debug output for details.",
				totalRenamedScripts, totalRenamedGroups);
		AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
	}
}

// == insertScript minus reloadPlayer/updateSelection/updateIcons; the selection bump the
// hidden tree used to make lands directly in m_curSelection.
void ScriptDialog::qtMInsertScript(Script *pNewScript)
{
	Int ndx;
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL)
	{
		ListType savSel = m_curSelection;
		Bool inGroup = savSel.m_objType == ListType::GROUP_TYPE	||
			savSel.m_objType == ListType::SCRIPT_IN_GROUP_TYPE;
		if (inGroup)
		{
			if (savSel.m_objType == ListType::GROUP_TYPE )
			{
				ndx = 0;
			}
			else
			{
				ndx = savSel.m_scriptIndex+1;
			}
			Int groupNdx;
			ScriptGroup *pGroup = pSL->getScriptGroup();
			for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext())
			{
				if (groupNdx == savSel.m_groupIndex)
				{
					pGroup->addScript(pNewScript, ndx);
					savSel.m_objType = ListType::SCRIPT_IN_GROUP_TYPE;
					savSel.m_scriptIndex = ndx;
					break;
				}
			}
		}
		else
		{
			if (m_curSelection.m_objType == ListType::PLAYER_TYPE )
			{
				ndx = 0;
			}
			else
			{
				ndx = m_curSelection.m_scriptIndex+1;
			}
			pSL->addScript(pNewScript, ndx);
			savSel.m_objType = ListType::SCRIPT_IN_PLAYER_TYPE;
			savSel.m_scriptIndex = ndx;
		}
		m_curSelection = savSel;
	}
}

// == OnNewFolder minus the tree refresh. Pops the same EditGroup name dialog the MFC path
// does (OnNewFolder never got a Qt-dialog branch; parity kept).
void ScriptDialog::qtMNewFolder(void)
{
	Int ndx;
	if (m_curSelection.m_objType == ListType::PLAYER_TYPE)
	{
		ndx = 0;
	}
	else
	{
		ndx = m_curSelection.m_groupIndex+1;
	}
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL)
	{
		ListType savSel = m_curSelection;
		ScriptGroup *pNewGroup = newInstance( ScriptGroup);
		EditGroup editDlg(pNewGroup);
		if (IDOK==editDlg.DoModal())
		{
			AsciiString name = pNewGroup->getName();
			name.trim();
			if (name.isEmpty())
			{
				AfxMessageBox(
					"Error: Script folder name cannot be empty.\n\n"
					"Please enter a valid name.",
					MB_OK | MB_ICONERROR
				);
				pNewGroup->deleteInstance();
				return;
			}
			qtPushUndoSnapshot();
			pSL->addGroup(pNewGroup, ndx);
			savSel.m_groupIndex = ndx;
			savSel.m_objType = ListType::GROUP_TYPE;
			m_curSelection = savSel;
		}
		else
		{
			pNewGroup->deleteInstance();
		}
	}
}

// == OnNewScript's Qt branch minus updateIcons.
void ScriptDialog::qtMNewScript(void)
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

	if (WBQtScriptEdit_Run(pNewScript, ::AfxGetMainWnd()->GetSafeHwnd()) != 0)
	{
		AsciiString newName = pNewScript->getName();
		newName.trim();
		if (newName.isEmpty())
		{
			AfxMessageBox(
				"Error: Script name cannot be empty.\n\n"
				"Please enter a valid name.",
				MB_OK | MB_ICONERROR
			);
			pNewScript->deleteInstance();
			return;
		}
		qtPushUndoSnapshot();
		qtMInsertScript(pNewScript);
	}
	else
	{
		pNewScript->deleteInstance();
	}
}

// == OnEditScript's Qt branches minus the tree-label/selection refresh (updateWarnings is
// static and windowless-safe, so warning recompute behavior matches).
void ScriptDialog::qtMEditScript(void)
{
	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();
	if (pScript == NULL)
	{
		if (pGroup)
		{
			// The group dialog edits pGroup in place, so snapshot first and discard the
			// snapshot if the dialog is cancelled (no mutation happened).
			qtPushUndoSnapshot();
			if (WBQtEditGroup_Run(pGroup, ::AfxGetMainWnd()->GetSafeHwnd()) != 0)
			{
				updateWarnings();
			}
			else
			{
				qtDropLastUndoSnapshot();
			}
		}
		return;
	}

	Script *pDup = pScript->duplicate();
	if (WBQtScriptEdit_Run(pDup, ::AfxGetMainWnd()->GetSafeHwnd()) != 0)
	{
		qtPushUndoSnapshot();
		pScript->updateFrom(pDup);
		pScript->setDirty(true);
		updateWarnings();
	}
	pDup->deleteInstance();
}

// == OnCopyScript minus the tree refresh.
void ScriptDialog::qtMCopyScript(void)
{
	Script *pScript = getCurScript();
	ScriptGroup *pGroup = getCurGroup();

	if (pScript)
	{
		Script *pDup = pScript->duplicate();
		if (m_bSmartCopyEnabled)
		{
			applySmartCopyIncrement(pDup);
		}
		AsciiString newName = pDup->getName();
		if(!m_bSmartCopyEnabled)
		{
			newName.concat(" C");
		}
		pDup->setName(newName);
		qtPushUndoSnapshot();
		qtMInsertScript(pDup);
		return;
	}

	if (pGroup && m_curSelection.m_objType == ListType::GROUP_TYPE)
	{
		ScriptGroup* pNewGroup = newInstance(ScriptGroup);
		AsciiString newGroupName = pGroup->getName();
		newGroupName.concat(" Copy");
		pNewGroup->setName(newGroupName);
		pNewGroup->setActive(pGroup->isActive());
		pNewGroup->setSubroutine(pGroup->isSubroutine());

		Int scriptIndex = 0;
		for (Script* pScr = pGroup->getScript(); pScr; pScr = pScr->getNext(), ++scriptIndex)
		{
			Script* pDup = pScr->duplicate();
			if (m_bSmartCopyEnabled)
			{
				applySmartCopyIncrement(pDup);
			}
			pNewGroup->addScript(pDup, scriptIndex);
		}

		ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
		if (pSL)
		{
			Int insertIndex = m_curSelection.m_groupIndex + 1;
			qtPushUndoSnapshot();
			pSL->addGroup(pNewGroup, insertIndex);
		}
	}
}

// == OnDelete minus the tree refresh (the m_curSelection fixups are already inline).
void ScriptDialog::qtMDelete(void)
{
	Script *pScript = getCurScript();
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	if (pSL)
	{
		qtPushUndoSnapshot();
		Bool inGroup = m_curSelection.m_objType != ListType::SCRIPT_IN_PLAYER_TYPE;
		if (inGroup)
		{
			Int groupNdx;
			ScriptGroup *pGroup = pSL->getScriptGroup();
			for (groupNdx = 0; pGroup; groupNdx++,pGroup=pGroup->getNext())
			{
				if (groupNdx == m_curSelection.m_groupIndex)
				{
					if (m_curSelection.m_objType == ListType::GROUP_TYPE)
					{
						pSL->deleteGroup(pGroup);
						m_curSelection.m_objType = ListType::PLAYER_TYPE;
					}
					else
					{
						pGroup->deleteScript(pScript);
						if (pGroup->getScript()==NULL)
						{
							m_curSelection.m_objType = ListType::GROUP_TYPE;
						}
					}
					break;
				}
			}
		}
		else
		{
			pSL->deleteScript(pScript);
			if (pSL->getScript()==NULL)
			{
				m_curSelection.m_objType = ListType::PLAYER_TYPE;
			}
		}
	}
}

// == OnAddDebug minus the tree refresh.
void ScriptDialog::qtMAddDebug(void)
{
	Script *pScript = getCurScript();
	if (!pScript)
	{
		AfxMessageBox("No script selected. Please select a script first.", MB_OK | MB_ICONWARNING);
		return;
	}

	qtPushUndoSnapshot();
	ScriptAction *debugAction = newInstance(ScriptAction)(ScriptAction::SHOW_MILITARY_CAPTION);

	AsciiString debugText = "[Debug] ";
	debugText.concat(pScript->getName());
	debugText.concat(" called");
	debugAction->getParameter(0)->friend_setString(debugText);

	const int msPerChar = 400;
	int textLength = debugText.getLength();
	int durationMs = textLength * msPerChar;
	if (durationMs < 2000)
	{
		durationMs = 2000;
	}
	if (durationMs > 15000)
	{
		durationMs = 15000;
	}
	debugAction->getParameter(1)->friend_setInt(durationMs);

	ScriptAction *lastAction = pScript->getAction();
	if (lastAction)
	{
		while (lastAction->getNext())
		{
			lastAction = lastAction->getNext();
		}
		lastAction->setNextAction(debugAction);
	}
	else
	{
		pScript->setAction(debugAction);
	}

	pScript->setDirty(true);
	updateWarnings();
	MessageBeep(MB_ICONWARNING);
}

// == OnRemoveDebug minus the tree refresh.
void ScriptDialog::qtMRemoveDebug(void)
{
	Script *pScript = getCurScript();
	if (!pScript)
	{
		AfxMessageBox("No script selected. Please select a script first.", MB_OK | MB_ICONWARNING);
		return;
	}

	qtPushUndoSnapshot();
	ScriptAction *currentAction = pScript->getAction();
	ScriptAction *prevAction = NULL;
	int removedCount = 0;

	while (currentAction)
	{
		bool shouldRemove = false;
		if (currentAction->getActionType() == ScriptAction::SHOW_MILITARY_CAPTION)
		{
			Parameter *param = currentAction->getParameter(0);
			if (param && param->getParameterType() == Parameter::TEXT_STRING)
			{
				AsciiString text = param->getString();
				if (text.startsWith("[Debug] "))
				{
					shouldRemove = true;
				}
			}
		}

		if (shouldRemove)
		{
			ScriptAction *toDelete = currentAction;
			currentAction = currentAction->getNext();
			if (prevAction)
			{
				prevAction->setNextAction(currentAction);
			}
			else
			{
				pScript->setAction(currentAction);
			}
			toDelete->setNextAction(NULL);
			toDelete->deleteInstance();
			removedCount++;
		}
		else
		{
			prevAction = currentAction;
			currentAction = currentAction->getNext();
		}
	}

	if (removedCount > 0)
	{
		pScript->setDirty(true);
		updateWarnings();
		CString msg;
		msg.Format("Removed %d debug action(s).", removedCount);
		AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		qtDropLastUndoSnapshot();	// nothing was removed; the snapshot is a no-op
		AfxMessageBox("No debug actions found in this script.", MB_OK | MB_ICONINFORMATION);
	}
}

// == OnPatchGC minus updateIcons (checkParametersForGC is a windowless-safe static).
void ScriptDialog::qtMPatchGC(void)
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
		qtPushUndoSnapshot();
		checkParametersForGC();
	}
}

// == OnScriptActivate minus the tree label/icon refresh.
void ScriptDialog::qtMToggleActive(void)
{
	if (getCurScript() != NULL)
	{
		qtPushUndoSnapshot();
		Bool active = getCurScript()->isActive();
		getCurScript()->setActive(!active);
	}
	else if (getCurGroup() != NULL)
	{
		qtPushUndoSnapshot();
		Bool active = getCurGroup()->isActive();
		getCurGroup()->setActive(!active);
	}
}

// == OnVerify minus updateIcons: try the per-map cache first, fall back to the slow
// updateWarnings(true) only when no valid cache exists (matches OnVerify's fast path).
void ScriptDialog::qtMVerify(void)
{
	// Flag to indicate if cache was successfully loaded
	Bool loadedFromCache = false;

	if (m_autoUpdateWarnings)
	{
		// Try loading cached warning state first
		if (LoadScriptWarningsState())
		{
			DEBUG_LOG(("ScriptDialog: Loaded script warning state from cache.\n"));
			loadedFromCache = true;
		}
		else
		{
			DEBUG_LOG(("ScriptDialog: No valid cache found. Will update warnings normally.\n"));
		}

		// If cache didn't load, fall back to the expensive update
		if (!loadedFromCache)
		{
			updateWarnings(true);
			DEBUG_LOG(("ScriptDialog: Ran updateWarnings(true) due to missing cache.\n"));
		}
	}
	else
	{
		updateWarnings(true);
	}
}

// == doDropOn keyed by the packed ListType ints directly (the MFC version resolves them
// from the hidden tree's item data; the model math below is verbatim). Reads the live Ctrl
// state itself, like doDropOn, so a Ctrl-drag in Qt still merges.
void ScriptDialog::qtMDropOn(int dragListType, int targetListType)
{
	if (dragListType == targetListType)
	{
		return;
	}
	ListType drag;
	drag.IntToList(dragListType);
	ListType target;
	target.IntToList(targetListType);

	Script *dragScript = NULL;
	ScriptGroup *dragGroup = NULL;
	m_curSelection = drag;
	Script *pScript = getCurScript();
	ScriptList *pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	ScriptGroup *pGroup = getCurGroup();
	bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (pSL == NULL)
	{
		return;
	}

	qtPushUndoSnapshot();

	if (pScript)
	{
		if (m_bAutoMergeScripts && isCtrlDown &&
		    (target.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE ||
		     target.m_objType == ListType::SCRIPT_IN_GROUP_TYPE))
		{
			m_curSelection = target;
			Script *targetScript = getCurScript();

			if (targetScript && pScript != targetScript)
			{
				Script *sourceScript = pScript;

				Int trueActionCount = 0;
				Int falseActionCount = 0;

				ScriptAction *pAction = sourceScript->getAction();
				ScriptAction *pTargetLastTrue = targetScript->getAction();
				if (pTargetLastTrue)
				{
					while (pTargetLastTrue->getNext())
					{
						pTargetLastTrue = pTargetLastTrue->getNext();
					}
				}
				while (pAction)
				{
					trueActionCount++;
					ScriptAction *pDup = pAction->duplicate();
					if (pTargetLastTrue)
					{
						pTargetLastTrue->setNextAction(pDup);
						pTargetLastTrue = pDup;
					}
					else
					{
						targetScript->setAction(pDup);
						pTargetLastTrue = pDup;
					}
					pAction = pAction->getNext();
				}

				pAction = sourceScript->getFalseAction();
				ScriptAction *pTargetLastFalse = targetScript->getFalseAction();
				if (pTargetLastFalse)
				{
					while (pTargetLastFalse->getNext())
					{
						pTargetLastFalse = pTargetLastFalse->getNext();
					}
				}
				while (pAction)
				{
					falseActionCount++;
					ScriptAction *pDup = pAction->duplicate();
					if (pTargetLastFalse)
					{
						pTargetLastFalse->setNextAction(pDup);
						pTargetLastFalse = pDup;
					}
					else
					{
						targetScript->setFalseAction(pDup);
						pTargetLastFalse = pDup;
					}
					pAction = pAction->getNext();
				}

				m_curSelection = drag;
				pGroup = getCurGroup();
				pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();

				if (pGroup)
				{
					pGroup->deleteScript(sourceScript);
				}
				else
				{
					pSL->deleteScript(sourceScript);
				}

				targetScript->setDirty(true);
				updateScriptWarning(targetScript);
				m_curSelection = target;

				CString msg;
				msg.Format("Successfully merged script:\n%d true action(s)\n%d false action(s)",
				           trueActionCount, falseActionCount);
				AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
				return;
			}
			m_curSelection = drag;
		}

		dragScript = pScript->duplicate();
		if (pGroup)
		{
			pGroup->deleteScript(pScript);
		}
		else
		{
			pSL->deleteScript(pScript);
		}
		if (drag.m_objType == target.m_objType &&
				drag.m_playerIndex == target.m_playerIndex &&
				drag.m_groupIndex == target.m_groupIndex &&
				drag.m_scriptIndex < target.m_scriptIndex)
		{
				target.m_scriptIndex--;
		}
	}
	else if (drag.m_objType == ListType::GROUP_TYPE)
	{
		if (m_bAutoMergeScripts && isCtrlDown && target.m_objType == ListType::GROUP_TYPE)
		{
			ScriptGroup *sourceGroup = pGroup;

			m_curSelection = target;
			ScriptGroup *targetGroup = getCurGroup();

			if (!targetGroup || !sourceGroup)
			{
				AfxMessageBox("Error: Could not find groups for merging.", MB_OK | MB_ICONERROR);
				return;
			}

			Int scriptCount = 0;
			Script *pScr = sourceGroup->getScript();
			while (pScr)
			{
				scriptCount++;
				pScr = pScr->getNext();
			}

			for (Script *pScrn = sourceGroup->getScript(); pScrn; pScrn = pScrn->getNext())
			{
				Script *pDup = pScrn->duplicate();
				Int endPos = 0;
				Script *pTargetScr = targetGroup->getScript();
				while (pTargetScr)
				{
					endPos++;
					pTargetScr = pTargetScr->getNext();
				}
				targetGroup->addScript(pDup, endPos);
			}

			m_curSelection = drag;
			pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
			pSL->deleteGroup(sourceGroup);
			m_curSelection = target;

			CString msg;
			msg.Format("Successfully merged %d script(s) into the target group.", scriptCount);
			AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
			return;
		}

		dragGroup = pGroup->duplicate();
		pSL->deleteGroup(pGroup);
		if (drag.m_objType != ListType::SCRIPT_IN_PLAYER_TYPE &&
				drag.m_playerIndex == target.m_playerIndex &&
				drag.m_groupIndex < target.m_groupIndex)
		{
				target.m_groupIndex--;
		}
	}

	m_curSelection = target;
	pScript = getCurScript();
	pSL = m_sides.getSideInfo(m_curSelection.m_playerIndex)->getScriptList();
	pGroup = getCurGroup();
	if (pSL == NULL)
	{
		return;
	}

	if (drag.m_objType == ListType::GROUP_TYPE)
	{
		if (target.m_objType == ListType::SCRIPT_IN_PLAYER_TYPE)
		{
			target.m_groupIndex = 9999;
		}
		if (target.m_objType == ListType::SCRIPT_IN_GROUP_TYPE)
		{
			target.m_groupIndex++;
		}
		target.m_objType = ListType::GROUP_TYPE;
	}

	if (dragScript)
	{
		if (pGroup)
		{
				pGroup->addScript(dragScript, target.m_scriptIndex);
		}
		else
		{
			pSL->addScript(dragScript, target.m_scriptIndex);
		}
	}
	else if (dragGroup)
	{
		pSL->addGroup(dragGroup, target.m_groupIndex);
		Int count = 0;
		ScriptGroup *pWalkGroup = pSL->getScriptGroup();
		while (pWalkGroup->getNext())
		{
			if (pWalkGroup==dragGroup)
			{
				break;
			}
			count++;
			pWalkGroup = pWalkGroup->getNext();
		}
		target.m_groupIndex = count;
	}
	m_curSelection = target;
}

// == qtSetCheckbox/On* handlers minus the control read-back and UI side effects: set the
// member, persist the same registry key, keep the user-facing info boxes.
void ScriptDialog::qtMSetCheckbox(int which, int checked)
{
	Bool on = (checked != 0);
	switch (which)
	{
		case WBQT_SCK_COMPRESS:
			m_bCompressed = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", m_bCompressed ? 1 : 0);
			break;
		case WBQT_SCK_NEWICONS:
			m_bNewIcons = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "NewIcons", m_bNewIcons ? 1 : 0);
			break;
		case WBQT_SCK_CLEANNAME:
			m_bCleanScriptName = on;
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
			break;
		case WBQT_SCK_AUTOVERIFY:
			m_autoUpdateWarnings = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "AutoVerifyScripts", m_autoUpdateWarnings ? 1 : 0);
			break;
		case WBQT_SCK_SMARTCOPY:
			m_bSmartCopyEnabled = on;
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
			break;
		case WBQT_SCK_FASTLOAD:
			m_bDisableDeepScan = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "DisableDeepScan", m_bDisableDeepScan ? 1 : 0);
			break;
		case WBQT_SCK_SCRIPTMERGE:
			m_bAutoMergeScripts = on;
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
			break;
		case WBQT_SCK_REFBYPARAM:
			m_bCheckByParameter = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "ReferenceCheckByParameter", m_bCheckByParameter ? 1 : 0);
			break;
		case WBQT_SCK_DISABLEREF:
			m_bDisableReferences = on;
			::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "DisableReferences", m_bDisableReferences ? 1 : 0);
			break;
		default:
			break;
	}
}

// == OnLoad minus the final reloadPlayer/updateIcons loop (no tree). The chunk parsers are
// the same protected statics; the SidesListUndoable commit + m_sides re-copy are verbatim.
void ScriptDialog::qtMImportScripts(void)
{
	CFileDialog fileDlg(true, ".scb", NULL, 0,
		"Script files (.scb)|*.scb||", this);

	Int result = fileDlg.DoModal();

	// Open document dialog may change working directory, change it back.
	char buf[_MAX_PATH];
	::GetModuleFileName(NULL, buf, sizeof(buf));
	char *pEnd = buf + strlen(buf);
	while (pEnd != buf)
	{
		if (*pEnd == '\\')
		{
			*pEnd = 0;
			break;
		}
		pEnd--;
	}
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	::SetCurrentDirectory(buf);
	if (IDCANCEL==result)
	{
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
		if (!file.parse(this))
		{
			throw(ERROR_CORRUPT_FILE_FORMAT);
		}
		pDoc->setNextWaypointID(m_maxWaypoint);

		SidesListUndoable *pUndo = new SidesListUndoable(m_sides, pDoc);
		pDoc->AddAndDoUndoable(pUndo);
		REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
		m_sides = *TheSidesList;
		// The import committed app-level and re-seeded the working copy; editor-local
		// snapshots from before it would desync the two, so drop them.
		qtClearUndoHistory();

		if (m_firstReadObject)
		{
			AddObjectUndoable *pObjUndo = new AddObjectUndoable(pDoc, m_firstReadObject);
			pDoc->AddAndDoUndoable(pObjUndo);
			REF_PTR_RELEASE(pObjUndo); // belongs to pDoc now.
			m_firstReadObject = NULL; // undoable owns it now.
		}
		PolygonTrigger *pTrig;
		PolygonTrigger *pNextTrig;
		for (pTrig=m_firstTrigger; pTrig; pTrig = pNextTrig)
		{
			pNextTrig = pTrig->getNext();
			pTrig->setNextPoly(NULL);
			PolygonTrigger::addPolygonTrigger(pTrig);
		}

		ScriptList *scripts[MAX_PLAYER_COUNT];
		Int count = ScriptList::getReadScripts(scripts);
		Int i;
		for (i=0; i<count; i++)
		{
			if (scripts[i]->getScript() == NULL && scripts[i]->getScriptGroup()==NULL)
			{
				continue;
			}
			Int curSide = -1;
			if (count==1)
			{
				curSide = m_curSelection.m_playerIndex;
			}
			else
			{
				Int j;
				for (j=0; j<m_sides.getNumSides(); j++)
				{
					// Using i as an index assumes that i < m_sides.getNumSides.  Is that safe???
					AsciiString name = m_sides.getSideInfo(i)->getDict()->getAsciiString(TheKey_playerName);
					if (name == m_readPlayerNames[j])
					{
						curSide = j;
						break;
					}
				}
				if (curSide == -1)
				{
					CString msg = "Could not find player";
					msg += m_readPlayerNames[i].str();
					msg += ", discarding scripts for this player.";
					::AfxMessageBox(msg);
					continue;
				}
			}
			if (curSide>= m_sides.getNumSides())
			{
				curSide = 0;
				::AfxMessageBox("Imported scripts came from more players than exist in this map.  Additional scripts moved to Neutral player.");
			}
			ScriptList *pSL = m_sides.getSideInfo(curSide)->getScriptList();

			if (pSL)
			{
				Script *pScr;
				Script *pNextScr;
				Int j=0;
				for (pScr = scripts[i]->getScript(); pScr; pScr=pNextScr)
				{
					pNextScr=pScr->getNext();
					pScr->setNextScript(NULL);
					pSL->addScript(pScr, j); //unlink it and add.
					j++;
				}
				j=0;
				ScriptGroup *pGroup;
				ScriptGroup *pNextGroup;
				for (pGroup = scripts[i]->getScriptGroup(); pGroup; pGroup=pNextGroup)
				{
					pNextGroup=pGroup->getNext();
					pGroup->setNextGroup(NULL);
					pSL->addGroup(pGroup, j);
					j++;
				}
				scripts[i]->discard(); /* Frees the script list, but none of it's children, as they have been
															copied into the current scripts. */
				scripts[i] = NULL;
			}
		}
	} catch(...) {
		DEBUG_CRASH(("threw exception in ScriptDialog::qtMImportScripts"));
	}
}

extern "C" {

int WBQtScript_IsActive(void)
{
	return (ScriptDialog::qtInstance() != NULL) ? 1 : 0;
}

int WBQtScript_GetNodeCount(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtGetNodeCount() : 0;
}

int WBQtScript_GetNode(int i, int *depthOut, int *listTypeOut, int *flagsOut, char *labelOut, int cap)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtGetNode(i, depthOut, listTypeOut, flagsOut, labelOut, cap) : 0;
}

void WBQtScript_SetSelection(int listTypeInt)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtSetSelection(listTypeInt);
	}
}

int WBQtScript_HasScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtHasScript() : 0;
}

int WBQtScript_HasGroup(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtHasGroup() : 0;
}

void WBQtScript_NewFolder(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMNewFolder();	// windowless model core (qt-debridge)
	}
}

void WBQtScript_NewScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMNewScript();
	}
}

void WBQtScript_EditScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMEditScript();
	}
}

void WBQtScript_CopyScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMCopyScript();
	}
}

void WBQtScript_Delete(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMDelete();
	}
}

int WBQtScript_FindScriptByName(const char *name)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtFindScriptByName(name) : -1;
}

int WBQtScript_Undo(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtMUndo() : 0;
}

int WBQtScript_Redo(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtMRedo() : 0;
}

void WBQtScript_Commit(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtCommitAndClose();	// commits; does NOT destroy the dialog
	}
	// Tear down the hidden MFC dialog here, AFTER qtCommitAndClose returned, so `this` was
	// valid throughout that call (closeScriptDialog does DestroyWindow + delete).
	if (CMainFrame::GetMainFrame() != NULL)
	{
		CMainFrame::GetMainFrame()->closeScriptDialog();
	}
}

void WBQtScript_Cancel(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtCancelAndClose();	// discards; does NOT destroy the dialog
	}
	if (CMainFrame::GetMainFrame() != NULL)
	{
		CMainFrame::GetMainFrame()->closeScriptDialog();
	}
}

void WBQtScript_DropOn(int dragListType, int targetListType)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMDropOn(dragListType, targetListType);
	}
}

int WBQtScript_FindNext(const char *text, int fromListType, int *outListType)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtFindNext(text, fromListType, outListType) : 0;
}

int WBQtScript_NodeMatches(int listType, const char *text, const char *label)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtNodeMatches(listType, text, label) : 1;
}

void WBQtScript_Verify(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMVerify();
	}
}

void WBQtScript_ToggleActive(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMToggleActive();
	}
}

void WBQtScript_GetDetail(int listTypeInt, char *descOut, int descCap, char *commentOut, int commentCap)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtGetDetail(listTypeInt, descOut, descCap, commentOut, commentCap);
	}
}

int WBQtScript_GetCheckbox(int which)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtGetCheckbox(which) : 0;
}

void WBQtScript_SetCheckbox(int which, int checked)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtMSetCheckbox(which, checked);
	}
}

void WBQtScript_AddDebug(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtMAddDebug(); }
}

void WBQtScript_RemoveDebug(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtMRemoveDebug(); }
}

void WBQtScript_PatchGC(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtMPatchGC(); }
}

void WBQtScript_ExportScripts(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtExportScripts(); }
}

void WBQtScript_ImportScripts(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtMImportScripts(); }
}

void WBQtScript_SaveNow(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL) { dlg->qtSaveNow(); }
}

}
#endif
