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

#include "StdAfx.h"
#include "resource.h"
#include "MinimapDialog.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "MainFrm.h"
#include "wbview3d.h"
#include "Common/MapObject.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingSort.h"
#include "Common/KindOf.h"
#include "Common/WellKnownKeys.h"
#include "Common/PlayerTemplate.h"
#include "Common/GlobalData.h"
#include "GameLogic/SidesList.h"
#include "GameClient/TerrainRoads.h"
#include "Common/FileSystem.h"
#include "Common/MapReaderWriterInfo.h"
#include "W3DDevice/GameClient/TileData.h"

Bool localIsUnderwater(Real x, Real y);
static void clearRoadTexCache();
Bool localHasWaterAreas(void);

MinimapDialog *TheMinimapDialog = NULL;

// Coalesce a burst of terrain edits into one rebuild, like the game radar's
// throttled refresh, so painting stays smooth instead of resampling on every
// height change.
static const UINT_PTR MINIMAP_REBUILD_TIMER = 0xB01;	// arbitrary timer id

// Display size of the dialog client (the buffer is stretched to fill it).
static const int MINIMAP_DISPLAY_SIZE = 256;

BEGIN_MESSAGE_MAP(MinimapDialog, CDialog)
	ON_WM_PAINT()
	ON_WM_MOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
END_MESSAGE_MAP()

MinimapDialog::MinimapDialog(CWnd *pParent)
	: CDialog(MinimapDialog::IDD, pParent),
	  m_pixelBuffer(NULL),
	  m_terrainBuffer(NULL),
	  m_terrainValid(false),
	  m_resolution(MINIMAP_RES_DEFAULT),
	  m_terrainBuilt(false),
	  m_dragging(false),
	  m_rebuildPending(false),
	  m_showObjects(true),
	  m_showRoads(true),
	  m_refreshDelayMs(250)
{
	// Load persisted config (clamp to valid ranges).
	m_resolution     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "Resolution", MINIMAP_RES_DEFAULT);
	m_showObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowObjects", 1) ? true : false;
	m_showRoads      = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowRoads", 1) ? true : false;
	m_refreshDelayMs = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "RefreshDelayMs", 250);
	if (m_resolution < MINIMAP_RES_MIN) m_resolution = MINIMAP_RES_MIN;
	if (m_resolution > MINIMAP_RES_MAX) m_resolution = MINIMAP_RES_MAX;
	if (m_refreshDelayMs < 0) m_refreshDelayMs = 0;

	allocBuffer();
}

MinimapDialog::~MinimapDialog()
{
	if (TheMinimapDialog == this)
		TheMinimapDialog = NULL;
	delete [] m_pixelBuffer;
	delete [] m_terrainBuffer;
	clearRoadTexCache();
}

void MinimapDialog::allocBuffer()
{
	delete [] m_pixelBuffer;
	delete [] m_terrainBuffer;
	Int n = m_resolution * m_resolution;
	m_pixelBuffer   = new UnsignedInt[n];
	m_terrainBuffer = new UnsignedInt[n];
	memset(m_pixelBuffer,   0, sizeof(UnsignedInt) * n);
	memset(m_terrainBuffer, 0, sizeof(UnsignedInt) * n);
	m_terrainBuilt = false;
	m_terrainValid = false;
}

BOOL MinimapDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	TheMinimapDialog = this;

	// Show the terrain buffer in a comfortably sized square client area; OnPaint
	// stretches the buffer to fit, so the display size is independent of resolution.
	CRect clientRect;
	GetClientRect(&clientRect);
	if (clientRect.Width() != MINIMAP_DISPLAY_SIZE || clientRect.Height() != MINIMAP_DISPLAY_SIZE)
	{
		CRect winRect;
		GetWindowRect(&winRect);
		int borderW = winRect.Width() - clientRect.Width();
		int borderH = winRect.Height() - clientRect.Height();
		SetWindowPos(NULL, 0, 0, MINIMAP_DISPLAY_SIZE + borderW, MINIMAP_DISPLAY_SIZE + borderH,
			SWP_NOMOVE | SWP_NOZORDER);
	}

	return TRUE;
}

void MinimapDialog::setShowObjects(Bool show)
{
	m_showObjects = show;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "ShowObjects", show ? 1 : 0);
	if (IsWindowVisible())
	{
		// Toggling objects doesn't change terrain; reuse the cached resample if we have one.
		if (m_terrainValid)
			refreshObjects();
		else
			rebuildTerrain();
	}
}

void MinimapDialog::setShowRoads(Bool show)
{
	m_showRoads = show;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "ShowRoads", show ? 1 : 0);
	if (IsWindowVisible())
	{
		// Roads are composited over the cached terrain; reuse the resample if valid.
		if (m_terrainValid)
			refreshObjects();
		else
			rebuildTerrain();
	}
}

void MinimapDialog::setRefreshDelayMs(Int ms)
{
	if (ms < 0) ms = 0;
	m_refreshDelayMs = ms;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "RefreshDelayMs", ms);
}

void MinimapDialog::setResolution(Int res)
{
	if (res < MINIMAP_RES_MIN) res = MINIMAP_RES_MIN;
	if (res > MINIMAP_RES_MAX) res = MINIMAP_RES_MAX;
	if (res == m_resolution)
		return;
	m_resolution = res;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Resolution", res);
	allocBuffer();
	if (IsWindowVisible())
		rebuildTerrain();
}

