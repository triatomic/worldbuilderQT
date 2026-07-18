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

#if !defined(AFX_WaterOptions_H__6B56E20C_582E_4132_A251_87902218852C__INCLUDED_)
#define AFX_WaterOptions_H__6B56E20C_582E_4132_A251_87902218852C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// WaterOptions.h : header file
//
#include "WBPopupSlider.h"
#include "OptionsPanel.h"
#include "Common/WellKnownKeys.h"

class MapObject;
class PolygonTrigger;
class MovePolygonUndoable;
/////////////////////////////////////////////////////////////////////////////
// WaterOptions dialog

class WaterOptions : public COptionsPanel, public PopupSliderOwner
{

// Construction
public:
	WaterOptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(WaterOptions)
	enum { IDD = IDD_WATER_OPTIONS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(WaterOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK(){return;};  //!< Modeless dialogs don't OK, so eat this for modeless.
	virtual void OnCancel(){return;}; //!< Modeless dialogs don't close on ESC, so eat this for modeless.
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(WaterOptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeWaterEdit();
	afx_msg void OnChangeHeightEdit();
	afx_msg void OnChangeSpacingEdit();
	afx_msg void OnWaterPolygon();
	afx_msg void OnMakeRiver();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
	static WaterOptions *m_staticThis;  ///< Reference to the floating panel so SetWidth and SetFeather can be static.
	Bool		m_updating; ///<true if the ui is updating itself.
	static Int m_waterHeight;
	WBPopupSliderButton m_waterHeightPopup;
	MovePolygonUndoable *m_moveUndoable;
	Int									 m_originalHeight;
	static Int					 m_waterPointSpacing;
	static Bool m_creatingWaterAreas; ///< True if we are creating flood fill water polygons, rather than dropping single points.
protected:
	void updateTheUI(void);
	void startUpdateHeight(void);
	void updateHeight(void);
	void endUpdateHeight(void);
	PolygonTrigger *adjustCount(PolygonTrigger *trigger, Int firstPt, Int lastPt, Int desiredPointCount);

public:
	static void update(void);
	static void setHeight(Int height);
	static Int getHeight(void) { return m_waterHeight;};
	static Int getSpacing(void) { return m_waterPointSpacing;};
	static Bool getCreatingWaterAreas(void) {return m_creatingWaterAreas;}

#ifdef RTS_HAS_QT
	// Qt panel support (WBQtWaterBridge): the Qt Water panel edits the same global tool state
	// (height / spacing / creating-water-areas) and the same single selected water-area
	// PolygonTrigger this dialog does. Defined in WaterOptions.cpp; member statics so the height
	// path can reuse the protected startUpdateHeight / updateHeight / endUpdateHeight helpers and
	// the name / river paths can reuse the same logic as the MFC On* handlers.
	static Bool qtHasSelection(void);	// any single selected polygon (name + Make River)
	static Bool qtIsWaterArea(void);	// that polygon is a water area (gates the height row)
	static Int  qtGetSelectionHeight(void);
	static void qtSetHeight(Int height);
	static void qtSetHeightDragStep(Int height);	// slider drag: keep one undoable across ticks
	static void qtEndHeightScrub(void);				// slider release: close that undoable
	static void qtSetSpacing(Int spacing);
	static void qtSetCreatingWaterAreas(Bool on);
	static Bool qtSetName(const char *name);
	static void qtSetRiver(Bool river);
#endif

public:

	virtual void GetPopSliderInfo(const long sliderID, long *pMin, long *pMax, long *pLineSize, long *pInitial);
	virtual void PopSliderChanged(const long sliderID, long theVal);
	virtual void PopSliderFinished(const long sliderID, long theVal);

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WaterOptions_H__6B56E20C_582E_4030_A251_879097C8853C__INCLUDED_)
