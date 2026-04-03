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

// FeatherTool.cpp
// Texture tiling tool for worldbuilder.
// Author: John Ahlquist, April 2001

#include "StdAfx.h" 
#include "resource.h"

#include "FeatherTool.h"
#include "FeatherOptions.h"
#include "CUndoable.h"
#include "DrawObject.h"
#include "MainFrm.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
//
// FeatherTool class.

Bool FeatherTool::m_enableMirror;
Bool FeatherTool::m_mirrorX;   // left / right
Bool FeatherTool::m_mirrorY;   // top  / bottom
Bool FeatherTool::m_mirrorDiag;   // diagonal-only (XY corner)

Int FeatherTool::m_feather = 0;
Int FeatherTool::m_rate = 0;
Int FeatherTool::m_radius = 0;

//
/// Constructor
FeatherTool::FeatherTool(void) :
	Tool(ID_FEATHERTOOL, IDC_BRUSH_CROSS)
{
	m_htMapEditCopy = NULL;
	m_htMapFeatherCopy = NULL;
	m_htMapRateCopy = NULL;
}
	
/// Destructor
FeatherTool::~FeatherTool(void) 
{
	REF_PTR_RELEASE(m_htMapEditCopy);
	REF_PTR_RELEASE(m_htMapFeatherCopy); 
	REF_PTR_RELEASE(m_htMapRateCopy); 
}



/// Shows the brush options panel.
void FeatherTool::activate() 
{
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_FEATHER_OPTIONS);
	DrawObject::setDoBrushFeedback(true);
	DrawObject::setBrushFeedbackParms(false, m_feather, 0);
}

/// Set the brush feather and notify the height options panel of the change.
void FeatherTool::setFeather(Int feather) 
{ 
	if (m_feather != feather) {
		m_feather = feather;
		// notify feather palette options panel
		FeatherOptions::setFeather(m_feather);
		DrawObject::setBrushFeedbackParms(false, m_feather, 0);
	}
};

/// Set the brush feather and notify the height options panel of the change.
void FeatherTool::setRate(Int rate) 
{ 
	if (m_rate != rate) {
		m_rate = rate;
		// notify feather palette options panel
		FeatherOptions::setRate(rate);
	}
};

/// Set the brush feather and notify the height options panel of the change.
void FeatherTool::setRadius(Int radius) 
{ 
	if (m_radius != radius) {
		m_radius = radius;
		// notify feather palette options panel
		FeatherOptions::setRadius(radius);
	}
};

/// Start tool.
/** Setup the tool to start brushing - make a copy of the height map
to edit, another copy because we need it :), and call mouseMovedDown. */
void FeatherTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc) 
{
	if (m != TRACK_L) return;

//	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	// just in case, release it.
	REF_PTR_RELEASE(m_htMapEditCopy);
	m_htMapEditCopy = pDoc->GetHeightMap()->duplicate();
	m_htMapFeatherCopy = pDoc->GetHeightMap()->duplicate();
	m_htMapRateCopy = pDoc->GetHeightMap()->duplicate();
	Int size = m_htMapRateCopy->getXExtent() * m_htMapRateCopy->getYExtent();
	UnsignedByte *pData = m_htMapRateCopy->getDataPtr();
	Int i;
	for (i=0; i<size; i++) {
		*pData++ = 0;
	}
	m_prevXIndex = -1;
	m_prevYIndex = -1;
	mouseMoved(m, viewPt, pView, pDoc);
}

/// End tool.
/** Finish the tool operation - create a command, pass it to the 
doc to execute, and cleanup ref'd objects. */
void FeatherTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc) 
{
	if (m != TRACK_L) return;

	WBDocUndoable *pUndo = new WBDocUndoable(pDoc, m_htMapEditCopy);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo); // belongs to pDoc now.
	REF_PTR_RELEASE(m_htMapEditCopy);
	REF_PTR_RELEASE(m_htMapFeatherCopy);
	REF_PTR_RELEASE(m_htMapRateCopy);
}