void MinimapDialog::OnCancel()
{
	ShowWindow(SW_HIDE);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowMinimap", 0);
}

void MinimapDialog::OnOK()
{
}

void MinimapDialog::OnMove(int x, int y)
{
	CDialog::OnMove(x, y);
	CRect rect;
	GetWindowRect(&rect);
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Top", rect.top);
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "Left", rect.left);
}

void MinimapDialog::requestRebuild(Bool terrainChanged)
{
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;

	// Object-only / camera changes are cheap (recomposite cached terrain, no resample),
	// just like the in-game radar. Refresh them immediately so units/buildings and the
	// view box track instantly, with no throttle delay. Only when the cached terrain is
	// stale (a terrain edit is pending) do we fall through to the debounced resample.
	if (!terrainChanged && m_terrainValid)
	{
		refreshObjects();
		return;
	}

	// 0 = manual: don't auto-rebuild on terrain edits (only on load/toggle).
	if (m_refreshDelayMs <= 0)
		return;

	// A terrain change invalidates the cached terrain resample; an object-only change
	// can reuse it (cheap recomposite). If any pending request is a terrain change, the
	// coalesced rebuild must do the full resample.
	if (terrainChanged)
		m_terrainValid = false;

	// (Re)start the one-shot throttle timer; the actual work happens in OnTimer once
	// edits stop arriving for m_refreshDelayMs.
	m_rebuildPending = true;
	SetTimer(MINIMAP_REBUILD_TIMER, (UINT)m_refreshDelayMs, NULL);
}

void MinimapDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == MINIMAP_REBUILD_TIMER)
	{
		KillTimer(MINIMAP_REBUILD_TIMER);
		if (m_rebuildPending)
		{
			m_rebuildPending = false;
			// Reuse the cached terrain when only objects changed; otherwise resample.
			if (m_terrainValid)
				refreshObjects();
			else
				rebuildTerrain();
		}
		return;
	}
	CDialog::OnTimer(nIDEvent);
}

void MinimapDialog::OnPaint()
{
	CPaintDC dc(this);

	if (!m_terrainBuilt)
		return;

	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = m_resolution;
	bmi.bmiHeader.biHeight = -m_resolution;	// negative = top-down DIB (row 0 is the top scanline)
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	// Stretch the terrain buffer to fill the dialog's client area.
	CRect clientRect;
	GetClientRect(&clientRect);

	SetStretchBltMode(dc.m_hDC, COLORONCOLOR);
	StretchDIBits(dc.m_hDC,
		0, 0, clientRect.Width(), clientRect.Height(),
		0, 0, m_resolution, m_resolution,
		m_pixelBuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);

	// Draw the camera view box as a GDI overlay at full client resolution (not baked
	// into the low-res buffer), so the lines are smooth and the thickness is crisp
	// regardless of the sampling resolution.
	drawViewBoxOverlay(dc.m_hDC, clientRect.Width(), clientRect.Height());
}

void MinimapDialog::centerViewAtClient(CPoint point)
{
	// The 128x128 buffer is stretched to fill the client area; map the client-space
	// point back to a minimap cell.
	CRect clientRect;
	GetClientRect(&clientRect);
	Int cw = clientRect.Width()  > 0 ? clientRect.Width()  : m_resolution;
	Int ch = clientRect.Height() > 0 ? clientRect.Height() : m_resolution;

	// Clamp to the client so dragging off the edge still tracks sensibly.
	if (point.x < 0) point.x = 0;
	if (point.x >= cw) point.x = cw - 1;
	if (point.y < 0) point.y = 0;
	if (point.y >= ch) point.y = ch - 1;

	Int cellX = (point.x * m_resolution) / cw;
	Int cellY = (point.y * m_resolution) / ch;

	// Buffer row 0 is the top (world-y = m_resolution-1), so flip to world-y.
	Real worldX, worldY;
	if (!minimapToWorld(cellX, m_resolution - 1 - cellY, &worldX, &worldY))
		return;

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return;

	// minimapToWorld returns the camera center in heightmap CELL units (index +
	// border). setCenterInView expects exactly that: it constrains against the map
	// extent and setupCamera multiplies by MAP_XY_FACTOR to get the world position.
	// (Do NOT pre-multiply by MAP_XY_FACTOR here, or the camera flies off the map
	// and the viewport shows only the gray clear color.)
	// Use the deferred variant so the 3D view renders from its own paint loop rather
	// than re-entrantly from this control bar's message handler.
	WbView3d *p3d = pDoc->Get3DView();
	if (p3d)
		p3d->setCenterInViewDeferred(worldX, worldY);
}

void MinimapDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_dragging = true;
	SetCapture();
	centerViewAtClient(point);
}

void MinimapDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragging && (nFlags & MK_LBUTTON))
		centerViewAtClient(point);
	else
		CDialog::OnMouseMove(nFlags, point);
}

void MinimapDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		m_dragging = false;
		if (::GetCapture() == m_hWnd)
			::ReleaseCapture();
	}
	CDialog::OnLButtonUp(nFlags, point);
}

Bool MinimapDialog::minimapToWorld(Int mx, Int my, Real *worldX, Real *worldY)
{
	if (!worldX || !worldY)
		return FALSE;

	if (mx < 0) mx = 0;
	if (mx >= m_resolution) mx = m_resolution - 1;
	if (my < 0) my = 0;
	if (my >= m_resolution) my = m_resolution - 1;

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return FALSE;

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
		return FALSE;

	Real xSample = INT_TO_REAL(pMap->getXExtent() - (2 * pMap->getBorderSize())) / (Real)m_resolution;
	Real ySample = INT_TO_REAL(pMap->getYExtent() - (2 * pMap->getBorderSize())) / (Real)m_resolution;

	*worldX = mx * xSample + pMap->getBorderSize();
	*worldY = my * ySample + pMap->getBorderSize();
	return TRUE;
}

