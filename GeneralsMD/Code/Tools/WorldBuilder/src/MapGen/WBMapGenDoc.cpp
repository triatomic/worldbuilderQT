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

// WBMapGenDoc.cpp
// Applies a generated map to the WorldBuilder document.

#include "StdAfx.h"
#include "MapGen/WBMapGenDoc.h"
#include "MapGen/WBMapGenerator.h"

#include "MapGen/WBMapGenAssets.h"
#include "MapGen/WBPerlinNoise.h"

#include "WorldBuilderDoc.h"
#include "WHeightMapEdit.h"
#include "CUndoable.h"
#include "WbView3d.h"
#include "WaypointOptions.h"
#include "ObjectOptions.h"
#include "mapobjectprops.h"
#include "WorldBuilder.h"
#include "Common/MapObject.h"
#include "Common/WellKnownKeys.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"

// Defined in GroveTool.cpp -- the placement checks the grove scatter already uses.
extern Bool localHasWaterAreas(void);
extern Bool localIsUnderwater(Real x, Real y);
extern Bool localIsInsideMapObject(Real x, Real y);

/// Keep well clear of the hard object limit: a generated map should leave a
/// mapper plenty of room to place their own things afterwards.
static const Int MAX_GENERATED_PROPS = 1200;

//=============================================================================
// buildStartWaypoints
//=============================================================================
/** Makes the Player_N_Start waypoints, chained together.

	Returns the head of the chain, or NULL if there was nothing to add. The
	caller hands the chain to one AddObjectUndoable, which walks it -- so every
	waypoint is added and removed as a single step.
*/
//=============================================================================
static MapObject *buildStartWaypoints(CWorldBuilderDoc *pDoc,
																			const std::vector<WBMapGenPoint> &starts,
																			Int border)
{
	// The names the game looks for. Player starts are ordinary waypoints whose
	// name is exactly one of these, so they must not go through the usual
	// "Waypoint N" unique-name generator.
	static const NameKeyType startKeys[8] =
	{
		TheKey_Player_1_Start,
		TheKey_Player_2_Start,
		TheKey_Player_3_Start,
		TheKey_Player_4_Start,
		TheKey_Player_5_Start,
		TheKey_Player_6_Start,
		TheKey_Player_7_Start,
		TheKey_Player_8_Start
	};

	MapObject *pHead = NULL;

	Int i;
	for (i = (Int)starts.size() - 1; i >= 0; i--)
	{
		if (i >= 8)
		{
			continue;	// the game only knows about eight players
		}

		// Cell indices include the border; world coordinates don't.
		Coord3D loc;
		loc.x = (Real)(starts[i].m_x - border) * MAP_XY_FACTOR;
		loc.y = (Real)(starts[i].m_y - border) * MAP_XY_FACTOR;
		loc.z = 0.0f;

		MapObject *pNew = newInstance(MapObject)(loc, AsciiString("*Waypoints/Waypoint"),
																						0.0f, 0, NULL, NULL);
		const Int id = pDoc->getNextWaypointID();
		pNew->setIsWaypoint();
		pNew->setWaypointID(id);
		pNew->setWaypointName(AsciiString(TheNameKeyGenerator->keyToName(startKeys[i]).str()));
		pNew->getProperties()->setAsciiString(TheKey_originalOwner, AsciiString("team"));

		pNew->setNextMap(pHead);
		pHead = pNew;
	}

	return pHead;
}

