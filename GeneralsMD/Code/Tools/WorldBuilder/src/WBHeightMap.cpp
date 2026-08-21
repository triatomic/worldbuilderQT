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

// FILE: Heightmap.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//                                                                          
//                       Westwood Studios Pacific.                          
//                                                                          
//                       Confidential Information                           
//                Copyright (C) 2001 - All Rights Reserved                  
//                                                                          
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: Heightmap.cpp
//
// Created:   Mark W., John Ahlquist, April/May 2001
//
// Desc:      Draw the terrain and scorchmarks in a scene.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes                                                      
//-----------------------------------------------------------------------------
#include "WBHeightMap.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/ThingTemplate.h"
#include <tri.h>
#include <colmath.h>
#include <coltest.h>

#define dontUSE_FLAT_HEIGHT_MAP
//-----------------------------------------------------------------------------
//         Private Data                                                     
//-----------------------------------------------------------------------------

//=============================================================================
// WBHeightMap::WBHeightMap
//=============================================================================
WBHeightMap::WBHeightMap() :
	m_drawEntireMap(false),
	m_flattenHeights(false)
{
}

//=============================================================================
// WBHeightMap::Render
//=============================================================================
/** Renders (draws) the terrain. */
//=============================================================================
void WBHeightMap::setFlattenHeights(Bool flat)
{
	if (m_flattenHeights != flat) {
		m_flattenHeights = flat;
#ifndef USE_FLAT_HEIGHT_MAP
		m_originX = 0;
		m_originY = 0;
 		updateBlock(0, 0, m_x-1, m_y-1, m_map, NULL);
#endif
	}
}

// THE_Z is just above the water plane, so the flattened terrain doesn't draw
// under water.
#define THE_Z (10)
//=============================================================================
// WBHeightMap::flattenHeights
//=============================================================================
/** Flattens the terrain for the top down view.. */
//=============================================================================
void WBHeightMap::flattenHeights(void) {
#ifndef USE_FLAT_HEIGHT_MAP
	Real theZ = THE_Z;
	Int i, j;
	for (j=0; j<m_numVBTilesY; j++)
		for (i=0; i<m_numVBTilesX; i++)
		{
			static int count = 0;
			count++;
			Int numVertex = (VERTEX_BUFFER_TILE_LENGTH*2)*(VERTEX_BUFFER_TILE_LENGTH*2);
			DX8VertexBufferClass::WriteLockClass lockVtxBuffer(m_vertexBufferTiles[j*m_numVBTilesX+i]);
			VERTEX_FORMAT *vbHardware = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
			Int vtx;
			for (vtx=0; vtx<numVertex; vtx++) {
				vbHardware->z = theZ;
				vbHardware++;
			}
		}
#endif
}

//=============================================================================
// WBHeightMap::getMaxCellHeight
//=============================================================================
/** Returns maximum height of the 4 corners containing the given point */
//=============================================================================
Real WBHeightMap::getMaxCellHeight(Real x, Real y)
{
	if (!m_flattenHeights) {
		return BaseHeightMapRenderObjClass::getMaxCellHeight(x,y);
	}
	// If we are flattening the height, all z values aret THE_Z.  jba.
	return THE_Z;
}


//=============================================================================
// WBHeightMap::getHeight
//=============================================================================
/** return the height and normal of the triangle plane containing given location within heightmap. */
//=============================================================================
Real WBHeightMap::getHeightMapHeight(Real x, Real y, Coord3D* normal)
{
	if (!m_flattenHeights) {
		return BaseHeightMapRenderObjClass::getHeightMapHeight(x,y,normal);
	}
	// If we are flattening the height, all z values aret THE_Z.  jba.
	if (normal) {
		normal->x = 0;
		normal->y = 0;
		normal->z = 1;
	}
	return THE_Z;
}



