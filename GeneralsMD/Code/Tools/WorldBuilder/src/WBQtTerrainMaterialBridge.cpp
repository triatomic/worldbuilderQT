// WBQtTerrainMaterialBridge.cpp -- the MFC side of the Qt Terrain Material-panel seam.
//
// Plain MFC TU (no Qt include). Implements the reverse callbacks the Qt Terrain Material panel
// (qt/panels/WBQtTerrainMaterialPanel.cpp) fires when its controls change, plus the getters the
// panel reads to seed itself. The Qt static lib resolves these against the exe at the final
// link (extern "C" keeps the names stable).
//
// The texture tree + tile-swatch pixels come straight off WorldHeightMapEdit's statics; the
// tree categorisation reproduces TerrainMaterial::addTerrain() so the Qt tree has the same shape
// as the MFC one. Selection / paint / copy / mirror all forward to the existing TerrainMaterial
// public statics and BigTileTool / FloodFillTool the tools already read, so those keep working
// unchanged. Favorites + the width/height edit boxes route through the guarded TerrainMaterial::
// qt* helpers (defined in TerrainMaterial.cpp) because they live on the hidden MFC dialog.
//
// Whole body is guarded by RTS_HAS_QT, so the default (Qt-off) build compiles this to an empty
// object and the MFC build is unchanged.
#define DEFINE_TERRAIN_TYPE_NAMES		// instantiate terrainTypeNames[] in this TU

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "TerrainMaterial.h"
#include "TileTool.h"
#include "FloodFillTool.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "Common/TerrainTypes.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/TileData.h"
#include "qt/panels/WBQtTerrainMaterialBridge.h"

#ifdef RTS_HAS_QT

//----------------------------------------------------------------------------------------
// Helpers.
//----------------------------------------------------------------------------------------
namespace
{
	void copyString(char *out, int cap, const char *src)
	{
		if (out == NULL || cap <= 0)
		{
			return;
		}
		if (src == NULL)
		{
			out[0] = 0;
			return;
		}
		strncpy(out, src, cap - 1);
		out[cap - 1] = 0;
	}

	// Append one path segment (separated by '\') to a "\"-joined path buffer.
	void appendSegment(char *path, int cap, const char *segment)
	{
		if (path == NULL || cap <= 0 || segment == NULL)
		{
			return;
		}
		int len = (int)strlen(path);
		if (len > 0 && len < cap - 1)
		{
			path[len] = '\\';
			path[len + 1] = 0;
			len++;
		}
		strncpy(path + len, segment, cap - 1 - len);
		path[cap - 1] = 0;
	}
}

