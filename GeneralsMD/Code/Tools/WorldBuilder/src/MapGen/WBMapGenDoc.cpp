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
#include "GameLogic/SidesList.h"
#include <math.h>
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
// addSkirmishSide
//=============================================================================
/** Adds one side, if the map doesn't already have it. */
//=============================================================================
static void addSkirmishSide(SidesList *pSides, const char *faction, const char *playerName,
														const wchar_t *displayName)
{
	if (pSides->findSideInfo(AsciiString(playerName)) != NULL)
	{
		return;
	}

	Dict newPlayerDict;
	UnicodeString displayStr;
	displayStr = displayName;
	newPlayerDict.setAsciiString(TheKey_playerName, AsciiString(playerName));
	newPlayerDict.setBool(TheKey_playerIsHuman, false);
	newPlayerDict.setUnicodeString(TheKey_playerDisplayName, displayStr);
	newPlayerDict.setAsciiString(TheKey_playerFaction, AsciiString(faction));
	newPlayerDict.setAsciiString(TheKey_playerEnemies, AsciiString(""));
	newPlayerDict.setAsciiString(TheKey_playerAllies, AsciiString(""));

	pSides->addSide(&newPlayerDict);
	pSides->validateSides();
}

//=============================================================================
// buildSkirmishSides
//=============================================================================
/** Fills in the sides a skirmish map needs.

	Without these a generated map has start positions but nobody to occupy them,
	and the game won't offer it as a skirmish map. Mirrors the Player List
	dialog's "Add Skirmish Players", so a generated map has the same side list a
	mapper would set up by hand.
*/
//=============================================================================
static void buildSkirmishSides(SidesList *pSides)
{
	addSkirmishSide(pSides, "FactionCivilian", "PlyrCivilian", L"PlyrCivilian");
	addSkirmishSide(pSides, "FactionAmerica", "SkirmishAmerica", L"SkirmishAmerica");
	addSkirmishSide(pSides, "FactionChina", "SkirmishChina", L"SkirmishChina");
	addSkirmishSide(pSides, "FactionGLA", "SkirmishGLA", L"SkirmishGLA");

	addSkirmishSide(pSides, "FactionAmericaAirForceGeneral", "SkirmishAmericaAirForceGeneral", L"SkirmishAmericaAirForceGeneral");
	addSkirmishSide(pSides, "FactionAmericaLaserGeneral", "SkirmishAmericaLaserGeneral", L"SkirmishAmericaLaserGeneral");
	addSkirmishSide(pSides, "FactionAmericaSuperWeaponGeneral", "SkirmishAmericaSuperWeaponGeneral", L"SkirmishAmericaSuperWeaponGeneral");
	addSkirmishSide(pSides, "FactionChinaTankGeneral", "SkirmishChinaTankGeneral", L"SkirmishChinaTankGeneral");
	addSkirmishSide(pSides, "FactionChinaNukeGeneral", "SkirmishChinaNukeGeneral", L"SkirmishChinaNukeGeneral");
	addSkirmishSide(pSides, "FactionChinaInfantryGeneral", "SkirmishChinaInfantryGeneral", L"SkirmishChinaInfantryGeneral");
	addSkirmishSide(pSides, "FactionGLADemolitionGeneral", "SkirmishGLADemolitionGeneral", L"SkirmishGLADemolitionGeneral");
	addSkirmishSide(pSides, "FactionGLAToxinGeneral", "SkirmishGLAToxinGeneral", L"SkirmishGLAToxinGeneral");
	addSkirmishSide(pSides, "FactionGLAStealthGeneral", "SkirmishGLAStealthGeneral", L"SkirmishGLAStealthGeneral");
}

