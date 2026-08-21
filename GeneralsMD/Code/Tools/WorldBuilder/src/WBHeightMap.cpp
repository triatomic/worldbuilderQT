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

	ICoord3D iLoc;
	iLoc.x = cellX;
	iLoc.y = cellY;
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

	const Real terrainZ = getHeightMapHeight(cellX*MAP_XY_FACTOR, cellY*MAP_XY_FACTOR, NULL);
	return (terrainZ < waterZ);
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

			Bool isCliff = false;
			if (m_showPathfindCliff) {
				isCliff = pMap->getCliffState(cellX, cellY);
			}

			Bool isWater = false;
			if (m_showPathfindWater) {
				isWater = isWaterCell(cellX, cellY);
			}

			if (isCliff || isWater) {
				// Water is drawn under cliff so an underwater cliff still reads as impassable.
				const UnsignedInt mask = isCliff ? 0xFFFF0000 : 0xFF0000FF;
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
