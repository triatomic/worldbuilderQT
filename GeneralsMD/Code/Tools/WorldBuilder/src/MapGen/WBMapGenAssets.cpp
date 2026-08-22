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

// WBMapGenAssets.cpp
// Picks the objects and textures the generator decorates a map with.

#include "StdAfx.h"
#include "MapGen/WBMapGenAssets.h"

#include "WHeightMapEdit.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingSort.h"
#include "Common/KindOf.h"
#include "Common/TerrainTypes.h"
#include "GameClient/TerrainRoads.h"

#include <string.h>

//=============================================================================
WBMapGenAssets::WBMapGenAssets() :
	m_groundTexture(-1),
	m_cliffTexture(-1)
{
}

//=============================================================================
// WBMapGenAssets::gather
//=============================================================================
void WBMapGenAssets::gather(void)
{
	gatherObjects();
	gatherTextures();
}

//=============================================================================
// WBMapGenAssets::gatherObjects
//=============================================================================
/** Collects the trees and rocks the loaded game data provides.

	The editor sorting is what the object palette itself groups by, so this picks
	up exactly what a mapper would find under those headings -- including a mod's
	own objects, with no list to keep up to date.
*/
//=============================================================================
void WBMapGenAssets::gatherObjects(void)
{
	m_trees.clear();
	m_rocks.clear();
	m_supplySource.clear();
	m_road.clear();

	if (TheThingFactory == NULL)
	{
		return;
	}

	const ThingTemplate *pTemplate;
	for (pTemplate = TheThingFactory->firstTemplate(); pTemplate;
			 pTemplate = pTemplate->friend_getNextTemplate())
	{
		const EditorSortingType sorting = pTemplate->getEditorSorting();
		if (sorting == ES_SHRUBBERY)
		{
			m_trees.push_back(pTemplate->getName());
		}
		else if (sorting == ES_MISC_NATURAL)
		{
			m_rocks.push_back(pTemplate->getName());
		}

		// Remember the first supply source as a fallback, in case this install
		// doesn't have the one we'd rather use.
		if (m_supplySource.isEmpty() && pTemplate->isKindOf(KINDOF_SUPPLY_SOURCE))
		{
			m_supplySource = pTemplate->getName();
		}
	}

	// Prefer the plain supply dock: it is the one mappers reach for, and the
	// KINDOF scan above would otherwise pick whichever supply object the game
	// data happens to define first (a crate, a hidden variant, a mod's own).
	{
		static const char *preferred[] = {"SupplyDock", "SupplyWarehouse"};
		Int i;
		for (i = 0; i < (Int)(sizeof(preferred)/sizeof(preferred[0])); i++)
		{
			const ThingTemplate *pFound = TheThingFactory->findTemplate(AsciiString(preferred[i]), FALSE);
			if (pFound != NULL)
			{
				m_supplySource = pFound->getName();
				break;
			}
		}
	}

	gatherRoad();
}

//=============================================================================
// WBMapGenAssets::gatherRoad
//=============================================================================
/** Picks a road type.

	Roads are NOT object templates: they are terrain road types, and they live in
	TheTerrainRoads rather than the thing factory. Looking them up in the object
	catalogue finds nothing at all, which is silent -- the generator simply lays
	no roads.

	A plain two-lane road reads as a main route; the first road in the data could
	just as easily be a dirt track or something decorative, so a preferred name is
	tried before falling back to whatever exists.
*/
//=============================================================================
void WBMapGenAssets::gatherRoad(void)
{
	m_road.clear();

	if (TheTerrainRoads == NULL)
	{
		return;
	}

	static const char *preferredRoads[] =
	{
		"TwoLaneDarkDotted", "TwoLaneDark", "FourLaneDark", "DirtRoad"
	};

	Int i;
	for (i = 0; i < (Int)(sizeof(preferredRoads)/sizeof(preferredRoads[0])); i++)
	{
		TerrainRoadType *pRoad = TheTerrainRoads->findRoad(AsciiString(preferredRoads[i]));
		if (pRoad != NULL)
		{
			m_road = pRoad->getName();
			return;
		}
	}

	// Nothing preferred is present: take the first road this install defines.
	TerrainRoadType *pFirst = TheTerrainRoads->firstRoad();
	if (pFirst != NULL)
	{
		m_road = pFirst->getName();
	}
}

//=============================================================================
// WBMapGenAssets::findTextureClass
//=============================================================================
/** Finds a loaded texture class whose name contains any of the given words.

	Earlier words win, so callers list their preferences most-wanted first.
*/
//=============================================================================
Int WBMapGenAssets::findTextureClass(const char *const *words, Int numWords)
{
	const Int numClasses = WorldHeightMapEdit::getNumTexClasses();

	Int wordNdx;
	for (wordNdx = 0; wordNdx < numWords; wordNdx++)
	{
		const char *word = words[wordNdx];
		if (word == NULL || word[0] == 0)
		{
			continue;
		}

		Int i;
		for (i = 0; i < numClasses; i++)
		{
			AsciiString name = WorldHeightMapEdit::getTexClassName(i);
			if (name.isEmpty())
			{
				continue;
			}
			// Case-insensitive substring match: texture names vary in casing
			// between the stock set and mod sets.
			if (_stricmp(name.str(), word) == 0 || strstr(name.str(), word) != NULL)
			{
				return i;
			}
		}
	}
	return -1;
}

//=============================================================================
// WBMapGenAssets::gatherTextures
//=============================================================================
/** Chooses a ground and a cliff texture from whatever the map has loaded.

	Only the textures already in the map's texture set can be used: adding new
	ones is a per-map allocation with a hard limit, and a generator that quietly
	filled that budget would break later hand-editing. So this picks the best
	available rather than importing anything.
*/
//=============================================================================
void WBMapGenAssets::gatherTextures(void)
{
	m_groundTexture = -1;
	m_cliffTexture = -1;

	// Preference order, most wanted first.
	static const char *groundWords[] = {"Grass", "Dirt", "Sand", "Snow"};
	static const char *cliffWords[] = {"Cliff", "Rock", "Mountain"};

	m_groundTexture = findTextureClass(groundWords,
																		 sizeof(groundWords)/sizeof(groundWords[0]));
	m_cliffTexture = findTextureClass(cliffWords,
																		sizeof(cliffWords)/sizeof(cliffWords[0]));

	// Nothing recognisable: fall back to the first class the map has, so the
	// generator still does something sensible rather than nothing at all.
	if (m_groundTexture < 0 && WorldHeightMapEdit::getNumTexClasses() > 0)
	{
		m_groundTexture = 0;
	}
}