//=============================================================================
// placeSupplies
//=============================================================================
/** Puts a pair of supply sources near each player's start.

	Every player gets the same count at the same distance, so nobody starts with
	an economic advantage. They sit outside the base pad but well within reach,
	and are nudged off cliffs and water if the first choice is unusable.
*/
//=============================================================================
static MapObject *placeSupplies(MapObject *pHead, const WBMapGenSettings &settings,
																const WBMapGenHeightField &field,
																const WBMapGenAssets &assets,
																const std::vector<WBMapGenPoint> &starts,
																Int border, Int *countInOut,
																std::vector<WBMapGenPoint> *placedOut)
{
	if (!assets.hasSupplySource())
	{
		return pHead;
	}

	const Bool haveWater = localHasWaterAreas();
	const Real twoPi = 6.283185307f;
	// Just outside the flattened base, close enough to be clearly "theirs".
	const Int distance = WB_MAPGEN_BASE_RADIUS + 12;
	const Int perPlayer = 2;

	WBRandom random((UnsignedInt)(settings.m_seed + 7103));
	// One angle for everyone, rotated per player, so each base's supplies sit in
	// the same relative spot -- the layout stays fair however the map came out.
	const Real baseAngle = random.nextReal() * twoPi;

	Int i;
	for (i = 0; i < (Int)starts.size(); i++)
	{
		Int n;
		for (n = 0; n < perPlayer; n++)
		{
			const Real angle = baseAngle + (twoPi * (Real)n / (Real)perPlayer);

			// Try the ideal spot, then walk around the ring if it is unusable.
			Int attempt;
			for (attempt = 0; attempt < 8; attempt++)
			{
				const Real tryAngle = angle + ((Real)attempt * 0.4f);
				const Int cellX = starts[i].m_x + (Int)((Real)cos(tryAngle) * (Real)distance);
				const Int cellY = starts[i].m_y + (Int)((Real)sin(tryAngle) * (Real)distance);

				const Real worldX = (Real)(cellX - border) * MAP_XY_FACTOR;
				const Real worldY = (Real)(cellY - border) * MAP_XY_FACTOR;

				if (!field.isValid(cellX, cellY))
				{
					continue;
				}
				if (field.hasFlag(cellX, cellY, WBMapGenHeightField::FLAG_CLIFF))
				{
					continue;
				}
				if (haveWater && localIsUnderwater(worldX, worldY))
				{
					continue;
				}
				if (localIsInsideMapObject(worldX, worldY))
				{
					continue;
				}

				MapObject *pNew = makeProp(assets.getSupplySource(), worldX, worldY, 0.0f);
				if (pNew != NULL)
				{
					pNew->setNextMap(pHead);
					pHead = pNew;
					(*countInOut)++;
					if (placedOut != NULL)
					{
						WBMapGenPoint placed;
						placed.m_x = cellX;
						placed.m_y = cellY;
						placedOut->push_back(placed);
					}
				}
				break;
			}
		}
	}

	return pHead;
}

//=============================================================================
// addRoadSegment
//=============================================================================
/** Lays one straight road segment between two cells.

	A segment is a pair of adjacent map objects, the first flagged as the start
	point and the second as the end. They have to stay next to each other in the
	object list, which is why both are chained together here rather than added
	one at a time.
*/
//=============================================================================
static MapObject *addRoadSegment(MapObject *pHead, const AsciiString &roadName,
																 const WBMapGenPoint &from, const WBMapGenPoint &to,
																 Int border, Int *countInOut)
{
	Coord3D loc1;
	loc1.x = (Real)(from.m_x - border) * MAP_XY_FACTOR;
	loc1.y = (Real)(from.m_y - border) * MAP_XY_FACTOR;
	loc1.z = 0.0f;

	Coord3D loc2;
	loc2.x = (Real)(to.m_x - border) * MAP_XY_FACTOR;
	loc2.y = (Real)(to.m_y - border) * MAP_XY_FACTOR;
	loc2.z = 0.0f;

	MapObject *pStart = newInstance(MapObject)(loc1, roadName, 0.0f, 0, NULL, NULL);
	MapObject *pEnd = newInstance(MapObject)(loc2, roadName, 0.0f, 0, NULL, NULL);

	pStart->setColor(RGB(255, 255, 0));	// road endpoints are drawn yellow
	pEnd->setColor(RGB(255, 255, 0));
	pStart->setFlag(FLAG_ROAD_POINT1);
	pEnd->setFlag(FLAG_ROAD_POINT2);
	pStart->getProperties()->setAsciiString(TheKey_originalOwner,
																				 AsciiString(NEUTRAL_TEAM_INTERNAL_STR));
	pEnd->getProperties()->setAsciiString(TheKey_originalOwner,
																			 AsciiString(NEUTRAL_TEAM_INTERNAL_STR));

	// POINT2 has to immediately follow POINT1 in the list.
	pStart->setNextMap(pEnd);
	pEnd->setNextMap(pHead);
	(*countInOut) += 2;
	return pStart;
}