extern "C" {

//----------------------------------------------------------------------------------------
// Texture tree enumeration. Reproduces TerrainMaterial::addTerrain(): derive the category
// path + "NN% name" leaf label for one texture class (or report it should be skipped).
//----------------------------------------------------------------------------------------
int WBQtTerrainMaterial_GetTexClassCount(void)
{
	return WorldHeightMapEdit::getNumTexClasses();
}

int WBQtTerrainMaterial_GetTexClassEntry(int texClass, char *pathOut, char *leafOut, int cap)
{
	if (pathOut == NULL || leafOut == NULL || cap <= 0)
	{
		return 0;
	}
	if (texClass < 0 || texClass >= WorldHeightMapEdit::getNumTexClasses())
	{
		return 0;
	}

	pathOut[0] = 0;
	leafOut[0] = 0;

	AsciiString uiName = WorldHeightMapEdit::getTexClassUiName(texClass);
	char pathBuf[_MAX_PATH];
	strncpy(pathBuf, uiName.str(), _MAX_PATH - 2);
	pathBuf[_MAX_PATH - 2] = 0;
	char *pPath = pathBuf;

	TerrainType *terrain = TheTerrainTypes->findTerrain(WorldHeightMapEdit::getTexClassName(texClass));
	Bool doAdd = FALSE;
	char nameBuf[_MAX_PATH];
	nameBuf[0] = 0;

	if (terrain)
	{
		if (terrain->isBlendEdge())
		{
			return 0;	// blend edges are not shown, matching addTerrain
		}
		for (TerrainClass i = TERRAIN_NONE; i < TERRAIN_NUM_CLASSES; i = (TerrainClass)(i + 1))
		{
			if (terrain->getClass() == i)
			{
				appendSegment(pathOut, cap, terrainTypeNames[i]);
				break;
			}
		}
		strcpy(nameBuf, terrain->getName().str());
		doAdd = TRUE;
	}
	else if (!WorldHeightMapEdit::getTexClassIsBlendEdge(texClass))
	{
		// Old-style entries go under an "**Eval" tree, one segment per path directory.
		appendSegment(pathOut, cap, "**Eval");
		Int i = 0;
		while (pPath[i] && i < (Int)sizeof(nameBuf))
		{
			if (pPath[i] == 0)
			{
				return 0;
			}
			if (pPath[i] == '\\' || pPath[i] == '/')
			{
				if (i > 0 && (i > 1 || nameBuf[0] != '.'))	// skip the "." directory
				{
					nameBuf[i] = 0;
					appendSegment(pathOut, cap, nameBuf);
				}
				pPath += i + 1;
				i = 0;
			}
			nameBuf[i] = pPath[i];
			nameBuf[i + 1] = 0;
			doAdd = TRUE;
			i++;
		}
	}

	if (!doAdd)
	{
		return 0;
	}

	Int tilesPerRow = TEXTURE_WIDTH / (2 * TILE_PIXEL_EXTENT + TILE_OFFSET);
	Int availableTiles = 4 * tilesPerRow * tilesPerRow;
	Int percent = (WorldHeightMapEdit::getTexClassNumTiles(texClass) * 100 + availableTiles / 2) / availableTiles;

	char label[_MAX_PATH];
	sprintf(label, "%d%% %s", percent, nameBuf);
	copyString(leafOut, cap, label);
	return 1;
}

int WBQtTerrainMaterial_GetTexClassUiName(int texClass, char *nameOut, int cap)
{
	if (nameOut == NULL || cap <= 0)
	{
		return 0;
	}
	if (texClass < 0 || texClass >= WorldHeightMapEdit::getNumTexClasses())
	{
		return 0;
	}
	copyString(nameOut, cap, WorldHeightMapEdit::getTexClassUiName(texClass).str());
	return 1;
}

//----------------------------------------------------------------------------------------
// Selection. Set the fg class the tools read; validate the size like OnNotify (the class is
// set either way, and the panel warns when it does not fit).
//----------------------------------------------------------------------------------------
int WBQtTerrainMaterial_GetFgTexClass(void)
{
	return TerrainMaterial::getFgTexClass();
}

int WBQtTerrainMaterial_GetBgTexClass(void)
{
	return TerrainMaterial::getBgTexClass();
}

int WBQtTerrainMaterial_SelectFgTexClass(int texClass)
{
	int fits = 1;
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc)
	{
		WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
		if (pMap && !pMap->canFitTexture(texClass))
		{
			fits = 0;
		}
	}
	// setFgTexClass invalidates the swatch + reselects the tree, matching OnNotify's success path.
	TerrainMaterial::setFgTexClass(texClass);
	return fits;
}

void WBQtTerrainMaterial_SwapTextures(void)
{
	// Mirror OnSwapTextures without needing the MFC handler: swap fg/bg via the setters, which
	// invalidate the swatch and reselect the tree.
	Int fg = TerrainMaterial::getFgTexClass();
	Int bg = TerrainMaterial::getBgTexClass();
	TerrainMaterial::setBgTexClass(fg);
	TerrainMaterial::setFgTexClass(bg);
}