void MinimapDialog::interpolateColorForHeight(RGBColor *color,
	Real height, Real hiZ, Real midZ, Real loZ)
{
	const Real howBright = 0.30f;	// match MapPreview / radar (bigger is brighter)
	const Real howDark   = 0.60f;	// match MapPreview / radar (bigger is darker)

	if (hiZ == midZ) hiZ = midZ + 0.1f;
	if (midZ == loZ) loZ = midZ - 0.1f;
	if (hiZ == loZ) hiZ = loZ + 0.2f;

	Real t;
	RGBColor colorTarget;

	if (height >= midZ)
	{
		t = (height - midZ) / (hiZ - midZ);
		colorTarget.red   = color->red   + (1.0f - color->red)   * howBright;
		colorTarget.green = color->green + (1.0f - color->green) * howBright;
		colorTarget.blue  = color->blue  + (1.0f - color->blue)  * howBright;
	}
	else
	{
		t = (midZ - height) / (midZ - loZ);
		colorTarget.red   = color->red   + (0.0f - color->red)   * howDark;
		colorTarget.green = color->green + (0.0f - color->green) * howDark;
		colorTarget.blue  = color->blue  + (0.0f - color->blue)  * howDark;
	}

	color->red   = color->red   + (colorTarget.red   - color->red)   * t;
	color->green = color->green + (colorTarget.green - color->green) * t;
	color->blue  = color->blue  + (colorTarget.blue  - color->blue)  * t;

	if (color->red   < 0.0f) color->red   = 0.0f;
	if (color->red   > 1.0f) color->red   = 1.0f;
	if (color->green < 0.0f) color->green = 0.0f;
	if (color->green > 1.0f) color->green = 1.0f;
	if (color->blue  < 0.0f) color->blue  = 0.0f;
	if (color->blue  > 1.0f) color->blue  = 1.0f;
}

void MinimapDialog::rebuildTerrain()
{
	// Road textures are tied to the map; clear the cache so the next drawRoads()
	// reloads textures fresh (handles map close/reopen and resolution changes).
	clearRoadTexCache();

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return;

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
		return;

	RGBColor waterColor;
	waterColor.red = 0.55f;
	waterColor.green = 0.55f;
	waterColor.blue = 1.0f;

	// Day/night tint: getTerrainColorAt returns the raw texture color (no lighting),
	// so apply the current time-of-day terrain lighting ourselves the way the 3D scene
	// does (ambient + sun diffuse), so the minimap darkens at night like the viewport.
	// Toggled with Ctrl+D (WbView3d::stepTimeOfDay changes TheGlobalData->m_timeOfDay).
	Real tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
	if (TheGlobalData)
	{
		const GlobalData::TerrainLighting *tl =
			&TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][0];	// 0 = sun
		tintR = tl->ambient.red   + tl->diffuse.red;
		tintG = tl->ambient.green + tl->diffuse.green;
		tintB = tl->ambient.blue  + tl->diffuse.blue;
		if (tintR > 1.0f) tintR = 1.0f;  if (tintR < 0.0f) tintR = 0.0f;
		if (tintG > 1.0f) tintG = 1.0f;  if (tintG < 0.0f) tintG = 0.0f;
		if (tintB > 1.0f) tintB = 1.0f;  if (tintB < 0.0f) tintB = 0.0f;
	}

	const Int   border = pMap->getBorderSize();
	const Real  xSample = INT_TO_REAL(pMap->getXExtent() - (2 * border)) / (Real)m_resolution;
	const Real  ySample = INT_TO_REAL(pMap->getYExtent() - (2 * border)) / (Real)m_resolution;
	const Int   res = m_resolution;

	// Precompute the per-index heightmap coords (world cell) and terrain-sample coords
	// (MAP_XY_FACTOR-scaled, border removed) once per row/column, instead of redoing
	// the multiplies for all 9 neighbors of every pixel.
	Real *cellX = new Real[res];	// heightmap index along X (for getHeight)
	Real *cellY = new Real[res];	// heightmap index along Y
	Real *mapX  = new Real[res];	// scaled sample coord along X (for getTerrainColorAt / water)
	Real *mapY  = new Real[res];	// scaled sample coord along Y
	for (Int i = 0; i < res; ++i)
	{
		cellX[i] = i * xSample + border;
		cellY[i] = i * ySample + border;
		mapX[i]  = MAP_XY_FACTOR * (i * xSample);	// == MAP_XY_FACTOR * (cell - border)
		mapY[i]  = MAP_XY_FACTOR * (i * ySample);
	}

	// Height stats prepass (reuse precomputed cell coords).
	Real maxHeight = -10000.0f;
	Real minHeight = 10000.0f;
	Real avgHeight = 0.0f;
	Int count = 0;
	for (Int y = 0; y < res; ++y)
	{
		for (Int x = 0; x < res; ++x)
		{
			Real h = pMap->getHeight(cellX[x], cellY[y]);
			avgHeight += h;
			if (h > maxHeight) maxHeight = h;
			if (h < minHeight) minHeight = h;
			++count;
		}
	}
	if (count > 0) avgHeight /= count;

	// Most maps have no water areas; if so, skip every localIsUnderwater scan (each
	// of which loops all polygon triggers) — the single biggest per-pixel cost.
	const Bool hasWater = localHasWaterAreas();

	for (Int y = 0; y < res; ++y)
	{
		for (Int x = 0; x < res; ++x)
		{
			Real z = pMap->getHeight(cellX[x], cellY[y]);

			RGBColor color;
			RGBColor sampleColor;
			Int samples = 0;
			sampleColor.red = sampleColor.green = sampleColor.blue = 0.0f;

			if (hasWater && localIsUnderwater(mapX[x], mapY[y]))
			{
				for (Int j = y - 1; j <= y + 1; ++j)
				{
					if (j < 0 || j >= res) continue;
					for (Int i = x - 1; i <= x + 1; ++i)
					{
						if (i < 0 || i >= res) continue;
						if (localIsUnderwater(mapX[i], mapY[j]))
						{
							color = waterColor;
							Real underwaterZ = pMap->getHeight(cellX[i], cellY[j]);
							interpolateColorForHeight(&color, underwaterZ,
								pMap->getMaxHeightValue(), avgHeight, pMap->getMinHeightValue());
							sampleColor.red   += color.red;
							sampleColor.green += color.green;
							sampleColor.blue  += color.blue;
							++samples;
						}
					}
				}
			}
			else
			{
				for (Int j = y - 1; j <= y + 1; ++j)
				{
					if (j < 0 || j >= res) continue;
					for (Int i = x - 1; i <= x + 1; ++i)
					{
						if (i < 0 || i >= res) continue;
						pMap->getTerrainColorAt(mapX[i], mapY[j], &color);
						interpolateColorForHeight(&color, z, maxHeight, avgHeight, minHeight);
						sampleColor.red   += color.red;
						sampleColor.green += color.green;
						sampleColor.blue  += color.blue;
						++samples;
					}
				}
			}

			if (samples == 0) samples = 1;
			Real inv = 1.0f / (Real)samples;
			color.red   = sampleColor.red   * inv * tintR;
			color.green = sampleColor.green * inv * tintG;
			color.blue  = sampleColor.blue  * inv * tintB;

			// 32-bit BI_RGB DIB: 0x00RRGGBB (byte order B,G,R,X). Top-down DIB
			// (biHeight<0) => buffer row 0 is the top scanline, so write world-row
			// y to buffer row (res-1 - y) to keep the map right-side up. Terrain goes
			// into the cached terrain buffer; objects are composited afterward.
			m_terrainBuffer[(res - 1 - y) * res + x] =
				(REAL_TO_INT(color.blue * 255))       |
				(REAL_TO_INT(color.green * 255) << 8)  |
				(REAL_TO_INT(color.red * 255) << 16)   |
				(255 << 24);
		}
	}

	delete [] cellX;
	delete [] cellY;
	delete [] mapX;
	delete [] mapY;

	m_terrainValid = true;		// cached terrain is now current

	// Composite terrain + objects into the displayed buffer.
	refreshObjects();
	return;
}