//=============================================================================
// paintTextures
//=============================================================================
/** Paints cliff faces and open ground with different textures.

	Which texture a cell gets is decided by the cliff flags the slope limiter left
	behind, not by re-measuring the slope here -- so what you see matches what the
	game treats as impassable.
*/
//=============================================================================
static void paintTextures(WorldHeightMapEdit *pMap, const WBMapGenHeightField &field,
													const WBMapGenAssets &assets)
{
	const Int ground = assets.getGroundTexture();
	const Int cliff = assets.getCliffTexture();
	if (ground < 0)
	{
		return;	// nothing usable loaded; leave the map's own texturing alone
	}

	Int j;
	for (j = 0; j < field.getHeight(); j++)
	{
		Int i;
		for (i = 0; i < field.getWidth(); i++)
		{
			Int texture = ground;
			if (cliff >= 0 && field.hasFlag(i, j, WBMapGenHeightField::FLAG_CLIFF))
			{
				texture = cliff;
			}
			pMap->setTextureClass(i, j, texture);
		}
	}

	// Bulk tile painting leaves the tile table unoptimized; the tile tools do the
	// same call after their own edits.
	pMap->optimizeTiles();
}

//=============================================================================
// makeProp
//=============================================================================
/** Creates one scattered object, or NULL if the name isn't placeable. */
//=============================================================================
static MapObject *makeProp(const AsciiString &name, Real worldX, Real worldY, Real angle)
{
	MapObject *pTemplateObj = ObjectOptions::getObjectNamed(name);
	if (pTemplateObj == NULL)
	{
		return NULL;
	}

	Coord3D loc;
	loc.x = worldX;
	loc.y = worldY;
	loc.z = 0.0f;

	MapObject *pNew = newInstance(MapObject)(loc, pTemplateObj->getName(), angle, 0, NULL,
																					pTemplateObj->getThingTemplate());
	pNew->getProperties()->setAsciiString(TheKey_originalOwner,
																			 AsciiString(NEUTRAL_TEAM_INTERNAL_STR));
	return pNew;
}

//=============================================================================
// canPlaceProp
//=============================================================================
/** Is this a sensible spot for a tree or rock?

	Reuses the same checks the grove tool applies when a mapper scatters trees by
	hand, so generated props land where hand-placed ones would.
*/
//=============================================================================
static Bool canPlaceProp(const WBMapGenHeightField &field, Int cellX, Int cellY,
												 Real worldX, Real worldY, Bool haveWater)
{
	if (!field.isValid(cellX, cellY))
	{
		return false;
	}
	// Off the rock faces and out of the players' build areas.
	if (field.hasFlag(cellX, cellY, WBMapGenHeightField::FLAG_CLIFF))
	{
		return false;
	}
	if (field.hasFlag(cellX, cellY, WBMapGenHeightField::FLAG_BASE))
	{
		return false;
	}
	if (haveWater && localIsUnderwater(worldX, worldY))
	{
		return false;
	}
	if (localIsInsideMapObject(worldX, worldY))
	{
		return false;
	}
	return true;
}

