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

// WaterOptions.cpp : implementation file
//

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "CUndoable.h"
#include "WaterOptions.h"
#include "WaypointOptions.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "PolygonTool.h"
#include "WaypointTool.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/WellKnownKeys.h"
#include "LayersList.h"
#ifdef RTS_HAS_QT
#include "qt/panels/WBQtWaterBridge.h"
#endif

WaterOptions *WaterOptions::m_staticThis = NULL;
Int WaterOptions::m_waterHeight = 7;
Int WaterOptions::m_waterPointSpacing = MAP_XY_FACTOR;
Bool WaterOptions::m_creatingWaterAreas = false;
/////////////////////////////////////////////////////////////////////////////
/// WaterOptions dialog trivial construstor - Create does the real work.


WaterOptions::WaterOptions(CWnd* pParent /*=NULL*/):
m_moveUndoable(NULL)
{
	//{{AFX_DATA_INIT(WaterOptions) 
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

/// Windows default stuff.
void WaterOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(WaterOptions)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

void WaterOptions::setHeight(Int height) 
{ 
	char buffer[50];
	sprintf(buffer, "%d", height);
	m_waterHeight = height;
	if (m_staticThis && !m_staticThis->m_updating) {
		CWnd *pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
		if (pEdit) pEdit->SetWindowText(buffer);
	}
}

void WaterOptions::updateTheUI(void) 
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	CWnd *pWnd = this->GetDlgItem(IDC_WATERNAME_EDIT);

	if (theTrigger && pWnd) {
		pWnd->SetWindowText(theTrigger->getTriggerName().str());
		setHeight(theTrigger->getPoint(0)->z);
	}	
	CButton *pButton = (CButton*)GetDlgItem(IDC_WATER_POLYGON);
	pButton->SetCheck(m_creatingWaterAreas ? 1:0);
	Bool isRiver = false;
	if (theTrigger) {
		isRiver = theTrigger->isRiver();
	}
	pButton = (CButton*)GetDlgItem(IDC_MAKE_RIVER);
	pButton->SetCheck(isRiver ? 1:0);
	pButton->EnableWindow(theTrigger!=NULL);

	pWnd = m_staticThis->GetDlgItem(IDC_SPACING);
	char buffer[_MAX_PATH];
	if (pWnd) {
		sprintf(buffer, "%d", m_waterPointSpacing);
		pWnd->SetWindowText(buffer);
	}
}

void WaterOptions::update(void) 
{
	if (m_staticThis) {
		m_staticThis->updateTheUI();
	}
#ifdef RTS_HAS_QT
	WBQtWater_PushRefresh();
#endif
}

/////////////////////////////////////////////////////////////////////////////
// WaterOptions message handlers

/// Dialog UI initialization.
/** Creates the slider controls, and sets the initial values for 
width and feather in the ui controls. */
BOOL WaterOptions::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_updating = true;

	m_waterHeightPopup.SetupPopSliderButton(this, IDC_HEIGHT_POPUP, this);
	m_waterPointSpacing = 10*MAP_XY_FACTOR;
	m_staticThis = this;
	m_updating = false;
	setHeight(m_waterHeight);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


BEGIN_MESSAGE_MAP(WaterOptions, COptionsPanel)
	//{{AFX_MSG_MAP(WaterOptions)
	ON_CBN_KILLFOCUS(IDC_WATERNAME_EDIT, OnChangeWaterEdit)
	ON_EN_CHANGE(IDC_HEIGHT_EDIT, OnChangeHeightEdit)	 
	ON_EN_CHANGE(IDC_SPACING, OnChangeSpacingEdit)	 
	ON_BN_CLICKED(IDC_WATER_POLYGON, OnWaterPolygon)
	ON_BN_CLICKED(IDC_MAKE_RIVER, OnMakeRiver)
	ON_CBN_SELENDOK(IDC_WATERNAME_EDIT, OnChangeWaterEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void WaterOptions::OnChangeWaterEdit() 
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	// get the combo box
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_WATERNAME_EDIT);
	if (pCombo) {
		// get the text out of the combo. If it is user-typed, sel will be -1, otherwise it will be >=0
		CString theText;
		Int sel = pCombo->GetCurSel();
		if (sel >= 0) {
			pCombo->GetLBText(sel, theText);
		} else {
			pCombo->GetWindowText(theText);
		}
		AsciiString name((LPCTSTR)theText);

		// check to see if the user-entered name is already in use.
		Bool didMatch = false;

		// check trigger area objects
		PolygonTrigger *pTrig;
		for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); !didMatch && pTrig; pTrig = pTrig->getNext()) {
			if (pTrig==theTrigger) continue; // don't check against yourself.
			AsciiString trigName = pTrig->getTriggerName();
			if (name == trigName) {
				if (pTrig->isValid()) {
					didMatch = true;
				} else {
					PolygonTrigger::removePolygonTrigger(pTrig);
				}
				break;
			}
		}

		// if there's a match, throw up a messagebox, otherwise set the name
		if (didMatch) {
			::AfxMessageBox("Name already in use");
		} else {
			if (theTrigger) {
				theTrigger->setTriggerName(name);
			}
		}
	}
}


