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

// ScriptActionsFalse.cpp : implementation file
//

#include "StdAfx.h"
#include "WorldBuilder.h"
#include "ScriptActionsFalse.h"
#include "GameLogic/Scripts.h"
#include "EditAction.h"	
#include "ScriptDialog.h"

/////////////////////////////////////////////////////////////////////////////
// ScriptActionsFalse property page

IMPLEMENT_DYNCREATE(ScriptActionsFalse, CPropertyPage)

ScriptActionsFalse::ScriptActionsFalse() : CPropertyPage(ScriptActionsFalse::IDD),
m_falseAction(NULL),
m_index(0),
m_bSmartCopyEnabled(false)
{
	//{{AFX_DATA_INIT(ScriptActionsFalse)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

ScriptActionsFalse::~ScriptActionsFalse()
{
}

void ScriptActionsFalse::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ScriptActionsFalse)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ScriptActionsFalse, CPropertyPage)
	//{{AFX_MSG_MAP(ScriptActionsFalse)
	ON_BN_CLICKED(IDC_EDIT, OnEditAction)
	ON_LBN_SELCHANGE(IDC_ACTION_LIST, OnSelchangeActionList)
	ON_LBN_DBLCLK(IDC_ACTION_LIST, OnDblclkActionList)
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_COPY, OnCopy)
	ON_BN_CLICKED(IDC_SMART_COPY, OnSmartCopy)
	ON_BN_CLICKED(IDC_MOVETOTRUE, OnMoveToTrue)
	ON_BN_CLICKED(IDC_MOVE_DOWN, OnMoveDown)
	ON_BN_CLICKED(IDC_MOVE_UP, OnMoveUp)
	ON_EN_CHANGE(IDC_EDIT_COMMENT, OnChangeEditComment)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ScriptActionsFalse message handlers

BOOL ScriptActionsFalse::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	CWnd *pWnd = GetDlgItem(IDC_EDIT_COMMENT);
	pWnd->SetWindowText(m_script->getActionComment().str());
	// loadList(); // Moved to OnSetActive so it refreshes when tab is clicked.
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// Adriane [Deatscythe]  This is done due to the new feature called move to false in the true actions page.
// We need the false actions page to refresh when the user clicks on it.
BOOL ScriptActionsFalse::OnSetActive()
{
    // CListBox *pList = (CListBox *)GetDlgItem(IDC_ACTION_LIST);
    // if (pList) {
    //     pList->ResetContent();  // clear only when activating the tab
    // }

	m_bSmartCopyEnabled=::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopyDeep", 0);

	CButton *pButton = (CButton*)GetDlgItem(IDC_SMART_COPY);
	pButton->SetCheck(m_bSmartCopyEnabled ? 1:0);

    loadList();  // repopulate list
    return CPropertyPage::OnSetActive();
}

void ScriptActionsFalse::loadList(void)
{
	m_falseAction = NULL;
	ScriptDialog::updateScriptWarning(m_script);
	CListBox *pList = (CListBox *)GetDlgItem(IDC_ACTION_LIST);
	Int count = 0;
	if (pList) {
		pList->ResetContent();
		ScriptAction *pAction = m_script->getFalseAction();
		while (pAction) {
			AsciiString astr = pAction->getUiText();
			if (astr.isEmpty()) {
				astr = "Invalid Action";
			}

			pList->AddString(astr.str());
			pAction = pAction->getNext();
			count++;
		}
		if (count>0 && count<=m_index) {
			m_index = count-1;
		}
		pList->SetCurSel(m_index);
		OnSelchangeActionList();
	}	
}

void ScriptActionsFalse::OnMoveToTrue() {
    if (!m_falseAction) return;

    // Step 1: Duplicate only the single action
    ScriptAction *pMove = m_falseAction->duplicate();
    pMove->setNextAction(NULL); // important!

    // Step 2: Remove from False list
    m_script->deleteFalseAction(m_falseAction);

    // Step 3: Append to True list
    if (m_script->getAction()) {
        ScriptAction *pTail = m_script->getAction();
        while (pTail->getNext()) {
            pTail = pTail->getNext();

        }
        pTail->setNextAction(pMove);
    } else {
        m_script->setAction(pMove);
    }

    // Step 4: Refresh True page UI
    m_index = 0;  // reset selection to avoid out-of-range index
    loadList();
}

void ScriptActionsFalse::OnEditAction() 
{
	CListBox *pList = (CListBox *)GetDlgItem(IDC_ACTION_LIST);
	if (m_falseAction == NULL) {
		return;
	}
	EditAction cDlg;
	cDlg.setAction(m_falseAction);
	cDlg.DoModal();
	ScriptDialog::updateScriptWarning(m_script);
	pList->DeleteString(m_index);
	pList->InsertString(m_index, m_falseAction->getUiText().str());
	pList->SetCurSel(m_index);
}

void ScriptActionsFalse::enableUI() 
{
	CWnd *pWnd = GetDlgItem(IDC_EDIT);
	pWnd->EnableWindow(m_falseAction!=NULL);
	
	pWnd = GetDlgItem(IDC_COPY);
	pWnd->EnableWindow(m_falseAction!=NULL);

	pWnd = GetDlgItem(IDC_DELETE);
	pWnd->EnableWindow(m_falseAction!=NULL);
	
	pWnd = GetDlgItem(IDC_MOVETOTRUE);
	pWnd->EnableWindow(m_falseAction!=NULL);

	pWnd = GetDlgItem(IDC_MOVE_DOWN);
	pWnd->EnableWindow(m_falseAction && m_falseAction->getNext());

	pWnd = GetDlgItem(IDC_MOVE_UP);
	pWnd->EnableWindow(m_falseAction && m_index>0);
	
}

