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

#if !defined(AFX_BuildList_H__D3FF66C5_7107_4DAC_8A29_5EBAB5C3A24E__INCLUDED_)
#define AFX_BuildList_H__D3FF66C5_7107_4DAC_8A29_5EBAB5C3A24E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BuildList.h : header file
//

#include "TerrainSwatches.h"
#include "OptionsPanel.h"
#include "WBPopupSlider.h"
#include "Common/AsciiString.h"
class WorldHeightMapEdit;
class BuildListInfo;
/////////////////////////////////////////////////////////////////////////////
// BuildList dialog

class BuildList : public COptionsPanel, public PopupSliderOwner
{
// Construction
public:
 	BuildList(CWnd* pParent = NULL);   ///< standard constructor

	~BuildList(void);   ///< standard destructor
	enum { NAME_MAX_LEN = 64 };
// Dialog Data
	//{{AFX_DATA(BuildList)
	enum { IDD = IDD_BUILD_LIST_PANEL };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(BuildList)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK(){return;};  ///< Modeless dialogs don't OK, so eat this for modeless.
	virtual void OnCancel(){return;}; ///< Modeless dialogs don't close on ESC, so eat this for modeless.
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(BuildList)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeSidesCombo();
	afx_msg void OnMoveUp();
	afx_msg void OnMoveDown();
	afx_msg void OnAddBuilding();
	afx_msg void OnSelchangeBuildList();
	afx_msg void OnAlreadyBuild();
	afx_msg void OnDeleteBuilding();
	afx_msg void OnSelendokRebuilds();
	afx_msg void OnEditchangeRebuilds();
	afx_msg void OnDblclkBuildList();
	afx_msg void OnChangeZOffset();
	afx_msg void OnChangeAngle();
	afx_msg void OnExport();
	afx_msg void OnImport();
	afx_msg void OnForcedShowObjects();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


protected:
	Int					m_curSide;
	Int					m_curBuildList;
	WBPopupSliderButton m_heightSlider;
	WBPopupSliderButton m_angleSlider;
	Real				m_angle;
	Real				m_height;
	Bool                m_forcedShowObjects;

	static BuildList	*m_staticThis;
	static Bool			m_updating;

protected:
	void loadSides(void);
	void updateCurSide(void);

public:
	static void addBuilding(Coord3D loc, Real angle, AsciiString name);
	static void update(void) {if (m_staticThis) m_staticThis->loadSides();};
	static void setSelectedBuildList(BuildListInfo *pInfo);

#ifdef RTS_HAS_QT
	// Qt front-end support (WBQtBuildListBridge). The MFC BuildList stays created + hidden
	// (m_staticThis, so the tool's addBuilding/setSelectedBuildList/update reach it); the Qt
	// panel reads state + drives commands through these. Defined in BuildList.cpp.
	static int  qtGetSideCount(void);
	static int  qtGetSideName(int i, char *out, int cap);
	static int  qtGetCurSide(void);
	static void qtSetCurSide(int i);
	static int  qtGetBuildCount(void);
	static int  qtGetBuildName(int i, char *out, int cap);
	static int  qtGetCurBuild(void);
	static void qtSetCurBuild(int i);
	// Current building's attributes.
	static int    qtHasCurBuild(void);
	static double qtGetAngle(void);
	static double qtGetZ(void);
	static int    qtGetAlreadyBuilt(void);
	static int    qtGetRebuilds(void);	// -1 == unlimited
	static void   qtSetAngle(double deg);
	static void   qtSetZ(double z);
	static void   qtSetAlreadyBuilt(int on);
	static void   qtSetRebuilds(int nr);	// -1 == unlimited
	// Power Used meter: 0..100 (== (1-energyUsed)*100, cached from updateCurSide).
	static int  qtGetPowerPercent(void);
	// Commands.
	static void qtMoveUp(void);
	static void qtMoveDown(void);
	static void qtAddBuilding(void);
	static void qtDeleteBuilding(void);
	static void qtExport(void);
	static void qtImport(void);
	static void qtEditProps(void);	// == OnDblclkBuildList (BaseBuildProps)
	static int  qtGetForcedShow(void);
	static void qtSetForcedShow(int on);
#endif

	virtual void GetPopSliderInfo(const long sliderID, long *pMin, long *pMax, long *pLineSize, long *pInitial);
	virtual void PopSliderChanged(const long sliderID, long theVal);
	virtual void PopSliderFinished(const long sliderID, long theVal);

}; 

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BuildList_H__D3FF66C5_711D_4DAC_8A29_5EAAB5C3A23E__INCLUDED_)