void WaterOptions::OnWaterPolygon() 
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_WATER_POLYGON);
	m_creatingWaterAreas = (pButton->GetCheck()==1);
}

void WaterOptions::OnMakeRiver() 
{
    CButton *pButton = (CButton*)GetDlgItem(IDC_MAKE_RIVER);
    Bool river = (pButton->GetCheck() == 1);
    PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
    
    if (!theTrigger) return;
    
    theTrigger->setRiver(river);
    
    if (river) {
        Int selectedPoint = PolygonTool::getSelectedPointNdx();
        if (selectedPoint < 0) selectedPoint = 0; // Default to first point if none selected
        
        // Determine river flow direction by checking which orientation has wider segments
        Int horizontalWideCount = 0;
        Int verticalWideCount = 0;
        Int i;
        
        for (i = 0; i < theTrigger->getNumPoints() - 1; i++) {
            ICoord3D pt1 = *theTrigger->getPoint(i);
            ICoord3D pt2 = *theTrigger->getPoint(i + 1);
            
            Real dx = abs(pt1.x - pt2.x);
            Real dy = abs(pt1.y - pt2.y);
            
            if (dx > dy) {
                horizontalWideCount++;
            } else {
                verticalWideCount++;
            }
        }
        
        // Flow is perpendicular to the wide segments
        Bool flowIsHorizontal = (verticalWideCount > horizontalWideCount);
        
        // Find the opposite end based on flow direction
        Int endPoint = 0;
        Real maxDistance = 0;
        
        ICoord3D startPt = *theTrigger->getPoint(selectedPoint);
        
        for (i = 0; i < theTrigger->getNumPoints(); i++) {
            if (i == selectedPoint) continue;
            
            ICoord3D pt = *theTrigger->getPoint(i);
            Real distance;
            
            if (flowIsHorizontal) {
                distance = abs(pt.x - startPt.x);
            } else {
                distance = abs(pt.y - startPt.y);
            }
            
            if (distance > maxDistance) {
                maxDistance = distance;
                endPoint = i;
            }
        }
        
        // Reorder points if needed so start is at index 0
        if (selectedPoint != 0) {
            // Store all points temporarily
            int numPoints = theTrigger->getNumPoints();
            ICoord3D *tempPoints = new ICoord3D[numPoints];
            
            for (i = 0; i < numPoints; i++) {
                tempPoints[i] = *theTrigger->getPoint(i);
            }
            
            // Clear existing points
            while (theTrigger->getNumPoints()) {
                theTrigger->deletePoint(theTrigger->getNumPoints() - 1);
            }
            
            // Add points in new order (from selected to end, then beginning to selected)
            for (i = selectedPoint; i < numPoints; i++) {
                theTrigger->addPoint(tempPoints[i]);
            }
            for (i = 0; i < selectedPoint; i++) {
                theTrigger->addPoint(tempPoints[i]);
            }
            
            delete[] tempPoints;
        }
        
        theTrigger->setRiverStart(0);
    }
}

