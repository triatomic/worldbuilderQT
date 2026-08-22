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

// WBPerlinNoise.h
// Perlin noise and a small deterministic PRNG for the map generator.
//
// Ported from the Genesis map generator (Genesis/Math/PerlinNoise.cs),
// Copyright 2013 The CWC Team, by Daniel Sklenitzka, licensed under the
// Apache License 2.0 -- http://www.apache.org/licenses/LICENSE-2.0
// The noise itself follows Ken Perlin's reference implementation.

#pragma once

#ifndef WB_PERLIN_NOISE_H
#define WB_PERLIN_NOISE_H

#include "always.h"

/*************************************************************************/
/**                            WBRandom
	A tiny self-contained PRNG.

	The generator must be reproducible: the same seed has to give the same map
	every time, which rules out rand() (its sequence isn't guaranteed between
	runtimes, and the engine's own random generator is shared global state that
	other code advances). This is a plain 32-bit xorshift -- cheap, deterministic,
	and good enough for scattering terrain and props.

	Note this does NOT reproduce Genesis's output: that used .NET's System.Random,
	whose sequence can't be recreated here. Maps are reproducible within
	WorldBuilder, not identical to the C# generator's.
***************************************************************************/
class WBRandom
{
public:
	WBRandom(UnsignedInt seed);

	/// Next raw 32-bit value.
	UnsignedInt next(void);
	/// Uniform in [0, limit), or 0 when limit <= 0.
	Int nextInt(Int limit);
	/// Uniform in [lo, hi), or lo when the range is empty.
	Int nextRange(Int lo, Int hi);
	/// Uniform in [0, 1).
	Real nextReal(void);

protected:
	UnsignedInt m_state;
};

/*************************************************************************/
/**                          WBPerlinNoise
	3D Perlin noise, Ken Perlin's reference implementation.
***************************************************************************/
class WBPerlinNoise
{
public:
	WBPerlinNoise(Int seed);

	/// Rebuild the permutation table from a seed.
	void permutate(Int seed);

	void setAmplitude(Real amplitude) {m_amplitude = amplitude;}
	void setFrequency(Real frequency) {m_frequency = frequency;}
	void setPersistence(Real persistence) {m_persistence = persistence;}
	void setOctaves(Int octaves) {m_octaves = octaves;}

	/// Summed octaves, clamped to [0,1].
	///
	/// Deliberately NOT normalized by the total amplitude, which is how the
	/// original behaves: with an amplitude above 1 the lower part of the range
	/// clamps flat at 0, which is what gives this generator its broad plains
	/// broken up by hills rather than uniform bumpiness. Don't "fix" this
	/// without expecting the terrain to change character completely.
	Real compute(Real x, Real y, Real z);

protected:
	Real noise(Real x, Real y, Real z);

	static Real fade(Real t);
	static Real lerp(Real alpha, Real a, Real b);
	static Real grad(Int hashCode, Real x, Real y, Real z);

protected:
	enum {PERM_SIZE = 256};
	Int m_p[PERM_SIZE * 2];	///< the permutation table, doubled so lookups can't run off the end

	Real m_frequency;
	Real m_amplitude;
	Real m_persistence;
	Int m_octaves;
};

#endif // WB_PERLIN_NOISE_H
