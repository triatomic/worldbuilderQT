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


#ifndef __WBHEIGHTMAP_H_
#define __WBHEIGHTMAP_H_

#include "W3DDevice/GameClient/FlatHeightMap.h"	
#include "W3DDevice/GameClient/HeightMap.h"	
#define dont_USE_FLAT_HEIGHT_MAP // Use the origina height map for mission disk. jba. [4/15/2003]
#ifdef USE_FLAT_HEIGHT_MAP
class WBHeightMap : public FlatHeightMapRenderObjClass
#else
class WBHeightMap : public HeightMapRenderObjClass
#endif	
{	

public:
	WBHeightMap(void);

	/////////////////////////////////////////////////////////////////////////////
	// Render Object Interface (W3D methods)
	/////////////////////////////////////////////////////////////////////////////
	virtual void					Render(RenderInfoClass & rinfo);
	virtual Bool					Cast_Ray(RayCollisionTestClass & raytest);

	virtual Real getHeightMapHeight(Real x, Real y, Coord3D* normal);	///<return height and normal at given point
	virtual Real getMaxCellHeight(Real x, Real y);	///< returns maximum height of the 4 cell corners.

	/// Rebuilds a block of terrain vertices, then applies the Debug menu's pathfind overlay
	/// tint on top of whatever the base class produced.  The tint lives here (rather than in
	/// the shared engine's updateVB) so this stays WorldBuilder-only code.
	virtual int updateBlock(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator);

	/// Debug menu pathfind-cell overlay.  Each flag tints the terrain for one cell class:
	/// cliff (impassable slope) red, water blue.  These read data WorldBuilder already
	/// maintains, so no AI/pathfinder subsystem is needed.
	static void setShowPathfindCliff(Bool show) {m_showPathfindCliff = show;}
	static Bool getShowPathfindCliff(void) {return m_showPathfindCliff;}
	static void setShowPathfindWater(Bool show) {m_showPathfindWater = show;}
	static Bool getShowPathfindWater(void) {return m_showPathfindWater;}
	static Bool anyPathfindOverlayOn(void) {return m_showPathfindCliff || m_showPathfindWater;}

	void setDrawEntireMap(Bool entire) {m_drawEntireMap = entire;};
	Bool getDrawEntireMap(void) {return m_drawEntireMap;};
	void setFlattenHeights(Bool flat);

protected:
	void flattenHeights(void);
protected:
	/// Walks the vertex-buffer tiles covering a block and tints each for the pathfind overlay.
	void applyPathfindTint(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap);
	/// Is this map cell inside a water polygon and below its surface?
	Bool isWaterCell(Int cellX, Int cellY);
	/// Tints the vertices of one already-built vertex-buffer tile.
	void tintVBTile(DX8VertexBufferClass *pVB, char *data, Int x0, Int y0, Int x1, Int y1,
									Int originX, Int originY, WorldHeightMap *pMap);

protected:
	Bool m_drawEntireMap;
	Bool m_flattenHeights;

	// Static so the menu handlers can toggle the overlay without holding a heightmap pointer;
	// there is only ever one terrain render object in WorldBuilder.
	static Bool m_showPathfindCliff;
	static Bool m_showPathfindWater;
};

#endif  // end __WBHEIGHTMAP_H_
