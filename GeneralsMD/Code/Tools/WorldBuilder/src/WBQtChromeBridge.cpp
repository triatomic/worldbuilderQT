// WBQtChromeBridge.cpp -- the MFC side of the Qt main-window chrome seam (Tier 4a-1).
// Plain MFC TU (no Qt include); whole body guarded by RTS_HAS_QT so the OFF build compiles
// it to an empty object. Supplies command enable/check state (== CCmdUI::DoUpdate against
// the frame's routing), the RC status prompts, the MRU list, and the ID_VIEW_TOOLBAR /
// ID_VIEW_STATUS_BAR special case. Also defines the guarded CMainFrame /
// CWorldBuilderApp members declared for the chrome (member functions may be defined in
// any TU).
#include "StdAfx.h"
#include <afxadv.h>		// CRecentFileList
#include <afxpriv.h>	// AFX_CMDHANDLERINFO
#include "resource.h"
#include "Lib/BaseType.h"
#include "MainFrm.h"
#include "WorldBuilder.h"
#include "qt/WBQtBridge.h"
#include "qt/WBQtChromeBridge.h"

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// CMainFrame::OnUpdateFrameMenu -- declared (guarded) in MainFrm.h. Once the Qt menu bar
// is installed the MFC menu is detached (SetMenu(NULL)); this keeps CFrameWnd from ever
// re-attaching m_hMenuDefault behind the chrome's back.
//----------------------------------------------------------------------------------------
void CMainFrame::OnUpdateFrameMenu(HMENU hMenuAlt)
{
	if (WBQtChrome_IsInstalled())
	{
		return;
	}
	CFrameWnd::OnUpdateFrameMenu(hMenuAlt);
}

//----------------------------------------------------------------------------------------
// CMainFrame::OnSetMessageString -- declared (guarded) in MainFrm.h, mapped via
// ON_MESSAGE(WM_SETMESSAGESTRING). Every CFrameWnd::SetMessageText writer (autosave,
// ruler, Wave Editor, the per-mouse-move readout in wbview.cpp) funnels through this
// message; mirror the resolved text to the Qt status row, then let the base handler keep
// the hidden MFC status bar canonical.
//----------------------------------------------------------------------------------------
LRESULT CMainFrame::OnSetMessageString(WPARAM wParam, LPARAM lParam)
{
	if (lParam != 0)
	{
		WBQtChrome_SetStatusText(reinterpret_cast<LPCTSTR>(lParam));
	}
	else if (wParam != 0)
	{
		// The resource-ID form (MFC idle sends AFX_IDS_IDLEMESSAGE == "Ready").
		CString text;
		if (text.LoadString((UINT)wParam))
		{
			WBQtChrome_SetStatusText((LPCTSTR)text);
		}
	}
	else
	{
		WBQtChrome_SetStatusText("");
	}
	return CFrameWnd::OnSetMessageString(wParam, lParam);
}

//----------------------------------------------------------------------------------------
// CMainFrame::OnUpdateFrameTitle -- declared (guarded) in MainFrm.h. MFC composes the
// frame title (doc name + FWS_ADDTOTITLE app name) on the hidden frame as before; mirror
// the result onto the Qt main window, the visible top-level (stage 1 inversion).
//----------------------------------------------------------------------------------------
void CMainFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
{
	CFrameWnd::OnUpdateFrameTitle(bAddToTitle);
	CString title;
	GetWindowText(title);
	WBQt_SetMainWindowTitle((LPCTSTR)title);
}

//----------------------------------------------------------------------------------------
// CWorldBuilderApp MRU accessors -- declared (guarded) in WorldBuilder.h.
// m_pRecentFileList is protected in CWinApp, so the bridge reads it through these.
//----------------------------------------------------------------------------------------
int CWorldBuilderApp::qtGetMruCount(void)
{
	return (m_pRecentFileList != NULL) ? m_pRecentFileList->GetSize() : 0;
}