//=============================================================================
// scatterProps
//=============================================================================
/** Scatters trees and rocks, chaining them onto pHead.

	Trees are placed in clumps rather than spread evenly: a seed point is picked
	at random and then a short random walk drops several more nearby, which reads
	as woodland instead of an orchard. Rocks are scattered uniformly, which is how
	loose rock actually looks.
*/
//=============================================================================
static MapObject *scatterProps(MapObject *pHead, const WBMapGenSettings &settings,
															 const WBMapGenHeightField &field,
															 const WBMapGenAssets &assets, Int border, Int *countInOut)
{
	const Bool haveWater = localHasWaterAreas();
	const Real twoPi = 6.283185307f;

	// Density scales with the map area, so a big map doesn't come out sparse.
	const Int cells = field.getWidth() * field.getHeight();
	Real treeScale = 3.0f;
	if (settings.m_treeDensity == WB_DENSITY_LOW)
	{
		treeScale = 1.5f;
	}
	else if (settings.m_treeDensity == WB_DENSITY_HIGH)
	{
		treeScale = 6.0f;
	}

	// --- Trees, in clumps ---
	if (settings.m_doTrees && assets.getNumTrees() > 0)
	{
		WBRandom random((UnsignedInt)(settings.m_seed + 5077));
		const Int wanted = (Int)(((Real)cells / 300.0f) * treeScale);

		Int placed = 0;
		Int attempts = 0;
		const Int maxAttempts = wanted * 8 + 64;
		while (placed < wanted && attempts < maxAttempts && *countInOut < MAX_GENERATED_PROPS)
		{
			attempts++;

			// Seed the clump anywhere on the map...
			Int cellX = random.nextInt(field.getWidth());
			Int cellY = random.nextInt(field.getHeight());

			// ...then wander a few cells at a time, dropping trees as we go.
			const Int steps = 1 + random.nextInt(12);
			Int step;
			for (step = 0; step < steps && placed < wanted && *countInOut < MAX_GENERATED_PROPS; step++)
			{
				cellX += random.nextRange(-5, 6);
				cellY += random.nextRange(-5, 6);

				const Real worldX = (Real)(cellX - border) * MAP_XY_FACTOR;
				const Real worldY = (Real)(cellY - border) * MAP_XY_FACTOR;
				if (!canPlaceProp(field, cellX, cellY, worldX, worldY, haveWater))
				{
					continue;
				}

				const AsciiString &name = assets.getTree(random.nextInt(assets.getNumTrees()));
				MapObject *pNew = makeProp(name, worldX, worldY, random.nextReal() * twoPi);
				if (pNew != NULL)
				{
					pNew->setNextMap(pHead);
					pHead = pNew;
					placed++;
					(*countInOut)++;
				}
			}
		}
	}

	// --- Rocks, spread out ---
	if (settings.m_doRocks && assets.getNumRocks() > 0)
	{
		WBRandom random((UnsignedInt)(settings.m_seed + 6091));
		const Int wanted = cells / 2000;

		Int placed = 0;
		Int attempts = 0;
		const Int maxAttempts = wanted * 12 + 64;
		while (placed < wanted && attempts < maxAttempts && *countInOut < MAX_GENERATED_PROPS)
		{
			attempts++;

			const Int cellX = random.nextInt(field.getWidth());
			const Int cellY = random.nextInt(field.getHeight());
			const Real worldX = (Real)(cellX - border) * MAP_XY_FACTOR;
			const Real worldY = (Real)(cellY - border) * MAP_XY_FACTOR;
			if (!canPlaceProp(field, cellX, cellY, worldX, worldY, haveWater))
			{
				continue;
			}

			const AsciiString &name = assets.getRock(random.nextInt(assets.getNumRocks()));
			MapObject *pNew = makeProp(name, worldX, worldY, random.nextReal() * twoPi);
			if (pNew != NULL)
			{
				pNew->setNextMap(pHead);
				pHead = pNew;
				placed++;
				(*countInOut)++;
			}
		}
	}

	return pHead;
}

