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
#include "GameClient/Water.h"		// TheWaterTransparency (map.ini-overridable radar water color)
#include "Common/FileSystem.h"
#include "Common/MapReaderWriterInfo.h"
#include "W3DDevice/GameClient/TileData.h"
#include "ddsfile.h"
#include "ww3dformat.h"
#include "WBParallel.h"		// parallel fork-join pool for the terrain resample

Bool localIsUnderwater(Real x, Real y);
static void clearRoadTexCache();
static void getDayNightTint(Real *tintR, Real *tintG, Real *tintB);
Bool localHasWaterAreas(void);

MinimapDialog *TheMinimapDialog = NULL;

// Coalesce a burst of terrain edits into one rebuild, like the game radar's
// throttled refresh, so painting stays smooth instead of resampling on every
// height change.
static const UINT_PTR MINIMAP_REBUILD_TIMER = 0xB01;	// arbitrary timer id

// Display size of the dialog client (the buffer is stretched to fill it).
static const int MINIMAP_DISPLAY_SIZE = 256;

// Cap on the resolution we actually RESAMPLE the terrain at (the expensive O(n^2)
// getTerrainColorAt loop). The buffer / display can be larger (e.g. the 2048
// Resolution setting) -- we resample at this cap and nearest-upsample into the full
// buffer. Since the dialog client is only ~256px, sampling above this is invisible
// but blocks the single UI thread long enough to stall the 3D viewport. 512 = 2x
// the display, plenty of detail.
static const int MINIMAP_RESAMPLE_CAP = 512;

Bool MinimapDialog::s_loading = false;

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
	  m_terrainRoadsBuffer(NULL),
	  m_roadsValid(false),
	  m_terrainValid(false),
	  m_resolution(MINIMAP_RES_DEFAULT),
	  m_terrainBuilt(false),
	  m_dragging(false),
	  m_rebuildPending(false),
	  m_inRebuild(false),
	  m_showObjects(true),
	  m_showRoads(true),
	  m_refreshDelayMs(250)
{
	// Load persisted config (clamp to valid ranges).
	m_resolution     = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "Resolution", MINIMAP_RES_DEFAULT);
	m_showObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowObjects", 1) ? true : false;
	m_showRoads      = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "ShowRoads", 1) ? true : false;
	m_cullObjects    = ::AfxGetApp()->GetProfileInt(MINIMAP_SECTION, "CullObjects", 0) ? true : false;
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
	delete [] m_terrainRoadsBuffer;
	clearRoadTexCache();
}

void MinimapDialog::allocBuffer()
{
	delete [] m_pixelBuffer;
	delete [] m_terrainBuffer;
	delete [] m_terrainRoadsBuffer;
	Int n = m_resolution * m_resolution;
	m_pixelBuffer        = new UnsignedInt[n];
	m_terrainBuffer      = new UnsignedInt[n];
	m_terrainRoadsBuffer = new UnsignedInt[n];
	memset(m_pixelBuffer,        0, sizeof(UnsignedInt) * n);
	memset(m_terrainBuffer,      0, sizeof(UnsignedInt) * n);
	memset(m_terrainRoadsBuffer, 0, sizeof(UnsignedInt) * n);
	m_terrainBuilt = false;
	m_terrainValid = false;
	m_roadsValid   = false;
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
	m_roadsValid = false;		// road visibility changed; rebuild the terrain+roads cache
	if (IsWindowVisible())
	{
		// Roads are composited over the cached terrain; reuse the resample if valid.
		if (m_terrainValid)
			refreshObjects();
		else
			rebuildTerrain();
	}
}

