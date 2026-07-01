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
#ifdef RTS_HAS_QT
#include "qt/panels/WBQtWaveBridge.h"
#endif

WaveEditorOptions*	WaveEditorOptions::m_staticThis = NULL;

// Bucket brush-size slider range (world units).
#define WAVE_BRUSH_MIN  30
#define WAVE_BRUSH_MAX  5000

// Posted to coalesce the per-row LVN_ITEMCHANGED storm of a big list selection into a
// single tool-side sync (see OnWaveListItemChanged).
#define WM_WAVE_SYNC_SELECTION  (WM_APP + 0x0057)

/////////////////////////////////////////////////////////////////////////////
WaveEditorOptions::WaveEditorOptions(CWnd* pParent /*=NULL*/)
{
	//{{AFX_DATA_INIT(WaveEditorOptions)
	//}}AFX_DATA_INIT
	m_updatingList = false;
	m_selSyncPending = false;
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

	// Restore the highlight to match the tool's selection set (multi-select), clearing
	// any rows that are no longer selected.
	Int rows = pList->GetItemCount();
	for (Int i = 0; i < rows; ++i) {
		Bool on = WaveEditorTool::isWaveSelected(i);
		pList->SetItemState(i, on ? LVIS_SELECTED : 0, LVIS_SELECTED);
	}
	Int anchor = WaveEditorTool::getSelectedWave();
	if (anchor >= 0 && anchor < rows) {
		pList->SetItemState(anchor, LVIS_FOCUSED, LVIS_FOCUSED);
		pList->EnsureVisible(anchor, FALSE);
	}

	m_updatingList = false;
}

void WaveEditorOptions::refresh(void)
{
	if (m_staticThis) {
		m_staticThis->updateTypeLabel();
		m_staticThis->populateList();
	}
#ifdef RTS_HAS_QT
	WBQtWave_PushRefresh();
#endif
}

BOOL WaveEditorOptions::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_staticThis = this;
	setupColumns();
	updateTypeLabel();
	populateList();

	// Reflect the tool's current Create/Manipulate/Paint/Bucket mode in the toggle buttons.
	syncModeButtons(WaveEditorTool::getEditorMode());

	// Bucket brush-size slider: set range + the tool's current value, and the readout.
	if (CSliderCtrl *pSlider = (CSliderCtrl*)GetDlgItem(IDC_WAVE_BRUSH_SIZE)) {
		pSlider->SetRange(WAVE_BRUSH_MIN, WAVE_BRUSH_MAX, TRUE);
		Int sz = WaveEditorTool::getBucketBrushSize();
		if (sz < WAVE_BRUSH_MIN) sz = WAVE_BRUSH_MIN;
		if (sz > WAVE_BRUSH_MAX) sz = WAVE_BRUSH_MAX;
		pSlider->SetPos(sz);
		WaveEditorTool::setBucketBrushSize(sz);	// clamp the tool to the slider's range
	}
	updateBrushSizeLabel();

	// Reflect the current wave-line overlay state in the checkbox.
	CButton *pChk = (CButton*)GetDlgItem(IDC_WAVE_SHOW_LINES);
	if (pChk)
		pChk->SetCheck(DrawObject::getDoWaveFeedback() ? BST_CHECKED : BST_UNCHECKED);

	// Reflect the red shoreline-guide state in its checkbox.
	CButton *pShore = (CButton*)GetDlgItem(IDC_WAVE_SHOW_SHORELINE);
	if (pShore)
		pShore->SetCheck(DrawObject::getShowShoreline() ? BST_CHECKED : BST_UNCHECKED);

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

/// Wipe every wave on the map.  Not undoable (delete clears the wave undo stack), so
/// confirm first.
void WaveEditorOptions::OnDeleteAll()
{
	Int count = WaveEditorTool::getWaveCount();
	if (count <= 0)
		return;

	CString msg;
	msg.Format("Delete all %d wave%s? This cannot be undone.", count, (count == 1 ? "" : "s"));
	if (MessageBox(msg, "Delete All Waves", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return;

	WaveEditorTool::deleteAllWaves();
	populateList();
}

/// Push-like radio buttons only auto-toggle within a contiguous group; the Paint
/// button sits outside that group (non-adjacent control ID), so set all three checks
/// by hand whenever the mode changes to keep exactly one pressed.
void WaveEditorOptions::syncModeButtons(WaveEditorTool::EditorMode mode)
{
	if (CButton *p = (CButton*)GetDlgItem(IDC_WAVE_MODE_CREATE))
		p->SetCheck(mode == WaveEditorTool::MODE_CREATE ? BST_CHECKED : BST_UNCHECKED);
	if (CButton *p = (CButton*)GetDlgItem(IDC_WAVE_MODE_MANIPULATE))
		p->SetCheck(mode == WaveEditorTool::MODE_MANIPULATE ? BST_CHECKED : BST_UNCHECKED);
	if (CButton *p = (CButton*)GetDlgItem(IDC_WAVE_MODE_PAINT))
		p->SetCheck(mode == WaveEditorTool::MODE_PAINT ? BST_CHECKED : BST_UNCHECKED);
	if (CButton *p = (CButton*)GetDlgItem(IDC_WAVE_MODE_BUCKET))
		p->SetCheck(mode == WaveEditorTool::MODE_BUCKET ? BST_CHECKED : BST_UNCHECKED);

	// The brush-size slider only applies to Bucket mode; hide it elsewhere so the
	// panel doesn't advertise a control that does nothing.
	Int showBrush = (mode == WaveEditorTool::MODE_BUCKET) ? SW_SHOW : SW_HIDE;
	if (CWnd *p = GetDlgItem(IDC_WAVE_BRUSH_SIZE))
		p->ShowWindow(showBrush);
	if (CWnd *p = GetDlgItem(IDC_WAVE_BRUSH_SIZE_LABEL))
		p->ShowWindow(showBrush);
}

void WaveEditorOptions::OnModeCreate()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_CREATE);
	syncModeButtons(WaveEditorTool::MODE_CREATE);
}

