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

// FeatherTool.h
// Texture tiling tools for worldbuilder.
// Author: John Ahlquist, April 2001

#pragma once

#ifndef FEATHERTOOL_H
#define FEATHERTOOL_H

#include "Tool.h"
class WorldHeightMapEdit;
/**************************************************************************/
/**                             FeatherTool
	 Does the smooth height map tool operation. 
***************************************************************************/
///  smooth height map tool.
class FeatherTool : public Tool 
{
protected:
	WorldHeightMapEdit *m_htMapEditCopy; //< ref counted.
	WorldHeightMapEdit *m_htMapFeatherCopy; //< ref counted.
	WorldHeightMapEdit *m_htMapRateCopy; //< ref counted.

	static Int m_feather;
	static Int m_rate;
	static Int m_radius;

	static Bool m_enableMirror;
	static Bool m_mirrorX;   // left/right
	static Bool m_mirrorY;   // top/bottom
	static Bool m_mirrorDiag; // diagonal only (XY corner)
public:
	FeatherTool(void);
	~FeatherTool(void);

	static Int getFeather(void) {return m_feather;}; ///<Returns feather (the brush size).
	static void setFeather(Int feather);
	static void setRate(Int rate);
	static void setRadius(Int Radius);

	static void toggleMirror() { m_enableMirror = !m_enableMirror; }
	static void toggleMirrorX() { m_mirrorX = !m_mirrorX; }
	static void toggleMirrorY() { m_mirrorY = !m_mirrorY; }
	static void toggleMirrorXY() { m_mirrorDiag = !m_mirrorDiag; }

public:
	void applyFeatherAt(CPoint ndx, IRegion2D& partialRange, Bool& redoRate);
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual WorldHeightMapEdit *getHeightMap(void) {return m_htMapEditCopy;};
	virtual void activate(); ///< Become the current tool.
};


#endif //FEATHERTOOL_H
