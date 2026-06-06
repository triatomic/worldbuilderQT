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

// WaveEditorOptions.h : options panel for the WorldBuilder wave editor.

#ifndef __WAVEEDITOROPTIONS_H_
#define __WAVEEDITOROPTIONS_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OptionsPanel.h"

/////////////////////////////////////////////////////////////////////////////
// WaveEditorOptions dialog

class WaveEditorOptions : public COptionsPanel
{
// Construction
public:
	WaveEditorOptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(WaveEditorOptions)
	enum { IDD = IDD_WAVE_EDITOR_OPTIONS };
	//}}AFX_DATA

// Overrides
	//{{AFX_VIRTUAL(WaveEditorOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK(){return;};  //!< Modeless dialogs don't OK, so eat this for modeless.
	virtual void OnCancel(){return;}; //!< Modeless dialogs don't close on ESC.
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(WaveEditorOptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnCycleType();
	afx_msg void OnUndo();
	afx_msg void OnSave();
	afx_msg void OnReload();
	afx_msg void OnDeleteSelected();
	afx_msg void OnModeCreate();
	afx_msg void OnModeManipulate();
	afx_msg void OnShowWaveLines();
	afx_msg void OnWaveListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	static WaveEditorOptions *m_staticThis;

	Bool m_updatingList;	///< guard so programmatic list changes don't re-fire selection

	void updateTypeLabel(void);
	void setupColumns(void);
	void populateList(void);

public:
	/// Refresh the "Type:" label and the wave list from the current wave system.
	static void refresh(void);
};

//{{AFX_INSERT_LOCATION}}

#endif // __WAVEEDITOROPTIONS_H_