/// Adjust the spacing.
PolygonTrigger * WaterOptions::adjustCount(PolygonTrigger *trigger, Int firstPt, Int lastPt, Int desiredPointCount)
{
	PolygonTrigger *pNew = newInstance(PolygonTrigger)(trigger->getNumPoints());
//	Real endLen=0;
	Real totalLen=0;	
	Real curSpacingLen = 10;
	Int curPoint = lastPt;
	ICoord3D pt;
	while (curPoint != firstPt) {
		pt = *trigger->getPoint(curPoint);
		pNew->addPoint(pt);
		curPoint++;
		if (curPoint>=trigger->getNumPoints()) {
			curPoint = 0;
		}
	}	

	curPoint = firstPt;
	while (curPoint != lastPt) {
		Int nextPoint = curPoint;
		nextPoint++;
		if (nextPoint>=trigger->getNumPoints()) {
			nextPoint = 0;
		}
		ICoord3D curPt = *trigger->getPoint(curPoint);
		ICoord3D nextPt = *trigger->getPoint(nextPoint);
		Real dx = nextPt.x-curPt.x;
		Real dy = nextPt.y-curPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		totalLen += curLen;
		curPoint = nextPoint;
	}
	Real spacing = totalLen/(desiredPointCount-1);

	Bool didCurPoint = true;

	curPoint = firstPt;
	pt = *trigger->getPoint(curPoint);
	pNew->addPoint(pt);
	while (curPoint != lastPt) {
		Int nextPoint = curPoint;
		nextPoint++;
		if (nextPoint>=trigger->getNumPoints()) {
			nextPoint = 0;
		}
		ICoord3D curPt = *trigger->getPoint(curPoint);
		ICoord3D nextPt = *trigger->getPoint(nextPoint);
		Real dx = nextPt.x-curPt.x;
		Real dy = nextPt.y-curPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		if (curLen > 4*MAP_XY_FACTOR && curLen>2*spacing) {
			if (!didCurPoint) pNew->addPoint(curPt);
			else pNew->setPoint(curPt, pNew->getNumPoints()-1);
			pNew->addPoint(nextPt);
			didCurPoint = true;
			curSpacingLen = spacing;
		} else if (curSpacingLen>curLen) {
			curSpacingLen -= curLen;
		} else {
			while (curLen >= curSpacingLen) {
				// cur len > curSpacingLen.
				Real factor = curSpacingLen/curLen;
				curPt.x += dx*factor;
				curPt.y += dy*factor;
				pNew->addPoint(curPt);
				didCurPoint = false;
				dx = nextPt.x-curPt.x;
				dy = nextPt.y-curPt.y;
				curLen -= curSpacingLen;
				curSpacingLen = spacing;
			}
			curSpacingLen -= curLen;
 			if ((curLen)<MAP_XY_FACTOR/2) {
				didCurPoint = true;
			}
		}
		curPoint = nextPoint;
	}
	return pNew;
}

void WaterOptions::GetPopSliderInfo(const long sliderID, long *pMin, long *pMax, long *pLineSize, long *pInitial)
{
	switch (sliderID) {


		case IDC_HEIGHT_POPUP:
			*pMin = 0;
			*pMax = 255*MAP_HEIGHT_SCALE;
			*pInitial = m_waterHeight;
			*pLineSize = 1;
			break;

		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}	// switch
}

void WaterOptions::PopSliderChanged(const long sliderID, long theVal)
{
	CString str;
	CWnd *pEdit;
	switch (sliderID) {


		case IDC_HEIGHT_POPUP:
			m_waterHeight = theVal;
			str.Format("%d",m_waterHeight);
			pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
			m_updating = true;
			if (pEdit) pEdit->SetWindowText(str);
			startUpdateHeight();
			updateHeight();
			m_updating = false;
			break;


		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}	// switch
}

void WaterOptions::PopSliderFinished(const long sliderID, long theVal)
{
	switch (sliderID) {
		case IDC_HEIGHT_POPUP:
			updateHeight();
			endUpdateHeight();
			break;

		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}	// switch

}

void WaterOptions::startUpdateHeight(void)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (!theTrigger) {
		REF_PTR_RELEASE(m_moveUndoable);
		return;
	}
	if (!theTrigger->isWaterArea()) {
		REF_PTR_RELEASE(m_moveUndoable); 
		return;
	}
	if (m_moveUndoable && theTrigger == m_moveUndoable->getTrigger()) {
		return;
	}
	m_originalHeight = theTrigger->getPoint(0)->z;
	Int i;
	for (i=0; i<theTrigger->getNumPoints(); i++) {
		ICoord3D loc = *theTrigger->getPoint(i);
		loc.z = m_originalHeight;
		theTrigger->setPoint(loc, i);
	}
	m_moveUndoable = new MovePolygonUndoable(theTrigger);
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	pDoc->AddAndDoUndoable(m_moveUndoable);
}


void WaterOptions::updateHeight(void)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (!theTrigger || !m_moveUndoable) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	if (!theTrigger->isWaterArea()) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	if (theTrigger != m_moveUndoable->getTrigger()) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	ICoord3D iLoc;
	Int dz = m_waterHeight - m_originalHeight;
	iLoc.x = 0;
	iLoc.y = 0;
	iLoc.z = dz;
	m_moveUndoable->SetOffset(iLoc);
	WbView3d *pView = CWorldBuilderDoc::GetActive3DView();
	pView->Invalidate();
}

