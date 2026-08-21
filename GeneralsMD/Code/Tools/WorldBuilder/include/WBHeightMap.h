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
#include <vector>

class MapObject;
class ThingTemplate;
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
	static void setShowPathfindObjects(Bool show) {m_showPathfindObjects = show; m_objectCellsDirty = true;}
	static Bool getShowPathfindObjects(void) {return m_showPathfindObjects;}
	/// Combined view: collapses the CHECKED layers into one color, answering "can a unit stand
	/// here" rather than showing each cause separately.  On its own it shows nothing -- it needs
	/// at least one of Cliff/Water/Objects ticked to have something to combine.
	static void setShowPassability(Bool show) {m_showPassability = show;}
	static Bool getShowPassability(void) {return m_showPassability;}
	static Bool anyPathfindOverlayOn(void)
		{return m_showPathfindCliff || m_showPathfindWater || m_showPathfindObjects;}
	/// The placed objects changed, so the obstacle cells need recomputing before the next tint.
	/// Also asks for a terrain refresh, since the tint only reapplies when the mesh rebuilds --
	/// but only while an overlay is actually on, and coalesced (this fires once PER OBJECT, so
	/// rebuilding here directly would mean one full terrain rebuild per band-selected object).
	static void invalidateObjectCells(void)
		{m_objectCellsDirty = true; if (anyPathfindOverlayOn()) {m_overlayRefreshPending = true;}}
	/// Has an overlay change asked for a terrain refresh since the last one was serviced?
	static Bool takeOverlayRefreshPending(void)
		{const Bool p = m_overlayRefreshPending; m_overlayRefreshPending = false; return p;}
	/// The terrain or water changed under an overlay, so it needs repainting.
	static void requestOverlayRefresh(void)
		{m_waterCellsDirty = true; if (anyPathfindOverlayOn()) {m_overlayRefreshPending = true;}}

	void setDrawEntireMap(Bool entire) {m_drawEntireMap = entire;};
	Bool getDrawEntireMap(void) {return m_drawEntireMap;};
	void setFlattenHeights(Bool flat);

protected:
	void flattenHeights(void);
protected:
	/// Walks the vertex-buffer tiles covering a block and tints each for the pathfind overlay.
	void applyPathfindTint(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap);
	/// Is this map cell inside a water polygon and below its surface?  Uncached -- walks the
	/// polygon list, so it is only for building the cache below, never for a per-cell test.
	Bool isWaterCell(Int cellX, Int cellY);
	/// Rebuilds the water cell set, if it went stale.
	void updateWaterCells(void);
	/// Cached water lookup, O(1).
	Bool isWaterCellCached(Int cellX, Int cellY) const;
	/// Rebuilds the obstacle cell set from the placed map objects, if it went stale.
	void updateObjectCells(void);
	/// Marks every cell covered by one object's footprint.
	void markObjectFootprint(const MapObject *pObj);
	/// Marks the cells one fence or wall segment blocks.
	void markFenceFootprint(const MapObject *pObj, const ThingTemplate *pTmpl);
	/// Is this map cell blocked by a placed object?
	Bool isObjectCell(Int cellX, Int cellY) const;
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
	static Bool m_showPathfindObjects;
	static Bool m_showPassability;

	// Obstacle cells, rebuilt from the map objects only when they change.  Walking every
	// object for every terrain cell would be far too slow, so the footprints are rasterized
	// once into this set and then looked up per cell.
	static Bool m_objectCellsDirty;
	static Bool m_overlayRefreshPending;
	static std::vector<bool> m_objectCells;
	static Int m_objectCellsWidth;
	static Int m_objectCellsHeight;

	// Water cells, cached for the same reason as the obstacle cells: the uncached test walks
	// every water polygon and samples the terrain height, and scrolling re-tints a strip of the
	// map every frame, so doing that per cell made panning stutter.
	static Bool m_waterCellsDirty;
	static std::vector<bool> m_waterCells;
	static Int m_waterCellsWidth;
	static Int m_waterCellsHeight;
};

#endif  // end __WBHEIGHTMAP_H_