void WaveEditorOptions::OnModeManipulate()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_MANIPULATE);
	syncModeButtons(WaveEditorTool::MODE_MANIPULATE);
}

void WaveEditorOptions::OnModePaint()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_PAINT);
	syncModeButtons(WaveEditorTool::MODE_PAINT);
}

void WaveEditorOptions::OnModeBucket()
{
	WaveEditorTool::setEditorMode(WaveEditorTool::MODE_BUCKET);
	syncModeButtons(WaveEditorTool::MODE_BUCKET);
}

/// Show the current bucket brush radius next to its slider.
void WaveEditorOptions::updateBrushSizeLabel(void)
{
	CWnd *pLabel = GetDlgItem(IDC_WAVE_BRUSH_SIZE_LABEL);
	if (pLabel) {
		CString buf;
		buf.Format("Bucket brush: %d", WaveEditorTool::getBucketBrushSize());
		pLabel->SetWindowText(buf);
	}
}

/// Step the bucket brush radius by delta (keyboard '['/']' in the view), clamped to the
/// slider's range.  Works even before the panel window exists; when it does, the slider
/// and the readout are kept in sync (SetPos doesn't re-fire OnHScroll, so no feedback loop).
void WaveEditorOptions::adjustBucketBrushSize(Int delta)
{
	Int sz = WaveEditorTool::getBucketBrushSize() + delta;
	if (sz < WAVE_BRUSH_MIN) sz = WAVE_BRUSH_MIN;
	if (sz > WAVE_BRUSH_MAX) sz = WAVE_BRUSH_MAX;
	WaveEditorTool::setBucketBrushSize(sz);
	if (m_staticThis) {
		CSliderCtrl *pSlider = (CSliderCtrl*)m_staticThis->GetDlgItem(IDC_WAVE_BRUSH_SIZE);
		if (pSlider) pSlider->SetPos(sz);
		m_staticThis->updateBrushSizeLabel();
	}
#ifdef RTS_HAS_QT
	WBQtWave_PushBrushSize();
#endif
}

/// The bucket brush-size slider moved: push the new radius into the tool and update the
/// readout.  (Trackbars report via WM_HSCROLL; this fires for any horizontal slider on
/// the panel, but the brush slider is the only one.)
void WaveEditorOptions::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CSliderCtrl *pSlider = (CSliderCtrl*)GetDlgItem(IDC_WAVE_BRUSH_SIZE);
	if (pSlider && pScrollBar == (CScrollBar*)pSlider)
	{
		WaveEditorTool::setBucketBrushSize(pSlider->GetPos());
		updateBrushSizeLabel();
	}
	COptionsPanel::OnHScroll(nSBCode, nPos, pScrollBar);
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

/// Toggle the red water/land shoreline guide drawn while the wave editor is open.
void WaveEditorOptions::OnShowShoreline()
{
	CButton *pChk = (CButton*)GetDlgItem(IDC_WAVE_SHOW_SHORELINE);
	Bool show = (pChk && pChk->GetCheck() == BST_CHECKED);
	DrawObject::setShowShoreline(show);

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
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

/// List selection changed -> rebuild the tool's multi-selection from the list's selected
/// rows.  Letting the native list control own selection means Ctrl-click (toggle) and
/// Shift-click (range) work for free; we just mirror the result into the wave tool.
///
/// LVN_ITEMCHANGED fires once PER ROW, so a Shift-click range or Ctrl+A over 1000+ waves
/// arrives as 1000+ notifications.  Syncing inside each one is O(rows) work per
/// notification (it re-walks the whole selection), which froze the UI on big maps.  So
/// don't sync here: post a one-shot message and rebuild the tool selection ONCE after the
/// notification storm drains.
void WaveEditorOptions::OnWaveListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;

	if (m_updatingList)
		return;	// programmatic change, not a user click

	// React once per selection-state change (it fires per row).
	if (!(pNMListView->uChanged & LVIF_STATE))
		return;
	if (!((pNMListView->uNewState ^ pNMListView->uOldState) & LVIS_SELECTED))
		return;	// this change wasn't about the selection bit

	if (!m_selSyncPending)
	{
		m_selSyncPending = true;
		PostMessage(WM_WAVE_SYNC_SELECTION);
	}
}

