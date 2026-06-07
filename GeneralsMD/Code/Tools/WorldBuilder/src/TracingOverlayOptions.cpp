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

// TracingOverlayOptions.cpp : implementation file
//

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "TracingOverlayOptions.h"
#include "DrawObject.h"

// Profile section/keys for the persisted overlay appearance settings.
#define APPEARANCE_SECTION			"Appearance"
#define KEY_TRACE_OPACITY			"TraceOverlayOpacity"
#define KEY_TRACE_FILTER			"TraceOverlayFilter"

Int TracingOverlayOptions::m_opacityPct = 100;
Int TracingOverlayOptions::m_filter = TracingOverlayOptions::FILTER_DEFAULT;
TracingOverlayOptions *TracingOverlayOptions::m_theDialog = NULL;

/////////////////////////////////////////////////////////////////////////////
// TracingOverlayOptions

TracingOverlayOptions::TracingOverlayOptions(CWnd* pParent /*=NULL*/)
	: CDialog(TracingOverlayOptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(TracingOverlayOptions)
	//}}AFX_DATA_INIT
}

void TracingOverlayOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(TracingOverlayOptions)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(TracingOverlayOptions, CDialog)
	//{{AFX_MSG_MAP(TracingOverlayOptions)
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_TRACE_FILTER_COMBO, OnSelchangeFilter)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// TracingOverlayOptions message handlers

void TracingOverlayOptions::updateOpacityLabel(Int pct)
{
	CWnd *pLabel = GetDlgItem(IDC_TRACE_OPACITY_LABEL);
	if (pLabel) {
		CString str;
		str.Format("%d%%", pct);
		pLabel->SetWindowText(str);
	}
}

BOOL TracingOverlayOptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Replace the placeholder static frame with a real slider, the way the other
	// WB option dialogs do (e.g. ContourOptions).
	CWnd *pWnd = GetDlgItem(IDC_TRACE_OPACITY_SLIDER);
	CRect rect;
	pWnd->GetWindowRect(&rect);
	pWnd->DestroyWindow();
	ScreenToClient(&rect);

	m_opacitySlider.Create(TBS_HORZ|TBS_BOTTOM|TBS_AUTOTICKS, rect, this, IDC_TRACE_OPACITY_SLIDER);
	m_opacitySlider.SetRange(OPACITY_MIN, OPACITY_MAX);
	m_opacitySlider.SetTicFreq(10);
	m_opacitySlider.SetPos(m_opacityPct);
	m_opacitySlider.ShowWindow(SW_SHOW);
	updateOpacityLabel(m_opacityPct);

	// Populate the interpolation combo. Order must match the FILTER_* enum.
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TRACE_FILTER_COMBO);
	if (pCombo) {
		pCombo->ResetContent();
		pCombo->AddString("Default");		// FILTER_DEFAULT
		pCombo->AddString("Nearest");		// FILTER_NEAREST
		pCombo->SetCurSel(m_filter);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
}

void TracingOverlayOptions::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar && pScrollBar->m_hWnd == m_opacitySlider.m_hWnd) {
		// Live: update the readout AND push the new opacity into the overlay so it
		// changes as the user drags, without waiting for OK -- but DON'T write the INI
		// here. OnHScroll fires on every drag tick (mouse held down); persisting each
		// tick hammers the registry. Save only when the drag ends.
		Int pct = m_opacitySlider.GetPos();
		updateOpacityLabel(pct);
		readControlsToStatics();
		applyToDrawObject();

		// SB_THUMBPOSITION / SB_ENDSCROLL mark the end of the drag (mouse released or
		// keyboard nudge settled). Persist the final value once, then.
		if (nSBCode == SB_THUMBPOSITION || nSBCode == SB_ENDSCROLL)
			persistToProfile();
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

// Combo selection changed -> apply the new resize interpolation live. This also
// re-loads the PNG with the matching D3DX filter (DrawObject re-decodes when the
// filter changes), so the overlay updates on the fly.
void TracingOverlayOptions::OnSelchangeFilter()
{
	readControlsToStatics();
	applyToDrawObject();
	persistToProfile();		// discrete event (not drag spam) -- safe to write the INI now.
}

// Pull the current control values into the persisted statics.
void TracingOverlayOptions::readControlsToStatics(void)
{
	m_opacityPct = m_opacitySlider.GetPos();

	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_TRACE_FILTER_COMBO);
	if (pCombo) {
		Int sel = pCombo->GetCurSel();
		if (sel >= 0) m_filter = sel;
	}
}