//=============================================================================
// buildRoadRoute
//=============================================================================
/** Runs a road from one cell to another in a few straight hops.

	The route is broken into segments and each joint is nudged off cliff cells,
	which is enough to stop a road running up a rock face without needing a full
	pathfinder. It is a road network rather than an optimal path -- and a road
	that bends a little looks better than a ruler-straight one anyway.
*/
//=============================================================================
static MapObject *buildRoadRoute(MapObject *pHead, const AsciiString &roadName,
																 const WBMapGenHeightField &field,
																 const WBMapGenPoint &from, const WBMapGenPoint &to,
																 Int border, Int *countInOut)
{
	const Int dx = to.m_x - from.m_x;
	const Int dy = to.m_y - from.m_y;
	const Int span = (Int)sqrt((Real)(dx*dx + dy*dy));
	if (span <= 0)
	{
		return pHead;
	}

	// A joint roughly every 25 cells, so the road has somewhere to bend.
	Int hops = span / 25;
	if (hops < 1)
	{
		hops = 1;
	}
	if (hops > 12)
	{
		hops = 12;
	}

	WBMapGenPoint prev = from;
	Int hop;
	for (hop = 1; hop <= hops; hop++)
	{
		WBMapGenPoint next;
		next.m_x = from.m_x + (dx * hop) / hops;
		next.m_y = from.m_y + (dy * hop) / hops;

		// Shift an intermediate joint off unusable ground. The endpoints stay put:
		// they are the places the road is meant to reach.
		if (hop < hops && field.hasFlag(next.m_x, next.m_y, WBMapGenHeightField::FLAG_CLIFF))
		{
			Int shift;
			for (shift = 1; shift <= 8; shift++)
			{
				// Step sideways from the road direction, so the detour goes around
				// the obstacle rather than back along the route.
				const Int tryX = next.m_x + ((dy > 0) ? shift : -shift);
				const Int tryY = next.m_y + ((dx > 0) ? -shift : shift);
				if (field.isValid(tryX, tryY) &&
						!field.hasFlag(tryX, tryY, WBMapGenHeightField::FLAG_CLIFF))
				{
					next.m_x = tryX;
					next.m_y = tryY;
					break;
				}
			}
		}

		pHead = addRoadSegment(pHead, roadName, prev, next, border, countInOut);
		prev = next;
	}

	return pHead;
}

//=============================================================================
// buildRoads
//=============================================================================
/** Links the player starts together with roads.

	The generator this was ported from grew its roads out of the villages and
	resource sites it stamped down from its own mod templates. Without those there
	are no road nodes to connect at all, so the network is seeded here from what a
	generated map does have: the start positions, and optionally the supply points
	belonging to each player.
*/
//=============================================================================
static MapObject *buildRoads(MapObject *pHead, const WBMapGenSettings &settings,
														 const WBMapGenHeightField &field,
														 const WBMapGenAssets &assets,
														 const std::vector<WBMapGenPoint> &starts,
														 const std::vector<WBMapGenPoint> &supplies,
														 Int border, Int *countInOut)
{
	if (settings.m_roadMode == WB_ROADS_NONE || !assets.hasRoad())
	{
		return pHead;
	}
	if (starts.size() < 2)
	{
		return pHead;
	}

	const AsciiString &roadName = assets.getRoad();
	const Int numStarts = (Int)starts.size();

	Int i;
	for (i = 0; i < numStarts; i++)
	{
		// Each start links to the next: one road across the map for two players,
		// a loop around it for more.
		if (numStarts == 2 && i == 1)
		{
			break;	// two players need the one road, not the same road twice
		}
		const Int nextNdx = (i + 1) % numStarts;

		if (settings.m_roadMode == WB_ROADS_SUPPLIES && !supplies.empty())
		{
			// Route by way of whichever supply point sits nearest the midpoint of
			// the two starts, so the road passes the economy instead of ignoring it.
			WBMapGenPoint mid;
			mid.m_x = (starts[i].m_x + starts[nextNdx].m_x) / 2;
			mid.m_y = (starts[i].m_y + starts[nextNdx].m_y) / 2;

			Int bestNdx = -1;
			Int bestDistSqr = 0;
			Int s;
			for (s = 0; s < (Int)supplies.size(); s++)
			{
				const Int sdx = supplies[s].m_x - mid.m_x;
				const Int sdy = supplies[s].m_y - mid.m_y;
				const Int distSqr = sdx*sdx + sdy*sdy;
				if (bestNdx < 0 || distSqr < bestDistSqr)
				{
					bestNdx = s;
					bestDistSqr = distSqr;
				}
			}

			if (bestNdx >= 0)
			{
				pHead = buildRoadRoute(pHead, roadName, field, starts[i], supplies[bestNdx],
															 border, countInOut);
				pHead = buildRoadRoute(pHead, roadName, field, supplies[bestNdx], starts[nextNdx],
															 border, countInOut);
				continue;
			}
		}

		pHead = buildRoadRoute(pHead, roadName, field, starts[i], starts[nextNdx],
													 border, countInOut);
	}

	return pHead;
}

