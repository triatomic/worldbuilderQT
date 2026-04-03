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

// BrushTool.h
// Texture tiling tools for worldbuilder.
// Author: John Ahlquist, April 2001

#pragma once

#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "Tool.h"
class WorldHeightMapEdit;
/*************************************************************************/
/**                             BrushTool
	 Does the Height Brush tool operation. 
***************************************************************************/
///  Height brush tool.
class BrushTool : public Tool 
{
protected:
	WorldHeightMapEdit *m_htMapEditCopy; ///< ref counted.
	WorldHeightMapEdit *m_htMapFeatherCopy; ///< ref counted.

	static Int m_brushWidth;
	static Int m_brushFeather;
	static Bool m_brushSquare;
	static Int m_brushHeight;

	static Bool m_enableMirror;
	static Bool m_mirrorX;   // left/right
    static Bool m_mirrorY;   // top/bottom
    static Bool m_mirrorDiag; // diagonal only (XY corner)


public:
	BrushTool(void);
	~BrushTool(void);

public:
	static Int getWidth(void) {return m_brushWidth;};  ///<Returns width.
	static Int getFeather(void) {return m_brushFeather;}; ///<Returns feather.
	static Int getHeight(void) {return m_brushHeight;}; ///<Returns height.
	static void setWidth(Int width);
	static void setFeather(Int feather);
	static void setHeight(Int height);

	static void toggleMirror() { m_enableMirror = !m_enableMirror; }
	static void toggleMirrorX() { m_mirrorX = !m_mirrorX; }
	static void toggleMirrorY() { m_mirrorY = !m_mirrorY; }
	static void toggleMirrorXY() { m_mirrorDiag = !m_mirrorDiag; }

public:
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	void applyBrushAt(CPoint ndx, WorldHeightMapEdit* pDoc_htMap, IRegion2D& partialRange);
	virtual WorldHeightMapEdit *getHeightMap(void) {return m_htMapEditCopy;};
	virtual void activate(); ///< Become the current tool.
	virtual Bool followsTerrain(void) {return false;};

};


#endif //BRUSHTOOL_H
