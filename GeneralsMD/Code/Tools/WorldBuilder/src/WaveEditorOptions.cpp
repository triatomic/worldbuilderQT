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

// WaveEditorOptions.cpp

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "WaveEditorOptions.h"
#include "WaveEditorTool.h"
#include "WorldBuilderDoc.h"
#include "MainFrm.h"
#include "DrawObject.h"
#include "wbview3d.h"

WaveEditorOptions*	WaveEditorOptions::m_staticThis = NULL;

/////////////////////////////////////////////////////////////////////////////
WaveEditorOptions::WaveEditorOptions(CWnd* pParent /*=NULL*/)
{
	//{{AFX_DATA_INIT(WaveEditorOptions)
	//}}AFX_DATA_INIT
	m_updatingList = false;
}

void WaveEditorOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(WaveEditorOptions)
	//}}AFX_DATA_MAP
}

/// Show the current wave type name next to the "Type:" label.
void WaveEditorOptions::updateTypeLabel(void)
{
	CWnd *pLabel = GetDlgItem(IDC_WAVE_TYPE_LABEL);
	if (pLabel) {
		CString buf;
		buf.Format("Type: %s", WaveEditorTool::getWaveTypeName());
		pLabel->SetWindowText(buf);
	}
}

/// One-time column setup for the wave list.
void WaveEditorOptions::setupColumns(void)
{
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_WAVE_LIST);
	if (!pList)
		return;

	pList->SetExtendedStyle(pList->GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	pList->InsertColumn(0, "#",       LVCFMT_LEFT,  30);
	pList->InsertColumn(1, "Start X", LVCFMT_RIGHT, 52);
	pList->InsertColumn(2, "Start Y", LVCFMT_RIGHT, 52);
	pList->InsertColumn(3, "End X",   LVCFMT_RIGHT, 52);
	pList->InsertColumn(4, "End Y",   LVCFMT_RIGHT, 52);
	pList->InsertColumn(5, "Type",    LVCFMT_LEFT,  100);
}

/// Rebuild the list from the current wave system.
void WaveEditorOptions::populateList(void)
{
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_WAVE_LIST);
	if (!pList)
		return;

	m_updatingList = true;
	pList->DeleteAllItems();

	Int count = WaveEditorTool::getWaveCount();
	for (Int i = 0; i < count; ++i) {
		float sx, sy, ex, ey;
		const char *typeName = "";
		if (!WaveEditorTool::getWaveRow(i, sx, sy, ex, ey, typeName))
			continue;

		CString buf;
		buf.Format("%03d", i + 1);
		Int row = pList->InsertItem(i, buf);

		buf.Format("%.3f", sx); pList->SetItemText(row, 1, buf);
		buf.Format("%.3f", sy); pList->SetItemText(row, 2, buf);
		buf.Format("%.3f", ex); pList->SetItemText(row, 3, buf);
		buf.Format("%.3f", ey); pList->SetItemText(row, 4, buf);
		pList->SetItemText(row, 5, typeName);
	}

	// Restore the highlight to match the tool's selected wave, if any.
	Int sel = WaveEditorTool::getSelectedWave();
	if (sel >= 0 && sel < pList->GetItemCount()) {
		pList->SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		pList->EnsureVisible(sel, FALSE);
	}

	m_updatingList = false;
}

void WaveEditorOptions::refresh(void)
{
	if (m_staticThis) {
		m_staticThis->updateTypeLabel();
		m_staticThis->populateList();
	}
}

