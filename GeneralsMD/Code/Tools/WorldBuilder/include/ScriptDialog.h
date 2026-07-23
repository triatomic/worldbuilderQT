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

#if !defined(AFX_SCRIPTDIALOG_H__885FEF28_85F9_4556_9908_1BEC0B6E4C62__INCLUDED_)
#define AFX_SCRIPTDIALOG_H__885FEF28_85F9_4556_9908_1BEC0B6E4C62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ScriptDialog.h : header file
//

#define SCRIPT_DIALOG_SECTION "ScriptDialog"

#include "GameLogic/SidesList.h"
#include <afxtempl.h> // for CArray, CMap, etc -- please review this one

class ListType  {
public:
	enum {BOGUS_TYPE = 0, PLAYER_TYPE = 1, GROUP_TYPE, SCRIPT_IN_PLAYER_TYPE, SCRIPT_IN_GROUP_TYPE};
	unsigned char m_objType;		 // 4 bits
	unsigned char m_playerIndex; // 4 bits
	unsigned short int m_groupIndex;	 // 12 bits
	unsigned short int m_scriptIndex; // 12 bits

	ListType(void) {m_objType=BOGUS_TYPE;m_playerIndex=0;m_groupIndex = 0; m_scriptIndex=0;}

	Int ListToInt(void) { return((m_objType<<28)+(m_playerIndex<<24)+(m_groupIndex<<12)+m_scriptIndex);}

	void IntToList(int i) {m_objType = ((i)>>28)&0x0F; m_playerIndex = ((i)>>24)&0x0F; m_groupIndex = ((i)>>12)&0x0FFF; m_scriptIndex = (i)&0x0FFF;}
};

class ScriptList;
class ScriptGroup;
class Script;
class Parameter;

/** Class Definition for overridden Tree control that
    supports Right-click context sensitive menu.*/
class CSDTreeCtrl : public CTreeCtrl
{
	public:
	
	protected:
		virtual void OnRButtonDown(UINT nFlags, CPoint point);
		DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// ScriptDialog dialog

class ScriptDialog : public CDialog
{
// Construction
public:
	ScriptDialog(CWnd* pParent = NULL);   // standard constructor
	~ScriptDialog();   //  destructor

// Dialog Data
	//{{AFX_DATA(ScriptDialog)
	enum { IDD = IDD_ScriptDialog };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ScriptDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	static void updateWarnings(Bool forceUpdate=false);
	static void updateScriptWarning(Script *pScript);

	static void patchScriptParametersForGC(Script *pScript);
	static void checkParametersForGC(void);

	static void appendWarningHintLazy(Script* pScript);
	static void SaveScriptWarningsState();
	static Bool LoadScriptWarningsState();