//=============================================================================
// WBHeightMap::Cast_Ray
//=============================================================================
/** Return intersection of a ray with the heightmap mesh.

*/
//=============================================================================
Bool WBHeightMap::Cast_Ray(RayCollisionTestClass & raytest)
{
	if (!m_flattenHeights) {
		return BaseHeightMapRenderObjClass::Cast_Ray(raytest);
	}
	Real theZ = THE_Z;
	TriClass tri;
	Bool hit = false;
	Int X,Y;
	Vector3 normal,P0,P1,P2,P3;

	if (!m_map)
		return false;	//need valid pointer to heightmap samples
//	HeightSampleType *pData = m_map->getDataPtr();
	//Clip ray to extents of heightfield
	AABoxClass hbox;
	LineSegClass lineseg,lineseg2;
	CastResultStruct	result;
	Int StartCellX;
	Int EndCellX;
 	Int StartCellY;
	Int EndCellY;
	const Int overhang = 2*32; // Allow picking past the edge for scrolling & objects.
 	Vector3 minPt(MAP_XY_FACTOR*(-overhang), MAP_XY_FACTOR*(-overhang), -MAP_XY_FACTOR);
	Vector3 maxPt(MAP_XY_FACTOR*(m_map->getXExtent()+overhang), 
		MAP_XY_FACTOR*(m_map->getYExtent()+overhang), MAP_HEIGHT_SCALE*m_map->getMaxHeightValue()+MAP_XY_FACTOR);
	MinMaxAABoxClass mmbox(minPt, maxPt);
	hbox.Init(mmbox);

	lineseg=raytest.Ray;

	//Set initial ray endpoints
	P0 = raytest.Ray.Get_P0();
	P1 = raytest.Ray.Get_P1();
	result.ComputeContactPoint=true;

	Int p;
	for (p=0; p<3; p++) {
		//find intersection point of ray and terrain bounding box
		if (CollisionMath::Collide(lineseg,hbox,&result))
		{	//ray intersects terrain or starts inside the terrain.
			if (!result.StartBad)	//check if start point inside terrain
				P0 = result.ContactPoint;			//make intersection point the new start of the ray.

			//reverse direction of original ray and clip again to extent of
			//heightmap
			result.Fraction=1.0f;	//reset the result
			result.StartBad=false;
			lineseg2.Set(lineseg.Get_P1(),lineseg.Get_P0());	//reverse line segment
			if (CollisionMath::Collide(lineseg2,hbox,&result))
			{	if (!result.StartBad)	//check if end point inside terrain
					P1 = result.ContactPoint;	//make intersection point the new end pont of ray
			}
		} else {
			return(false);
		}

		// Take the 2D bounding box of ray and check heights
		// inside this box for intersection.
		if (P0.X > P1.X) {	//flip start/end points
			StartCellX = floor(P1.X/MAP_XY_FACTOR);
			EndCellX = ceil(P0.X/MAP_XY_FACTOR);
		}	else {
			StartCellX = floor(P0.X/MAP_XY_FACTOR);
			EndCellX = ceil(P1.X/MAP_XY_FACTOR);
		}
		if (P0.Y > P1.Y) {	//flip start/end points
			StartCellY=floor(P1.Y/MAP_XY_FACTOR);
			EndCellY=ceil(P0.Y/MAP_XY_FACTOR);
		}	else {
			StartCellY = floor(P0.Y/MAP_XY_FACTOR);
			EndCellY = ceil(P1.Y/MAP_XY_FACTOR);
		}

		Vector3 minPt(MAP_XY_FACTOR*(StartCellX-1), MAP_XY_FACTOR*(StartCellY-1), theZ-1);
		Vector3 maxPt(MAP_XY_FACTOR*(EndCellX+1), MAP_XY_FACTOR*(EndCellY+1), theZ+1);
		MinMaxAABoxClass mmbox(minPt, maxPt);
		hbox.Init(mmbox);
	}

	raytest.Result->ComputeContactPoint=true;	//tell CollisionMath that we need point.

	Int offset;
	for (offset = 1; offset < 5; offset *= 3) {
		for (Y=StartCellY-offset; Y<=EndCellY+offset; Y++) { 
			//if (Y<0) continue;
			//if (Y>=m_map->getYExtent()-1) continue;

			for (X=StartCellX-offset; X<=EndCellX+offset; X++) {
				//test the 2 triangles in this cell
				//	3-----2
				//  |    /|
				//  |  /  |
				//	|/    |
				//  0-----1

				//bottom triangle first
				P0.X=X*MAP_XY_FACTOR;
				P0.Y=Y*MAP_XY_FACTOR;
				P0.Z=THE_Z;

				P1.X=(X+1)*MAP_XY_FACTOR;
				P1.Y=Y*MAP_XY_FACTOR;
				P1.Z=THE_Z;

				P2.X=(X+1)*MAP_XY_FACTOR;
				P2.Y=(Y+1)*MAP_XY_FACTOR;
				P2.Z=THE_Z;

				P3.X=X*MAP_XY_FACTOR;
				P3.Y=(Y+1)*MAP_XY_FACTOR;
				P3.Z=THE_Z;


				tri.V[0] = &P0; 
				tri.V[1] = &P1;
				tri.V[2] = &P2;
				
				tri.N = &normal;

				tri.Compute_Normal();

				hit = hit | CollisionMath::Collide(raytest.Ray, tri, raytest.Result);

				if (raytest.Result->StartBad)
					return true;

				//top triangle
				tri.V[0] = &P2; 
				tri.V[1] = &P3;
				tri.V[2] = &P0;
				
				tri.N = &normal;

				tri.Compute_Normal();

				hit = hit | CollisionMath::Collide(raytest.Ray, tri, raytest.Result);

				if (hit)
					raytest.Result->SurfaceType = SURFACE_TYPE_DEFAULT;	///@todo: WW3D uses this to return dirt, grass, etc.  Do we need this?
			}
			if (hit) break;
		}
		if (hit) break;
	}
	return hit;
}


