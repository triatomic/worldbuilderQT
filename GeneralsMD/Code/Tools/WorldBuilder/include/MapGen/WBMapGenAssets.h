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

// WBMapGenAssets.h
// Picks the objects and textures the generator decorates a map with.

#pragma once

#ifndef WB_MAP_GEN_ASSETS_H
#define WB_MAP_GEN_ASSETS_H

#include "always.h"
#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include <vector>

/*************************************************************************/
/**                          WBMapGenAssets
	The trees, rocks and textures available on this install.

	Everything is looked up from the loaded game data rather than hardcoded. The
	generator this was ported from shipped fixed lists of its own mod's asset
	names, which simply don't exist in a stock game -- a map generated from those
	would be full of missing objects. Reading what is actually loaded means the
	generator works on vanilla Zero Hour and on any mod, using whatever that mod
	happens to provide.
***************************************************************************/
class WBMapGenAssets
{
public:
	WBMapGenAssets();

	/// Reads the object catalogue and the map's texture set.
	void gather(void);

	Int getNumTrees(void) const {return (Int)m_trees.size();}
	const AsciiString &getTree(Int ndx) const {return m_trees[ndx];}

	Int getNumRocks(void) const {return (Int)m_rocks.size();}
	const AsciiString &getRock(Int ndx) const {return m_rocks[ndx];}

	/// Texture class index for open ground, or -1 when nothing suitable was found.
	Int getGroundTexture(void) const {return m_groundTexture;}
	/// Texture class index for cliff faces, or -1.
	Int getCliffTexture(void) const {return m_cliffTexture;}

protected:
	void gatherObjects(void);
	void gatherTextures(void);

	/// Finds a texture class whose name contains one of the given words.
	static Int findTextureClass(const char *const *words, Int numWords);

protected:
	std::vector<AsciiString> m_trees;
	std::vector<AsciiString> m_rocks;
	Int m_groundTexture;
	Int m_cliffTexture;
};

#endif // WB_MAP_GEN_ASSETS_H