	/// To allow CSDTreeCtrl access to these member functions of ScriptDialog
	Script *friend_getCurScript(void);
	ScriptGroup *friend_getCurGroup(void);

#ifdef RTS_HAS_QT
	// Qt Script-editor front-end support (WBQtScriptBridge / ScriptDialog.cpp). The Qt
	// window drives this still-created-but-hidden MFC dialog: it reads the flat tree model
	// below, pushes its selection into m_curSelection, and invokes the same command
	// handlers, so m_sides / the sub-editors / OK-Cancel commit all stay MFC-side.
	int  qtGetNodeCount(void);
	// Fill node i (pre-order): depth (0 player / 1 group or ungrouped script / 2 script in
	// group), its packed ListType int, and its display label. Returns 0 past the end.
	int  qtGetNode(int i, int *depthOut, int *listTypeOut, int *flagsOut, char *labelOut, int cap);
	void qtSetSelection(int listTypeInt);
	int  qtGetSelection(void);
	int  qtHasScript(void);	// current selection resolves to a Script (Edit/Copy/Delete)
	int  qtHasGroup(void);	// current selection resolves to a ScriptGroup
	Script *qtCurScript(void);	// == getCurScript() for the current selection (9b search)
	void qtDoNewFolder(void);
	void qtDoNewScript(void);
	void qtDoEditScript(void);
	void qtDoCopyScript(void);
	void qtDoDelete(void);
	void qtCommitAndClose(void);	// == OnOK (commit m_sides via SidesListUndoable)
	void qtCancelAndClose(void);	// == OnCancel (discard)
	// 9b: drag-drop + search. qtDropOn resolves both nodes by ListType and reuses doDropOn
	// (reorder / move / Ctrl auto-merge). qtFindNext walks the model in tree order and
	// returns the next node (label + deep-content match) after fromListType, or 0 if none.
	void qtDropOn(int dragListType, int targetListType);
	int  qtFindNext(const char *text, int fromListType, int *outListType);
	// Live tree filter: does this node match the search text (label or, for a script, deep scan)?
	// label is the node's already-formatted tree label; empty text matches everything.
	int  qtNodeMatches(int listTypeInt, const char *text, const char *label);
	// Find/replace across every script's condition/action parameter VALUES. Returns the match
	// count. doReplace 0 = count only; 1 = rewrite matches to `replace` (snapshots for undo first,
	// only when there's a match). matchCase / wholeValue toggle case sensitivity and exact-value.
	// Resolve a packed ListType to its Script* without disturbing the tree selection (NULL if none).
	Script *qtScriptForListType(int listType);
	// scopeListType -1 = all scripts; else limit to that one selected script.
	int  qtScriptReplace(const char *find, const char *replace,
		int matchCase, int wholeValue, int doReplace, int scopeListType = -1);
	// Navigate to the next script (tree order, after fromListType) with a matching PARAMETER value
	// -- same scope as qtScriptReplace, so the replace bar's Next/Prev agrees with its count.
	int  qtFindNextParamMatch(int fromListType, const char *find,
		int matchCase, int wholeValue, int *outListType);
	// Autocomplete: distinct parameter values containing `substr` (case-insensitive), each with its
	// use count, written as "value\tcount\n" lines into buf (most-used first). Returns the total
	// distinct count (may exceed what fit in buf).
	int  qtCollectParamValues(const char *substr, char *buf, int cap, int scopeListType = -1);
	// 9c: recompute warnings (== OnVerifyAll) and toggle the current script/group active
	// flag (== OnScriptActivate). The Qt window rebuilds after to pick up the new flags.
	void qtVerify(void);
	void qtToggleActive(void);
	// 9d: fill the description (getUiText) + comment panels for the node at listTypeInt,
	// including the cross-script "[Referenced in]" tag ('Disable references' skips it).
	void qtGetDetail(int listTypeInt, char *descOut, int descCap, char *commentOut, int commentCap);
	// 9d option checkboxes (which == the WBQT_SCK_* ids) + remaining command buttons.
	int  qtGetCheckbox(int which);
	void qtSetCheckbox(int which, int checked);
	void qtAddDebug(void);
	void qtRemoveDebug(void);
	void qtPatchGC(void);
	void qtExportScripts(void);
	void qtImportScripts(void);
	void qtSaveNow(void);
	static ScriptDialog *qtInstance(void) { return m_staticThis; }

	// De-bridged (windowless) mode -- branch qt-debridge. In Qt mode the ScriptDialog
	// window is never Create()d: the OBJECT is the model container only (m_sides +
	// m_curSelection + option members), so no hidden tree/controls exist. qtOpenModelOnly
	// seeds it exactly like OnInitDialog minus the UI; the qtM* variants replicate the
	// On* handlers' MODEL cores (the tree-refresh tails do not apply -- the Qt tree
	// rebuilds from the model after every command). All DEFINED in
	// src/WBQtScriptBridge.cpp so this class's .cpp stays untouched for the OFF build.
	void qtOpenModelOnly(void);
	void qtMInsertScript(Script *pNewScript);
	void qtMNewFolder(void);
	void qtMNewScript(void);
	void qtMEditScript(void);
	void qtMCopyScript(void);
	int  qtMRenameSelection(const char *newName);	// rename the current script/group in place (undoable)
	void qtGetSelectionName(char *buf, int cap);	// the current script/group's bare name (rename prefill)
	void qtMDelete(void);
	void qtMAddDebug(void);
	void qtMRemoveDebug(void);
	void qtMPatchGC(void);
	void qtMToggleActive(void);
	void qtMDropOn(int dragListType, int targetListType);
	void qtMVerify(void);
	void qtMSetCheckbox(int which, int checked);
	void qtMImportScripts(void);
	// Exact-name script lookup for the detail pane's clickable "[Referenced in]" links:
	// returns the packed ListType int, or -1 when no script has that name.
	int  qtFindScriptByName(const char *name);
	// Editor-local undo/redo over the working copy (m_sides snapshots). Cleared per
	// session (qtOpenModelOnly) and on import (which self-commits app-level).
	void qtPushUndoSnapshot(void);
	void qtDropLastUndoSnapshot(void);
	void qtClearUndoHistory(void);
	int  qtMUndo(void);
	int  qtMRedo(void);
#endif

protected:
	Bool m_bSmartCopyEnabled;
	Bool m_bAutoMergeScripts;