//=============================================================================
// WBHeightMap::Render
//=============================================================================
/** Renders (draws) the terrain. */
//=============================================================================
void WBHeightMap::Render(RenderInfoClass & rinfo)
{
	if (m_flattenHeights) {
		flattenHeights();
	}
#ifdef USE_FLAT_HEIGHT_MAP
	FlatHeightMapRenderObjClass::Render(rinfo);
#else
	HeightMapRenderObjClass::Render(rinfo);
#endif 
}




//=============================================================================
// WBHeightMap pathfind-cell overlay (Debug menu)
//=============================================================================
Bool WBHeightMap::m_showPathfindCliff = false;
Bool WBHeightMap::m_showPathfindWater = false;
Bool WBHeightMap::m_showPathfindObjects = false;
Bool WBHeightMap::m_showPassability = false;
Bool WBHeightMap::m_objectCellsDirty = true;
Bool WBHeightMap::m_overlayRefreshPending = false;
std::vector<bool> WBHeightMap::m_objectCells;
Int WBHeightMap::m_objectCellsWidth = 0;
Int WBHeightMap::m_objectCellsHeight = 0;

//=============================================================================
// WBHeightMap::updateBlock
//=============================================================================
/** Builds the terrain vertices normally, then tints them for the pathfind overlay. */
//=============================================================================
int WBHeightMap::updateBlock(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator)
{
#ifdef USE_FLAT_HEIGHT_MAP
	int result = FlatHeightMapRenderObjClass::updateBlock(x0, y0, x1, y1, pMap, pLightsIterator);
#else
	int result = HeightMapRenderObjClass::updateBlock(x0, y0, x1, y1, pMap, pLightsIterator);
#endif

	if (result == 0 && anyPathfindOverlayOn()) {
		if (m_showPathfindObjects) {
			updateObjectCells();
		}
		applyPathfindTint(x0, y0, x1, y1, pMap);
	}
	return result;
}

//=============================================================================
// WBHeightMap::applyPathfindTint
//=============================================================================
/** Recolors already-built terrain vertices to show pathfind cell classes.

	The cell data is whatever WorldBuilder already keeps up to date:
	  - cliff: WorldHeightMap's m_cellCliffState bitfield, computed from heights with the
	    pathfinder's own PATHFIND_CLIFF_SLOPE_LIMIT_F and refreshed on every terrain edit
	    (see WorldHeightMapEdit::setHeight), so this needs no AI subsystem.
	  - water: the polygon water areas, via getWaterHeightIfUnderwater.

	The tint masks the diffuse channel the same way the engine's impassable-areas overlay
	does, which keeps terrain shading visible through the color instead of flat-filling it.
*/
//=============================================================================
void WBHeightMap::applyPathfindTint(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap)
{
	if (pMap == NULL || m_vertexBufferTiles == NULL || m_vertexBufferBackup == NULL) {
		return;
	}

	Int i, j;
	for (j = 0; j < m_numVBTilesY; j++)
	{
		Int originY = j*VERTEX_BUFFER_TILE_LENGTH;
		Int yMin = originY;
		if (y0 > yMin) {
			yMin = y0;
		}
		Int yMax = originY+VERTEX_BUFFER_TILE_LENGTH;
		if (y1 < yMax) {
			yMax = y1;
		}
		if (yMin >= yMax) {
			continue;
		}
		for (i = 0; i < m_numVBTilesX; i++)
		{
			Int originX = i*VERTEX_BUFFER_TILE_LENGTH;
			Int xMin = originX;
			if (xMin < x0) {
				xMin = x0;
			}
			Int xMax = originX+VERTEX_BUFFER_TILE_LENGTH;
			if (xMax > x1) {
				xMax = x1;
			}
			if (xMin >= xMax) {
				continue;
			}

			DX8VertexBufferClass *pVB = *(m_vertexBufferTiles+j*m_numVBTilesX+i);
			char *pData = *(m_vertexBufferBackup+j*m_numVBTilesX+i);
			if (pVB == NULL || pData == NULL) {
				continue;
			}
			tintVBTile(pVB, pData, xMin, yMin, xMax, yMax, originX, originY, pMap);
		}
	}
}

