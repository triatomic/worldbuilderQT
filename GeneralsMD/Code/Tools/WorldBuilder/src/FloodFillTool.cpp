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

// FloodFillTool.cpp
// Texture tiling tool for worldbuilder.
// Author: John Ahlquist, April 2001

#include "StdAfx.h" 
#include "resource.h"

#include "FloodFillTool.h"
#include "CUndoable.h"
#include "DrawObject.h"
#include "MainFrm.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "TerrainMaterial.h"
//
// FloodFillTool class.
//

Bool FloodFillTool::m_adjustCliffTextures = false;
Bool FloodFillTool::m_enableMirror;
Bool FloodFillTool::m_mirrorX;
Bool FloodFillTool::m_mirrorY;
Bool FloodFillTool::m_mirrorDiag;

/// Constructor
FloodFillTool::FloodFillTool(void) :
	Tool(ID_TILE_FLOOD_FILL, IDC_FLOOD_FILL),
	m_cliffCursor(NULL)
{
}
	
/// Destructor
FloodFillTool::~FloodFillTool(void) 
{
	if (m_cliffCursor) {
		::DestroyCursor(m_cliffCursor);
	}
}


/// Shows the terrain materials options panel.
void FloodFillTool::activate() 
{
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_TERRAIN_MATERIAL);
	TerrainMaterial::setToolOptions(true, true);
	DrawObject::setDoBrushFeedback(false);
	m_adjustCliffTextures = false;
}

/** Set the cursor. */
void FloodFillTool::setCursor(void)   
{
	if (m_adjustCliffTextures) {
		if (m_cliffCursor == NULL) {
			m_cliffCursor = AfxGetApp()->LoadCursor(MAKEINTRESOURCE(IDC_CLIFF));
		}
		::SetCursor(m_cliffCursor);
	} else {
		Tool::setCursor();
	}
}

void FloodFillTool::applyFillAt(CPoint pt, WorldHeightMapEdit* htMapEditCopy, Bool shiftKey, Bool &didIt)
{
    if (pt.x < 0 || pt.x >= htMapEditCopy->getXExtent()) return;
    if (pt.y < 0 || pt.y >= htMapEditCopy->getYExtent()) return;

    if (m_adjustCliffTextures) {
        if (htMapEditCopy->doCliffAdjustment(pt.x, pt.y))
            didIt = true;
    } else {
        if (htMapEditCopy->floodFill(pt.x, pt.y, m_textureClassToDraw, shiftKey))
            didIt = true;
    }
}

/// Left click code.  Sets m_textureClassToDraw and calls eitherMouseDown()
/// Perform the tool behavior on mouse down.
/** Creates a copy of the height map, flood fills it at pt with m_textureClassToDraw which
has been set by the calling routine.  Then builds
the command, and passes it to the doc. */
void FloodFillTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc) 
{
    Coord3D cpt;
    pView->viewToDocCoords(viewPt, &cpt);

    CPoint ndx;
    if (!pDoc->getCellIndexFromCoord(cpt, &ndx)) {
        return;
    }

    if (m == TRACK_L)
        m_textureClassToDraw = TerrainMaterial::getFgTexClass();
    else 
        m_textureClassToDraw = TerrainMaterial::getBgTexClass();

    WorldHeightMapEdit *htMapEditCopy = pDoc->GetHeightMap()->duplicate();
    Bool didIt = false;
    Bool shiftKey = (0x8000 & ::GetAsyncKeyState(VK_SHIFT)) != 0;

    // Primary stroke
    applyFillAt(ndx, htMapEditCopy, shiftKey, didIt);

    // Mirror across X centre (left/right)
    if (m_enableMirror && (m_mirrorX || m_mirrorDiag))
    {
        CPoint mirrorX;
        mirrorX.x = htMapEditCopy->getXExtent() - 1 - ndx.x;
        mirrorX.y = ndx.y;
        if (m_mirrorX)
            applyFillAt(mirrorX, htMapEditCopy, shiftKey, didIt);

        // Mirror across Y centre (top/bottom)
        if (m_mirrorY)
        {
            CPoint mirrorY;
            mirrorY.x = ndx.x;
            mirrorY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyFillAt(mirrorY, htMapEditCopy, shiftKey, didIt);
        }

        // Diagonal: opposite corner
        if (m_mirrorDiag || (m_mirrorX && m_mirrorY))
        {
            CPoint mirrorXY;
            mirrorXY.x = htMapEditCopy->getXExtent() - 1 - ndx.x;
            mirrorXY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
            applyFillAt(mirrorXY, htMapEditCopy, shiftKey, didIt);
        }
    }
    else if (m_enableMirror && m_mirrorY)
    {
        CPoint mirrorY;
        mirrorY.x = ndx.x;
        mirrorY.y = htMapEditCopy->getYExtent() - 1 - ndx.y;
        applyFillAt(mirrorY, htMapEditCopy, shiftKey, didIt);
    }

    if (didIt) {
        htMapEditCopy->optimizeTiles();
        IRegion2D partialRange = {0, 0, 0, 0};
        pDoc->updateHeightMap(htMapEditCopy, false, partialRange);
        WBDocUndoable *pUndo = new WBDocUndoable(pDoc, htMapEditCopy);
        pDoc->AddAndDoUndoable(pUndo);
        REF_PTR_RELEASE(pUndo);
    }
    REF_PTR_RELEASE(htMapEditCopy);
}