void CWorldBuilderApp::qtGetMruPath(int i, CString &out)
{
	out.Empty();
	if (m_pRecentFileList != NULL && i >= 0 && i < m_pRecentFileList->GetSize())
	{
		// == CRecentFileList::UpdateMenu: show the condensed display name (just the file
		// name when the entry is under the current directory, otherwise the
		// AbbreviateName'd path), not the raw full path the list stores.
		TCHAR szCurDir[_MAX_PATH];
		DWORD dwDirLen = ::GetCurrentDirectory(_MAX_PATH, szCurDir);
		int nCurDir = 0;
		if (dwDirLen > 0 && dwDirLen < _MAX_PATH - 1)
		{
			nCurDir = (int)dwDirLen;
			szCurDir[nCurDir++] = '\\';
			szCurDir[nCurDir] = '\0';
		}
		else
		{
			szCurDir[0] = '\0';
		}
		if (!m_pRecentFileList->GetDisplayName(out, i, szCurDir, nCurDir, TRUE))
		{
			out = (*m_pRecentFileList)[i];
		}
	}
}

//----------------------------------------------------------------------------------------
// The CCmdUI capture: base Enable/SetCheck/SetRadio/SetText drive a real menu or control
// (m_pMenu/m_pOther) and assert with both NULL, so every virtual is overridden to record
// into members instead. Enable sets m_bEnableChanged, which gates the no-handler probe
// exactly like CCmdUI::DoUpdate.
//----------------------------------------------------------------------------------------
class WBQtChromeCmdUI : public CCmdUI
{
public:
	BOOL m_qtEnabled;
	int m_qtChecked;		// -1 = the update handler never set a check

	WBQtChromeCmdUI()
	{
		m_qtEnabled = TRUE;
		m_qtChecked = -1;
	}
	virtual void Enable(BOOL bOn)
	{
		m_qtEnabled = bOn;
		m_bEnableChanged = TRUE;
	}
	virtual void SetCheck(int nCheck)
	{
		// == the menu behavior: MF_CHECKED for any non-zero (2 = indeterminate elsewhere).
		m_qtChecked = (nCheck != 0) ? 1 : 0;
	}
	virtual void SetRadio(BOOL bOn)
	{
		m_qtChecked = bOn ? 1 : 0;
	}
	virtual void SetText(LPCTSTR)
	{
	}
};

extern "C" int WBQtChromeData_QueryCommand(int id, int *enabledOut, int *checkedOut)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return 0;
	}
	WBQtChromeCmdUI ui;
	ui.m_nID = (UINT)id;
	pFrame->OnCmdMsg((UINT)id, CN_UPDATE_COMMAND_UI, &ui, NULL);
	if (!ui.m_bEnableChanged)
	{
		// == CCmdUI::DoUpdate's disable-if-no-handler pass: probe (pHandlerInfo != NULL
		// never executes) for a CN_COMMAND handler anywhere on the routing. The ad-hoc
		// tool-palette IDs never reach this probe -- their update branch above called
		// Enable() (WorldBuilder.cpp OnCmdMsg).
		AFX_CMDHANDLERINFO info;
		info.pTarget = NULL;
		info.pmf = NULL;
		ui.m_qtEnabled = pFrame->OnCmdMsg((UINT)id, CN_COMMAND, &ui, &info);
	}
	if (enabledOut != NULL)
	{
		*enabledOut = ui.m_qtEnabled ? 1 : 0;
	}
	if (checkedOut != NULL)
	{
		*checkedOut = ui.m_qtChecked;
	}
	return 1;
}

// Stage 1 phase 2 (keyboard flip): the WBQtShortcuts table posts hotkey commands through
// here so a disabled command is swallowed (== the accelerator table's behavior -- a grayed
// command's accelerator is a no-op) instead of firing.
extern "C" void WBQtShortcuts_PostCommand(int commandId)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return;
	}
	int enabled = 1;
	int checked = -1;
	if (WBQtChromeData_QueryCommand(commandId, &enabled, &checked) && !enabled)
	{
		return;
	}
	::PostMessage(pFrame->GetSafeHwnd(), WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
}

