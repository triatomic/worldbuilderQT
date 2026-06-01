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

Bool localIsUnderwater(Real x, Real y);
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
	  m_refreshDelayMs(250)
{
	// Load persisted config (clamp to valid ranges).
	m_resolution     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "Resolution", MINIMAP_RES_DEFAULT);
	m_showObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowObjects", 1) ? true : false;
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

	// 0 = manual: don't auto-rebuild on edits (only on load/toggle).
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

	if (m_showObjects)
		drawObjects();

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

// Overlay map objects, adapting the Thrax minimap upgrade:
//   - units:     filled square blip in the owner's house color
//   - structures: black-outlined box with house-color fill (distinct from units)
//   - resource structures (supply/oil): solid black marker
// Sizes derive from Thrax's base values (unit 8, structure 10, outline 2 at a
// 512-cell reference) scaled to the current minimap resolution. Buffer is top-down,
// so world-row maps to (m_resolution-1 - row).
void MinimapDialog::drawObjects()
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
		return;
	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
		return;

	Int border = pMap->getBorderSize();
	Real xSpan = INT_TO_REAL(pMap->getXExtent() - 2 * border);
	Real ySpan = INT_TO_REAL(pMap->getYExtent() - 2 * border);
	if (xSpan <= 0.0f || ySpan <= 0.0f)
		return;

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

		// World position -> cell-space (index from border) -> minimap cell.
		const Coord3D *loc = pObj->getLocation();
		Real cellX = loc->x / MAP_XY_FACTOR;
		Real cellY = loc->y / MAP_XY_FACTOR;

		Int mx = REAL_TO_INT((cellX / xSpan) * m_resolution);
		Int my = REAL_TO_INT((cellY / ySpan) * m_resolution);
		if (mx < 0 || mx >= m_resolution || my < 0 || my >= m_resolution)
			continue;

		Int cy = m_resolution - 1 - my;				// flip to top-down buffer row

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