int WBQtTerrainMaterial_GetFgLeafName(char *nameOut, int cap)
{
	if (nameOut == NULL || cap <= 0)
	{
		return 0;
	}
	nameOut[0] = 0;
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
	{
		return 0;
	}
	AsciiString name = pDoc->GetHeightMap()->getTexClassUiName(TerrainMaterial::getFgTexClass());
	const char *tName = name.str();
	if (tName == NULL)
	{
		return 0;
	}
	// Last path element, matching updateLabel().
	const char *leaf = tName;
	while (*tName)
	{
		if ((tName[0] == '\\' || tName[0] == '/') && tName[1])
		{
			leaf = tName + 1;
		}
		tName++;
	}
	copyString(nameOut, cap, leaf);
	return 1;
}

//----------------------------------------------------------------------------------------
// Tile swatch pixels. Same fixed-square 32bpp BGRA DIB TerrainSwatches::DrawMyTexture blits.
//----------------------------------------------------------------------------------------
int WBQtTerrainMaterial_GetSwatchExtent(void)
{
	return TILE_PIXEL_EXTENT;
}

int WBQtTerrainMaterial_GetSwatchPixels(int texClass, unsigned char *bgraOut, int cap)
{
	if (bgraOut == NULL)
	{
		return 0;
	}
	int need = TILE_PIXEL_EXTENT * TILE_PIXEL_EXTENT * 4;
	if (cap < need)
	{
		return 0;
	}
	UnsignedByte *pData = WorldHeightMapEdit::getPointerToClassTileData(texClass);
	if (pData == NULL)
	{
		return 0;
	}
	memcpy(bgraOut, pData, need);
	return 1;
}

//----------------------------------------------------------------------------------------
// Brush size / z-height. Route through TerrainMaterial's edit boxes (qtGet/SetWidthEdit etc.)
// so the tools and the hidden MFC edits stay in sync.
//----------------------------------------------------------------------------------------
int WBQtTerrainMaterial_GetWidth(void)
{
	return TerrainMaterial::qtGetWidthEdit();
}

void WBQtTerrainMaterial_SetWidth(int width)
{
	TerrainMaterial::qtSetWidthEdit(width);
}

int WBQtTerrainMaterial_GetHeight(void)
{
	return TerrainMaterial::qtGetHeightEdit();
}

void WBQtTerrainMaterial_SetHeight(int height)
{
	TerrainMaterial::qtSetHeightEdit(height);
}

int WBQtTerrainMaterial_GetMinTileSize(void)
{
	return 2;	// TerrainMaterial::MIN_TILE_SIZE
}

int WBQtTerrainMaterial_GetMaxTileSize(void)
{
	return 100;	// TerrainMaterial::MAX_TILE_SIZE
}

int WBQtTerrainMaterial_GetMinZHeight(void)
{
	return -50;	// TerrainMaterial::MIN_Z_HEIGHT
}

int WBQtTerrainMaterial_GetMaxZHeight(void)
{
	return 100;	// TerrainMaterial::MAX_Z_HEIGHT
}

double WBQtTerrainMaterial_GetFeetPerCell(void)
{
	return 10.0;	// MAP_XY_FACTOR
}

int WBQtTerrainMaterial_IsSingleCell(void)
{
	return TerrainMaterial::qtIsSingleCell();
}

//----------------------------------------------------------------------------------------
// Pathing painting. Mirrors OnPassableCheck / OnPassable / OnImpassable.
//----------------------------------------------------------------------------------------
void WBQtTerrainMaterial_SetPaintPathing(int on)
{
	// Toggle the impassable overlay + rerender exactly like OnPassableCheck.
	Bool isChecked = on ? true : false;
	Bool showImpassable = false;
	if (TheTerrainRenderObject)
	{
		showImpassable = TheTerrainRenderObject->getShowImpassableAreas();
	}
	if (showImpassable != isChecked && TheTerrainRenderObject)
	{
		TheTerrainRenderObject->setShowImpassableAreas(isChecked);
		IRegion2D range = { 0, 0, 0, 0 };
		CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
		if (pDoc)
		{
			WbView3d *p3View = pDoc->GetActive3DView();
			if (p3View)
			{
				p3View->updateHeightMapInView(pDoc->GetHeightMap(), false, range);
			}
		}
	}
	TerrainMaterial::qtSetPaintingPathing(isChecked);
}

