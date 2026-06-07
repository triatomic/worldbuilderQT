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

#if !defined(AFX_TRACINGOVERLAYOPTIONS_H__INCLUDED_)
#define AFX_TRACINGOVERLAYOPTIONS_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// TracingOverlayOptions.h : header file
//

/////////////////////////////////////////////////////////////////////////////
/// TracingOverlayOptions modeless dialog - controls the tracing overlay's
/// opacity and resize interpolation (nearest / default). Opened each time the
/// overlay is enabled and STAYS open: dragging the slider or changing the combo
/// applies to the overlay live (on the fly), no OK needed. A single instance is
/// reused (it is raised to front if already open). Settings persist under the
/// "Appearance" profile section.

class TracingOverlayOptions : public CDialog
{
// Construction
public:
	// Resize interpolation filter choices (combo box order).
	enum {	FILTER_DEFAULT = 0,		///< linear / box (D3DX_FILTER_LINEAR)
				FILTER_NEAREST = 1 };		///< point sampling (D3DX_FILTER_POINT)

	enum {	OPACITY_MIN = 0,
				OPACITY_MAX = 100 };

	TracingOverlayOptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(TracingOverlayOptions)
	enum { IDD = IDD_TRACING_OVERLAY_OPTIONS };
	//}}AFX_DATA

// Overrides
	//{{AFX_VIRTUAL(TracingOverlayOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(TracingOverlayOptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSelchangeFilter();		///< combo change -> apply filter live.
	afx_msg void OnDestroy();
	virtual void OnOK();					///< OK / Enter just hides; keeps the instance.
	virtual void OnCancel();				///< Esc / close hides too (modeless).
	//}}AFX_MSG
	virtual void PostNcDestroy();
	DECLARE_MESSAGE_MAP()

	CSliderCtrl	m_opacitySlider;		///< 0..100 opacity slider.

	void updateOpacityLabel(Int pct);
	void applyToDrawObject(void);		///< push current ui values into DrawObject (live preview, no INI).
	void persistToProfile(void);		///< write the current statics to the [Appearance] INI keys.
	void readControlsToStatics(void);	///< pull live ui values into the statics.

	static TracingOverlayOptions *m_theDialog;	///< the single modeless instance (NULL when closed).

protected: // persisted settings (mirrors the [Appearance] INI keys).
	static Int	m_opacityPct;			///< 0..100, persisted as "TraceOverlayOpacity".
	static Int	m_filter;				///< FILTER_* enum, persisted as "TraceOverlayFilter".

public:
	// Load persisted settings from the profile and push them into DrawObject.
	// Called at view startup so the overlay is correct before the dialog opens.
	static void loadAndApplySettings(void);

	static Int getOpacityPct(void) { return m_opacityPct; }
	static Int getFilter(void) { return m_filter; }

	// Open (or raise) the modeless settings dialog, parented to pParent.
	static void showDialog(CWnd *pParent);
	// Close the dialog if it is open (e.g. when the overlay is turned off).
	static void closeDialog(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TRACINGOVERLAYOPTIONS_H__INCLUDED_)
