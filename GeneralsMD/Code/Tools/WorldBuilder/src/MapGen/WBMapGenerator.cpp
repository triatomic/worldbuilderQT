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

// WBMapGenerator.cpp
// The map generator core.
//
// Ported from the Genesis map generator, Copyright 2013 The CWC Team, by
// Daniel Sklenitzka, licensed under the Apache License 2.0 --
// http://www.apache.org/licenses/LICENSE-2.0

#include "StdAfx.h"
#include "MapGen/WBMapGenerator.h"
#include "MapGen/WBPerlinNoise.h"

#include <math.h>

// Salts so each step gets its own reproducible random stream (see stepSeed).
enum
{
	SALT_STARTS = 0,
	SALT_TERRAIN = 1013,
	SALT_SLOPES = 2027,
	SALT_CLIFFS = 3041,
	SALT_TEXTURES = 4057,
	SALT_TREES = 5077,
	SALT_ROCKS = 6091
};

/// Biggest height step allowed between neighbouring cells before the slope
/// limiter cuts it back and calls the result a cliff.
static const Int MAX_HEIGHT_DIFFERENCE = 15;

//=============================================================================
// WBMapGenHeightField
//=============================================================================
WBMapGenHeightField::WBMapGenHeightField(Int width, Int height, UnsignedByte initialHeight) :
	m_width(width),
	m_height(height)
{
	if (m_width < 0)
	{
		m_width = 0;
	}
	if (m_height < 0)
	{
		m_height = 0;
	}
	const Int count = m_width * m_height;
	m_heights.assign(count, initialHeight);
	m_flags.assign(count, 0);
}

UnsignedByte WBMapGenHeightField::getHeight(Int x, Int y) const
{
	if (!isValid(x, y))
	{
		return 0;
	}
	return m_heights[x + y*m_width];
}

void WBMapGenHeightField::setHeight(Int x, Int y, UnsignedByte height)
{
	if (!isValid(x, y))
	{
		return;
	}
	m_heights[x + y*m_width] = height;
}

//=============================================================================
// WBMapGenHeightField::addHeight
//=============================================================================
/** Adds a signed delta to a cell.

	This wraps rather than clamping, which is deliberate: the original relies on
	byte wraparound for its negative layers (the "Depressions" layer subtracts by
	adding a value that wraps), and clamping instead would flatten those features
	away. The mask makes the wrap well defined -- casting a negative float
	straight to an unsigned char is undefined behaviour in C++.
*/
//=============================================================================
void WBMapGenHeightField::addHeight(Int x, Int y, Int delta)
{
	if (!isValid(x, y))
	{
		return;
	}
	const Int ndx = x + y*m_width;
	const Int sum = (Int)m_heights[ndx] + delta;
	m_heights[ndx] = (UnsignedByte)(sum & 0xFF);
}

UnsignedByte WBMapGenHeightField::getFlags(Int x, Int y) const
{
	if (!isValid(x, y))
	{
		return 0;
	}
	return m_flags[x + y*m_width];
}

void WBMapGenHeightField::setFlag(Int x, Int y, UnsignedByte flag)
{
	if (!isValid(x, y))
	{
		return;
	}
	m_flags[x + y*m_width] |= flag;
}

Bool WBMapGenHeightField::hasFlag(Int x, Int y, UnsignedByte flag) const
{
	return (getFlags(x, y) & flag) != 0;
}

//=============================================================================
// WBMapGenerator
//=============================================================================
WBMapGenerator::WBMapGenerator(const WBMapGenSettings &settings) :
	m_settings(settings),
	m_layers(NULL),
	m_numLayers(0)
{
	m_layers = WBMapGen_GetDefaultLayers(&m_numLayers);
}

//=============================================================================
// WBMapGenerator::generate
//=============================================================================
/** Runs the generation steps.

	The order matters and follows the original: the start positions are chosen
	while the map is still flat, then terrain is built over them, then the pads
	are flattened back down. Picking the starts after the terrain existed would
	mean hunting for somewhere flat enough instead of simply making it so.
*/
//=============================================================================
void WBMapGenerator::generate(WBMapGenHeightField &field, std::vector<WBMapGenPoint> &startsOut)
{
	placeStartPositions(field, startsOut);

	generateTerrain(field, false);
	flattenStartPositions(field, startsOut);
	generateTerrain(field, true);

	if (m_settings.m_doCliffs)
	{
		limitSlopes(field);
	}
}