BOOL WaveEditorOptions::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_staticThis = this;
	setupColumns();
	updateTypeLabel();
	populateList();

	// Reflect the tool's current Create/Manipulate mode in the toggle buttons.
	Bool create = (WaveEditorTool::getEditorMode() == WaveEditorTool::MODE_CREATE);
	CheckRadioButton(IDC_WAVE_MODE_CREATE, IDC_WAVE_MODE_MANIPULATE,
									 create ? IDC_WAVE_MODE_CREATE : IDC_WAVE_MODE_MANIPULATE);

	// Reflect the current wave-line overlay state in the checkbox.
	CButton *pChk = (CButton*)GetDlgItem(IDC_WAVE_SHOW_LINES);
	if (pChk)
		pChk->SetCheck(DrawObject::getDoWaveFeedback() ? BST_CHECKED : BST_UNCHECKED);

	return TRUE;
}

void WaveEditorOptions::OnCycleType()
{
	WaveEditorTool::cycleWaveType();
	updateTypeLabel();
}

void WaveEditorOptions::OnUndo()
{
	WaveEditorTool::undoLast();
	populateList();
}

void WaveEditorOptions::OnSave()
{
	WaveEditorTool::saveTracks(CWorldBuilderDoc::GetActiveDoc());
}

void WaveEditorOptions::OnReload()
{
	WaveEditorTool::loadTracks(CWorldBuilderDoc::GetActiveDoc(), true /*announce*/);
	populateList();
}

void WaveEditorOptions::OnDeleteSelected()
{
	WaveEditorTool::deleteSelectedWave();
	populateList();
}

void WaveEditorOptions::OnModeCreate()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_CREATE);
}

void WaveEditorOptions::OnModeManipulate()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_MANIPULATE);
}

void WaveEditorOptions::OnShowWaveLines()
{
	CButton *pChk = (CButton*)GetDlgItem(IDC_WAVE_SHOW_LINES);
	Bool show = (pChk && pChk->GetCheck() == BST_CHECKED);

	// Route through the 3D view so the View ▸ Show Wave Lines menu, the registry,
	// and the DrawObject overlay all stay in sync with this checkbox.
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->setShowWaveLines(show);
	else
		DrawObject::setDoWaveFeedback(show);	// fallback: at least toggle the overlay
}

/// When the panel is hidden (e.g. right-click closes the options dialog), drop the
/// wave highlight so a wave doesn't stay selected/yellow with no panel to manage it.
void WaveEditorOptions::OnShowWindow(BOOL bShow, UINT nStatus)
{
	COptionsPanel::OnShowWindow(bShow, nStatus);

	if (!bShow)
	{
		WaveEditorTool::selectWaveNoCenter(-1);	// clear highlight + refresh list

		WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
		if (p3View)
			p3View->Invalidate();
	}
}

/// List selection changed -> tell the tool to highlight + center on that wave.
void WaveEditorOptions::OnWaveListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;

	if (m_updatingList)
		return;	// programmatic change, not a user click

	// Only react when an item gains selection.
	if ((pNMListView->uChanged & LVIF_STATE) &&
			(pNMListView->uNewState & LVIS_SELECTED) &&
			!(pNMListView->uOldState & LVIS_SELECTED))
	{
		WaveEditorTool::selectWave(pNMListView->iItem);
	}
}

BEGIN_MESSAGE_MAP(WaveEditorOptions, COptionsPanel)
	//{{AFX_MSG_MAP(WaveEditorOptions)
	ON_BN_CLICKED(IDC_WAVE_CYCLE_TYPE, OnCycleType)
	ON_BN_CLICKED(IDC_WAVE_UNDO, OnUndo)
	ON_BN_CLICKED(IDC_WAVE_SAVE, OnSave)
	ON_BN_CLICKED(IDC_WAVE_RELOAD, OnReload)
	ON_BN_CLICKED(IDC_WAVE_DELETE, OnDeleteSelected)
	ON_BN_CLICKED(IDC_WAVE_MODE_CREATE, OnModeCreate)
	ON_BN_CLICKED(IDC_WAVE_MODE_MANIPULATE, OnModeManipulate)
	ON_BN_CLICKED(IDC_WAVE_SHOW_LINES, OnShowWaveLines)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_WAVE_LIST, OnWaveListItemChanged)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
