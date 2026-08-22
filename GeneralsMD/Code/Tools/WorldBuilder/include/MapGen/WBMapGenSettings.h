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

// WBMapGenSettings.h
// Settings for the map generator.
//
// The terrain layer values are those shipped by the Genesis map generator
// (Genesis.UI/MapLayouts.xml), Copyright 2013 The CWC Team, by Daniel
// Sklenitzka, licensed under the Apache License 2.0 --
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#ifndef WB_MAP_GEN_SETTINGS_H
#define WB_MAP_GEN_SETTINGS_H

#include "always.h"

/// How much of a road network to lay down.
enum WBMapGenRoadMode
{
	WB_ROADS_NONE = 0,		///< no roads at all
	WB_ROADS_STARTS = 1,	///< link the player starts to each other
	WB_ROADS_SUPPLIES = 2	///< route those roads via each player's supply points
};

/// Density steps for props and cliffs.
enum WBMapGenDensity
{
	WB_DENSITY_LOW = 0,
	WB_DENSITY_MEDIUM = 1,
	WB_DENSITY_HIGH = 2
};

/*************************************************************************/
/**                          WBTerrainLayer
	One octave-set of noise added to the height map.

	Layers stack: each one samples the noise with its own settings and adds the
	result. A layer with a negative height carves down instead of building up.
***************************************************************************/
struct WBTerrainLayer
{
	const char *m_name;
	Int m_height;			///< amplitude in height units; MAY BE NEGATIVE
	Real m_elevation;		///< -> noise amplitude
	Real m_jaggedness;		///< -> noise base frequency
	Real m_ruggedness;		///< -> noise persistence
	Int m_levelOfDetail;	///< -> noise octaves
	Bool m_isDetail;		///< applied in the second pass, after the bases are flattened
};

/*************************************************************************/
/**                        WBMapGenSettings
	Everything the generator needs. One seed drives the whole thing, so the
	same settings always produce the same map.
***************************************************************************/
struct WBMapGenSettings
{
	Int m_seed;

	Int m_playableWidth;	///< not counting the border
	Int m_playableHeight;
	Int m_border;

	Int m_numPlayers;		///< 2 or 4
	Int m_baseHeight;		///< every cell starts here

	Int m_treeDensity;		///< WBMapGenDensity
	Int m_cliffDensity;		///< WBMapGenDensity

	Bool m_doCliffs;
	Bool m_doTextures;
	Bool m_doTrees;
	Bool m_doRocks;
	Bool m_doPlayers;	///< add the skirmish sides, so the map is playable as one
	Bool m_doSupplies;	///< put supply sources near each start
	Int m_roadMode;		///< WBMapGenRoadMode

	/// Fills in the shipped defaults.
	void setDefaults(void);
};

/// The default terrain layers, and how many there are.
const WBTerrainLayer *WBMapGen_GetDefaultLayers(Int *countOut);

/// Smallest playable width/height that fits this many players.
///
/// Start positions sit on a ring inside the map edge, so the more players there
/// are the further apart the ring has to push them to keep their base areas from
/// crowding each other. Below this size the starts end up on top of one another.
Int WBMapGen_GetMinimumSizeForPlayers(Int numPlayers);

#endif // WB_MAP_GEN_SETTINGS_H