// Cheap path: copy the cached terrain into the displayed buffer and overlay objects,
// without resampling the terrain. Used when only objects changed.
void MinimapDialog::refreshObjects()
{
	Int n = m_resolution * m_resolution;
	memcpy(m_pixelBuffer, m_terrainBuffer, sizeof(UnsignedInt) * n);

	// Roads first (under object dots); independently toggleable from objects.
	if (m_showRoads)
		drawRoads();

	if (m_showObjects)
		drawObjects();

	// The camera view box is drawn as a GDI overlay in OnPaint (display resolution),
	// not baked into the buffer.

	m_terrainBuilt = true;
	if (IsWindow(m_hWnd))
		Invalidate(FALSE);
}

// Resolve a MapObject's house color exactly the way the 3D view does (and the game
// radar effectively does): originalOwner -> team -> side -> explicit playerColor
// override, else the faction PlayerTemplate's preferred color. Returns 0x00RRGGBB.
// 0xFFFFFF (white) for neutral / unowned.
static Int getMapObjectHouseColor(MapObject *pObj)
{
	Int playerColor = 0xFFFFFF;
	Bool exists = false;
	AsciiString owner = pObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
	if (!exists)
		return playerColor;

	TeamsInfo *teamInfo = TheSidesList->findTeamInfo(owner);
	if (!teamInfo)
		return playerColor;

	AsciiString teamOwner = teamInfo->getDict()->getAsciiString(TheKey_teamOwner);
	SidesInfo *pSide = TheSidesList->findSideInfo(teamOwner);
	if (!pSide)
		return playerColor;

	Bool hasColor = false;
	Int color = pSide->getDict()->getInt(TheKey_playerColor, &hasColor);
	if (hasColor)
		return color;

	AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
	const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
	if (pt)
		playerColor = pt->getPreferredColor()->getAsInt();
	return playerColor;
}

// 0x00RRGGBB -> the buffer's BGRA word (opaque).
static inline UnsignedInt packBGRA(Int rgb)
{
	Int r = (rgb >> 16) & 0xFF;
	Int g = (rgb >> 8)  & 0xFF;
	Int b =  rgb        & 0xFF;
	return (UnsignedInt)(b | (g << 8) | (r << 16) | (255 << 24));
}

