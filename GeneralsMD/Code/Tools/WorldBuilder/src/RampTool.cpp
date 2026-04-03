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

// FILE: RampTool.cpp 
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  RampTool.cpp                                                  */
/* Created:    John K. McDonald, Jr., 4/19/2002                              */
/* Desc:       // Ramp tool implementation                                   */
/* Revision History:                                                         */
/*		4/19/2002 : Initial creation                                           */
/*---------------------------------------------------------------------------*/
#include "StdAfx.h"
#include "RampTool.h"

#include "CUndoable.h"
#include "MainFrm.h"
#include "DrawObject.h"
#include "RampOptions.h"
#include "resource.h"
#include "wbview.h"
#include "WHeightMapEdit.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"

#include "GameClient/Line2D.h"


#include "W3DDevice/GameClient/HeightMap.h"

// ---------------------------------------------------------------------------
// Static mirror state (matches the pattern used by AutoEdgeOutTool)
// ---------------------------------------------------------------------------
Bool RampTool::m_enableMirror;
Bool RampTool::m_mirrorX;   // left / right
Bool RampTool::m_mirrorY;   // top  / bottom
Bool RampTool::m_mirrorDiag;   // diagonal-only (XY corner)

// ---------------------------------------------------------------------------

RampTool::RampTool() : Tool(ID_RAMPTOOL, IDC_RAMP )
{

}

void RampTool::activate()
{
	Tool::activate();
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_RAMP_OPTIONS);

	mIsMouseDown = false;
}

void RampTool::deactivate()
{
	DrawObject::setDoRampFeedback(false);
	mIsMouseDown = false;
}

Bool RampTool::followsTerrain(void)
{
	return true;
}

void RampTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (!((m == TRACK_L) && mIsMouseDown)) {
		if (TheRampOptions->shouldApplyTheRamp()) {
			// Call me now for your free ramp application!
			applyRamp(pDoc);
			return;
		}
	} else if (m == TRACK_L) {
		Coord3D docPt;
		pView->viewToDocCoords(viewPt, &docPt);
		docPt.z = TheTerrainRenderObject->getHeightMapHeight(docPt.x, docPt.y, NULL);
		mEndPoint = docPt;
	}

	drawFeedback(&mEndPoint);

	pView->Invalidate();
	pDoc->updateAllViews();
}

void RampTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) {
		return;
	}

	Coord3D docPt;
	pView->viewToDocCoords(viewPt, &docPt);
	mStartPoint = docPt;
	mStartPoint.z = TheTerrainRenderObject->getHeightMapHeight(mStartPoint.x, mStartPoint.y, NULL);

	mIsMouseDown = true;
}

void RampTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{

	if (!((m == TRACK_L) && mIsMouseDown)) {
		return;
	}

	Coord3D docPt;
	pView->viewToDocCoords(viewPt, &docPt);
	mEndPoint = docPt;
	mEndPoint.z = TheTerrainRenderObject->getHeightMapHeight(mEndPoint.x, mEndPoint.y, NULL);

	mIsMouseDown = false;
}

void RampTool::drawFeedback(Coord3D* endPoint)
{
	DrawObject::setDoRampFeedback(true);
	DrawObject::setRampFeedbackParms(&mStartPoint, endPoint, TheRampOptions->getRampWidth());
}

// ---------------------------------------------------------------------------
/** Helper: mirror a world-space X coordinate about the map's centre.
 *  mapWidth is the cell extent in the X direction. */
static Real mirrorWorldX(Real x, Int mapXExtent)
{
	// World coords run from 0 to mapXExtent * cellSize; mirror about the midpoint.
	// We work in cell-index space for simplicity: cell = x / MAP_XY_FACTOR (or
	// whatever the engine uses).  Since all we need is the reflected world
	// position we can mirror symmetrically about the map half-width.
	Real mapWidth = (Real)(mapXExtent - 1);   // cells wide
	return mapWidth - x;
}

/** Helper: mirror a world-space Y coordinate about the map's centre. */
static Real mirrorWorldY(Real y, Int mapYExtent)
{
	Real mapHeight = (Real)(mapYExtent - 1);
	return mapHeight - y;
}