int WBQtTerrainMaterial_IsPaintingPathing(void)
{
	return TerrainMaterial::isPaintingPathingInfo() ? 1 : 0;
}

void WBQtTerrainMaterial_SetPassable(int passable)
{
	TerrainMaterial::qtSetPassable(passable ? true : false);
}

int WBQtTerrainMaterial_IsPassable(void)
{
	return TerrainMaterial::isPaintingPassable() ? 1 : 0;
}

//----------------------------------------------------------------------------------------
// Pattern paint mode. Mirrors OnTogglePaintMode / OnPaintModeCombo. The mode names match the
// combo TerrainMaterial::OnInitDialog fills (Blob..Square); modes are 1-based like the MFC combo.
//----------------------------------------------------------------------------------------
static const char *kPaintModeNames[] =
{
	"Blob", "Scatter A", "Scatter B", "Snake", "Octopus", "Circle", "Ring", "Square"
};

int WBQtTerrainMaterial_GetPaintModeCount(void)
{
	return (int)(sizeof(kPaintModeNames) / sizeof(kPaintModeNames[0]));
}

int WBQtTerrainMaterial_GetPaintModeName(int index, char *nameOut, int cap)
{
	if (nameOut == NULL || cap <= 0)
	{
		return 0;
	}
	if (index < 0 || index >= WBQtTerrainMaterial_GetPaintModeCount())
	{
		return 0;
	}
	copyString(nameOut, cap, kPaintModeNames[index]);
	return 1;
}

void WBQtTerrainMaterial_SetPatternPaint(int on)
{
	TerrainMaterial::qtSetPatternPaint(on ? true : false);
}

int WBQtTerrainMaterial_IsPatternPaint(void)
{
	return TerrainMaterial::isTogglePaintMode() ? 1 : 0;
}

void WBQtTerrainMaterial_SetPaintMode(int mode)
{
	TerrainMaterial::qtSetPaintMode(mode);
}

int WBQtTerrainMaterial_GetPaintMode(void)
{
	return TerrainMaterial::getPaintMode();
}

void WBQtTerrainMaterial_SetPaintDensity(int density)
{
	TerrainMaterial::setPaintDensity(density);
}

int WBQtTerrainMaterial_GetPaintDensity(void)
{
	return TerrainMaterial::getPaintDensity();
}

//----------------------------------------------------------------------------------------
// No-mixing toggle. Mirrors OnToggleNoMixing.
//----------------------------------------------------------------------------------------
void WBQtTerrainMaterial_SetNoMixing(int on)
{
	if ((BigTileTool::getEnableNoMixing() ? 1 : 0) != (on ? 1 : 0))
	{
		BigTileTool::toggleNoMixing();
	}
}

int WBQtTerrainMaterial_IsNoMixing(void)
{
	return BigTileTool::getEnableNoMixing() ? 1 : 0;
}

//----------------------------------------------------------------------------------------
// Copy mode. Mirrors OnCopyMode / OnCopyModeTerrain / OnRaiseOnly / OnCopySelect / OnCopyApply
// / OnRotate*.
//----------------------------------------------------------------------------------------
void WBQtTerrainMaterial_SetCopyTextureMode(int on)
{
	TerrainMaterial::qtSetCopyTextureMode(on ? true : false);
}

int WBQtTerrainMaterial_IsCopyTextureMode(void)
{
	return TerrainMaterial::isCopyTextureMode() ? 1 : 0;
}