extern "C" int WBQtChromeData_GetPrompt(int id, char *bufOut, int cap)
{
	if (bufOut == NULL || cap <= 0)
	{
		return 0;
	}
	bufOut[0] = 0;
	CString s;
	if (!s.LoadString((UINT)id))
	{
		return 0;
	}
	// The RC convention is "status prompt\ntooltip"; the prompt is the first part.
	int nl = s.Find('\n');
	if (nl >= 0)
	{
		s = s.Left(nl);
	}
	strncpy(bufOut, (LPCTSTR)s, cap - 1);
	bufOut[cap - 1] = 0;
	return 1;
}

extern "C" int WBQtChromeData_GetTooltip(int id, char *bufOut, int cap)
{
	if (bufOut == NULL || cap <= 0)
	{
		return 0;
	}
	bufOut[0] = 0;
	CString s;
	if (!s.LoadString((UINT)id))
	{
		return 0;
	}
	// The RC convention is "status prompt\ntooltip"; the tooltip is the second part.
	int nl = s.Find('\n');
	if (nl < 0)
	{
		return 0;
	}
	s = s.Mid(nl + 1);
	if (s.IsEmpty())
	{
		return 0;
	}
	strncpy(bufOut, (LPCTSTR)s, cap - 1);
	bufOut[cap - 1] = 0;
	return 1;
}

extern "C" int WBQtChromeData_GetMruCount(void)
{
	CWorldBuilderApp *pApp = (CWorldBuilderApp *)AfxGetApp();
	return (pApp != NULL) ? pApp->qtGetMruCount() : 0;
}

extern "C" int WBQtChromeData_GetMruPath(int i, char *bufOut, int cap)
{
	if (bufOut == NULL || cap <= 0)
	{
		return 0;
	}
	bufOut[0] = 0;
	CWorldBuilderApp *pApp = (CWorldBuilderApp *)AfxGetApp();
	if (pApp == NULL)
	{
		return 0;
	}
	CString path;
	pApp->qtGetMruPath(i, path);
	if (path.IsEmpty())
	{
		return 0;
	}
	strncpy(bufOut, (LPCTSTR)path, cap - 1);
	bufOut[cap - 1] = 0;
	return 1;
}

extern "C" void WBQtChrome_SetFrameStatusText(const char *text)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return;
	}
	if (text != NULL && text[0] != 0)
	{
		pFrame->SetMessageText(text);
	}
	else
	{
		// == menu dismissal in MFC: back to the idle message ("Ready").
		pFrame->SetMessageText(AFX_IDS_IDLEMESSAGE);
	}
}

static CControlBar *wbQtChromeBar(CMainFrame *pFrame, int statusBar)
{
	return (CControlBar *)pFrame->GetControlBar(statusBar ? AFX_IDW_STATUS_BAR : AFX_IDW_TOOLBAR);
}

extern "C" void WBQtChrome_ToggleMfcBar(int statusBar)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return;
	}
	CControlBar *pBar = wbQtChromeBar(pFrame, statusBar);
	if (pBar == NULL)
	{
		return;
	}
	// == CFrameWnd::OnBarCheck (ShowControlBar recalcs the layout); then re-flow the Qt
	// viewport host into the grown/shrunk pane.
	pFrame->ShowControlBar(pBar, !pBar->IsWindowVisible(), FALSE);
	pFrame->positionQtViewportHost();
}

extern "C" int WBQtChrome_IsMfcBarVisible(int statusBar)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return 0;
	}
	CControlBar *pBar = wbQtChromeBar(pFrame, statusBar);
	return (pBar != NULL && pBar->IsWindowVisible()) ? 1 : 0;
}

extern "C" void WBQtChrome_SetMfcBarVisible(int statusBar, int visible)
{
	CMainFrame *pFrame = CMainFrame::GetMainFrame();
	if (pFrame == NULL || pFrame->GetSafeHwnd() == NULL)
	{
		return;
	}
	CControlBar *pBar = wbQtChromeBar(pFrame, statusBar);
	if (pBar == NULL)
	{
		return;
	}
	pFrame->ShowControlBar(pBar, visible ? TRUE : FALSE, FALSE);
	pFrame->positionQtViewportHost();
}

#endif // RTS_HAS_QT