//=============================================================================
// WBMapGenerator::placeStartPositions
//=============================================================================
/** Places the players evenly around the map centre.

	Everyone sits on one line through the centre, rotated by a seeded angle, and
	spaced 360/N apart -- so two players face each other across the map and four
	sit at the corners of a square. The rotation is what stops every generated
	map opening with the same north/south layout.
*/
//=============================================================================
void WBMapGenerator::placeStartPositions(WBMapGenHeightField &field, std::vector<WBMapGenPoint> &startsOut)
{
	startsOut.clear();

	Int numPlayers = m_settings.m_numPlayers;
	if (numPlayers < 2)
	{
		numPlayers = 2;
	}

	const Int border = m_settings.m_border;
	// Work in playable space; the border is added back at the end.
	const Real halfWidth = (Real)m_settings.m_playableWidth / 2.0f;
	const Real halfHeight = (Real)m_settings.m_playableHeight / 2.0f;

	// How far out the starts sit: as close to the edge as the base pad allows.
	Real radiusX = halfWidth - (Real)WB_MAPGEN_BASE_RADIUS;
	Real radiusY = halfHeight - (Real)WB_MAPGEN_BASE_RADIUS;
	if (radiusX < 1.0f)
	{
		radiusX = 1.0f;
	}
	if (radiusY < 1.0f)
	{
		radiusY = 1.0f;
	}

	WBRandom random((UnsignedInt)stepSeed(SALT_STARTS));
	const Real twoPi = 6.283185307f;
	const Real baseAngle = random.nextReal() * twoPi;

	Int i;
	for (i = 0; i < numPlayers; i++)
	{
		const Real angle = baseAngle + (twoPi * (Real)i / (Real)numPlayers);

		// An ellipse inscribed in the playable area, so the starts stay inside
		// the map on non-square maps instead of running off the short side.
		const Real fx = (Real)cos(angle) * radiusX;
		const Real fy = (Real)sin(angle) * radiusY;

		WBMapGenPoint pt;
		pt.m_x = border + (Int)(halfWidth + fx);
		pt.m_y = border + (Int)(halfHeight + fy);

		if (pt.m_x < 0)
		{
			pt.m_x = 0;
		}
		if (pt.m_y < 0)
		{
			pt.m_y = 0;
		}
		if (pt.m_x >= field.getWidth())
		{
			pt.m_x = field.getWidth() - 1;
		}
		if (pt.m_y >= field.getHeight())
		{
			pt.m_y = field.getHeight() - 1;
		}

		startsOut.push_back(pt);
	}

	// Mark the base areas so cliffs and props keep away from them.
	const Int radiusSqr = WB_MAPGEN_BASE_RADIUS * WB_MAPGEN_BASE_RADIUS;
	for (i = 0; i < (Int)startsOut.size(); i++)
	{
		const WBMapGenPoint &start = startsOut[i];
		Int y;
		for (y = start.m_y - WB_MAPGEN_BASE_RADIUS; y <= start.m_y + WB_MAPGEN_BASE_RADIUS; y++)
		{
			Int x;
			for (x = start.m_x - WB_MAPGEN_BASE_RADIUS; x <= start.m_x + WB_MAPGEN_BASE_RADIUS; x++)
			{
				const Int dx = x - start.m_x;
				const Int dy = y - start.m_y;
				if (dx*dx + dy*dy <= radiusSqr)
				{
					field.setFlag(x, y, WBMapGenHeightField::FLAG_BASE);
				}
			}
		}
	}
}

//=============================================================================
// WBMapGenerator::generateTerrain
//=============================================================================
/** Adds the noise layers to the height field.

	One noise object is permutated once and shared by every layer, exactly as the
	original does: the layers differ only in amplitude, frequency, persistence and
	octaves. Giving each layer its own permutation would decorrelate them and lose
	the way the depressions follow the hills.
*/
//=============================================================================
void WBMapGenerator::generateTerrain(WBMapGenHeightField &field, Bool detailPass)
{
	if (m_layers == NULL || m_numLayers <= 0)
	{
		return;
	}

	WBPerlinNoise noise(stepSeed(SALT_TERRAIN));

	// The third noise axis just picks a slice, so the same settings with a
	// different seed give a completely different landscape.
	const Real z = (Real)(m_settings.m_seed & 0xFF);

	Int j;
	for (j = 0; j < field.getHeight(); j++)
	{
		Int i;
		for (i = 0; i < field.getWidth(); i++)
		{
			Int layerNdx;
			for (layerNdx = 0; layerNdx < m_numLayers; layerNdx++)
			{
				const WBTerrainLayer &layer = m_layers[layerNdx];
				if ((layer.m_isDetail ? true : false) != (detailPass ? true : false))
				{
					continue;
				}

				noise.setAmplitude(layer.m_elevation);
				noise.setFrequency(layer.m_jaggedness);
				noise.setPersistence(layer.m_ruggedness);
				noise.setOctaves(layer.m_levelOfDetail);

				const Real normalized = noise.compute((Real)i, (Real)j, z);
				const Int delta = (Int)(normalized * (Real)layer.m_height);
				field.addHeight(i, j, delta);
			}
		}
	}
}

//=============================================================================
// WBMapGenerator::flattenStartPositions
//=============================================================================
void WBMapGenerator::flattenStartPositions(WBMapGenHeightField &field,
																					 const std::vector<WBMapGenPoint> &starts)
{
	Int i;
	for (i = 0; i < (Int)starts.size(); i++)
	{
		const WBMapGenPoint &start = starts[i];
		// Level the pad to whatever the terrain happens to be at its centre, so
		// bases sit naturally in the landscape rather than on a plateau.
		const UnsignedByte target = field.getHeight(start.m_x, start.m_y);
		flattenArea(field, start.m_x, start.m_y, target, WB_MAPGEN_BASE_RADIUS, 20);
	}
}