//=============================================================================
// WBHeightMap::isWaterCell
//=============================================================================
/** Is this map cell underwater?

	Water comes from two places, and a cell counts as water if the HIGHEST of them covers the
	terrain (the same "tallest water wins" rule TerrainLogic::getWaterHandle uses):

	  - the water area polygons (rivers, lakes drawn in the editor)
	  - the global water level, which fills low ground on maps that don't draw a water area

	The polygons store their points in MAP CELL units, so the point-in-polygon test has to be
	done in cell space -- note getWaterHeightIfUnderwater() floors its arguments straight into
	pointInTrigger(), so passing it world coordinates silently tests the wrong place.  The water
	heights are world Zs, so they are compared against the terrain height sampled in world units.
*/
//=============================================================================
Bool WBHeightMap::isWaterCell(Int cellX, Int cellY)
{
	Bool haveWater = false;
	Real waterZ = 0.0f;

	if (TheGlobalData != NULL && TheGlobalData->m_waterPositionZ > 0.0f) {
		waterZ = TheGlobalData->m_waterPositionZ;
		haveWater = true;
	}

	// cellX/cellY are terrain cell indices, which include the heightmap's border ring; the
	// polygon points and world heights are in border-free map space, so take it back off.
	const Int borderSize = (m_map != NULL) ? m_map->getBorderSizeInline() : 0;
	const Int mapX = cellX - borderSize;
	const Int mapY = cellY - borderSize;

	ICoord3D iLoc;
	iLoc.x = mapX;
	iLoc.y = mapY;
	iLoc.z = 0;

	for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext())
	{
		if (!pTrig->isWaterArea()) {
			continue;
		}
		if (!pTrig->pointInTrigger(iLoc)) {
			continue;
		}
		const Real polyZ = pTrig->getPoint(0)->z;
		if (!haveWater || polyZ >= waterZ) {
			waterZ = polyZ;
			haveWater = true;
		}
	}

	if (!haveWater) {
		return false;
	}

	const Real terrainZ = getHeightMapHeight(mapX*MAP_XY_FACTOR, mapY*MAP_XY_FACTOR, NULL);
	return (terrainZ < waterZ);
}

