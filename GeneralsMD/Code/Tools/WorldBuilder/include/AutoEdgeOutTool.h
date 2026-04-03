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

// AutoEdgeOutTool.h
// Texture tiling tools for worldbuilder.
// Author: John Ahlquist, April 2001

#pragma once

#ifndef AUTOEDGEOUTTOOL_H
#define AUTOEDGEOUTTOOL_H

#include "Tool.h"
class WorldHeightMapEdit;
/*************************************************************************/
/**                             AutoEdgeOutTool
	 Does the BlendEdgesOut tool operation. 
***************************************************************************/
///  Blend edges out tool.
class AutoEdgeOutTool : public Tool 
{
protected:
	static Bool m_autoEdgeToolActive;
	static Bool m_enableMirror;
	static Bool m_mirrorX;   // left/right
    static Bool m_mirrorY;   // top/bottom
    static Bool m_mirrorDiag; // diagonal only (XY corner)

public:
	AutoEdgeOutTool(void);
	~AutoEdgeOutTool(void);

	static void toggleMirror() { m_enableMirror = !m_enableMirror; }
	static void toggleMirrorX() { m_mirrorX = !m_mirrorX; }
	static void toggleMirrorY() { m_mirrorY = !m_mirrorY; }
	static void toggleMirrorXY() { m_mirrorDiag = !m_mirrorDiag; }

public:
	void applyEdgeAt(CPoint pt, WorldHeightMapEdit* htMapEditCopy, Bool shiftKey);
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	/// Perform tool on mouse down.
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void activate(); ///< Become the current tool.
	virtual void deactivate();

	static Bool isActive(void) {return m_autoEdgeToolActive; }
};


#endif //TOOL_H