void FeatherTool::applyFeatherAt(CPoint ndx, IRegion2D& partialRange, Bool& redoRate)
{
    int brushWidth = m_feather;
    int sub = brushWidth / 2;
    int add = brushWidth - sub;

    Int i, j;
    for (i = ndx.x - sub; i < ndx.x + add; i++) {
        if (i < 0 || i >= m_htMapEditCopy->getXExtent()) continue;
        for (j = ndx.y - sub; j < ndx.y + add; j++) {
            if (j < 0 || j >= m_htMapEditCopy->getYExtent()) continue;

            Real blendFactor = calcRoundBlendFactor(ndx, i, j, m_feather, 0);
            if (blendFactor > 0.0f) {
                Int rate = m_htMapRateCopy->getHeight(i, j);
                rate += blendFactor * m_rate * 5;
                if (rate > 255) { rate = 255; redoRate = true; }
                m_htMapRateCopy->setHeight(i, j, rate);

                Int total = 0;
                Real numSamples = 0;
                Int radius = m_radius;
                if (radius < 1) radius = 1;
                if (radius > FeatherOptions::MAX_RADIUS) radius = FeatherOptions::MAX_RADIUS;

                Int ii, jj;
                for (ii = i - radius; ii < i + radius + 1; ii++) {
                    for (jj = j - radius; jj < j + radius + 1; jj++) {
                        Real dist = sqrt((float)((ii-i)*(ii-i) + (jj-j)*(jj-j)));
                        if (dist < 1.0f) dist = 1.0f;
                        // factor unused here but kept for clarity; clamped sampling below
                        int iNdx = ii; if (iNdx < 0) iNdx = 1;
                        if (iNdx >= m_htMapEditCopy->getXExtent()) iNdx = m_htMapEditCopy->getXExtent() - 1;
                        int jNdx = jj; if (jNdx < 0) jNdx = 1;
                        if (jNdx >= m_htMapEditCopy->getYExtent()) jNdx = m_htMapEditCopy->getYExtent() - 1;
                        total += m_htMapFeatherCopy->getHeight(iNdx, jNdx);
                        numSamples += 1;
                    }
                }
                total = floor(total / numSamples);
                UnsignedByte origHeight = m_htMapFeatherCopy->getHeight(i, j);
                float rateF = rate / 255.0f;
                total = floor(origHeight * (1.0f - rateF) + total * rateF + 0.5f);
                m_htMapEditCopy->setHeight(i, j, total);
            }
        }
    }

    // Expand dirty region
    if (ndx.x - brushWidth < partialRange.lo.x) partialRange.lo.x = ndx.x - brushWidth;
    if (ndx.x + brushWidth > partialRange.hi.x) partialRange.hi.x = ndx.x + brushWidth;
    if (ndx.y - brushWidth < partialRange.lo.y) partialRange.lo.y = ndx.y - brushWidth;
    if (ndx.y + brushWidth > partialRange.hi.y) partialRange.hi.y = ndx.y + brushWidth;
}

/// Execute the tool.
/** Smooth the height map at the current point. */
void FeatherTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
    Coord3D cpt;
    pView->viewToDocCoords(viewPt, &cpt);
    DrawObject::setFeedbackPos(cpt);
    pView->Invalidate();
    pDoc->updateAllViews();
    if (m != TRACK_L) return;

    CPoint ndx;
    getCenterIndex(&cpt, m_feather, &ndx, pDoc);
    m_prevXIndex = ndx.x;
    m_prevYIndex = ndx.y;

    int brushWidth = m_feather;
    IRegion2D partialRange;
    partialRange.lo.x = ndx.x - brushWidth;
    partialRange.hi.x = ndx.x + brushWidth;
    partialRange.lo.y = ndx.y - brushWidth;
    partialRange.hi.y = ndx.y + brushWidth;

    Bool redoRate = false;

    // Primary stroke
    applyFeatherAt(ndx, partialRange, redoRate);

    // Mirror across X (left/right)
    if (m_enableMirror && (m_mirrorX || m_mirrorDiag))
    {
        CPoint mirrorX;
        mirrorX.x = m_htMapEditCopy->getXExtent() - 1 - ndx.x;
        mirrorX.y = ndx.y;
        if (m_mirrorX)
            applyFeatherAt(mirrorX, partialRange, redoRate);

        // Mirror across Y (top/bottom)
        if (m_mirrorY)
        {
            CPoint mirrorY;
            mirrorY.x = ndx.x;
            mirrorY.y = m_htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyFeatherAt(mirrorY, partialRange, redoRate);
        }

        // Diagonal corner — when both axes or diag-only toggle
        if (m_mirrorDiag || (m_mirrorX && m_mirrorY))
        {
            CPoint mirrorXY;
            mirrorXY.x = m_htMapEditCopy->getXExtent() - 1 - ndx.x;
            mirrorXY.y = m_htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyFeatherAt(mirrorXY, partialRange, redoRate);
        }
    }
    else if (m_enableMirror && m_mirrorY)
    {
        CPoint mirrorY;
        mirrorY.x = ndx.x;
        mirrorY.y = m_htMapEditCopy->getYExtent() - 1 - ndx.y;
        applyFeatherAt(mirrorY, partialRange, redoRate);
    }

    pDoc->updateHeightMap(m_htMapEditCopy, true, partialRange);

    if (redoRate) {
        Int size = m_htMapRateCopy->getXExtent() * m_htMapRateCopy->getYExtent();
        UnsignedByte *pData    = m_htMapRateCopy->getDataPtr();
        UnsignedByte *pFeather = m_htMapFeatherCopy->getDataPtr();
        UnsignedByte *pEdit    = m_htMapEditCopy->getDataPtr();
        for (Int i = 0; i < size; i++) {
            *pData++ = 0;
            *pFeather = *pEdit;
            pFeather++; pEdit++;
        }
    }
}