// Resource structures (supply docks + oil derricks) get a distinct solid-black
// marker, matching the Thrax minimap's only building-type-specific styling.
static Bool isResourceStructure(const ThingTemplate *t)
{
	if (!t)
		return FALSE;
	if (t->isKindOf(KINDOF_SUPPLY_SOURCE))
		return TRUE;
	return t->getName() == "TechOilDerrick";
}

// Fill an axis-aligned rect (centered at cx,cy in top-down buffer space) with color,
// clipped to the buffer. w/h are full extents.
void MinimapDialog::fillRect(Int cx, Int cy, Int w, Int h, UnsignedInt color)
{
	Int left = cx - w / 2;
	Int top  = cy - h / 2;
	for (Int yy = top; yy < top + h; ++yy)
	{
		if (yy < 0 || yy >= m_resolution) continue;
		for (Int xx = left; xx < left + w; ++xx)
		{
			if (xx < 0 || xx >= m_resolution) continue;
			pixel(xx, yy) = color;
		}
	}
}

// Fill a diamond (rotated square) of the given size (centered at cx,cy in top-down
// buffer space) with color, clipped to the buffer. Used for units so they read as
// diamonds vs. the square building markers. 'size' is the full diagonal extent.
void MinimapDialog::fillDiamond(Int cx, Int cy, Int size, UnsignedInt color)
{
	if (size < 1) size = 1;
	Int r = size / 2;
	if (r < 1)
	{
		// Single pixel for tiny blips so it doesn't vanish at low resolution.
		if (cx >= 0 && cx < m_resolution && cy >= 0 && cy < m_resolution)
			pixel(cx, cy) = color;
		return;
	}

	for (Int dy = -r; dy <= r; ++dy)
	{
		Int yy = cy + dy;
		if (yy < 0 || yy >= m_resolution) continue;
		Int span = r - (dy < 0 ? -dy : dy);		// |dx| + |dy| <= r
		for (Int dx = -span; dx <= span; ++dx)
		{
			Int xx = cx + dx;
			if (xx < 0 || xx >= m_resolution) continue;
			pixel(xx, yy) = color;
		}
	}
}

// Map a world position to a minimap buffer cell (top-down, row already flipped).
// Always writes a cell CLAMPED to the buffer; returns FALSE if the point was off-map
// (caller may skip object dots) or TRUE if in-range. Shared by dots and the view box.
Bool MinimapDialog::worldToMinimap(Real worldX, Real worldY, Int *mx, Int *my)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return FALSE;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return FALSE;

	Int border = pMap->getBorderSize();
	Real xSpan = INT_TO_REAL(pMap->getXExtent() - 2 * border);
	Real ySpan = INT_TO_REAL(pMap->getYExtent() - 2 * border);
	if (xSpan <= 0.0f || ySpan <= 0.0f) return FALSE;

	// Exact inverse of minimapToWorld: that maps cell mx -> (mx*span/res + border)
	// in heightmap CELL units. getViewFrustumGroundCorners returns WORLD units
	// (already * MAP_XY_FACTOR), so convert to cells, subtract the border, then
	// scale to minimap resolution. Omitting the border shifts the view box away
	// from where a click lands (the two transforms must be inverses).
	Real cellX = worldX / MAP_XY_FACTOR - border;
	Real cellY = worldY / MAP_XY_FACTOR - border;
	Int x = REAL_TO_INT((cellX / xSpan) * m_resolution);
	Int y = REAL_TO_INT((cellY / ySpan) * m_resolution);
	Bool inRange = (x >= 0 && x < m_resolution && y >= 0 && y < m_resolution);

	if (x < 0) x = 0;  if (x >= m_resolution) x = m_resolution - 1;
	if (y < 0) y = 0;  if (y >= m_resolution) y = m_resolution - 1;

	*mx = x;
	*my = m_resolution - 1 - y;		// flip to top-down buffer row
	return inRange;
}

// Draw the 3D view's camera frustum as a box, like the game radar's view box. Drawn
// as a GDI overlay in client space (full display resolution) so the lines are smooth
// and the thickness is crisp regardless of the sampling resolution. Corners come from
// WbView3d::getViewFrustumGroundCorners (ground-plane projection of the 4 viewport
// corners).
void MinimapDialog::drawViewBoxOverlay(HDC hdc, Int clientW, Int clientH)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;
	WbView3d *p3d = pDoc->Get3DView();
	if (!p3d) return;

	Coord3D corners[4];
	if (!p3d->getViewFrustumGroundCorners(corners))
		return;

	Int border = pMap->getBorderSize();
	Real xSpan = INT_TO_REAL(pMap->getXExtent() - 2 * border);
	Real ySpan = INT_TO_REAL(pMap->getYExtent() - 2 * border);
	if (xSpan <= 0.0f || ySpan <= 0.0f) return;

	// Map each world corner to client pixels (fraction of map span * client size),
	// clamped to the client so off-map corners still bound the box. Y is flipped
	// (world +y is up, client +y is down). Subtract the border so this matches the
	// click mapping (minimapToWorld) exactly -- otherwise the box is shifted from
	// where a click recenters the camera.
	POINT pts[5];
	for (int i = 0; i < 4; ++i)
	{
		Real fx = (corners[i].x / MAP_XY_FACTOR - border) / xSpan;
		Real fy = (corners[i].y / MAP_XY_FACTOR - border) / ySpan;
		if (fx < 0.0f) fx = 0.0f;  if (fx > 1.0f) fx = 1.0f;
		if (fy < 0.0f) fy = 0.0f;  if (fy > 1.0f) fy = 1.0f;
		pts[i].x = (LONG)(fx * clientW);
		pts[i].y = (LONG)((1.0f - fy) * clientH);
	}
	pts[4] = pts[0];	// close the loop

	// Thick yellow pen.
	Int thickness = clientW / 128;		// ~2px at 256 client, scales with size
	if (thickness < 2) thickness = 2;
	HPEN pen = CreatePen(PS_SOLID, thickness, RGB(255, 255, 0));
	HGDIOBJ oldPen = SelectObject(hdc, pen);
	HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
	Polyline(hdc, pts, 5);
	SelectObject(hdc, oldPen);
	SelectObject(hdc, oldBrush);
	DeleteObject(pen);
}