void ScriptActionsFalse::OnSelchangeActionList() 
{
	m_falseAction = NULL;
	CListBox *pList = (CListBox *)GetDlgItem(IDC_ACTION_LIST);
	if (pList) {
		Int count = pList->GetCurSel();
		m_index = count;
		if (count<0) {
			enableUI();
			return;
		}
		count++;
		m_falseAction = m_script->getFalseAction();
		while (m_falseAction) {
			count--;
			if (count==0) {
				enableUI(); // Enable buttons based on selection.
				return;
			}
			m_falseAction = m_falseAction->getNext();
		}
	}	
	enableUI(); // Enable buttons based on selection.
}

void ScriptActionsFalse::OnDblclkActionList() 
{
	OnEditAction();
}



void ScriptActionsFalse::OnNew() 
{
	ScriptAction *pAct = newInstance( ScriptAction)(ScriptAction::DEBUG_MESSAGE_BOX);
	EditAction aDlg;
	aDlg.setAction(pAct);
	if (IDOK==aDlg.DoModal()) {
		if (m_falseAction) {
			pAct->setNextAction(m_falseAction->getNext());
			m_falseAction->setNextAction(pAct);
		} else {
			pAct->setNextAction(m_script->getFalseAction());
			m_script->setFalseAction(pAct);
		} 
		m_index++;
		loadList();
	} else {
		pAct->deleteInstance();
	}
}

void ScriptActionsFalse::OnDelete() 
{
	if (m_falseAction) {
		m_script->deleteFalseAction(m_falseAction);
		loadList();
	}
}


void ScriptActionsFalse::OnSmartCopy()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_SMART_COPY);
	m_bSmartCopyEnabled = (pButton->GetCheck() == 1);
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "SmartCopyDeep", m_bSmartCopyEnabled ? 1 : 0);


	if (m_bSmartCopyEnabled && !::AfxGetApp()->GetProfileInt("ToolTips", "SmartCopyInfoShown", 0))
	{
		AfxMessageBox(
			"This feature will auto increment values on your copied script's parameters\n\n"
			"Example:   Add  1  to counter 'Counter01' -> click copy ->   Add  1  to counter 'Counter02'\n\n"
			"Note: This does not support all parameters. Contact Adriane if you want other parameters to be supported adios.",
			MB_OK | MB_ICONINFORMATION
		);
		::AfxGetApp()->WriteProfileInt("ToolTips", "SmartCopyInfoShown", 1);
	}
}

void ScriptActionsFalse::applySmartCopyToAction(ScriptAction* pAction)
{
    if (!pAction) return;

    // Increment parameters inside the action
    for (int i = 0; i < pAction->getNumParameters(); ++i) {
        Parameter* param = pAction->getParameter(i);
        if (!param) continue;
        
        // For now, always increment these parameter types (condition set to true)
        if (true) {  // You can adjust this condition later for testing
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

AsciiString ScriptActionsFalse::incrementStringNumber(const AsciiString& input)
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

void ScriptActionsFalse::OnCopy() 
{
	if (m_falseAction) {
		ScriptAction *pCopy = m_falseAction->duplicate();
        
		// Apply smart copy increment to the copied action
		if (m_bSmartCopyEnabled)
			applySmartCopyToAction(pCopy);

		pCopy->setNextAction(m_falseAction->getNext());
		m_falseAction->setNextAction(pCopy);
		m_index++;
		loadList();
	}
}

Bool ScriptActionsFalse::doMoveDown() 
{
	if (m_falseAction && m_falseAction->getNext()) {
		ScriptAction *pNext = m_falseAction->getNext();
		ScriptAction *pCur = m_script->getFalseAction();
		ScriptAction *pPrev = NULL;
		while (pCur != m_falseAction) {
			pPrev = pCur;
			pCur = pCur->getNext();
		}
		DEBUG_ASSERTCRASH(pCur, ("Didn't find action in list."));
		if (!pCur) return false;
		if (pPrev) {
			pPrev->setNextAction(pNext);
			pCur->setNextAction(pNext->getNext());
			pNext->setNextAction(pCur);
		} else {
			DEBUG_ASSERTCRASH(m_falseAction == m_script->getFalseAction(), ("Logic error."));
			pCur->setNextAction(pNext->getNext());
			pNext->setNextAction(pCur);
			m_script->setFalseAction(pNext);
		}
		return true;
	}
	return false;
}

void ScriptActionsFalse::OnMoveDown() 
{
	if (doMoveDown()) {
		m_index++;
		loadList();
	}
}

void ScriptActionsFalse::OnMoveUp() 
{
	if (m_falseAction && m_index>0) {
//		ScriptAction *pNext = m_falseAction;
		ScriptAction *pPrev = m_script->getFalseAction();
		while (pPrev->getNext() != m_falseAction) {
			pPrev = pPrev->getNext();
		}
		if (pPrev) {
			m_falseAction = pPrev;
			m_index--;
			if (doMoveDown()) {
				loadList();
			}
		}
	}
}

void ScriptActionsFalse::OnChangeEditComment() 
{
	CWnd *pWnd = GetDlgItem(IDC_EDIT_COMMENT);
	CString comment;
	pWnd->GetWindowText(comment);
	m_script->setActionComment(AsciiString((LPCTSTR)comment));
}
