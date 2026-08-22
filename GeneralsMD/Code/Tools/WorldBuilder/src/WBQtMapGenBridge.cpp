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

// WBQtMapGenBridge.cpp -- the MFC half of the Map Generator facade.

#include "StdAfx.h"

#ifdef RTS_HAS_QT

#include "qt/panels/WBQtMapGenBridge.h"
#include "MapGen/WBMapGenSettings.h"
#include "WorldBuilderDoc.h"
#include "WHeightMapEdit.h"

extern "C" int WBQtMapGen_GetMinimumSize(int numPlayers)
{
	return (int)WBMapGen_GetMinimumSizeForPlayers((Int)numPlayers);
}

extern "C" void WBQtMapGen_GetCurrentSize(int *widthOut, int *heightOut)
{
	if (widthOut != NULL)
	{
		*widthOut = 0;
	}
	if (heightOut != NULL)
	{
		*heightOut = 0;
	}

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc == NULL)
	{
		return;
	}
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (pMap == NULL)
	{
		return;
	}

	const Int border = pMap->getBorderSizeInline();
	if (widthOut != NULL)
	{
		*widthOut = (int)(pMap->getXExtent() - 2*border);
	}
	if (heightOut != NULL)
	{
		*heightOut = (int)(pMap->getYExtent() - 2*border);
	}
}

#endif // RTS_HAS_QT