//=============================================================================
// WBMapGenerator::flattenArea
//=============================================================================
/** Flattens a disc, easing back into the surrounding terrain over falloff cells.

	The easing uses tanh rather than a straight line: a linear blend leaves a
	visible crease where the ramp meets the hillside, while the S-curve flattens
	off at both ends and reads as a natural shoulder.
*/
//=============================================================================
void WBMapGenerator::flattenArea(WBMapGenHeightField &field, Int centerX, Int centerY,
																 UnsignedByte targetHeight, Int radius, Int falloff)
{
	if (radius <= 0)
	{
		return;
	}
	if (falloff < 0)
	{
		falloff = 0;
	}

	const Int outer = radius + falloff;
	// tanh(pi/2), used to renormalize the curve back to exactly [0,1].
	const Real piOverTwo = 1.570796327f;
	const Real range = (Real)tanh(piOverTwo);

	Int y;
	for (y = centerY - outer; y <= centerY + outer; y++)
	{
		Int x;
		for (x = centerX - outer; x <= centerX + outer; x++)
		{
			if (!field.isValid(x, y))
			{
				continue;
			}

			const Real dx = (Real)(x - centerX);
			const Real dy = (Real)(y - centerY);
			const Real dist = (Real)sqrt(dx*dx + dy*dy);

			if (dist <= (Real)radius)
			{
				field.setHeight(x, y, targetHeight);
				continue;
			}
			if (dist >= (Real)outer || falloff == 0)
			{
				continue;
			}

			// 0 at the pad edge, 1 where the terrain takes over again.
			Real weight = (dist - (Real)radius) / (Real)falloff;
			weight = (Real)((tanh(((weight * 2.0f) - 1.0f) * piOverTwo) + range) / (2.0f * range));

			const Real current = (Real)field.getHeight(x, y);
			const Real blended = (Real)targetHeight + (current - (Real)targetHeight) * weight;
			field.setHeight(x, y, (UnsignedByte)(blended + 0.5f));
		}
	}
}

//=============================================================================
// WBMapGenerator::limitSlopes
//=============================================================================
/** Cuts back over-steep transitions and marks what remains as cliff.

	Anything steeper than MAX_HEIGHT_DIFFERENCE per cell is pulled back to that
	limit, and both cells are flagged as cliff. This is what turns raw noise into
	terrain units can actually move across, and the flags it leaves behind are
	what the texturing and prop steps use to keep off the rock faces.

	Base areas are skipped -- a cliff carved through a player's start would make
	the map unplayable.
*/
//=============================================================================
void WBMapGenerator::limitSlopes(WBMapGenHeightField &field)
{
	WBRandom random((UnsignedInt)stepSeed(SALT_SLOPES));

	Int j;
	for (j = 0; j < field.getHeight() - 1; j++)
	{
		Int i;
		for (i = 0; i < field.getWidth() - 1; i++)
		{
			if (field.hasFlag(i, j, WBMapGenHeightField::FLAG_BASE))
			{
				continue;
			}

			const Int here = (Int)field.getHeight(i, j);
			const Int lo = (here - MAX_HEIGHT_DIFFERENCE > 0) ? here - MAX_HEIGHT_DIFFERENCE : 0;
			const Int hi = (here + MAX_HEIGHT_DIFFERENCE < 255) ? here + MAX_HEIGHT_DIFFERENCE : 255;

			// Only the forward neighbours: the pass sweeps right and down, so
			// every pair gets looked at exactly once.
			Int n;
			for (n = 0; n < 3; n++)
			{
				Int nx = i;
				Int ny = j;
				if (n == 0)
				{
					nx = i + 1;
				}
				else if (n == 1)
				{
					ny = j + 1;
				}
				else
				{
					nx = i + 1;
					ny = j + 1;
				}

				if (field.hasFlag(nx, ny, WBMapGenHeightField::FLAG_BASE))
				{
					continue;
				}

				const Int there = (Int)field.getHeight(nx, ny);
				if (there > hi)
				{
					// Back off by a random cell or two so the cliff tops don't all
					// land on exactly the same contour.
					Int clamped = hi - random.nextInt(5);
					if (clamped < 0)
					{
						clamped = 0;
					}
					field.setHeight(nx, ny, (UnsignedByte)clamped);
					field.setFlag(i, j, WBMapGenHeightField::FLAG_CLIFF);
					field.setFlag(nx, ny, WBMapGenHeightField::FLAG_CLIFF);
				}
				else if (there < lo)
				{
					Int clamped = lo + random.nextInt(5);
					if (clamped > 255)
					{
						clamped = 255;
					}
					field.setHeight(nx, ny, (UnsignedByte)clamped);
					field.setFlag(i, j, WBMapGenHeightField::FLAG_CLIFF);
					field.setFlag(nx, ny, WBMapGenHeightField::FLAG_CLIFF);
				}
			}
		}
	}
}