void WBQtTerrainMaterial_SetCopyTerrainMode(int on)
{
	TerrainMaterial::qtSetCopyTerrainMode(on ? true : false);
}

int WBQtTerrainMaterial_IsCopyTerrainMode(void)
{
	return TerrainMaterial::isCopyTerrainMode() ? 1 : 0;
}

void WBQtTerrainMaterial_SetRaiseOnly(int on)
{
	TerrainMaterial::qtSetRaiseOnly(on ? true : false);
}

int WBQtTerrainMaterial_IsRaiseOnly(void)
{
	return TerrainMaterial::isRaiseOnly() ? 1 : 0;
}

void WBQtTerrainMaterial_SetCopySelectMode(void)
{
	TerrainMaterial::qtSetCopySelectMode();
}

int WBQtTerrainMaterial_IsCopySelectMode(void)
{
	return TerrainMaterial::isCopySelectMode() ? 1 : 0;
}

void WBQtTerrainMaterial_SetCopyApplyMode(void)
{
	TerrainMaterial::qtSetCopyApplyMode();
}

int WBQtTerrainMaterial_IsCopyApplyMode(void)
{
	return TerrainMaterial::isCopyApplyMode() ? 1 : 0;
}

void WBQtTerrainMaterial_SetCopyRotation(int degrees)
{
	TerrainMaterial::qtSetCopyRotation(degrees);
}

int WBQtTerrainMaterial_GetCopyRotation(void)
{
	return TerrainMaterial::getCopyRotation();
}

//----------------------------------------------------------------------------------------
// Mirror toggles. Mirrors OnToggleMirror / OnToggleMirrorX/Y/XY (both tools).
//----------------------------------------------------------------------------------------
void WBQtTerrainMaterial_ToggleMirror(void)
{
	BigTileTool::toggleMirror();
	FloodFillTool::toggleMirror();
}

int WBQtTerrainMaterial_GetMirror(void)
{
	return BigTileTool::getEnableMirror() ? 1 : 0;
}

void WBQtTerrainMaterial_ToggleMirrorX(void)
{
	BigTileTool::toggleMirrorX();
	FloodFillTool::toggleMirrorX();
}

int WBQtTerrainMaterial_GetMirrorX(void)
{
	return BigTileTool::getMirrorX() ? 1 : 0;
}

void WBQtTerrainMaterial_ToggleMirrorY(void)
{
	BigTileTool::toggleMirrorY();
	FloodFillTool::toggleMirrorY();
}

int WBQtTerrainMaterial_GetMirrorY(void)
{
	return BigTileTool::getMirrorY() ? 1 : 0;
}

void WBQtTerrainMaterial_ToggleMirrorXY(void)
{
	BigTileTool::toggleMirrorXY();
	FloodFillTool::toggleMirrorXY();
}

int WBQtTerrainMaterial_GetMirrorXY(void)
{
	return BigTileTool::getMirrorDiag() ? 1 : 0;
}

//----------------------------------------------------------------------------------------
// Favorites. Forward to the guarded TerrainMaterial::qtFav* helpers (they own m_favTreeView).
//----------------------------------------------------------------------------------------
int WBQtTerrainMaterial_GetFavoriteCount(void)
{
	return TerrainMaterial::qtGetFavoriteCount();
}

int WBQtTerrainMaterial_GetFavorite(int index, char *nameOut, int cap, int *texClassOut)
{
	return TerrainMaterial::qtGetFavorite(index, nameOut, cap, texClassOut) ? 1 : 0;
}

int WBQtTerrainMaterial_AddFavorite(int texClass, const char *label)
{
	return TerrainMaterial::qtAddFavorite(texClass, label) ? 1 : 0;
}

void WBQtTerrainMaterial_DeleteFavorite(int index)
{
	TerrainMaterial::qtDeleteFavorite(index);
}

int WBQtTerrainMaterial_ImportFavorites(void)
{
	return TerrainMaterial::qtImportFavorites();
}

}
#endif