//=============================================================================
// WBHeightMap::markFenceFootprint
//=============================================================================
/** Marks the cells one fence or wall segment blocks.

	Mirrors Pathfinder::classifyFence: a fence blocks a thin strip fenceWidth long and a tenth
	of a cell deep, shifted along its own axis by the template's fence X offset -- not the full
	object bounds, which would wall off far more than the fence actually does.
*/
//=============================================================================
void WBHeightMap::markFenceFootprint(const MapObject *pObj, const ThingTemplate *pTmpl)
{
	const Coord3D *pos = pObj->getLocation();
	if (pos == NULL) {
		return;
	}

	const Int borderSize = (m_map != NULL) ? m_map->getBorderSizeInline() : 0;

	const Real angle = pObj->getAngle();
	const Real halfsizeX = pTmpl->getFenceWidth()/2;
	const Real halfsizeY = MAP_XY_FACTOR/10.0f;
	const Real fenceOffset = pTmpl->getFenceXOffset();

	const Real c = (Real)Cos(angle);
	const Real s = (Real)Sin(angle);

	const Real STEP_SIZE = MAP_XY_FACTOR * 0.5f;
	const Real ydx = s * STEP_SIZE;
	const Real ydy = -c * STEP_SIZE;
	const Real xdx = c * STEP_SIZE;
	const Real xdy = s * STEP_SIZE;

	const Int numStepsX = REAL_TO_INT_CEIL(2.0f * halfsizeX / STEP_SIZE);
	const Int numStepsY = REAL_TO_INT_CEIL(2.0f * halfsizeY / STEP_SIZE);

	Real tl_x = pos->x - fenceOffset*c - halfsizeY*s;
	Real tl_y = pos->y + halfsizeY*c - fenceOffset*s;

	Int iy;
	for (iy = 0; iy < numStepsY; ++iy, tl_x += ydx, tl_y += ydy)
	{
		Real x = tl_x;
		Real y = tl_y;
		Int ix;
		for (ix = 0; ix < numStepsX; ++ix, x += xdx, y += xdy)
		{
			const Int cx = REAL_TO_INT_FLOOR((x + 0.5f)/MAP_XY_FACTOR) + borderSize;
			const Int cy = REAL_TO_INT_FLOOR((y + 0.5f)/MAP_XY_FACTOR) + borderSize;
			if (cx >= 0 && cy >= 0 && cx < m_objectCellsWidth && cy < m_objectCellsHeight) {
				m_objectCells[cx + cy*m_objectCellsWidth] = true;
			}
		}
	}
}

//=============================================================================
// WBHeightMap::isObjectCell
//=============================================================================
/** Is this map cell blocked by a placed object? */
//=============================================================================
Bool WBHeightMap::isObjectCell(Int cellX, Int cellY) const
{
	if (cellX < 0 || cellY < 0 || cellX >= m_objectCellsWidth || cellY >= m_objectCellsHeight) {
		return false;
	}
	if (m_objectCells.empty()) {
		return false;
	}
	return m_objectCells[cellX + cellY*m_objectCellsWidth];
}

//=============================================================================
// WBHeightMap::updateObjectCells
//=============================================================================
/** Rebuilds the obstacle cell set from the placed map objects.

	This is the editor-side equivalent of the game's Pathfinder::classifyObjectFootprint.  The
	game walks live Objects; WorldBuilder only has MapObjects and their ThingTemplates, so the
	tests that need a live object (isMobile, height above terrain) are approximated from the
	template instead -- see the notes at each check.

	Rasterizing every object once into a cell set (rather than testing objects per terrain cell)
	keeps this off the per-cell path, which runs for the whole map on every terrain rebuild.
*/
//=============================================================================
void WBHeightMap::updateObjectCells(void)
{
	if (!m_objectCellsDirty) {
		return;
	}
	m_objectCellsDirty = false;

	// Size the grid in pathfind cells.  MAP_XY_FACTOR == PATHFIND_CELL_SIZE_F, so terrain cells
	// and pathfind cells are the same size (the engine asserts this too).
	m_objectCellsWidth = 0;
	m_objectCellsHeight = 0;
	if (m_map != NULL) {
		m_objectCellsWidth = m_map->getXExtent();
		m_objectCellsHeight = m_map->getYExtent();
	}
	if (m_objectCellsWidth <= 0 || m_objectCellsHeight <= 0) {
		m_objectCells.clear();
		return;
	}

	m_objectCells.assign(m_objectCellsWidth*m_objectCellsHeight, false);

	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
	{
		markObjectFootprint(pObj);
	}
}