// ----------------------------------------------------------------------------
// Road texture cache: loads each road TGA from Art/Terrain/ once per map at its
// NATIVE dimensions into a simple RGBA buffer (not the 64-tile grid that
// WorldHeightMap::readTiles assumes), for per-pixel sampling in drawThickLine.
// Cleared in rebuildTerrain() (map change) and in the destructor.

struct RoadTex
{
	UnsignedByte *rgba;		// w*h*4, RGBA, top-down. NULL = load failed.
	Int           w, h;
};

struct RoadTexEntry
{
	AsciiString name;
	RoadTex     tex;
};

static RoadTexEntry s_roadTexCache[64];
static Int          s_roadTexCount = 0;

// Minimal TGA loader: handles 24/32-bit uncompressed (0x2) and RLE (0xA) targa,
// the two formats the game's own targa reader supports. Loads at native size,
// stored top-down RGBA. Returns false on failure.
static Bool loadTGA(const char *path, RoadTex *out)
{
	out->rgba = NULL; out->w = out->h = 0;

	CachedFileInputStream stream;
	if (!stream.open(AsciiString(path)))
		return FALSE;

#pragma pack(push, 1)
	struct TgaHdr {
		UnsignedByte idLength, colorMapType, imageType;
		UnsignedByte colorMapInfo[5];
		Short xOrigin, yOrigin, imageWidth, imageHeight;
		UnsignedByte pixelDepth, flags;
	} hdr;
#pragma pack(pop)

	if (stream.read(&hdr, sizeof(hdr)) != sizeof(hdr)) return FALSE;
	if (hdr.colorMapType != 0) return FALSE;
	if (hdr.imageType != 0x2 && hdr.imageType != 0xA) return FALSE;
	if (hdr.pixelDepth < 24 || hdr.pixelDepth > 32) return FALSE;

	Int w = hdr.imageWidth, h = hdr.imageHeight;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return FALSE;

	if (hdr.idLength > 0) stream.absoluteSeek(stream.tell() + hdr.idLength);

	Int bpp = (hdr.pixelDepth + 7) / 8;	// 3 or 4
	Bool compressed = (hdr.imageType & 0x08) != 0;
	Bool topDown = (hdr.flags & 0x20) != 0;

	UnsignedByte *rgba = new UnsignedByte[w * h * 4];
	Int total = w * h;
	Int pixelIdx = 0;
	UnsignedByte buf[4];
	Int repeat = 0; Bool running = false;

	while (pixelIdx < total) {
		if (compressed && repeat == 0) {
			UnsignedByte flag;
			if (stream.read(&flag, 1) != 1) { delete [] rgba; return FALSE; }
			repeat = (flag & 0x7f) + 1;
			running = (flag & 0x80) != 0;
			if (running) stream.read(buf, bpp);
		}
		if (compressed) --repeat;
		if (!compressed || !running)
			stream.read(buf, bpp);

		UnsignedByte b = buf[0], g = buf[1], r = buf[2];
		UnsignedByte a = (bpp == 4) ? buf[3] : 255;

		// Destination row: TGA is bottom-up unless the top-down flag is set.
		Int x = pixelIdx % w;
		Int srcRow = pixelIdx / w;
		Int dstRow = topDown ? srcRow : (h - 1 - srcRow);
		UnsignedByte *d = rgba + (dstRow * w + x) * 4;
		d[0] = r; d[1] = g; d[2] = b; d[3] = a;
		++pixelIdx;
	}

	out->rgba = rgba; out->w = w; out->h = h;
	return TRUE;
}

static RoadTex *getRoadTex(const AsciiString &texName)
{
	for (Int i = 0; i < s_roadTexCount; ++i)
		if (s_roadTexCache[i].name == texName)
			return s_roadTexCache[i].tex.rgba ? &s_roadTexCache[i].tex : NULL;

	RoadTex tex; tex.rgba = NULL; tex.w = tex.h = 0;
	if (!texName.isEmpty()) {
		char path[_MAX_PATH];
		sprintf(path, "%s%s", TERRAIN_TGA_DIR_PATH, texName.str());
		loadTGA(path, &tex);
	}

	RoadTex *result = NULL;
	if (s_roadTexCount < 64) {
		s_roadTexCache[s_roadTexCount].name = texName;
		s_roadTexCache[s_roadTexCount].tex = tex;
		result = tex.rgba ? &s_roadTexCache[s_roadTexCount].tex : NULL;
		++s_roadTexCount;
	} else if (tex.rgba) {
		delete [] tex.rgba;	// cache full; drop
	}
	return result;
}

static void clearRoadTexCache()
{
	for (Int i = 0; i < s_roadTexCount; ++i)
		if (s_roadTexCache[i].tex.rgba) {
			delete [] s_roadTexCache[i].tex.rgba;
			s_roadTexCache[i].tex.rgba = NULL;
		}
	s_roadTexCount = 0;
}