void WaterOptions::endUpdateHeight(void)
{
	REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
}


 /// Handles width edit ui messages.
/** Gets the new edit control text, converts it to an int, then updates
		the slider and brush tool. */
void WaterOptions::OnChangeHeightEdit() 
{
	if (m_updating) return;
	CWnd *pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
	char buffer[_MAX_PATH];
	if (pEdit) {
		pEdit->GetWindowText(buffer, sizeof(buffer));
		Int height;
		m_updating = true;
		if (1==sscanf(buffer, "%d", &height)) {
			m_waterHeight = height;
			startUpdateHeight();
			updateHeight();
			endUpdateHeight();
		}
		m_updating = false;
	}
}

 /// Handles width edit ui messages.
/** Gets the new edit control text, converts it to an int, then updates
		the slider and brush tool. */
void WaterOptions::OnChangeSpacingEdit() 
{
	if (m_updating) return;
	CWnd *pEdit = m_staticThis->GetDlgItem(IDC_SPACING);
	char buffer[_MAX_PATH];
	if (pEdit) {
		pEdit->GetWindowText(buffer, sizeof(buffer));
		Int height;
		m_updating = true;
		if (1==sscanf(buffer, "%d", &height)) {
			m_waterPointSpacing = height;
		}	else {
			sprintf(buffer, "%d", m_waterPointSpacing);
			pEdit->SetWindowText(buffer);
		}
		m_updating = false;
	}
}


#ifdef RTS_HAS_QT
//----------------------------------------------------------------------------------------
// Qt panel support (declared in WaterOptions.h). These reuse the same global statics and the
// same protected height helpers the MFC handlers use, so the Qt Water panel and the tools
// stay in step. See src/WBQtWaterBridge.cpp for the extern "C" seam that calls these.
//----------------------------------------------------------------------------------------

// True when a single water-area PolygonTrigger is selected (the only case where name / height
// / Make River are meaningful), mirroring updateTheUI keying those controls off the trigger.
Bool WaterOptions::qtHasSelection(void)
{
	// == MFC updateTheUI: the name box + Make River button are shown for ANY single selected
	// polygon (Make River's whole point is turning a plain polygon into one). Only the HEIGHT
	// row is water-area-only -- see qtIsWaterArea.
	return WaypointOptions::getSingleSelectedPolygon() != NULL;
}

Bool WaterOptions::qtIsWaterArea(void)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger == NULL)
	{
		return false;
	}
	return theTrigger->isWaterArea();
}

// The selected trigger's current height (point 0's z), matching updateTheUI's setHeight() seed.
Int WaterOptions::qtGetSelectionHeight(void)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger != NULL && theTrigger->isWaterArea() && theTrigger->getNumPoints() > 0)
	{
		return theTrigger->getPoint(0)->z;
	}
	return m_waterHeight;
}

// Apply a new water height to the selected trigger through the same MovePolygonUndoable
// sequence the MFC edit / slider path uses (startUpdateHeight -> updateHeight ->
// endUpdateHeight). Runs on the hidden MFC panel instance so the protected helpers are reachable.
void WaterOptions::qtSetHeight(Int height)
{
	m_waterHeight = height;
	if (m_staticThis == NULL)
	{
		return;
	}
	m_staticThis->startUpdateHeight();
	m_staticThis->updateHeight();
	m_staticThis->endUpdateHeight();
}

// == MFC's PopSliderChanged/PopSliderFinished protocol: a slider DRAG keeps one
// MovePolygonUndoable alive across all its ticks (startUpdateHeight's reuse guard makes the
// repeat starts no-ops), so Ctrl+Z undoes the whole drag rather than one tick at a time.
// qtSetHeightDragStep sets + updates but does NOT end; qtEndHeightScrub ends when the drag
// finishes. Non-drag changes (spinbox, typed) keep using qtSetHeight (one commit each).
void WaterOptions::qtSetHeightDragStep(Int height)
{
	m_waterHeight = height;
	if (m_staticThis == NULL)
	{
		return;
	}
	m_staticThis->startUpdateHeight();	// reuses the live undoable after the first tick
	m_staticThis->updateHeight();
}

void WaterOptions::qtEndHeightScrub(void)
{
	if (m_staticThis != NULL)
	{
		m_staticThis->endUpdateHeight();
	}
}