//=============================================================================
// WBHeightMap::markObjectFootprint
//=============================================================================
/** Marks the cells one object covers, following the game's obstacle rules. */
//=============================================================================
void WBHeightMap::markObjectFootprint(const MapObject *pObj)
{
	if (pObj == NULL || pObj->isWaypoint()) {
		return;
	}

	const ThingTemplate *pTmpl = pObj->getThingTemplate();
	if (pTmpl == NULL) {
		return;
	}

	// The same exclusions Pathfinder::classifyObjectFootprint applies.
	if (pTmpl->isKindOf(KINDOF_MINE) || pTmpl->isKindOf(KINDOF_PROJECTILE) ||
			pTmpl->isKindOf(KINDOF_BRIDGE_TOWER)) {
		return;
	}

	// Fences block movement through a thin strip rather than their whole bounds, and the game
	// classifies them before the structure test -- a fence is usually not a KINDOF_STRUCTURE,
	// so it would otherwise be dropped.  Defensive walls deliberately fall through to the
	// normal footprint path below, exactly as classifyObjectFootprint does.
	if (pTmpl->getFenceWidth() > 0.0f && !pTmpl->isKindOf(KINDOF_DEFENSIVE_WALL))
	{
		markFenceFootprint(pObj, pTmpl);
		return;
	}
	// Only structures are pathed around...
	if (!pTmpl->isKindOf(KINDOF_STRUCTURE)) {
		return;
	}
	// ...and only the ones that can't move.  The game asks the live object (isMobile); the
	// template equivalent is the IMMOBILE kind-of flag.
	if (!pTmpl->isKindOf(KINDOF_IMMOBILE)) {
		return;
	}
	// Small objects are never obstacles.
	const GeometryInfo &geom = pTmpl->getTemplateGeometryInfo();
	if (geom.getIsSmall()) {
		return;
	}

	const Coord3D *pos = pObj->getLocation();
	if (pos == NULL) {
		return;
	}

	// Object positions are world coordinates, but the cell grid is indexed the way the terrain
	// is -- including the heightmap's border ring (see isCliffCell, which adds the same offset,
	// and ADJUST_FROM_INDEX_TO_REAL, which subtracts it going the other way).  Without this the
	// whole overlay lands shifted off its objects.
	const Int borderSize = (m_map != NULL) ? m_map->getBorderSizeInline() : 0;

	switch (geom.getGeomType())
	{
		case GEOMETRY_BOX:
		{
			const Real angle = pObj->getAngle();
			const Real halfsizeX = geom.getMajorRadius();
			const Real halfsizeY = geom.getMinorRadius();

			const Real c = (Real)Cos(angle);
			const Real s = (Real)Sin(angle);

			// Half a cell, matching the game: a full cell step aliases badly on rotated boxes.
			const Real STEP_SIZE = MAP_XY_FACTOR * 0.5f;
			const Real ydx = s * STEP_SIZE;
			const Real ydy = -c * STEP_SIZE;
			const Real xdx = c * STEP_SIZE;
			const Real xdy = s * STEP_SIZE;

			const Int numStepsX = REAL_TO_INT_CEIL(2.0f * halfsizeX / STEP_SIZE);
			const Int numStepsY = REAL_TO_INT_CEIL(2.0f * halfsizeY / STEP_SIZE);

			Real tl_x = pos->x - halfsizeX*c - halfsizeY*s;
			Real tl_y = pos->y + halfsizeY*c - halfsizeX*s;

			Int iy;
			for (iy = 0; iy < numStepsY; ++iy, tl_x += ydx, tl_y += ydy)
			{
				Real x = tl_x;
				Real y = tl_y;
				Int ix;
				for (ix = 0; ix < numStepsX; ++ix, x += xdx, y += xdy)
				{
					const Int cx = REAL_TO_INT_FLOOR((x + 0.5f)/MAP_XY_FACTOR) + borderSize;
					const Int cy = REAL_TO_INT_FLOOR((y + 0.5f)/MAP_XY_FACTOR) + borderSize;
					if (cx >= 0 && cy >= 0 && cx < m_objectCellsWidth && cy < m_objectCellsHeight) {
						m_objectCells[cx + cy*m_objectCellsWidth] = true;
					}
				}
			}
			break;
		}

		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			const Real radius = geom.getMajorRadius();
			Real size = radius/MAP_XY_FACTOR;
			const Real centerX = pos->x/MAP_XY_FACTOR;
			const Real centerY = pos->y/MAP_XY_FACTOR;

			const Int topLeftX = REAL_TO_INT_FLOOR(0.5f + (pos->x - radius)/MAP_XY_FACTOR)-1;
			const Int topLeftY = REAL_TO_INT_FLOOR(0.5f + (pos->y - radius)/MAP_XY_FACTOR)-1;

			size += 0.4f;
			const Real r2 = size*size;

			const Int bottomRightX = topLeftX + REAL_TO_INT_CEIL(2*size) + 2;
			const Int bottomRightY = topLeftY + REAL_TO_INT_CEIL(2*size) + 2;

			Int j;
			for (j = topLeftY; j < bottomRightY; j++)
			{
				Int i;
				for (i = topLeftX; i < bottomRightX; i++)
				{
					const Real dx = i+0.5f - centerX;
					const Real dy = j+0.5f - centerY;
					if (dx*dx + dy*dy <= r2)
					{
						const Int cx = i + borderSize;
						const Int cy = j + borderSize;
						if (cx >= 0 && cy >= 0 && cx < m_objectCellsWidth && cy < m_objectCellsHeight) {
							m_objectCells[cx + cy*m_objectCellsWidth] = true;
						}
					}
				}
			}
			break;
		}

		default:
			break;
	}
}