/// The coalesced selection sync posted by OnWaveListItemChanged: runs once after a burst
/// of per-row selection notifications has been processed.
LRESULT WaveEditorOptions::OnSyncSelectionFromList(WPARAM, LPARAM)
{
	m_selSyncPending = false;
	syncToolSelectionFromList();
	return 0;
}

/// Right-click on the wave list: pop up a menu of wave types and retype every selected
/// wave to the chosen one.  Operates on the current multi-selection; if nothing is
/// selected we first select the right-clicked row so a single right-click still works.
void WaveEditorOptions::OnWaveListRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;

	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_WAVE_LIST);
	if (!pList)
		return;

	// If the user right-clicked a row that isn't part of the selection (or nothing is
	// selected), select just that row first so the menu has something to act on.
	Int clicked = pNMListView->iItem;
	if (WaveEditorTool::getSelectionCount() <= 0 ||
			(clicked >= 0 && !WaveEditorTool::isWaveSelected(clicked)))
	{
		if (clicked < 0)
			return;	// right-clicked empty space with no selection - nothing to do
		Int rows = pList->GetItemCount();
		for (Int i = 0; i < rows; ++i)
			pList->SetItemState(i, (i == clicked) ? LVIS_SELECTED : 0, LVIS_SELECTED);
		pList->SetItemState(clicked, LVIS_FOCUSED, LVIS_FOCUSED);
		syncToolSelectionFromList();
	}

	Int typeCount = WaveEditorTool::getWaveTypeCount();
	if (typeCount <= 0 || WaveEditorTool::getSelectionCount() <= 0)
		return;

	// Build the type menu.  TPM_RETURNCMD hands us the chosen id directly, so we don't
	// need command-map entries for these throwaway ids.
	CMenu menu;
	if (!menu.CreatePopupMenu())
		return;
	const UINT ID_TYPE_BASE = 0x9100;
	for (Int i = 0; i < typeCount; ++i)
		menu.AppendMenu(MF_STRING, ID_TYPE_BASE + i, WaveEditorTool::getWaveTypeNameAt(i));

	CPoint pt;
	::GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
																 pt.x, pt.y, this);
	if (cmd >= ID_TYPE_BASE && cmd < ID_TYPE_BASE + (UINT)typeCount)
	{
		WaveEditorTool::setSelectedWavesType((Int)(cmd - ID_TYPE_BASE));
		populateList();	// reflect the new type names (and keep the selection highlit)
	}
}

/// Gather every selected row in the list and push that set into the wave tool, using
/// the row the user just acted on (the focused row) as the camera/anchor target.
void WaveEditorOptions::syncToolSelectionFromList(void)
{
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_WAVE_LIST);
	if (!pList)
		return;

	WaveEditorTool::beginListSelection();

	Int anchor = -1;
	POSITION pos = pList->GetFirstSelectedItemPosition();
	while (pos)
	{
		Int row = pList->GetNextSelectedItem(pos);
		WaveEditorTool::addListSelection(row);
		if (anchor < 0)
			anchor = row;
	}

	// Prefer the focused row as the anchor (the one the user just clicked).
	Int focus = pList->GetNextItem(-1, LVNI_FOCUSED);
	if (focus >= 0 && pList->GetItemState(focus, LVIS_SELECTED))
		anchor = focus;

	WaveEditorTool::endListSelection(anchor);
}

BEGIN_MESSAGE_MAP(WaveEditorOptions, COptionsPanel)
	//{{AFX_MSG_MAP(WaveEditorOptions)
	ON_BN_CLICKED(IDC_WAVE_CYCLE_TYPE, OnCycleType)
	ON_BN_CLICKED(IDC_WAVE_UNDO, OnUndo)
	ON_BN_CLICKED(IDC_WAVE_SAVE, OnSave)
	ON_BN_CLICKED(IDC_WAVE_RELOAD, OnReload)
	ON_BN_CLICKED(IDC_WAVE_DELETE, OnDeleteSelected)
	ON_BN_CLICKED(IDC_WAVE_DELETE_ALL, OnDeleteAll)
	ON_BN_CLICKED(IDC_WAVE_MODE_CREATE, OnModeCreate)
	ON_BN_CLICKED(IDC_WAVE_MODE_MANIPULATE, OnModeManipulate)
	ON_BN_CLICKED(IDC_WAVE_MODE_PAINT, OnModePaint)
	ON_BN_CLICKED(IDC_WAVE_MODE_BUCKET, OnModeBucket)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_WAVE_SHOW_LINES, OnShowWaveLines)
	ON_BN_CLICKED(IDC_WAVE_SHOW_SHORELINE, OnShowShoreline)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_WAVE_LIST, OnWaveListItemChanged)
	ON_NOTIFY(NM_RCLICK, IDC_WAVE_LIST, OnWaveListRClick)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_WAVE_SYNC_SELECTION, OnSyncSelectionFromList)
END_MESSAGE_MAP()