//=============================================================================
// WBMapGen_RunOnDocument
//=============================================================================
Bool WBMapGen_RunOnDocument(CWorldBuilderDoc *pDoc, const WBMapGenSettings &settings,
													Bool clearExisting)
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

			// resize() grows the height map but leaves the playable boundary alone,
			// and the boundary is what the GAME reads as the map size -- without
			// this the map stays locked to its old dimensions in game however big
			// the terrain got.
			ICoord2D bounds;
			bounds.x = playableWidth;
			bounds.y = playableHeight;
			if (pCopy->getNumBoundaries() > 0)
			{
				pCopy->changeBoundary(0, &bounds);
			}
			else
			{
				pCopy->addBoundary(&bounds);
			}
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

	// Wipe what is already on the map, so a regenerate starts clean rather than
	// stacking this run's trees, rocks and roads on top of the last run's. Only the
	// objects go -- the terrain is replaced wholesale by the height map undoable.
	//
	// Done BEFORE any new object is created: DeleteObjectUndoable snapshots the
	// selection in its constructor, so building it here makes it impossible for it
	// to pick up the objects this run is about to add.
	DeleteObjectUndoable *pDelete = NULL;
	if (clearExisting && MapObject::getFirstMapObject() != NULL)
	{
		MapObject *pObj;
		for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
		{
			pObj->setSelected(true);
		}
		pDelete = new DeleteObjectUndoable(pDoc);
	}

	MapObject *pStartHead = buildStartWaypoints(pDoc, starts, border);

	Int propCount = 0;
	std::vector<WBMapGenPoint> supplyPoints;
	if (actual.m_doSupplies)
	{
		pStartHead = placeSupplies(pStartHead, actual, field, assets, starts, border,
															 &propCount, &supplyPoints);
	}
	if (actual.m_roadMode != WB_ROADS_NONE)
	{
		pStartHead = buildRoads(pStartHead, actual, field, assets, starts, supplyPoints,
														border, &propCount);
	}
	if (actual.m_doTrees || actual.m_doRocks)
	{
		pStartHead = scatterProps(pStartHead, actual, field, assets, border, &propCount);
	}

	// One composite undoable, so a single Undo takes the whole generated map
	// back out -- terrain and waypoints together.
	MultipleUndoable *pBatch = new MultipleUndoable;

	// addUndoable prepends, so add in reverse of the order things should happen.

	// Sides: without these the map has start positions but no players to use them,
	// and the game won't list it as a skirmish map.
	if (actual.m_doPlayers && TheSidesList != NULL)
	{
		SidesList newSides = *TheSidesList;
		buildSkirmishSides(&newSides);
		SidesListUndoable *pSidesUndo = new SidesListUndoable(newSides, pDoc);
		pBatch->addUndoable(pSidesUndo);
		REF_PTR_RELEASE(pSidesUndo);	// belongs to pBatch now
	}

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

	// Added last of all, so it is the first thing to run -- the map is cleared
	// before the new terrain and objects land.
	if (pDelete != NULL)
	{
		pBatch->addUndoable(pDelete);
		REF_PTR_RELEASE(pDelete);	// belongs to pBatch now
		pDelete = NULL;
	}

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