	ListType	m_curSelection;
	CImageList m_imageList;
	SidesList	m_sides;
	static ScriptDialog *m_staticThis;
	CSDTreeCtrl *mTree;
	Bool			m_draggingTreeView;
	Bool m_autoUpdateWarnings;	///< flag whether we should updateWarnings on script editor actions.

	HTREEITEM m_dragItem;

	MapObject *m_firstReadObject;
	PolygonTrigger *m_firstTrigger;
	Int							m_waypointBase;
	Int							m_maxWaypoint;

	AsciiString			m_readPlayerNames[MAX_PLAYER_COUNT];

	CString m_searchText;
	HTREEITEM m_lastFoundItem;

	CFont m_treeFont;
	CFont* m_pOldFont;
	BOOL m_bCompressed;
	BOOL m_bNewIcons;
	BOOL m_updating;

	BOOL m_bDisableDeepScan;
	BOOL m_bCleanScriptName;
	BOOL m_bCheckByParameter; //reference mode default
	BOOL m_bDisableReferences;

protected:
	HTREEITEM addPlayer(Int playerIndx);
	void addScriptList(HTREEITEM hPlayer, Int playerIndex, ScriptList *pSL);
	void doDropOn(HTREEITEM hDrop, HTREEITEM hTarget);
	Script *getCurScript(void);
	ScriptGroup *getCurGroup(void);
	void reloadPlayer(Int playerIndex, ScriptList *pSL);
	HTREEITEM findItem(ListType sel, Bool failSafe = FALSE);
	void insertScript(Script *pNewScript);
	void scanForWaypointsAndTeams(Script *pScript, Bool doUnits, Bool doWaypoints, Bool doTriggers, Bool doTeams);
	void scanParmForWaypointsAndTeams(Parameter *pParm, Bool doUnits, Bool doWaypoints, Bool doTriggers, Bool doTeams);
	void updateSelection(ListType sel);
	void setIconScript(HTREEITEM item);
	void setIconGroup(HTREEITEM item);
	Bool updateIcons(HTREEITEM hItem);
	void markWaypoint(MapObject *pObj);
	void SetItemIconIfDifferent(CTreeCtrl* pTree, HTREEITEM hItem, int desiredIndex);

	static Bool ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParsePolygonTriggersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseTeamsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParsePlayersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);

	AsciiString incrementStringNumber(const AsciiString& input);
	void applySmartCopyIncrement(Script* pScr);
	AsciiString buildReferencedInTag(Script *pScript); ///< "[Referenced in] : ..." (empty if none/disabled)
	AsciiString buildUsesTag(Script *pScript); ///< "[Uses] : ..." (scripts pScript calls; empty if none/disabled)
	AsciiString buildParamTypeTag(Script *pScript, int paramType, const char *label); ///< "<label>v1, v2" of one param type
	AsciiString buildMissingTag(Script *pScript); ///< "[Missing] : ..." object types absent from the current data (empty if none/disabled)

	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:

	// Generated message map functions
	//{{AFX_MSG(ScriptDialog)
	afx_msg void OnSelchangedScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnNewFolder();
	afx_msg void OnNewScript();
	afx_msg void OnEditScript();
	afx_msg void OnCopyScript();
	afx_msg void OnDelete();
	afx_msg void OnAddDebug();
	afx_msg void OnRemoveDebug();
	afx_msg void OnAutoMergeScripts();
	afx_msg void OnVerify();
	afx_msg void OnVerifyAll();
	afx_msg void OnPatchGC();
	afx_msg void OnFindNext();
	afx_msg void OnAutoVerify();
	afx_msg void OnSmartCopy();
	afx_msg void OnCompress();
	afx_msg void OnDisableDeepScan();
	afx_msg void OnCheckByParameterForReference();
	afx_msg void OnDisableReferencesEntirely();
	afx_msg void OnCleanScriptName();
	afx_msg void OnNewIcons();
	afx_msg void OnSave();
	afx_msg void OnSaveActual();

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	
	afx_msg void OnLoad();
	afx_msg void OnDblclkScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnBegindragScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnScriptActivate();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMove(int x, int y);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCRIPTDIALOG_H__885FEF28_85F9_4556_9908_1BEC0B6E4C62__INCLUDED_)
