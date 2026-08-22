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

// WBMapGenerator.h
// The map generator core: terrain heights and start positions.
//
// Ported from the Genesis map generator, Copyright 2013 The CWC Team, by
// Daniel Sklenitzka, licensed under the Apache License 2.0 --
// http://www.apache.org/licenses/LICENSE-2.0
//
// This file deliberately knows nothing about MFC, Qt or the document: it works
// on a plain height field so the generation can be reasoned about (and fixed)
// without the editor in the way. WBMapGenDoc applies the result.

#pragma once

#ifndef WB_MAP_GENERATOR_H
#define WB_MAP_GENERATOR_H

#include "always.h"
#include "MapGen/WBMapGenSettings.h"
#include <vector>

/// A cell position in height-map space (includes the border).
struct WBMapGenPoint
{
	Int m_x;
	Int m_y;
};

/*************************************************************************/
/**                        WBMapGenHeightField
	The height grid the generator works on, plus per-cell flags.

	Indices include the border, matching WorldHeightMapEdit's own indexing, so
	positions can be handed straight across without a coordinate shift.
***************************************************************************/
class WBMapGenHeightField
{
public:
	enum
	{
		FLAG_CLIFF = 0x01,	///< too steep to walk; drives texturing and prop rejection
		FLAG_BASE  = 0x02	///< inside a player's start area; keeps props and cliffs out
	};

	WBMapGenHeightField(Int width, Int height, UnsignedByte initialHeight);

	Int getWidth(void) const {return m_width;}
	Int getHeight(void) const {return m_height;}

	Bool isValid(Int x, Int y) const
	{
		return (x >= 0 && y >= 0 && x < m_width && y < m_height);
	}

	UnsignedByte getHeight(Int x, Int y) const;
	void setHeight(Int x, Int y, UnsignedByte height);

	/// Adds a signed delta, wrapping like the original does rather than clamping.
	void addHeight(Int x, Int y, Int delta);

	UnsignedByte getFlags(Int x, Int y) const;
	void setFlag(Int x, Int y, UnsignedByte flag);
	Bool hasFlag(Int x, Int y, UnsignedByte flag) const;

protected:
	Int m_width;
	Int m_height;
	std::vector<UnsignedByte> m_heights;
	std::vector<UnsignedByte> m_flags;
};

/*************************************************************************/
/**                          WBMapGenerator
	Runs the generation steps over a height field.
***************************************************************************/
class WBMapGenerator
{
public:
	WBMapGenerator(const WBMapGenSettings &settings);

	/// Generates terrain into the field and reports where the players start.
	void generate(WBMapGenHeightField &field, std::vector<WBMapGenPoint> &startsOut);

protected:
	/// Chooses the player start cells, before any terrain exists.
	void placeStartPositions(WBMapGenHeightField &field, std::vector<WBMapGenPoint> &startsOut);
	/// Adds the noise layers. detailPass picks which half of the layer table to apply.
	void generateTerrain(WBMapGenHeightField &field, Bool detailPass);
	/// Flattens a pad around each start so bases can be built there.
	void flattenStartPositions(WBMapGenHeightField &field, const std::vector<WBMapGenPoint> &starts);
	/// Clamps over-steep neighbours and marks what is left as cliff.
	void limitSlopes(WBMapGenHeightField &field);

	/// Flattens a disc with a smooth (tanh-eased) falloff into the terrain around it.
	void flattenArea(WBMapGenHeightField &field, Int centerX, Int centerY,
									 UnsignedByte targetHeight, Int radius, Int falloff);

	/// Seeds for the individual steps.
	///
	/// Each step re-seeds from the map seed plus a fixed salt rather than sharing
	/// one stream, so turning a later step off can't change the output of an
	/// earlier one -- toggling trees must not move the hills.
	Int stepSeed(Int salt) const {return m_settings.m_seed + salt;}

protected:
	WBMapGenSettings m_settings;
	const WBTerrainLayer *m_layers;
	Int m_numLayers;
};

/// The radius, in cells, of the flat pad each player starts on.
enum {WB_MAPGEN_BASE_RADIUS = 40};

#endif // WB_MAP_GENERATOR_H
