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
#include "qt/WBQtPanelBridge.h"

#ifdef RTS_HAS_QT
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

int WBQtScript_GetNode(int i, int *depthOut, int *listTypeOut, char *labelOut, int cap)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtGetNode(i, depthOut, listTypeOut, labelOut, cap) : 0;
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
		dlg->qtDoNewFolder();
	}
}

void WBQtScript_NewScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtDoNewScript();
	}
}

void WBQtScript_EditScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtDoEditScript();
	}
}

void WBQtScript_CopyScript(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtDoCopyScript();
	}
}

void WBQtScript_Delete(void)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	if (dlg != NULL)
	{
		dlg->qtDoDelete();
	}
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
		dlg->qtDropOn(dragListType, targetListType);
	}
}

int WBQtScript_FindNext(const char *text, int fromListType, int *outListType)
{
	ScriptDialog *dlg = ScriptDialog::qtInstance();
	return (dlg != NULL) ? dlg->qtFindNext(text, fromListType, outListType) : 0;
}

}
#endif