// ---------------------------------------------------------------------------
/** Apply a single ramp stroke defined by [start, end] into worldHeightDup. */
void RampTool::applyRampStroke(
    CWorldBuilderDoc*   pDoc,
    WorldHeightMapEdit* worldHeightDup,
    const Coord3D&      start,
    const Coord3D&      end)
{
	Coord3D bl, tl, br, tr;
	VecHeightMapIndexes indices;

	Real width = TheRampOptions->getRampWidth();

	BuildRectFromSegmentAndWidth(
		const_cast<Coord3D*>(&start),
		const_cast<Coord3D*>(&end),
		width, &bl, &tl, &br, &tr);

	if (bl == br || bl == tl) {
		return;
	}

	getAllIndexesIn(&bl, &br, &tl, &tr, 0, pDoc, &indices);
	int indiceCount = indices.size();
	if (indiceCount == 0) {
		return;
	}

	for (int i = 0; i < indiceCount; ++i) {
		Coord3D pt;
		pDoc->getCoordFromCellIndex(indices[i], &pt);

		Real uVal;
		Coord2D seg0 = { start.x, start.y };
		Coord2D seg1 = { end.x,   end.y   };
		Coord2D pt2D = { pt.x,    pt.y    };

		ShortestDistancePointToSegment2D(&seg0, &seg1, &pt2D, NULL, NULL, &uVal);
		Real height = start.z + uVal * (end.z - start.z);

		worldHeightDup->setHeight(
			indices[i].x, indices[i].y,
			(UnsignedByte)(height / MAP_HEIGHT_SCALE));
	}
}

// ---------------------------------------------------------------------------
void RampTool::applyRamp(CWorldBuilderDoc* pDoc)
{
	WorldHeightMapEdit *worldHeightDup = pDoc->GetHeightMap()->duplicate();
	// DEBUG_LOG(("Mirror Toggle - enable:%d  X:%d  Y:%d  Diag:%d",
	// 	m_enableMirror,
	// 	m_mirrorX,
	// 	m_mirrorY,
	// 	m_mirrorDiag));

	// --- Primary stroke ---
	applyRampStroke(pDoc, worldHeightDup, mStartPoint, mEndPoint);

	// --- Mirror strokes --------------------------------------------------
	if (m_enableMirror)
	{
		Int xExt = worldHeightDup->getXExtent();
		Int yExt = worldHeightDup->getYExtent();

		// Convert world endpoints to cell indices
		CPoint startNdx, endNdx;
		pDoc->getCellIndexFromCoord(mStartPoint, &startNdx);
		pDoc->getCellIndexFromCoord(mEndPoint,   &endNdx);

		// Mirror the indices exactly like BrushTool does
		CPoint mxStart, mxEnd, myStart, myEnd, mxyStart, mxyEnd;
		mxStart.x  = xExt - 1 - startNdx.x;  mxStart.y  = startNdx.y;
		mxEnd.x    = xExt - 1 - endNdx.x;    mxEnd.y    = endNdx.y;
		myStart.x  = startNdx.x;              myStart.y  = yExt - 1 - startNdx.y;
		myEnd.x    = endNdx.x;                myEnd.y    = yExt - 1 - endNdx.y;
		mxyStart.x = xExt - 1 - startNdx.x;  mxyStart.y = yExt - 1 - startNdx.y;
		mxyEnd.x   = xExt - 1 - endNdx.x;    mxyEnd.y   = yExt - 1 - endNdx.y;

		// Convert mirrored indices back to world coords
		Coord3D wmxStart, wmxEnd, wmyStart, wmyEnd, wmxyStart, wmxyEnd;
		pDoc->getCoordFromCellIndex(mxStart,  &wmxStart);
		pDoc->getCoordFromCellIndex(mxEnd,    &wmxEnd);
		pDoc->getCoordFromCellIndex(myStart,  &wmyStart);
		pDoc->getCoordFromCellIndex(myEnd,    &wmyEnd);
		pDoc->getCoordFromCellIndex(mxyStart, &wmxyStart);
		pDoc->getCoordFromCellIndex(mxyEnd,   &wmxyEnd);

		// Preserve Z (height) from the original endpoints
		wmxStart.z  = mStartPoint.z;  wmxEnd.z  = mEndPoint.z;
		wmyStart.z  = mStartPoint.z;  wmyEnd.z  = mEndPoint.z;
		wmxyStart.z = mStartPoint.z;  wmxyEnd.z = mEndPoint.z;

		if (m_mirrorX)
			applyRampStroke(pDoc, worldHeightDup, wmxStart, wmxEnd);

		if (m_mirrorY)
			applyRampStroke(pDoc, worldHeightDup, wmyStart, wmyEnd);

		if (m_mirrorDiag || (m_mirrorX && m_mirrorY))
			applyRampStroke(pDoc, worldHeightDup, wmxyStart, wmxyEnd);
	}
	// ---------------------------------------------------------------------

	IRegion2D partialRange = {0,0,0,0};
	pDoc->updateHeightMap(worldHeightDup, false, partialRange);

	WBDocUndoable *pUndo = new WBDocUndoable(pDoc, worldHeightDup);
	pDoc->AddAndDoUndoable(pUndo);
	REF_PTR_RELEASE(pUndo);          // belongs to pDoc now.
	REF_PTR_RELEASE(worldHeightDup);

	// Once we've applied the ramp, it's no longer a mutable thing, so blow away the feedback
	DrawObject::setDoRampFeedback(false);
}