// Sample a native-size RGBA road texture at normalized (u, v) in [0,1], tiling,
// with bilinear filtering. Returns packed 0x00RRGGBB (B in low byte for the DIB).
static UnsignedInt sampleRoadTex(const RoadTex *t, Real u, Real v)
{
	u = u - (Real)floor(u);	// wrap into [0,1)
	v = v - (Real)floor(v);

	Real fx = u * t->w - 0.5f;
	Real fy = v * t->h - 0.5f;
	Int x0 = (Int)floor(fx), y0 = (Int)floor(fy);
	Real du = fx - x0, dv = fy - y0;
	Int x1 = x0 + 1, y1 = y0 + 1;
	// Wrap (tile) the integer coords.
	x0 = ((x0 % t->w) + t->w) % t->w;  x1 = ((x1 % t->w) + t->w) % t->w;
	y0 = ((y0 % t->h) + t->h) % t->h;  y1 = ((y1 % t->h) + t->h) % t->h;

	const UnsignedByte *p = t->rgba;
	const UnsignedByte *c00 = p + (y0 * t->w + x0) * 4;
	const UnsignedByte *c10 = p + (y0 * t->w + x1) * 4;
	const UnsignedByte *c01 = p + (y1 * t->w + x0) * 4;
	const UnsignedByte *c11 = p + (y1 * t->w + x1) * 4;

	Real r = c00[0]*(1-du)*(1-dv) + c10[0]*du*(1-dv) + c01[0]*(1-du)*dv + c11[0]*du*dv;
	Real g = c00[1]*(1-du)*(1-dv) + c10[1]*du*(1-dv) + c01[1]*(1-du)*dv + c11[1]*du*dv;
	Real b = c00[2]*(1-du)*(1-dv) + c10[2]*du*(1-dv) + c01[2]*(1-du)*dv + c11[2]*du*dv;

	UnsignedInt ri = (UnsignedInt)(r + 0.5f); if (ri > 255) ri = 255;
	UnsignedInt gi = (UnsignedInt)(g + 0.5f); if (gi > 255) gi = 255;
	UnsignedInt bi = (UnsignedInt)(b + 0.5f); if (bi > 255) bi = 255;
	return (bi) | (gi << 8) | (ri << 16) | (0xFFu << 24);
}

// Textured thick line: for each pixel, compute UV from (U=distance along road tiling
// every 64 buffer pixels, V=perpendicular offset 0=left edge 1=right edge 0.5=center),
// sample the road texture at that UV with bilinear filtering. Falls back to flat color
// if no tile is available.
void MinimapDialog::drawThickLine(Int x0, Int y0, Int x1, Int y1, Int halfW, UnsignedInt color,
	RoadTex *tex, Real segLenPx)
{
	Int dx = x1 - x0, dy = y1 - y0;
	Int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
	if (steps == 0) {
		fillRect(x0, y0, halfW * 2 + 1, halfW * 2 + 1, color);
		return;
	}

	Real stepX = (Real)dx / steps;
	Real stepY = (Real)dy / steps;
	Real cx = (Real)x0, cy = (Real)y0;
	Int  diameter = halfW * 2 + 1;

	// Texture tiling rate along the road: tile roughly every 32 buffer pixels so the
	// asphalt detail reads at minimap scale (the road texture's length wraps).
	const Real tileEveryPx = 32.0f;

	for (Int s = 0; s <= steps; ++s) {
		Int px = REAL_TO_INT(cx);
		Int py = REAL_TO_INT(cy);

		Real u = (Real)s / tileEveryPx;	// along the road (sampleRoadTex wraps it)

		for (Int oy = -halfW; oy <= halfW; ++oy) {
			Int py2 = py + oy;
			if (py2 < 0 || py2 >= m_resolution) continue;
			for (Int ox = -halfW; ox <= halfW; ++ox) {
				Int px2 = px + ox;
				if (px2 < 0 || px2 >= m_resolution) continue;

				UnsignedInt c;
				if (tex) {
					// V: across the road width, 0 (one edge) -> 1 (other edge).
					Real v = (diameter > 1) ? ((Real)(oy + halfW) / (Real)(diameter - 1)) : 0.5f;
					c = sampleRoadTex(tex, u, v);
				} else {
					c = color;
				}
				m_pixelBuffer[py2 * m_resolution + px2] = c;
			}
		}
		cx += stepX;
		cy += stepY;
	}
}