// Modeless: OK (Enter) just closes the window; settings were already applied live.
void TracingOverlayOptions::OnOK()
{
	readControlsToStatics();
	applyToDrawObject();
	persistToProfile();
	DestroyWindow();
}

// Modeless: Esc / close box also just closes; keep whatever was applied live.
void TracingOverlayOptions::OnCancel()
{
	DestroyWindow();
}

void TracingOverlayOptions::OnDestroy()
{
	// Persist the final values when the dialog goes away.
	readControlsToStatics();
	applyToDrawObject();
	persistToProfile();
	CDialog::OnDestroy();
}

// Modeless dialogs must delete themselves and clear the singleton here.
void TracingOverlayOptions::PostNcDestroy()
{
	if (m_theDialog == this) m_theDialog = NULL;
	CDialog::PostNcDestroy();
	delete this;
}

// Push the current settings into DrawObject for live preview. Does NOT touch the INI:
// this is called continuously while the opacity slider is dragged, and writing the
// profile on every WM_HSCROLL tick hammers the registry. Persistence is separate --
// see persistToProfile(), called only when the drag ends / the dialog closes.
void TracingOverlayOptions::applyToDrawObject(void)
{
	// Opacity is 0..100 in the ui; DrawObject wants 0..255 alpha.
	Int alpha = (m_opacityPct * 255) / 100;
	DrawObject::setTracingOverlayOpacity(alpha);
	DrawObject::setTracingOverlayFilter(m_filter);
}

// Write the current settings to the [Appearance] INI keys. Call this only on discrete
// commit points (slider release, combo change, OK, dialog close) -- never on every
// slider-drag tick.
void TracingOverlayOptions::persistToProfile(void)
{
	::AfxGetApp()->WriteProfileInt(APPEARANCE_SECTION, KEY_TRACE_OPACITY, m_opacityPct);
	::AfxGetApp()->WriteProfileInt(APPEARANCE_SECTION, KEY_TRACE_FILTER, m_filter);
}

// Static: read persisted settings and apply them, without opening the dialog.
void TracingOverlayOptions::loadAndApplySettings(void)
{
	m_opacityPct = ::AfxGetApp()->GetProfileInt(APPEARANCE_SECTION, KEY_TRACE_OPACITY, 100);
	m_filter     = ::AfxGetApp()->GetProfileInt(APPEARANCE_SECTION, KEY_TRACE_FILTER, FILTER_DEFAULT);

	if (m_opacityPct < OPACITY_MIN) m_opacityPct = OPACITY_MIN;
	if (m_opacityPct > OPACITY_MAX) m_opacityPct = OPACITY_MAX;
	if (m_filter != FILTER_NEAREST) m_filter = FILTER_DEFAULT;

	Int alpha = (m_opacityPct * 255) / 100;
	DrawObject::setTracingOverlayOpacity(alpha);
	DrawObject::setTracingOverlayFilter(m_filter);
}

// Static: open the modeless dialog (or raise it if it is already open). The
// single instance is owned by MFC and deletes itself in PostNcDestroy.
void TracingOverlayOptions::showDialog(CWnd *pParent)
{
	if (m_theDialog != NULL && ::IsWindow(m_theDialog->m_hWnd)) {
		// Already open -- just bring it to the front.
		m_theDialog->SetForegroundWindow();
		return;
	}

	m_theDialog = new TracingOverlayOptions(pParent);
	if (!m_theDialog->Create(TracingOverlayOptions::IDD, pParent)) {
		delete m_theDialog;
		m_theDialog = NULL;
		return;
	}
	m_theDialog->ShowWindow(SW_SHOW);
}

// Static: close the dialog if it is currently open.
void TracingOverlayOptions::closeDialog(void)
{
	if (m_theDialog != NULL && ::IsWindow(m_theDialog->m_hWnd)) {
		m_theDialog->DestroyWindow();
	}
}
