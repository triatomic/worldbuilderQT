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

// WBMapGenSettings.cpp
// Defaults for the map generator.
//
// The terrain layer values are those shipped by the Genesis map generator
// (Genesis.UI/MapLayouts.xml), Copyright 2013 The CWC Team, by Daniel
// Sklenitzka, licensed under the Apache License 2.0 --
// http://www.apache.org/licenses/LICENSE-2.0

#include "StdAfx.h"
#include "MapGen/WBMapGenSettings.h"
#include "CustomConfigProfile.h"

//=============================================================================
// The default terrain layers.
//=============================================================================
/** These are the tuned values from the original generator's "Default" layout,
	kept as-is because they are what make the terrain look hand-made rather than
	like raw noise: broad hills, shallow depressions cut back into them, and a
	fine bumpiness applied last so it survives the base flattening.

	Height may be negative -- that layer subtracts.
*/
//=============================================================================
static const WBTerrainLayer s_defaultLayers[] =
{
	// name           height  elevation  jaggedness  ruggedness  LOD  isDetail
	{ "Hills",           200,      1.3f,     0.005f,      0.25f,   3,   false },
	{ "Depressions",     -40,     1.22f,     0.014f,       0.7f,   2,   false },
	{ "Bumps",            13,     0.79f,     0.038f,       0.8f,   3,   true  }
};

//=============================================================================
// WBMapGen_GetDefaultLayers
//=============================================================================
const WBTerrainLayer *WBMapGen_GetDefaultLayers(Int *countOut)
{
	if (countOut != NULL)
	{
		*countOut = sizeof(s_defaultLayers) / sizeof(s_defaultLayers[0]);
	}
	return s_defaultLayers;
}

//=============================================================================
// WBMapGen_GetMinimumSizeForPlayers
//=============================================================================
/** Smallest playable size that fits this many players.

	The starts sit on a ring inset from the map edge by the base radius, spaced
	evenly around it. For their base areas to stay clear of each other the ring has
	to grow with the player count, and the map has to grow with the ring.

	The numbers are anchored on the original generator, which required 150 for two
	players and 300 for four. Four players at 300 works out to roughly four base
	radii between neighbours, which matches its stated minimum spacing, so the same
	spacing is extrapolated to six and eight. Two players keeps the original's more
	generous 150 rather than the 236 the formula would ask for -- a small head to
	head map is a perfectly reasonable thing to want.
*/
//=============================================================================
Int WBMapGen_GetMinimumSizeForPlayers(Int numPlayers)
{
	switch (numPlayers)
	{
		case 2:
			return 150;
		case 4:
			return 300;
		case 6:
			return 400;
		case 8:
			return 490;
		default:
			break;
	}
	// Anything unexpected: fall back to the largest we know about, which is safe
	// in the sense that it will not crowd the starts.
	return 490;
}

//=============================================================================
// WBMapGenSettings::setDefaults
//=============================================================================
void WBMapGenSettings::setDefaults(void)
{
	m_seed = 12345;

	// The original's defaults. The border is what the game needs around the
	// playable area for the camera to sit outside the map.
	m_playableWidth = 250;
	m_playableHeight = 250;
	m_border = 30;

	m_numPlayers = 2;
	m_baseHeight = 40;

	m_treeDensity = WB_DENSITY_MEDIUM;
	m_cliffDensity = WB_DENSITY_MEDIUM;

	m_doCliffs = true;
	m_doTextures = true;
	m_doTrees = true;
	m_doRocks = true;
	m_doPlayers = true;
	m_doSupplies = true;
	m_roadMode = WB_ROADS_STARTS;
}

//=============================================================================
// Settings persistence
//=============================================================================
/** The generator remembers what you last used.

	Kept alongside the editor's other preferences rather than in the map, since
	these are how YOU like to generate maps, not a property of any one map. This
	is also what makes the Randomize command work: it reuses these and changes
	only the seed.
*/
//=============================================================================
static const char *MAPGEN_SECTION = "MapGenerator";

void WBMapGenSettings::load(void)
{
	setDefaults();

	m_seed = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "Seed", m_seed);
	m_numPlayers = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "Players", m_numPlayers);
	m_baseHeight = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "BaseHeight", m_baseHeight);
	m_treeDensity = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "TreeDensity", m_treeDensity);
	m_cliffDensity = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "CliffDensity", m_cliffDensity);
	m_roadMode = CustomConfigProfile::ReadInt(MAPGEN_SECTION, "RoadMode", m_roadMode);

	m_doCliffs = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoCliffs", m_doCliffs ? 1 : 0) != 0);
	m_doTextures = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoTextures", m_doTextures ? 1 : 0) != 0);
	m_doTrees = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoTrees", m_doTrees ? 1 : 0) != 0);
	m_doRocks = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoRocks", m_doRocks ? 1 : 0) != 0);
	m_doPlayers = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoPlayers", m_doPlayers ? 1 : 0) != 0);
	m_doSupplies = (CustomConfigProfile::ReadInt(MAPGEN_SECTION, "DoSupplies", m_doSupplies ? 1 : 0) != 0);

	// Guard against a hand-edited or stale ini leaving something out of range.
	if (m_numPlayers < 2 || m_numPlayers > 8 || (m_numPlayers & 1) != 0)
	{
		m_numPlayers = 2;
	}
	if (m_treeDensity < 0 || m_treeDensity > WB_DENSITY_HIGH)
	{
		m_treeDensity = WB_DENSITY_MEDIUM;
	}
	if (m_cliffDensity < 0 || m_cliffDensity > WB_DENSITY_HIGH)
	{
		m_cliffDensity = WB_DENSITY_MEDIUM;
	}
	if (m_roadMode < WB_ROADS_NONE || m_roadMode > WB_ROADS_SUPPLIES)
	{
		m_roadMode = WB_ROADS_STARTS;
	}
	if (m_baseHeight < 0 || m_baseHeight > 200)
	{
		m_baseHeight = 40;
	}
}

void WBMapGenSettings::save(void) const
{
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "Seed", m_seed);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "Players", m_numPlayers);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "BaseHeight", m_baseHeight);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "TreeDensity", m_treeDensity);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "CliffDensity", m_cliffDensity);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "RoadMode", m_roadMode);

	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoCliffs", m_doCliffs ? 1 : 0);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoTextures", m_doTextures ? 1 : 0);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoTrees", m_doTrees ? 1 : 0);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoRocks", m_doRocks ? 1 : 0);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoPlayers", m_doPlayers ? 1 : 0);
	CustomConfigProfile::WriteInt(MAPGEN_SECTION, "DoSupplies", m_doSupplies ? 1 : 0);
}
