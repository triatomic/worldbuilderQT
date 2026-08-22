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

// WBPerlinNoise.cpp
// Perlin noise and a small deterministic PRNG for the map generator.
//
// Ported from the Genesis map generator (Genesis/Math/PerlinNoise.cs),
// Copyright 2013 The CWC Team, by Daniel Sklenitzka, licensed under the
// Apache License 2.0 -- http://www.apache.org/licenses/LICENSE-2.0

#include "StdAfx.h"
#include "MapGen/WBPerlinNoise.h"

#include <math.h>

//=============================================================================
// WBRandom
//=============================================================================
WBRandom::WBRandom(UnsignedInt seed)
{
	// A zero state would make xorshift produce nothing but zeros forever.
	m_state = (seed != 0) ? seed : 0x9E3779B9u;
}

UnsignedInt WBRandom::next(void)
{
	// xorshift32.
	m_state ^= (m_state << 13);
	m_state ^= (m_state >> 17);
	m_state ^= (m_state << 5);
	return m_state;
}

Int WBRandom::nextInt(Int limit)
{
	if (limit <= 0)
	{
		return 0;
	}
	// Drop the sign bit before the modulo -- the raw value uses the full 32 bits.
	return (Int)((next() >> 1) % (UnsignedInt)limit);
}

Int WBRandom::nextRange(Int lo, Int hi)
{
	if (hi <= lo)
	{
		return lo;
	}
	return lo + nextInt(hi - lo);
}

Real WBRandom::nextReal(void)
{
	// 24 bits of mantissa is all a float can hold anyway.
	return (Real)(next() >> 8) / (Real)(1 << 24);
}

//=============================================================================
// WBPerlinNoise
//=============================================================================
WBPerlinNoise::WBPerlinNoise(Int seed) :
	m_frequency(0.023f),
	m_amplitude(2.2f),
	m_persistence(0.9f),
	m_octaves(2)
{
	permutate(seed);
}

//=============================================================================
// WBPerlinNoise::permutate
//=============================================================================
/** Builds the permutation table: a shuffle of 0..255, stored twice back to back
	so noise() can index p[x+1] without a bounds check. */
//=============================================================================
void WBPerlinNoise::permutate(Int seed)
{
	Int i;
	for (i = 0; i < PERM_SIZE; i++)
	{
		m_p[i] = i;
	}

	// Fisher-Yates. The original used rejection sampling, which needs an
	// unbounded number of random draws for the same result; this is equivalent
	// and finishes in one pass.
	WBRandom random((UnsignedInt)seed);
	for (i = PERM_SIZE - 1; i > 0; i--)
	{
		Int j = random.nextInt(i + 1);
		Int swap = m_p[i];
		m_p[i] = m_p[j];
		m_p[j] = swap;
	}

	for (i = 0; i < PERM_SIZE; i++)
	{
		m_p[PERM_SIZE + i] = m_p[i];
	}
}

//=============================================================================
// WBPerlinNoise::compute
//=============================================================================
Real WBPerlinNoise::compute(Real x, Real y, Real z)
{
	Real total = 0.0f;
	Real amp = m_amplitude;
	Real freq = m_frequency;

	Int i;
	for (i = 0; i < m_octaves; i++)
	{
		total += noise(x * freq, y * freq, z * freq) * amp;
		freq *= 2.0f;			// each octave doubles the frequency
		amp *= m_persistence;	// ...and fades by the persistence
	}

	// Clamp, don't normalize -- see the note on the declaration.
	if (total < 0.0f)
	{
		return 0.0f;
	}
	if (total > 1.0f)
	{
		return 1.0f;
	}
	return total;
}

//=============================================================================
// WBPerlinNoise::noise
//=============================================================================
Real WBPerlinNoise::noise(Real x, Real y, Real z)
{
	// The unit cube containing the point.
	Int iX = (Int)floor(x) & 255;
	Int iY = (Int)floor(y) & 255;
	Int iZ = (Int)floor(z) & 255;

	// Position within that cube.
	x -= (Real)floor(x);
	y -= (Real)floor(y);
	z -= (Real)floor(z);

	const Real u = fade(x);
	const Real v = fade(y);
	const Real w = fade(z);

	// Hash the 8 corners.
	const Int A = m_p[iX] + iY;
	const Int AA = m_p[A] + iZ;
	const Int AB = m_p[A + 1] + iZ;
	const Int B = m_p[iX + 1] + iY;
	const Int BA = m_p[B] + iZ;
	const Int BB = m_p[B + 1] + iZ;

	return lerp(w, lerp(v, lerp(u, grad(m_p[AA], x, y, z),
									grad(m_p[BA], x - 1, y, z)),
							lerp(u, grad(m_p[AB], x, y - 1, z),
									grad(m_p[BB], x - 1, y - 1, z))),
					lerp(v, lerp(u, grad(m_p[AA + 1], x, y, z - 1),
									grad(m_p[BA + 1], x - 1, y, z - 1)),
							lerp(u, grad(m_p[AB + 1], x, y - 1, z - 1),
									grad(m_p[BB + 1], x - 1, y - 1, z - 1))));
}

//=============================================================================
// WBPerlinNoise::fade
//=============================================================================
Real WBPerlinNoise::fade(Real t)
{
	// 6t^5 - 15t^4 + 10t^3
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

//=============================================================================
// WBPerlinNoise::lerp
//=============================================================================
Real WBPerlinNoise::lerp(Real alpha, Real a, Real b)
{
	return a + alpha * (b - a);
}

//=============================================================================
// WBPerlinNoise::grad
//=============================================================================
/** Turns the low 4 bits of a hash into one of 12 gradient directions. */
//=============================================================================
Real WBPerlinNoise::grad(Int hashCode, Real x, Real y, Real z)
{
	const Int h = hashCode & 15;
	const Real u = (h < 8) ? x : y;
	Real v;
	if (h < 4)
	{
		v = y;
	}
	else if (h == 12 || h == 14)
	{
		v = x;
	}
	else
	{
		v = z;
	}
	return (((h & 1) == 0) ? u : -u) + (((h & 2) == 0) ? v : -v);
}