// Store the point spacing (the global static), mirroring OnChangeSpacingEdit's accepted-value
// case (the Qt spinbox has already parsed the int).
void WaterOptions::qtSetSpacing(Int spacing)
{
	m_waterPointSpacing = spacing;
}

// Flip the "creating water areas" toggle (the global static), mirroring OnWaterPolygon.
void WaterOptions::qtSetCreatingWaterAreas(Bool on)
{
	m_creatingWaterAreas = on;
}

// Rename the selected trigger, reusing OnChangeWaterEdit's duplicate-name check. Returns true
// if the name was applied, false if it was already in use (a message box was shown) / nothing
// was selected.
Bool WaterOptions::qtSetName(const char *name)
{
	if (name == NULL)
	{
		return false;
	}
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger == NULL)
	{
		return false;
	}
	AsciiString newName(name);

	// check to see if the user-entered name is already in use (mirrors OnChangeWaterEdit).
	Bool didMatch = false;
	PolygonTrigger *pTrig;
	for (pTrig = PolygonTrigger::getFirstPolygonTrigger(); !didMatch && pTrig; pTrig = pTrig->getNext())
	{
		if (pTrig == theTrigger)
		{
			continue; // don't check against yourself.
		}
		AsciiString trigName = pTrig->getTriggerName();
		if (newName == trigName)
		{
			if (pTrig->isValid())
			{
				didMatch = true;
			}
			else
			{
				PolygonTrigger::removePolygonTrigger(pTrig);
			}
			break;
		}
	}

	if (didMatch)
	{
		::AfxMessageBox("Name already in use");
		return false;
	}
	theTrigger->setTriggerName(newName);
	return true;
}

// Flip the Make River flag on the selected trigger, reusing OnMakeRiver's flow-direction
// reorder when turning it on (OnMakeRiver reads the button state; this factors the body out to
// a parameterised static).
void WaterOptions::qtSetRiver(Bool river)
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger == NULL)
	{
		return;
	}

	theTrigger->setRiver(river);

	if (river)
	{
		Int selectedPoint = PolygonTool::getSelectedPointNdx();
		if (selectedPoint < 0)
		{
			selectedPoint = 0; // Default to first point if none selected
		}

		// Determine river flow direction by checking which orientation has wider segments
		Int horizontalWideCount = 0;
		Int verticalWideCount = 0;
		Int i;

		for (i = 0; i < theTrigger->getNumPoints() - 1; i++)
		{
			ICoord3D pt1 = *theTrigger->getPoint(i);
			ICoord3D pt2 = *theTrigger->getPoint(i + 1);

			Real dx = abs(pt1.x - pt2.x);
			Real dy = abs(pt1.y - pt2.y);

			if (dx > dy)
			{
				horizontalWideCount++;
			}
			else
			{
				verticalWideCount++;
			}
		}

		// Flow is perpendicular to the wide segments
		Bool flowIsHorizontal = (verticalWideCount > horizontalWideCount);

		// Find the opposite end based on flow direction
		Int endPoint = 0;
		Real maxDistance = 0;

		ICoord3D startPt = *theTrigger->getPoint(selectedPoint);

		for (i = 0; i < theTrigger->getNumPoints(); i++)
		{
			if (i == selectedPoint)
			{
				continue;
			}

			ICoord3D pt = *theTrigger->getPoint(i);
			Real distance;

			if (flowIsHorizontal)
			{
				distance = abs(pt.x - startPt.x);
			}
			else
			{
				distance = abs(pt.y - startPt.y);
			}

			if (distance > maxDistance)
			{
				maxDistance = distance;
				endPoint = i;
			}
		}

		// Reorder points if needed so start is at index 0
		if (selectedPoint != 0)
		{
			// Store all points temporarily
			int numPoints = theTrigger->getNumPoints();
			ICoord3D *tempPoints = new ICoord3D[numPoints];

			for (i = 0; i < numPoints; i++)
			{
				tempPoints[i] = *theTrigger->getPoint(i);
			}

			// Clear existing points
			while (theTrigger->getNumPoints())
			{
				theTrigger->deletePoint(theTrigger->getNumPoints() - 1);
			}

			// Add points in new order (from selected to end, then beginning to selected)
			for (i = selectedPoint; i < numPoints; i++)
			{
				theTrigger->addPoint(tempPoints[i]);
			}
			for (i = 0; i < selectedPoint; i++)
			{
				theTrigger->addPoint(tempPoints[i]);
			}

			delete[] tempPoints;
		}

		theTrigger->setRiverStart(0);
	}
}
#endif