//=============================================================================
// WBHeightMap::tintVBTile
//=============================================================================
/** Recolors one vertex-buffer tile's cells for the pathfind overlay.

	The row/vertex indexing here mirrors HeightMapRenderObjClass::updateVB so the same cell
	maps to the same 4 vertices; the base class has already filled both the backup buffer and
	the hardware buffer, so this only rewrites the diffuse color and copies the touched cells.
*/
//=============================================================================
void WBHeightMap::tintVBTile(DX8VertexBufferClass *pVB, char *data, Int x0, Int y0, Int x1, Int y1,
														 Int originX, Int originY, WorldHeightMap *pMap)
{
	const Int vertsPerRow = (VERTEX_BUFFER_TILE_LENGTH)*4;
	// Mirrors the file-local HALF_RES_MESH in the engine's HeightMap.cpp, which the vertex
	// layout below has to agree with.  It is a compile-time constant false there.
	const Bool halfResMesh = false;
	const Int cellOffset = halfResMesh ? 2 : 1;

	DX8VertexBufferClass::WriteLockClass lockVtxBuffer(pVB);
	VERTEX_FORMAT *vbHardware = (VERTEX_FORMAT*)lockVtxBuffer.Get_Vertex_Array();
	VERTEX_FORMAT *vBase = (VERTEX_FORMAT*)data;

	Int i, j;
	for (j = y0; j < y1; j++)
	{
		VERTEX_FORMAT *vb = vBase;
		if (halfResMesh) {
			if (j&1) {
				continue;
			}
			vb += ((j-originY)/2)*vertsPerRow/2;
			vb += ((x0-originX)/2)*4;
		} else {
			vb += (j-originY)*vertsPerRow;
			vb += (x0-originX)*4;
		}

		for (i = x0; i < x1; i += cellOffset)
		{
			// Cell indices in the same space getCliffState() expects (see BaseHeightMap's
			// isCliffCell, which adds the draw origin the same way).
			const Int cellX = getXWithOrigin(i) + pMap->getDrawOrgX();
			const Int cellY = getYWithOrigin(j) + pMap->getDrawOrgY();

			// Passability answers one question -- can a unit stand here -- by collapsing the
			// checked layers into a single color.  It only ever considers the layers that are
			// actually ticked, so Cliff+Passability shows terrain blocking alone; ticking
			// Objects as well folds those in.  Without Passability the layers stay diagnostic
			// and keep their own colors, so you can see WHY a cell is blocked.
			const Bool wantCliff = m_showPathfindCliff;
			const Bool wantWater = m_showPathfindWater;
			const Bool wantObject = m_showPathfindObjects;

			Bool isCliff = false;
			if (wantCliff) {
				isCliff = pMap->getCliffState(cellX, cellY);
			}

			Bool isWater = false;
			if (wantWater) {
				isWater = isWaterCell(cellX, cellY);
			}

			Bool isObject = false;
			if (wantObject) {
				isObject = isObjectCell(cellX, cellY);
			}

			if (isCliff || isWater || isObject) {
				UnsignedInt mask;
				if (m_showPassability) {
					mask = 0xFFFF0000;	// red with alpha -- blocked, whatever the reason.
				} else if (isObject) {
					// Priority matches the game's cell classification: an object footprint
					// overrides the terrain underneath it, and a cliff outranks water.
					mask = 0xFFFFFF00;	// yellow with alpha.
				} else if (isCliff) {
					mask = 0xFFFF0000;	// red with alpha.
				} else {
					mask = 0xFF0000FF;	// blue with alpha.
				}
				Int k;
				for (k = 0; k < 4; k++) {
					vb[k].diffuse &= mask;
				}
				const Int offset = vb - vBase;
				memcpy(vbHardware+offset, vb, 4*sizeof(VERTEX_FORMAT));
			}
			vb += 4;
		}
	}
}