void MinimapDialog::setCullObjects(Bool cull)
{
	m_cullObjects = cull;
	::AfxGetApp()->WriteProfileInt(MINIMAP_SECTION, "CullObjects", cull ? 1 : 0);
	if (IsWindowVisible())
	{
		// Object-only change: reuse the cached terrain resample (cheap recomposite).
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

// Bracket a map load/teardown. While loading, suppress all minimap rebuilds (a modal
// MessageBox during OnOpenDocument pumps messages and could otherwise fire the pending
// rebuild timer against a half-swapped document). When loading ends, kick one clean
// rebuild against the now-valid new map.
void MinimapDialog::setLoading(Bool loading)
{
	s_loading = loading;
	if (!TheMinimapDialog)
		return;
	if (loading)
	{
		// Cancel any pending throttled rebuild from the outgoing map.
		if (::IsWindow(TheMinimapDialog->m_hWnd))
			TheMinimapDialog->KillTimer(MINIMAP_REBUILD_TIMER);
		TheMinimapDialog->m_rebuildPending = false;
		TheMinimapDialog->m_terrainValid = false;
	}
	else if (TheMinimapDialog->IsWindowVisible())
	{
		// Force a full resample for the new map (terrain, water color from any new
		// map.ini override, roads, objects) and repaint -- don't let a cached buffer
		// or the m_inRebuild guard skip it.
		TheMinimapDialog->m_terrainValid = false;
		TheMinimapDialog->m_inRebuild    = false;
		TheMinimapDialog->rebuildTerrain();
		if (::IsWindow(TheMinimapDialog->m_hWnd))
			TheMinimapDialog->Invalidate(FALSE);
	}
}

void MinimapDialog::requestRebuild(Bool terrainChanged)
{
	if (s_loading)					// a map load/teardown is in progress -- don't fight it
		return;
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;

	// Object change (no terrain resample needed): recomposite the cached terrain +
	// objects. Cheap relative to a resample. An object edit may be a road edit (roads
	// are MapObjects), so invalidate the terrain+roads cache -- only camera-only
	// refreshes (requestViewBoxRefresh) keep it, which is where the savings matter.
	if (!terrainChanged && m_terrainValid)
	{
		m_roadsValid = false;
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
	{
		m_terrainValid = false;
		m_roadsValid   = false;		// terrain (and possibly road geometry) changed
	}

	// (Re)start the one-shot throttle timer; the actual work happens in OnTimer once
	// edits stop arriving for m_refreshDelayMs.
	m_rebuildPending = true;
	SetTimer(MINIMAP_REBUILD_TIMER, (UINT)m_refreshDelayMs, NULL);
}

// A pure CAMERA move (panning, zoom, and especially dragging the minimap itself).
// The composited buffer -- terrain, roads, object blips -- is all world space and
// does NOT move; only the yellow view box (a cheap GDI overlay in OnPaint) follows.
// So just invalidate to repaint the box; do NOT recomposite. Recompositing roads +
// objects on every WM_MOUSEMOVE was what made the minimap drag clunky in the 3D view
// (a full overlay rebuild ran on the UI thread ahead of the 3D paint, per event).
//
// EXCEPTION: with "Cull Objects To View" on, the visible blip set depends on the
// camera, so the buffer must be recomposited -- coalesce it through the throttle
// timer so a drag doesn't rebuild on every event.
void MinimapDialog::requestViewBoxRefresh()
{
	if (s_loading)
		return;
	if (!::IsWindow(m_hWnd) || !IsWindowVisible())
		return;

	if (m_cullObjects && m_terrainValid)
	{
		m_rebuildPending = true;
		SetTimer(MINIMAP_REBUILD_TIMER, (UINT)(m_refreshDelayMs > 0 ? m_refreshDelayMs : 16), NULL);
		return;
	}

	Invalidate(FALSE);		// repaint cached buffer + view-box overlay; no recomposite
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
	// Suppress while a map load/teardown is in progress (setLoading(false) clears the
	// flag before kicking the one intended post-load rebuild, so that call passes).
	if (s_loading)
		return;

	// Re-entrancy guard: a modal MessageBox / nested message pump can fire the rebuild
	// timer while we're already mid-rebuild. Don't recurse into a half-done resample.
	if (m_inRebuild)
		return;
	m_inRebuild = true;

	// Road textures are tied to the map; clear the cache so the next drawRoads()
	// reloads textures fresh (handles map close/reopen and resolution changes).
	clearRoadTexCache();

	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc)
	{
		m_inRebuild = false;
		return;
	}

	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	if (!pMap)
	{
		m_inRebuild = false;
		return;
	}

	// Water color: use the engine's radar water color, which map.ini can override
	// (WaterTransparency block, "RadarWaterColor"). TheWaterTransparency is an
	// OVERRIDE<> smart pointer, so this resolves to the map.ini value when one is
	// loaded, else the default. parseRGBColor stores 0..1, but the built-in default
	// is the legacy raw 140/140/255 (> 1); normalize those by /255 so both forms work.
	RGBColor waterColor;
	waterColor.red = 0.55f;
	waterColor.green = 0.55f;
	waterColor.blue = 1.0f;
	if (TheWaterTransparency)
	{
		RGBColor rc = TheWaterTransparency->m_radarColor;
		if (rc.red > 1.0f || rc.green > 1.0f || rc.blue > 1.0f)
		{	// legacy raw 0..255 default
			rc.red /= 255.0f; rc.green /= 255.0f; rc.blue /= 255.0f;
		}
		waterColor = rc;
	}

	// Day/night tint: getTerrainColorAt returns the raw texture color (no lighting),
	// so apply the current time-of-day terrain lighting ourselves (the same tint roads
	// use), so the minimap darkens at night like the viewport.
	Real tintR, tintG, tintB;
	getDayNightTint(&tintR, &tintG, &tintB);

	const Int   border = pMap->getBorderSize();
	const Int   res = m_resolution;					// full buffer / display resolution

	// Resample the terrain at a capped resolution (sampleRes), then nearest-upsample
	// into the full-res buffer. This keeps the expensive O(n^2) getTerrainColorAt loop
	// bounded -- at the 2048 setting it would otherwise run ~4M times and, because the
	// whole rebuild is synchronous on the single UI thread, block (stall) the 3D
	// viewport's paint for the duration. The dialog client is only ~256px, so sampling
	// above the cap is invisible anyway.
	const Int   sampleRes = (res < MINIMAP_RESAMPLE_CAP) ? res : MINIMAP_RESAMPLE_CAP;
	const Real  xSample = INT_TO_REAL(pMap->getXExtent() - (2 * border)) / (Real)sampleRes;
	const Real  ySample = INT_TO_REAL(pMap->getYExtent() - (2 * border)) / (Real)sampleRes;

	// Precompute the per-index heightmap coords (world cell) and terrain-sample coords
	// (MAP_XY_FACTOR-scaled, border removed) once per row/column.
	Real *cellX = new Real[sampleRes];	// heightmap index along X (for getHeight)
	Real *cellY = new Real[sampleRes];	// heightmap index along Y
	Real *mapX  = new Real[sampleRes];	// scaled sample coord along X (for getTerrainColorAt / water)
	Real *mapY  = new Real[sampleRes];	// scaled sample coord along Y
	for (Int i = 0; i < sampleRes; ++i)
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
	for (Int y = 0; y < sampleRes; ++y)
	{
		for (Int x = 0; x < sampleRes; ++x)
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

	// Resample into a sampleRes x sampleRes scratch buffer (top-down, world-row y ->
	// row (sampleRes-1 - y)). Single sample per pixel.
	UnsignedInt *sample = new UnsignedInt[sampleRes * sampleRes];

	// Resample across worker threads: each band owns a disjoint range of rows,
	// and every pixel writes a unique slot in `sample`, so no locking is needed.
	// Everything the body reads (heightmap, tile mips via getTerrainColorAt, the
	// polygon-trigger water scan, the precomputed coord tables, and the scalar
	// tint/water color) is read-only for the duration of this loop. Set
	// WB_PARALLEL=0 to force this fully serial for A/B validation.
	WBParallel::parallelFor(0, sampleRes, [&](int yBegin, int yEnd)
	{
		for (Int y = yBegin; y < yEnd; ++y)
		{
			for (Int x = 0; x < sampleRes; ++x)
			{
				Real z = pMap->getHeight(cellX[x], cellY[y]);

				RGBColor color;

				if (hasWater && localIsUnderwater(mapX[x], mapY[y]))
				{
					color = waterColor;
					interpolateColorForHeight(&color, z,
						pMap->getMaxHeightValue(), avgHeight, pMap->getMinHeightValue());
				}
				else
				{
					pMap->getTerrainColorAt(mapX[x], mapY[y], &color);
					interpolateColorForHeight(&color, z, maxHeight, avgHeight, minHeight);
				}

				color.red   *= tintR;
				color.green *= tintG;
				color.blue  *= tintB;

				sample[(sampleRes - 1 - y) * sampleRes + x] =
					(REAL_TO_INT(color.blue * 255))       |
					(REAL_TO_INT(color.green * 255) << 8)  |
					(REAL_TO_INT(color.red * 255) << 16)   |
					(255 << 24);
			}
		}
	});

	// Upsample (nearest) the scratch buffer into the full-res cached terrain buffer.
	// Both are already top-down, so this is a straight scale with no row flip.
	if (sampleRes == res)
	{
		memcpy(m_terrainBuffer, sample, sizeof(UnsignedInt) * res * res);
	}
	else
	{
		for (Int y = 0; y < res; ++y)
		{
			Int sy = (y * sampleRes) / res;
			const UnsignedInt *srow = sample + sy * sampleRes;
			UnsignedInt *drow = m_terrainBuffer + y * res;
			for (Int x = 0; x < res; ++x)
				drow[x] = srow[(x * sampleRes) / res];
		}
	}

	delete [] sample;
	delete [] cellX;
	delete [] cellY;
	delete [] mapX;
	delete [] mapY;

	m_terrainValid = true;		// cached terrain is now current
	m_roadsValid   = false;		// fresh terrain (and tint) -> rebuild the terrain+roads cache

	// Composite terrain + objects into the displayed buffer.
	refreshObjects();
	m_inRebuild = false;
}

// Cheap path: copy the cached terrain into the displayed buffer and overlay objects,
// without resampling the terrain. Used when only objects changed.
void MinimapDialog::refreshObjects()
{
	Int n = m_resolution * m_resolution;

	// Roads are camera-invariant, but drawRoads() (per-segment lookup + oriented-quad
	// rasterize over the whole road network) is expensive. With "Cull Objects To View"
	// on, every camera move recomposites -- so re-rasterizing roads each time stutters
	// the minimap-drag pan. Cache a terrain+roads layer and rebuild it only when terrain
	// or roads actually change; camera-only recomposites then just copy the cache and
	// redraw the (cheap) object blips.
	if (!m_roadsValid)
	{
		memcpy(m_terrainRoadsBuffer, m_terrainBuffer, sizeof(UnsignedInt) * n);
		if (m_showRoads)
		{
			// drawRoads / drawThickLine write into m_pixelBuffer; point the composite at
			// the roads-cache buffer for the duration so roads bake into the cache.
			UnsignedInt *saved = m_pixelBuffer;
			m_pixelBuffer = m_terrainRoadsBuffer;
			drawRoads();
			m_pixelBuffer = saved;
		}
		m_roadsValid = true;
	}

	memcpy(m_pixelBuffer, m_terrainRoadsBuffer, sizeof(UnsignedInt) * n);

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

	// MapObject::getLocation() (and the road points) are in BORDER-RELATIVE world
	// units: getCoordFromCellIndex defines worldX = (cell - border) * MAP_XY_FACTOR,
	// so worldX / MAP_XY_FACTOR already equals (cell - border) -- the border is
	// removed. This must match the terrain resample mapping, which places minimap
	// pixel i at world coord MAP_XY_FACTOR * (i * span/res), i.e. NO extra border
	// term. (Do NOT subtract border again here -- that double-counts it and shifts
	// every object/road by one border width. The view box uses a different,
	// absolute-world transform because the camera/frustum coords include the border.)
	Real cellX = worldX / MAP_XY_FACTOR;
	Real cellY = worldY / MAP_XY_FACTOR;
	Int x = REAL_TO_INT((cellX / xSpan) * m_resolution);
	Int y = REAL_TO_INT((cellY / ySpan) * m_resolution);
	Bool inRange = (x >= 0 && x < m_resolution && y >= 0 && y < m_resolution);

	if (x < 0) x = 0;  if (x >= m_resolution) x = m_resolution - 1;
	if (y < 0) y = 0;  if (y >= m_resolution) y = m_resolution - 1;

	*mx = x;
	*my = m_resolution - 1 - y;		// flip to top-down buffer row
	return inRange;
}

// Is the world-space point (worldX, worldY) inside the 3D camera's ground footprint
// (the same convex quad the yellow view box draws)? Used to cull object blips to only
// those the 3D viewer can see. Corners come from WbView3d::getViewFrustumGroundCorners
// in world units -- the same units as MapObject::getLocation -- so we test directly.
// Standard convex-polygon test: the point is inside iff it lies on the same side of
// every edge (all edge cross-products share one sign). A small epsilon keeps blips on
// the boundary visible.
Bool MinimapDialog::isInViewFrustum(Real worldX, Real worldY)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) return TRUE;					// no doc -> don't cull
	WbView3d *p3d = pDoc->Get3DView();
	if (!p3d) return TRUE;

	Coord3D corners[4];
	if (!p3d->getViewFrustumGroundCorners(corners))
		return TRUE;						// can't determine -> show it (fail open)

	Int positive = 0, negative = 0;
	for (Int i = 0; i < 4; ++i)
	{
		const Coord3D &a = corners[i];
		const Coord3D &b = corners[(i + 1) & 3];
		Real ex = b.x - a.x, ey = b.y - a.y;	// edge vector
		Real px = worldX - a.x, py = worldY - a.y;	// point relative to edge start
		Real cross = ex * py - ey * px;
		if (cross >  0.001f) ++positive;
		if (cross < -0.001f) ++negative;
	}
	// Inside a convex quad: never straddles (only one sign present).
	return (positive == 0 || negative == 0);
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
	UnsignedInt   avg;		// average color of the whole texture, packed 0x00RRGGBB
							// (DIB byte order: B | G<<8 | R<<16). Used to draw roads as a
							// single flat color so per-pixel asphalt detail doesn't read
							// as noise at minimap scale.
};

struct RoadTexEntry
{
	AsciiString name;
	RoadTex     tex;
};

static RoadTexEntry s_roadTexCache[64];
static Int          s_roadTexCount = 0;

// Load a road texture by its (.tga) name into a native-size RGBA buffer.
//
// Road textures are NOT loose files under Art/Terrain/ -- they live inside the
// game's .big archives and ship as DXT-compressed .dds (the engine references the
// .tga name but loads the .dds equivalent). So we reuse the engine's own
// DDSFileClass: it swaps the .tga extension to .dds, resolves the file through the
// file factory (which sees the BIG archives), and decodes the DXT block compression.
// Get_Pixel returns 0xAARRGGBB; we store it top-down as RGBA. Returns false on
// failure (caller falls back to a flat color).
static Bool loadRoadTexture(const AsciiString &texName, RoadTex *out)
{
	out->rgba = NULL; out->w = out->h = 0; out->avg = 0;
	if (texName.isEmpty())
		return FALSE;

	// DDSFileClass rewrites the last 3 chars to "dds", so the name must end in a
	// 3-char extension (the road type names always carry ".tga").
	DDSFileClass dds(texName.str(), 0);
	if (!dds.Is_Available() || !dds.Load())
		return FALSE;

	Int w = (Int)dds.Get_Width(0);
	Int h = (Int)dds.Get_Height(0);
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
		return FALSE;

	UnsignedByte *rgba = new UnsignedByte[w * h * 4];
	UnsignedInt sumR = 0, sumG = 0, sumB = 0;
	for (Int y = 0; y < h; ++y) {
		for (Int x = 0; x < w; ++x) {
			unsigned argb = dds.Get_Pixel(0, x, y);	// 0xAARRGGBB
			UnsignedByte r = (UnsignedByte)((argb >> 16) & 0xff);
			UnsignedByte g = (UnsignedByte)((argb >>  8) & 0xff);
			UnsignedByte b = (UnsignedByte)((argb      ) & 0xff);
			UnsignedByte *d = rgba + (y * w + x) * 4;
			d[0] = r; d[1] = g; d[2] = b;
			d[3] = (UnsignedByte)((argb >> 24) & 0xff);	// A
			sumR += r; sumG += g; sumB += b;
		}
	}

	// Average color of the texture (flat road fill). Packed DIB order: B | G<<8 | R<<16.
	UnsignedInt n = (UnsignedInt)(w * h);
	UnsignedInt aR = sumR / n, aG = sumG / n, aB = sumB / n;
	out->avg = aB | (aG << 8) | (aR << 16);

	out->rgba = rgba; out->w = w; out->h = h;
	return TRUE;
}

static RoadTex *getRoadTex(const AsciiString &texName)
{
	for (Int i = 0; i < s_roadTexCount; ++i)
		if (s_roadTexCache[i].name == texName)
			return s_roadTexCache[i].tex.rgba ? &s_roadTexCache[i].tex : NULL;

	RoadTex tex; tex.rgba = NULL; tex.w = tex.h = 0;
	loadRoadTexture(texName, &tex);

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

// Alpha-blend src (0x__RRGGBB, in DIB byte order B|G<<8|R<<16) over dst by coverage
// alpha in [0,255]. Used to feather road edges into the terrain underneath.
static inline UnsignedInt blendOver(UnsignedInt dst, UnsignedInt src, UnsignedInt a)
{
	if (a >= 255) return src | 0xFF000000u;
	if (a == 0)   return dst;
	UnsignedInt ia = 255 - a;
	UnsignedInt sb = src & 0xff, sg = (src >> 8) & 0xff, sr = (src >> 16) & 0xff;
	UnsignedInt db = dst & 0xff, dg = (dst >> 8) & 0xff, dr = (dst >> 16) & 0xff;
	UnsignedInt ob = (sb * a + db * ia + 127) / 255;
	UnsignedInt og = (sg * a + dg * ia + 127) / 255;
	UnsignedInt or_ = (sr * a + dr * ia + 127) / 255;
	return ob | (og << 8) | (or_ << 16) | 0xFF000000u;
}

// Current time-of-day terrain lighting tint (ambient + sun diffuse), clamped to
// [0,1] per channel. The terrain resample applies this so the minimap darkens at
// night like the 3D view; roads apply the same tint so they track day/night too.
// Toggled with Ctrl+D (WbView3d::stepTimeOfDay changes TheGlobalData->m_timeOfDay).
static void getDayNightTint(Real *tintR, Real *tintG, Real *tintB)
{
	*tintR = *tintG = *tintB = 1.0f;
	if (!TheGlobalData)
		return;
	const GlobalData::TerrainLighting *tl =
		&TheGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][0];	// 0 = sun
	Real r = tl->ambient.red   + tl->diffuse.red;
	Real g = tl->ambient.green + tl->diffuse.green;
	Real b = tl->ambient.blue  + tl->diffuse.blue;
	if (r > 1.0f) r = 1.0f;  if (r < 0.0f) r = 0.0f;
	if (g > 1.0f) g = 1.0f;  if (g < 0.0f) g = 0.0f;
	if (b > 1.0f) b = 1.0f;  if (b < 0.0f) b = 0.0f;
	*tintR = r; *tintG = g; *tintB = b;
}

// Multiply a packed 0x__RRGGBB color (DIB byte order: B | G<<8 | R<<16) by a tint.
static inline UnsignedInt applyTint(UnsignedInt c, Real tintR, Real tintG, Real tintB)
{
	UnsignedInt b = c & 0xff, g = (c >> 8) & 0xff, r = (c >> 16) & 0xff;
	UnsignedInt rr = (UnsignedInt)(r * tintR + 0.5f); if (rr > 255) rr = 255;
	UnsignedInt gg = (UnsignedInt)(g * tintG + 0.5f); if (gg > 255) gg = 255;
	UnsignedInt bb = (UnsignedInt)(b * tintB + 0.5f); if (bb > 255) bb = 255;
	return bb | (gg << 8) | (rr << 16) | (c & 0xFF000000u);
}

// Rasterize a road segment as an ORIENTED QUAD rather than stamping a square per
// centerline step. For every pixel inside the segment's bounding box we project onto
// the segment axis (-> U, distance along the road, used for texture tiling) and onto
// the perpendicular (-> signed distance from the centerline). Pixels within halfW of
// the centerline are part of the road; the outer ~1px is feathered with coverage
// anti-aliasing, and the result is alpha-blended into the terrain using the texture's
// own alpha. Each covered pixel is written exactly once (no diagonal overdraw), so
// this is both straighter-edged AND cheaper than the old square-stamp loop.
//
// halfEnd extends the band slightly past each endpoint so consecutive segments meet
// without a gap at bends (a cheap miter substitute).
void MinimapDialog::drawThickLine(Int x0, Int y0, Int x1, Int y1, Int halfW, UnsignedInt color,
	RoadTex *tex, Real segLenPx, Real tintR, Real tintG, Real tintB)
{
	Real ax = (Real)(x1 - x0), ay = (Real)(y1 - y0);
	Real len = sqrtf(ax * ax + ay * ay);

	Real fHalf = (Real)halfW + 0.5f;			// half-width including the AA edge pixel
	const Real aaWidth = 1.0f;					// width (px) of the feathered edge band

	// Roads are drawn as a single FLAT color (the texture's average) rather than
	// per-pixel sampling: the asphalt cracks/detail just read as noise at minimap
	// scale. tex->avg is the precomputed average of the loaded road texture; fall
	// back to the flat fallback color if no texture loaded. Day/night tint applied.
	UnsignedInt roadC = applyTint(tex ? tex->avg : color, tintR, tintG, tintB);

	if (len < 0.5f) {
		// Degenerate (single point): draw a small filled disc so dots/joints still show.
		Int r = halfW; if (r < 1) r = 1;
		for (Int oy = -r; oy <= r; ++oy)
			for (Int ox = -r; ox <= r; ++ox) {
				Int px = x0 + ox, py = y0 + oy;
				if (px < 0 || px >= m_resolution || py < 0 || py >= m_resolution) continue;
				if (ox*ox + oy*oy > r*r) continue;
				m_pixelBuffer[py * m_resolution + px] =
					blendOver(m_pixelBuffer[py * m_resolution + px], roadC, 255);
			}
		return;
	}

	Real inv = 1.0f / len;
	Real ux = ax * inv, uy = ay * inv;			// unit axis along the road
	// Perpendicular is (-uy, ux).

	// Bounding box of the oriented quad (centerline +/- fHalf), clipped to the buffer.
	Real cxm = (x0 + x1) * 0.5f, cym = (y0 + y1) * 0.5f;
	Real halfLen = len * 0.5f;
	Real bxr = fabsf(ux) * halfLen + fabsf(uy) * fHalf;
	Real byr = fabsf(uy) * halfLen + fabsf(ux) * fHalf;
	Int minX = (Int)floor(cxm - bxr), maxX = (Int)ceil(cxm + bxr);
	Int minY = (Int)floor(cym - byr), maxY = (Int)ceil(cym + byr);
	if (minX < 0) minX = 0;  if (maxX >= m_resolution) maxX = m_resolution - 1;
	if (minY < 0) minY = 0;  if (maxY >= m_resolution) maxY = m_resolution - 1;

	for (Int py = minY; py <= maxY; ++py) {
		Real ry = (Real)py - (Real)y0;
		for (Int px = minX; px <= maxX; ++px) {
			Real rx = (Real)px - (Real)x0;

			// Project (rx,ry) onto axis (along) and perpendicular (perp).
			Real along = rx * ux + ry * uy;
			Real perp  = rx * (-uy) + ry * ux;

			// Allow the band to run the full segment length (clamp along to [0,len] for
			// the texture coordinate, but draw a touch past the ends for joint coverage).
			if (along < -0.5f || along > len + 0.5f) continue;

			Real dist = fabsf(perp);
			if (dist > fHalf) continue;

			// Edge coverage: full inside, linearly fading over the outer aaWidth pixels.
			Real cov = 1.0f;
			Real edge = fHalf - dist;			// distance inside the outer edge
			if (edge < aaWidth) cov = edge / aaWidth;
			if (cov <= 0.0f) continue;
			if (cov > 1.0f) cov = 1.0f;

			// Flat averaged road color (precomputed above). The INTERIOR is fully
			// opaque; the only feathering is the geometric edge AA (cov), which blends
			// the road's outline into the terrain.
			UnsignedInt a = (UnsignedInt)(cov * 255.0f + 0.5f);
			if (a == 0) continue;
			Int idx = py * m_resolution + px;
			m_pixelBuffer[idx] = blendOver(m_pixelBuffer[idx], roadC, a);
		}
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

	// The buffer (m_resolution) is StretchDIBits-shrunk to the dialog client, so a width
	// measured in BUFFER pixels shrinks on screen as resolution rises -- at 2048 a road
	// becomes sub-pixel after the shrink and aliases away (the bridge vanishes first).
	// Size roads by a DISPLAY-pixel target and convert to buffer pixels the same way the
	// object blips do (see drawObjects), so on-screen thickness is constant at every
	// resolution. Use the real client width (the dialog is resizable).
	CRect clientRect;
	GetClientRect(&clientRect);
	Int clientPx = clientRect.Width() > 0 ? clientRect.Width() : MINIMAP_DISPLAY_SIZE;
	#define ROAD_DISP_TO_BUF(d) (((d) * m_resolution + clientPx / 2) / clientPx)

	// Fallback flat colors used only when no texture could be loaded.
	const UnsignedInt roadColor   = packBGRA(0x504848);
	const UnsignedInt bridgeColor = packBGRA(0x706858);

	// Same day/night tint the terrain resample uses, so roads darken at night too.
	Real tintR, tintG, tintB;
	getDayNightTint(&tintR, &tintG, &tintB);

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

		// World-unit width -> DISPLAY-pixel half-width, then -> buffer pixels. roadWidth is
		// world units; /MAP_XY_FACTOR gives cells; *(clientPx/span) gives display pixels
		// across the on-screen minimap. Sizing in display px (not buffer px) keeps the
		// on-screen thickness constant across resolutions -- the 256 look is the reference.
		Real dispPerCell = (Real)clientPx / (xSpan < ySpan ? xSpan : ySpan);
		Real halfDisp = roadWidth / MAP_XY_FACTOR * dispPerCell / 2.0f;
		// Clamp in DISPLAY px: floor at 1 (never sub-pixel after the shrink, so the bridge
		// stays visible at 2048), cap so roads stay road-like (not slabs) at any resolution.
		if (halfDisp < 1.0f) halfDisp = 1.0f;
		if (halfDisp > 4.0f) halfDisp = 4.0f;
		Int halfW = ROAD_DISP_TO_BUF(REAL_TO_INT(halfDisp));
		if (halfW < 1) halfW = 1;

		// Segment length in buffer pixels, for UV tiling.
		Real ddx = (Real)(mx2 - mx1), ddy = (Real)(my2 - my1);
		Real segLen = sqrtf(ddx * ddx + ddy * ddy);

		UnsignedInt fallback = isBridge ? bridgeColor : roadColor;
		drawThickLine(mx1, my1, mx2, my2, halfW, fallback, tex, segLen, tintR, tintG, tintB);
	}
	#undef ROAD_DISP_TO_BUF
}

// Overlay map objects, adapting the Thrax minimap upgrade:
//   - units:     filled square blip in the owner's house color
//   - structures: black-outlined box with house-color fill (distinct from units)
//   - resource structures (supply/oil): solid black marker
// Buffer is top-down, so world-row maps to (m_resolution-1 - row).
void MinimapDialog::drawObjects()
{
	// (Roads are drawn separately in refreshObjects, before this, so they sit under
	// the object dots and are independently toggleable.)

	// Blips are rasterized into the buffer (m_resolution) but the buffer is stretched to
	// the dialog client, so a fixed BUFFER size looks huge at 128 and like specks at 2048.
	// Size them by a target in DISPLAY (client) pixels and convert to buffer pixels:
	//   bufferPx = displayPx * m_resolution / clientPx
	// so the on-screen size is the same at every resolution. Use the REAL client size
	// (the dialog is resizable; a hardcoded 256 was wrong), and keep floors at 1 buffer
	// px -- a higher floor at low res is exactly what made 128 blips too big. (At very
	// low res the buffer granularity still limits how small a blip can be, but no longer
	// inflates it beyond the target.)
	CRect clientRect;
	GetClientRect(&clientRect);
	Int clientPx = clientRect.Width() > 0 ? clientRect.Width() : MINIMAP_DISPLAY_SIZE;
	#define DISP_TO_BUF(d) (((d) * m_resolution + clientPx / 2) / clientPx)
	Int unitSize     = DISP_TO_BUF(6);		// ~6 display px
	Int structSize   = DISP_TO_BUF(6);
	Int outlineWidth = DISP_TO_BUF(2);		// ~2 display px black border
	#undef DISP_TO_BUF
	// Minimal floors (1 buffer px) so a blip never vanishes; odd unit diameter for a
	// symmetric disc/diamond.
	if (unitSize     < 1) unitSize     = 1;
	if ((unitSize & 1) == 0) unitSize += 1;
	if (structSize   < 1) structSize   = 1;
	// Outline is a thin rim: clamp to <= structSize/5 per side so the colored inner
	// fill stays >= ~60% of the box at every resolution (at 2048 the unclamped "2
	// display px" would otherwise be a ~16px slab dwarfing the fill).
	Int maxOutline = structSize / 5;
	if (maxOutline < 1) maxOutline = 1;
	if (outlineWidth > maxOutline) outlineWidth = maxOutline;
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

		// Optional cull: only blips inside the 3D view frustum (what the 3D viewer sees).
		if (m_cullObjects && !isInViewFrustum(loc->x, loc->y))
			continue;

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
			// Solid black marker, slightly larger than a normal structure.
			Int rs = (structSize * 7) / 6;		// ~7 display px (structSize is ~6)
			if (rs <= structSize) rs = structSize + 1;
			fillRect(mx, cy, rs, rs, black);
			continue;
		}

		// Outlined box: black outer rect, house-color inner fill -- but only when the
		// box is big enough that the rim stays a minority (inner fill at least as wide
		// as the two borders combined). At very low resolution (e.g. 128) the box is
		// only ~3 buffer px, where a 1px rim leaves a 1px center and the outline looks
		// "super thick"; there, just draw a solid house-color box (still a square, so
		// it stays distinct from the diamond units).
		UnsignedInt houseColor = packBGRA(getMapObjectHouseColor(pObj));
		Int innerW = structSize - outlineWidth * 2;
		if (innerW >= outlineWidth * 2)
		{
			fillRect(mx, cy, structSize, structSize, black);
			fillRect(mx, cy, innerW, innerW, houseColor);
		}
		else
		{
			fillRect(mx, cy, structSize, structSize, houseColor);
		}
	}
}