// Rasterize all road and bridge segments into the pixel buffer, drawn BEFORE object
// dots so roads appear underneath units/structures. Per-pixel colors come from the
// road type's TGA texture (loaded via CachedFileInputStream, cached by name), sampled
// with bilinear filtering and proper UV tiling along the road length.
void MinimapDialog::drawRoads()
{
	if (!TheTerrainRoads)
		return;

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap) return;

	Int border = pMap->getBorderSize();
	Real xSpan = INT_TO_REAL(pMap->getXExtent() - 2 * border);
	Real ySpan = INT_TO_REAL(pMap->getYExtent() - 2 * border);
	if (xSpan <= 0.0f || ySpan <= 0.0f) return;

	// Scale factor: world units -> buffer pixels (use the smaller axis to be conservative).
	Real worldToPixel = (Real)m_resolution / (xSpan < ySpan ? xSpan : ySpan);

	// Fallback flat colors used only when no texture could be loaded.
	const UnsignedInt roadColor   = packBGRA(0x504848);
	const UnsignedInt bridgeColor = packBGRA(0x706858);

	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
	{
		// Roads are stored as linked pairs: POINT1 -> POINT2 (adjacent in the list).
		if (!pObj->getFlag(FLAG_ROAD_POINT1) && !pObj->getFlag(FLAG_BRIDGE_POINT1))
			continue;

		Bool isBridge = pObj->getFlag(FLAG_BRIDGE_POINT1);

		MapObject *pObj2 = pObj->getNext();
		if (!pObj2)
			continue;
		if (isBridge ? !pObj2->getFlag(FLAG_BRIDGE_POINT2) : !pObj2->getFlag(FLAG_ROAD_POINT2))
			continue;

		const Coord3D *loc1 = pObj->getLocation();
		const Coord3D *loc2 = pObj2->getLocation();

		Int mx1, my1, mx2, my2;
		worldToMinimap(loc1->x, loc1->y, &mx1, &my1);
		worldToMinimap(loc2->x, loc2->y, &mx2, &my2);

		// Look up road type for width and texture.
		Real roadWidth = 16.0f;
		RoadTex *tex = NULL;
		TerrainRoadType *roadType = TheTerrainRoads->findRoadOrBridge(pObj->getName());
		if (roadType) {
			roadWidth = roadType->getRoadWidth();
			tex = getRoadTex(roadType->getTexture());
		}

		// World-unit width -> buffer half-width. worldToPixel is pixels-per-cell and
		// roadWidth is world units, so divide by MAP_XY_FACTOR to get cells first.
		// Cap so roads stay road-like (not slabs) at any resolution.
		Int halfW = REAL_TO_INT(roadWidth * worldToPixel / MAP_XY_FACTOR / 2.0f);
		if (halfW < 1) halfW = 1;
		Int maxHalf = m_resolution / 64;	// ~ up to 4px at 256, 8px at 512
		if (maxHalf < 1) maxHalf = 1;
		if (halfW > maxHalf) halfW = maxHalf;

		// Segment length in buffer pixels, for UV tiling.
		Real ddx = (Real)(mx2 - mx1), ddy = (Real)(my2 - my1);
		Real segLen = sqrtf(ddx * ddx + ddy * ddy);

		UnsignedInt fallback = isBridge ? bridgeColor : roadColor;
		drawThickLine(mx1, my1, mx2, my2, halfW, fallback, tex, segLen);
	}
}

// Overlay map objects, adapting the Thrax minimap upgrade:
//   - units:     filled square blip in the owner's house color
//   - structures: black-outlined box with house-color fill (distinct from units)
//   - resource structures (supply/oil): solid black marker
// Sizes derive from Thrax's base values (unit 8, structure 10, outline 2 at a
// 512-cell reference) scaled to the current minimap resolution. Buffer is top-down,
// so world-row maps to (m_resolution-1 - row).
void MinimapDialog::drawObjects()
{
	// (Roads are drawn separately in refreshObjects, before this, so they sit under
	// the object dots and are independently toggleable.)

	// Thrax reference sizes are tuned for a 512-cell radar; scale to our resolution.
	const Int kRefRes = 512;
	Int unitSize     = (12 * m_resolution + kRefRes - 1) / kRefRes;
	Int structSize   = (12 * m_resolution + kRefRes - 1) / kRefRes;
	Int outlineWidth = (2  * m_resolution + kRefRes - 1) / kRefRes;
	// Units are circles, so they need a few pixels of diameter to read as round
	// (the buffer is stretched up to the display, but the disc is rasterized at
	// buffer resolution). Keep an odd diameter so the disc is symmetric.
	if (unitSize     < 5) unitSize     = 5;
	if ((unitSize & 1) == 0) unitSize += 1;
	if (structSize   < 5) structSize   = 5;
	if (outlineWidth < 1) outlineWidth = 1;

	const UnsignedInt black = packBGRA(0x000000);

	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext())
	{
		if (pObj->isWaypoint())
			continue;
		const ThingTemplate *t = pObj->getThingTemplate();
		if (!t)
			continue;

		EditorSortingType es = t->getEditorSorting();
		Bool isStructure = (es == ES_STRUCTURE);
		Bool isUnit      = (es == ES_INFANTRY || es == ES_VEHICLE);
		if (!isStructure && !isUnit)
			continue;								// skip props/trees/debris/system/audio

		// World position -> minimap cell (top-down, row flipped). Skip if off-map.
		const Coord3D *loc = pObj->getLocation();
		Int mx, cy;
		if (!worldToMinimap(loc->x, loc->y, &mx, &cy))
			continue;

		if (isUnit)
		{
			// Units/infantry render as diamonds to distinguish them from buildings.
			fillDiamond(mx, cy, unitSize, packBGRA(getMapObjectHouseColor(pObj)));
			continue;
		}

		// Structure.
		if (isResourceStructure(t))
		{
			// Solid black marker, slightly larger (matches Thrax resource styling).
			Int rs = structSize < 7 ? 7 : structSize;
			fillRect(mx, cy, rs, rs, black);
			continue;
		}

		// Outlined box: black outer rect, house-color inner fill.
		fillRect(mx, cy, structSize, structSize, black);
		Int innerW = structSize - outlineWidth * 2;
		if (innerW < 1) innerW = 1;
		fillRect(mx, cy, innerW, innerW, packBGRA(getMapObjectHouseColor(pObj)));
	}
}