//=============================================================================
// WBMapGen_RunOnDocument
//=============================================================================
Bool WBMapGen_RunOnDocument(CWorldBuilderDoc *pDoc, const WBMapGenSettings &settings)
{
	if (pDoc == NULL)
	{
		return false;
	}

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (pMap == NULL)
	{
		return false;
	}

	// Work on a copy and hand it to the undoable, exactly as the resize path
	// does. Building a fresh WorldHeightMapEdit instead would be wrong: its
	// constructor frees the global map object list, so every existing object
	// would vanish outside of the undo system.
	WorldHeightMapEdit *pCopy = pMap->duplicate();
	if (pCopy == NULL)
	{
		return false;
	}

	Int border = pCopy->getBorderSizeInline();
	Int playableWidth = pCopy->getXExtent() - 2*border;
	Int playableHeight = pCopy->getYExtent() - 2*border;

	// Grow the map if it is too small for this many players. The starts sit on a
	// ring inside the map edge, so on a small map a high player count would push
	// them into each other's base areas -- better to enlarge the map than to hand
	// back a map nobody can play.
	Coord3D objOffset;
	objOffset.x = 0.0f;
	objOffset.y = 0.0f;
	objOffset.z = 0.0f;
	Bool didResize = false;
	{
		const Int minSize = WBMapGen_GetMinimumSizeForPlayers(settings.m_numPlayers);
		Int wantWidth = (playableWidth < minSize) ? minSize : playableWidth;
		Int wantHeight = (playableHeight < minSize) ? minSize : playableHeight;
		if (wantWidth != playableWidth || wantHeight != playableHeight)
		{
			if (!pCopy->resize(wantWidth, wantHeight, settings.m_baseHeight, border,
												 false, false, false, false, &objOffset))
			{
				REF_PTR_RELEASE(pCopy);
				return false;
			}
			didResize = true;
			border = pCopy->getBorderSizeInline();
			playableWidth = pCopy->getXExtent() - 2*border;
			playableHeight = pCopy->getYExtent() - 2*border;
		}
	}

	// The generator works in the copy's own dimensions rather than the settings',
	// so it fills exactly the map the document has (after any resize above).
	WBMapGenSettings actual = settings;
	actual.m_border = border;
	actual.m_playableWidth = playableWidth;
	actual.m_playableHeight = playableHeight;

	WBMapGenHeightField field(pCopy->getXExtent(), pCopy->getYExtent(),
														(UnsignedByte)actual.m_baseHeight);
	std::vector<WBMapGenPoint> starts;

	WBMapGenerator generator(actual);
	generator.generate(field, starts);

	// Push the heights across. setHeight keeps the cliff flags in step, which
	// the pathfinding overlays and the game both read.
	Int j;
	for (j = 0; j < field.getHeight(); j++)
	{
		Int i;
		for (i = 0; i < field.getWidth(); i++)
		{
			pCopy->setHeight(i, j, field.getHeight(i, j));
		}
	}

	// Decorate: textures follow the cliff flags, props avoid them.
	WBMapGenAssets assets;
	assets.gather();

	if (actual.m_doTextures)
	{
		paintTextures(pCopy, field, assets);
	}

	MapObject *pStartHead = buildStartWaypoints(pDoc, starts, border);
	if (actual.m_doTrees || actual.m_doRocks)
	{
		Int propCount = 0;
		pStartHead = scatterProps(pStartHead, actual, field, assets, border, &propCount);
	}

	// One composite undoable, so a single Undo takes the whole generated map
	// back out -- terrain and waypoints together.
	MultipleUndoable *pBatch = new MultipleUndoable;

	// addUndoable prepends, so add in reverse of the order things should happen.
	if (pStartHead != NULL)
	{
		AddObjectUndoable *pObjs = new AddObjectUndoable(pDoc, pStartHead);
		pBatch->addUndoable(pObjs);
		REF_PTR_RELEASE(pObjs);	// belongs to pBatch now
		pStartHead = NULL;		// the undoable owns the chain
	}

	// Built last but run first. Its constructor snapshots the document's current
	// height map, so nothing may swap that pointer before this point.
	WBDocUndoable *pTerrain = new WBDocUndoable(pDoc, pCopy,
																						 didResize ? &objOffset : NULL);
	pBatch->addUndoable(pTerrain);
	REF_PTR_RELEASE(pTerrain);	// belongs to pBatch now

	pDoc->AddAndDoUndoable(pBatch);
	REF_PTR_RELEASE(pBatch);
	REF_PTR_RELEASE(pCopy);

	// Refresh the views, matching the resize path's tail.
	WbView3d *p3View = pDoc->Get3DView();
	if (p3View != NULL)
	{
		IRegion2D partialRange = {0, 0, 0, 0};
		p3View->updateHeightMapInView(pDoc->GetHeightMap(), false, partialRange);
	}

	POSITION pos = pDoc->GetFirstViewPosition();
	while (pos != NULL)
	{
		CView *pView = pDoc->GetNextView(pos);
		WbView *pWView = (WbView *)pView;
		ASSERT_VALID(pWView);
		pWView->adjustDocSize();
		pWView->Invalidate();
	}

	WaypointOptions::update();

	return true;
}
