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

// AutoEdgeOutTool.cpp
// Texture tiling tool for worldbuilder.
// Author: John Ahlquist, April 2001

#include "StdAfx.h" 
#include "resource.h"

#include "AutoEdgeOutTool.h"
#include "CUndoable.h"
#include "MainFrm.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "PointerTool.h"


Bool AutoEdgeOutTool::m_autoEdgeToolActive = false;
Bool AutoEdgeOutTool::m_enableMirror;
Bool AutoEdgeOutTool::m_mirrorX;
Bool AutoEdgeOutTool::m_mirrorY;
Bool AutoEdgeOutTool::m_mirrorDiag;
//
// AutoEdgeOutTool class.
//
/// Constructor
AutoEdgeOutTool::AutoEdgeOutTool(void) :
	Tool(ID_AUTO_EDGE_OUT_TOOL, IDC_AUTO_EDGE_OUT) 
{
}
	
/// Destructor
AutoEdgeOutTool::~AutoEdgeOutTool(void) 
{
}

/// Shows the brush options panel.
void AutoEdgeOutTool::activate() 
{
	Tool::activate();
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_BLEND_MATERIAL);
	m_autoEdgeToolActive = true;
	BlendMaterial::updateBlendPointerToolTip();
}

void AutoEdgeOutTool::deactivate() 
{
	m_autoEdgeToolActive = false;
	PointerTool::setLastPointerInfoString("");
	Tool::deactivate();
}

void AutoEdgeOutTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc) {
	// Call this ridiculously expensive redraw function every time the mouse moves
	// Greatly smooths the framerate when just moving the mouse around, with or without any selection
	// At the cost of your GPU usage 
	pView->Invalidate();
	pDoc->updateAllViews();
	return;
}

/** Helper: apply blend or unblend at a given cell index. */
void AutoEdgeOutTool::applyEdgeAt(CPoint pt, WorldHeightMapEdit* htMapEditCopy, Bool shiftKey)
{
    if (pt.x < 0 || pt.x >= htMapEditCopy->getXExtent()) return;
    if (pt.y < 0 || pt.y >= htMapEditCopy->getYExtent()) return;

    if (shiftKey) {
        htMapEditCopy->unblendArea(pt.x, pt.y);
    } else {
        htMapEditCopy->autoBlendOut(
            pt.x,
            pt.y,
            BlendMaterial::getBlendTexClass(),
            BlendMaterial::isHorizVertGap(),
            BlendMaterial::isDiagGap(),
            BlendMaterial::isRevalBlends()
        );
    }
}

/** Execute the tool on mouse down - Create a copy of the height map
* to edit, blend the edges, and give the undoable command to the doc.
* If Shift is held, unblend the area instead (remove blend tiles). */
void AutoEdgeOutTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc) 
{
    if (m != TRACK_L) return;

    Coord3D cpt;
    pView->viewToDocCoords(viewPt, &cpt);

    CPoint ndx;
    if (!pDoc->getCellIndexFromCoord(cpt, &ndx)) {
        return;
    }

    Bool shiftKey = (0x8000 & ::GetAsyncKeyState(VK_SHIFT)) != 0;
    WorldHeightMapEdit *htMapEditCopy = pDoc->GetHeightMap()->duplicate();

    // Primary stroke
    applyEdgeAt(ndx, htMapEditCopy, shiftKey);

    // Mirror across X centre (left/right)
    if (m_enableMirror && (m_mirrorX || m_mirrorDiag))
    {
        CPoint mirrorX;
        mirrorX.x = htMapEditCopy->getXExtent() - 1 - ndx.x;
        mirrorX.y = ndx.y;
        if (m_mirrorX)
            applyEdgeAt(mirrorX, htMapEditCopy, shiftKey);

        // Mirror across Y centre (top/bottom)
        if (m_mirrorY)
        {
            CPoint mirrorY;
            mirrorY.x = ndx.x;
            mirrorY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyEdgeAt(mirrorY, htMapEditCopy, shiftKey);
        }

        // Diagonal: opposite corner
        if (m_mirrorDiag || (m_mirrorX && m_mirrorY))
        {
            CPoint mirrorXY;
            mirrorXY.x = htMapEditCopy->getXExtent() - 1 - ndx.x;
            mirrorXY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyEdgeAt(mirrorXY, htMapEditCopy, shiftKey);
        }
    }
    else if (m_enableMirror && m_mirrorY)
    {
        CPoint mirrorY;
        mirrorY.x = ndx.x;
        mirrorY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
        applyEdgeAt(mirrorY, htMapEditCopy, shiftKey);
    }

    IRegion2D partialRange = {0, 0, 0, 0};
    pDoc->updateHeightMap(htMapEditCopy, false, partialRange);
    WBDocUndoable *pUndo = new WBDocUndoable(pDoc, htMapEditCopy);
    pDoc->AddAndDoUndoable(pUndo);
    REF_PTR_RELEASE(pUndo);
    REF_PTR_RELEASE(htMapEditCopy);
}