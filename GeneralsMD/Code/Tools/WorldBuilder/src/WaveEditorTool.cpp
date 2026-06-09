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

// WaveEditorTool.cpp
// Native WorldBuilder wave editor.  Replaces the old "launch generals_wave.exe"
// path: the user clicks a start then an end point in the 3D view to place a
// water-track wave, which is rendered live by the engine's WaterTracksRenderSystem
// and saved to the map's .wak file.

#include "StdAfx.h"
#include "resource.h"

#include "WaveEditorTool.h"
#include "WaveEditorOptions.h"
#include "WorldBuilder.h"	// WbApp(), getSelTool()/getWaveEditorTool() for isEditorActive()
#include "MainFrm.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"
#include "DrawObject.h"

#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/W3DWaterTracks.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"	// TheTerrainRenderObject for shore-aware paint
#include "Lib/BaseType.h"
#include "vector2.h"

// Forward decl: shore-facing direction sampler (defined below; used by the Paint hover
// preview before its definition).
static Bool computeShoreDirection(float cx, float cy, float &outDirX, float &outDirY);

// WB's water-area surface lookup (DrawObject.cpp): returns the height of the water-area
// polygon covering (x,y), or -FLT_MAX when the point isn't over water.  We hand this to
// the wave render system so waves seat on the map's real per-area water surface instead
// of the flat global water level (see ensureSystem()).
extern Real getWaterHeightIfUnderwater(Real x, Real y);

// Saved off so that static functions (panel handlers) can reach instance state.
WaveEditorTool*	WaveEditorTool::m_staticThis = NULL;
Int				WaveEditorTool::m_selectedWave = -1;
WaveEditorTool::EditorMode	WaveEditorTool::m_editorMode = WaveEditorTool::MODE_CREATE;

Bool	WaveEditorTool::m_tracksLoaded = false;
CString	WaveEditorTool::m_loadedMapPath;

Bool	WaveEditorTool::m_ghostActive  = false;
float	WaveEditorTool::m_ghostCenterX = 0.0f;
float	WaveEditorTool::m_ghostCenterY = 0.0f;
float	WaveEditorTool::m_ghostDirX    = 0.0f;
float	WaveEditorTool::m_ghostDirY    = 1.0f;

WaveEditorTool::WaveUndo	WaveEditorTool::m_undoStack[WaveEditorTool::WAVE_UNDO_MAX];
Int		WaveEditorTool::m_undoTop = 0;
Int		WaveEditorTool::m_undoNextGroup = 1;

Int		WaveEditorTool::m_selSet[WaveEditorTool::WAVE_SEL_MAX];
Int		WaveEditorTool::m_selCount = 0;
WaveEditorTool::GrabWave	WaveEditorTool::m_grabSet[WaveEditorTool::WAVE_SEL_MAX];
Int		WaveEditorTool::m_grabCount = 0;
float	WaveEditorTool::m_grabPivotX = 0.0f;
float	WaveEditorTool::m_grabPivotY = 0.0f;
float	WaveEditorTool::m_pendCenterX = 0.0f;
float	WaveEditorTool::m_pendCenterY = 0.0f;
float	WaveEditorTool::m_pendDirX    = 0.0f;
float	WaveEditorTool::m_pendDirY    = 0.0f;

/// Constructor
WaveEditorTool::WaveEditorTool(void) :
Tool(ID_WAVE_EDITOR_TOOL, IDC_POINTER)
{
	m_centerPt3d.set(0.0f, 0.0f, 0.0f);
	m_dragMode = DRAG_NONE;
	m_editWave = -1;
	m_currentType = 0;
	m_View = NULL;
	m_dragArmed = false;
	m_pressViewPt = CPoint(0, 0);
	m_grabOffsetX = 0.0f;
	m_grabOffsetY = 0.0f;
	m_rotStartDirX = 0.0f;
	m_rotStartDirY = 1.0f;
	m_rotStartCursorAng = 0.0f;
	m_paintHasLast = false;
	m_paintLastX = 0.0f;
	m_paintLastY = 0.0f;
	m_paintCount = 0;
	m_staticThis = this;
}

/// Destructor
WaveEditorTool::~WaveEditorTool(void)
{
}

/// Make sure the wave system exists and the globals that gate its rendering are on.
void WaveEditorTool::ensureSystem(void)
{
	// Never let the legacy in-game GetAsyncKeyState editor run inside WB.
	if (TheWritableGlobalData) {
		TheWritableGlobalData->m_usingWaterTrackEditor = false;
	}
	// NOTE: we intentionally do NOT force m_showSoftWaterEdge here. That global is the
	// user's View > Show Soft Water setting and also drives shoreline rendering; forcing
	// it on left it stuck on for the whole session (deactivate() isn't reliably called),
	// so the wave renderer ran every frame even when the wave tool wasn't selected. The
	// render path now calls flush() only while the wave editor is the selected tool, so
	// the soft-water gate inside flush() is bypassed for waves and no longer needed here.

	if (!TheWaterTracksRenderSystem) {
		TheWaterTracksRenderSystem = NEW WaterTracksRenderSystem;
		TheWaterTracksRenderSystem->init();
	}

	// WB water surfaces are polygon-trigger water areas, each at its own Z.  With no
	// TheTerrainLogic the renderer would seat waves on the flat global water level and
	// they'd sink below (and get clipped by) a higher water area.  Feed it WB's real
	// per-area lookup so waves sit just above the actual surface.  Idempotent - safe to
	// set every call (the system may already exist, created by the 3D view).
	TheWaterTracksRenderSystem->setEditorWaterHeightFunc(getWaterHeightIfUnderwater);
}

/// Derive "<map>.wak" from the document's map path.  Returns false if unsaved.
Bool WaveEditorTool::getWakPath(CWorldBuilderDoc *pDoc, char *buffer, Int bufLen)
{
	if (!pDoc)
		return false;

	CString mapPath = pDoc->getMapPath();
	if (mapPath.IsEmpty() || mapPath.GetLength() < 4 || mapPath.GetLength() >= bufLen)
		return false;

	strcpy(buffer, (LPCSTR)mapPath);
	Int len = strlen(buffer);
	strcpy(buffer + len - 4, ".wak");	// replace the .map extension
	return true;
}

// True when the wave editor is the selected palette tool. We check getSelTool() (not
// getCurTool()) so transient Space/Alt/Ctrl tool swaps don't make the editor's overlay/
// tracks flicker off mid-edit.
Bool WaveEditorTool::isEditorActive(void)
{
	return WbApp() && WbApp()->getSelTool() == (Tool *)WbApp()->getWaveEditorTool();
}

// Activate.
void WaveEditorTool::activate()
{
	Tool::activate();
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_WAVE_EDITOR_OPTIONS);

	ensureSystem();
	m_dragMode = DRAG_NONE;

	// Auto-load the map's saved waves ONCE per map, not on every (re)activation.  A
	// right-click pan transiently swaps to the hand-scroll tool and swaps back, which
	// re-runs activate(); reloading here would reset() the wave system and silently
	// discard waves the user placed but hasn't saved to the .wak yet.  We only (re)load
	// when the active map differs from the one we last loaded for, so in-memory waves
	// persist across tool swaps/pans (like a paint tool's edits) until the user saves,
	// reloads, or switches maps.
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	CString curPath = pDoc ? pDoc->getMapPath() : CString();
	if (!m_tracksLoaded || curPath != m_loadedMapPath) {
		loadTracks(pDoc);
		m_tracksLoaded  = true;
		m_loadedMapPath = curPath;
	}
}

// Deactivate.
void WaveEditorTool::deactivate()
{
	Tool::deactivate();

	m_dragMode  = DRAG_NONE;
	m_dragArmed = false;
	m_paintHasLast = false;
	clearPreviewWave();	// also clears m_ghostActive
	if (m_View != NULL) {
		m_View->doRulerFeedback(RULER_NONE);
	}
}

/** Set the cursor. */
void WaveEditorTool::setCursor(void)
{
	Tool::setCursor();
}

/** Mouse down: if the press lands on an existing wave, grab it for move (body) or
	rotate (arrow); otherwise begin dragging out a brand-new wave on empty water. */
void WaveEditorTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;

	if (m_View == NULL) {
		m_View = pView;
	}

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	pView->snapPoint(&cpt);

	ensureSystem();

	// Paint mode: a press starts a stroke and drops the first wave right away. The
	// rest are laid in mouseMoved as the cursor travels, spaced out so a drag doesn't
	// spawn thousands of overlapping waves. There's no direction yet on the first
	// drop, so aim it "up" (+Y); subsequent waves follow the stroke direction.
	if (m_editorMode == MODE_PAINT)
	{
		m_dragMode    = DRAG_PAINT;
		m_dragArmed   = false;	// paint applies immediately, no dead-zone
		m_ghostActive = false;
		m_paintHasLast = false;
		m_paintCount   = 0;
		clearPreviewWave();

		paintWaveAt(cpt.x, cpt.y, 0.0f, 1.0f);

		pView->Invalidate();
		pDoc->updateAllViews();
		WaveEditorOptions::refresh();
		return;
	}

	// In Manipulate mode, presses only ever grab existing waves (never create).
	// In Create mode, presses always drag out a new wave (skip hit-testing).
	Int hitWave = -1;
	Bool hitArrow = false;
	if (m_editorMode == MODE_MANIPULATE && TheWaterTracksRenderSystem)
		hitWave = TheWaterTracksRenderSystem->pickWave(Vector2(cpt.x, cpt.y), hitArrow);

	Bool ctrlDown  = (0x8000 & ::GetAsyncKeyState(VK_CONTROL)) != 0;
	Bool shiftDown = (0x8000 & ::GetAsyncKeyState(VK_SHIFT))   != 0;

	if (m_editorMode == MODE_MANIPULATE && hitWave < 0)
	{
		// Clicked empty water while manipulating.  Shift-click on empty water keeps the
		// current multi-selection (so a mis-aimed Shift-click doesn't wipe it); a plain
		// click clears the selection.
		m_dragMode  = DRAG_NONE;
		m_dragArmed = false;
		if (!shiftDown)
			selectWaveNoCenter(-1);
		CMainFrame::GetMainFrame()->SetMessageText("Manipulate mode: click a wave to grab it. Shift+click to multi-select, Ctrl+drag to rotate.");
		pView->Invalidate();
		pDoc->updateAllViews();
		return;
	}

	if (hitWave >= 0)
	{
		// --- Shift-click: toggle this wave in/out of the multi-selection, no drag. ---
		if (shiftDown)
		{
			m_dragMode  = DRAG_NONE;
			m_dragArmed = false;
			m_editWave  = -1;
			toggleWaveSelection(hitWave);
			CMainFrame::GetMainFrame()->SetMessageText("Toggled wave selection (Shift+click). Drag a selected wave to move the group.");
			pView->Invalidate();
			pDoc->updateAllViews();
			return;
		}

		// --- Grabbing a wave to move/rotate it (and the rest of the group with it). ---
		// If the grabbed wave isn't already part of the selection, a plain grab replaces
		// the selection with just it; grabbing a wave that's already selected keeps the
		// whole group so the drag moves them together.
		if (!isWaveSelected(hitWave))
			selectWaveNoCenter(hitWave);	// replace selection with just this wave (keeps camera)
		else
			m_selectedWave = hitWave;		// keep the group; make the grabbed wave the anchor

		m_editWave = hitWave;

		Vector2 s, e;
		TheWaterTracksRenderSystem->getWaveSegment(hitWave, s, e);	// s = center, e = center+dir
		m_centerPt3d.set(s.X, s.Y, 0.0f);

		// Snapshot every selected wave's transform BEFORE the edit, so the drag applies
		// one shared delta to all of them and Undo can restore the whole group.
		m_grabCount   = 0;
		m_grabPivotX  = 0.0f;
		m_grabPivotY  = 0.0f;
		for (Int i = 0; i < m_selCount && m_grabCount < WAVE_SEL_MAX; ++i)
		{
			Vector2 gs, ge;
			if (!TheWaterTracksRenderSystem->getWaveSegment(m_selSet[i], gs, ge))
				continue;
			GrabWave &g = m_grabSet[m_grabCount++];
			g.wave    = m_selSet[i];
			g.centerX = gs.X;       g.centerY = gs.Y;
			g.dirX    = ge.X - gs.X; g.dirY   = ge.Y - gs.Y;
			m_grabPivotX += gs.X;   m_grabPivotY += gs.Y;
		}
		if (m_grabCount > 0)
		{
			m_grabPivotX /= m_grabCount;	// group centroid = rotate pivot
			m_grabPivotY /= m_grabCount;
		}

		// Ctrl to ROTATE the group around its centroid; otherwise MOVE (slide).
		m_dragMode = ctrlDown ? DRAG_ROTATE : DRAG_MOVE;

		// Offset between where we grabbed and the grabbed wave's center, so a move slides
		// the group WITH the cursor (keeps the grab point under the mouse).
		m_grabOffsetX = s.X - cpt.x;
		m_grabOffsetY = s.Y - cpt.y;

		// Rotate anchors: cursor angle around the pivot at grab time; rotation applies
		// the cursor's angle DELTA to every wave's stored transform.
		m_rotStartDirX = e.X - s.X;
		m_rotStartDirY = e.Y - s.Y;
		m_rotStartCursorAng = atan2f(cpt.y - m_grabPivotY, cpt.x - m_grabPivotX);

		// Seed the ghost from the grabbed wave's CURRENT transform so the preview starts
		// exactly on top of it (the ghost shows the anchor wave; the rest move with it).
		m_ghostActive  = true;
		m_ghostCenterX = s.X;
		m_ghostCenterY = s.Y;
		m_ghostDirX    = e.X - s.X;
		m_ghostDirY    = e.Y - s.Y;

		CString msg;
		if (m_grabCount > 1)
			msg.Format("Drag to %s %d waves; release to apply.", ctrlDown ? "rotate" : "move", m_grabCount);
		else
			msg = ctrlDown ? "Drag to rotate this wave; release to apply."
										 : "Drag to move this wave (Ctrl to rotate, Shift+click to multi-select); release to apply.";
		CMainFrame::GetMainFrame()->SetMessageText(msg);
	}
	else
	{
		// --- Creating a new wave on empty water ---
		m_editWave = -1;
		m_dragMode = DRAG_CREATE;
		m_centerPt3d = cpt;
		m_grabOffsetX = 0.0f;
		m_grabOffsetY = 0.0f;

		m_ghostActive  = true;
		m_ghostCenterX = cpt.x;
		m_ghostCenterY = cpt.y;
		m_ghostDirX    = 0.0f;
		m_ghostDirY    = 0.0f;

		CMainFrame::GetMainFrame()->SetMessageText("Drag to aim the new wave's travel direction, then release.");
	}

	// Arm the drag: it won't actually move/rotate/size anything until the cursor
	// travels past the dead-zone (see mouseMoved).  A click that doesn't move just
	// selects (edit) or is ignored (create).
	m_dragArmed   = true;
	m_pressViewPt = viewPt;

	pView->Invalidate();
	pDoc->updateAllViews();
}

/** Mouse move: while dragging, update the light-cyan ghost to preview the pending
	create / move / rotate.  DrawObject draws the same crest-bar + arrow glyph. */
void WaveEditorTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m_View == NULL) {
		m_View = pView;
	}

	// HOVER (no button held): show a follow-the-cursor ghost + a live animated preview
	// wave, so you can see exactly what you'll place before clicking. Shown in both
	// Create and Paint modes (Manipulate has nothing to preview - it edits existing waves).
	if (m_dragMode == DRAG_NONE)
	{
		if ((m_editorMode == MODE_CREATE || m_editorMode == MODE_PAINT) && m == TRACK_NONE)
		{
			Coord3D hp;
			pView->viewToDocCoords(viewPt, &hp, false);
			pView->snapPoint(&hp);

			m_ghostActive  = true;
			m_ghostCenterX = hp.x;
			m_ghostCenterY = hp.y;

			// Paint aims each wave toward shore, so the hover ghost previews that same
			// shore-facing direction (falling back to "up" on flat water where there's no
			// slope). Create has no fixed direction yet, so it aims "up" (+Y) until the
			// user drags to set the real direction.
			float sdx, sdy;
			if (m_editorMode == MODE_PAINT && computeShoreDirection(hp.x, hp.y, sdx, sdy))
			{
				m_ghostDirX = sdx;
				m_ghostDirY = sdy;
			}
			else
			{
				m_ghostDirX = 0.0f;
				m_ghostDirY = 1.0f;
			}

			updatePreviewWave();
			pView->Invalidate();
			pDoc->updateAllViews();
		}
		return;
	}

	// Past here we are in an active drag (left button held).
	if (m != TRACK_L) {
		return;
	}

	// Paint stroke: drop a new wave every time the cursor has travelled at least
	// m_paintSpacing world units from the last drop, aimed along the stroke. Done
	// before the dead-zone check (paint has no dead-zone; it starts on mouseDown).
	if (m_dragMode == DRAG_PAINT)
	{
		Coord3D pp;
		pView->viewToDocCoords(viewPt, &pp, false);
		pView->snapPoint(&pp);

		// Spacing between painted waves, in world units. Big enough that a normal drag
		// lays a readable trail instead of a solid wall of overlapping crests.
		const float PAINT_SPACING = 30.0f;

		if (m_paintHasLast)
		{
			float dx = pp.x - m_paintLastX;
			float dy = pp.y - m_paintLastY;
			float distSq = dx*dx + dy*dy;
			if (distSq >= (PAINT_SPACING * PAINT_SPACING))
			{
				// Aim each painted wave along the direction the stroke is moving.
				paintWaveAt(pp.x, pp.y, dx, dy);
				pView->Invalidate();
				pDoc->updateAllViews();
				WaveEditorOptions::refresh();
			}
		}
		return;
	}

	// Dead-zone: ignore tiny movements so a plain click doesn't nudge the wave.
	// Stay "armed" (no change applied) until the cursor leaves a small pixel radius.
	if (m_dragArmed)
	{
		const Int DRAG_DEADZONE_PX = 4;
		Int dx = viewPt.x - m_pressViewPt.x;
		Int dy = viewPt.y - m_pressViewPt.y;
		if ((dx*dx + dy*dy) < (DRAG_DEADZONE_PX * DRAG_DEADZONE_PX))
			return;	// hasn't moved enough yet - leave the wave exactly where it was
		m_dragArmed = false;	// crossed the threshold; from here on the drag applies
	}

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt, false);
	pView->snapPoint(&cpt);

	if (m_dragMode == DRAG_MOVE)
	{
		// Slide the grabbed wave by the cursor, preserving where on the wave we grabbed
		// it (center = cursor + grab offset).  The translation delta (new center minus
		// the grabbed wave's original center) is applied to every selected wave live, so
		// the whole group slides together.
		m_ghostCenterX = cpt.x + m_grabOffsetX;
		m_ghostCenterY = cpt.y + m_grabOffsetY;

		// The grabbed wave is m_grabSet[*] whose .wave == m_editWave; find its snapshot
		// to derive the translation delta.
		float baseX = m_ghostCenterX, baseY = m_ghostCenterY;
		for (Int i = 0; i < m_grabCount; ++i)
			if (m_grabSet[i].wave == m_editWave)
				{ baseX = m_grabSet[i].centerX; baseY = m_grabSet[i].centerY; break; }
		float dX = m_ghostCenterX - baseX;
		float dY = m_ghostCenterY - baseY;

		if (TheWaterTracksRenderSystem)
			for (Int i = 0; i < m_grabCount; ++i)
			{
				const GrabWave &g = m_grabSet[i];
				TheWaterTracksRenderSystem->setWaveTransform(g.wave,
					Vector2(g.centerX + dX, g.centerY + dY),
					Vector2(g.dirX, g.dirY));
			}
	}
	else if (m_dragMode == DRAG_ROTATE)
	{
		// Turn the group by how far the cursor has swept around the group PIVOT (the
		// centroid) since grab.  Each wave's center orbits the pivot and its travel
		// direction rotates by the same angle, so the whole group turns as one.
		Real curAng = atan2f(cpt.y - m_grabPivotY, cpt.x - m_grabPivotX);
		Real delta  = curAng - m_rotStartCursorAng;
		Real cs = cosf(delta), sn = sinf(delta);

		if (TheWaterTracksRenderSystem)
			for (Int i = 0; i < m_grabCount; ++i)
			{
				const GrabWave &g = m_grabSet[i];
				// Rotate this wave's center around the pivot.
				float ox = g.centerX - m_grabPivotX;
				float oy = g.centerY - m_grabPivotY;
				float ncx = m_grabPivotX + (ox * cs - oy * sn);
				float ncy = m_grabPivotY + (ox * sn + oy * cs);
				// Rotate its travel direction by the same angle.
				float ndx = g.dirX * cs - g.dirY * sn;
				float ndy = g.dirX * sn + g.dirY * cs;
				TheWaterTracksRenderSystem->setWaveTransform(g.wave,
					Vector2(ncx, ncy), Vector2(ndx, ndy));
				if (g.wave == m_editWave)
				{
					// Keep the ghost on the anchor wave so the overlay highlight tracks it.
					m_ghostCenterX = ncx; m_ghostCenterY = ncy;
					m_ghostDirX = ndx;    m_ghostDirY = ndy;
				}
			}
	}
	else	// DRAG_CREATE: aim direction = center -> cursor (placing a new wave).
	{
		m_ghostDirX = cpt.x - m_ghostCenterX;
		m_ghostDirY = cpt.y - m_ghostCenterY;
	}

	// Keep the live animated preview wave in step with the ghost while creating.
	if (m_dragMode == DRAG_CREATE)
		updatePreviewWave();

	pView->Invalidate();
	pDoc->updateAllViews();
}

/** Push the current ghost transform into the engine's live preview wave (Create
	mode only) so the actual breaking-wave animation tracks the cursor.  Uses the
	current Cycle-Type selection - this is a brand-new wave being previewed. */
void WaveEditorTool::updatePreviewWave(void)
{
	if (!TheWaterTracksRenderSystem)
		return;

	Int type = m_staticThis ? m_staticThis->m_currentType : 0;
	TheWaterTracksRenderSystem->setPreviewWave(
		Vector2(m_ghostCenterX, m_ghostCenterY),
		Vector2(m_ghostDirX, m_ghostDirY),
		type);
}

/** Find the "toward land" direction at a world point by sampling the terrain height
	gradient: real shore waves roll perpendicular to the shoreline, toward the beach.
	We sample terrain height a wave-width out along +/-X and +/-Y and point up the
	steepest rise (= toward shallower water / land).  Returns false on flat ground (open
	water or flat land) where there's no meaningful shore direction, so the caller can
	fall back to the stroke direction. */
static Bool computeShoreDirection(float cx, float cy, float &outDirX, float &outDirY)
{
	if (!TheTerrainRenderObject)
		return false;

	// Sample radius: about one heightmap cell-and-a-half out, big enough to span the
	// water->land slope without reaching across a whole bay.
	const float R = 1.5f * MAP_XY_FACTOR;

	Real hL = TheTerrainRenderObject->getHeightMapHeight(cx - R, cy, NULL);
	Real hR = TheTerrainRenderObject->getHeightMapHeight(cx + R, cy, NULL);
	Real hD = TheTerrainRenderObject->getHeightMapHeight(cx, cy - R, NULL);
	Real hU = TheTerrainRenderObject->getHeightMapHeight(cx, cy + R, NULL);

	// Gradient of terrain height; it points toward RISING terrain, i.e. toward land.
	float gx = (Real)(hR - hL);
	float gy = (Real)(hU - hD);

	float len = sqrtf(gx * gx + gy * gy);
	const float FLAT_EPS = 0.25f;	// world-unit height delta below which we treat it as flat
	if (len < FLAT_EPS)
		return false;	// no discernible slope - let the stroke direction stand

	outDirX = gx / len;
	outDirY = gy / len;
	return true;
}

/** Drop one wave at (cx,cy) aimed along (dirX,dirY) as part of a paint stroke, using
	the current Cycle-Type selection.  Records it on the undo stack (UNDO_CREATE) so
	each painted wave can be undone individually, just like a single placement, and
	advances the stroke's "last drop" anchor + count.

	If the drop point sits near a water/land slope, the wave is re-aimed to roll toward
	land (perpendicular to the shoreline) instead of following the raw stroke direction,
	so painted waves look like real shore waves.  On flat ground the stroke direction is
	kept. */
void WaveEditorTool::paintWaveAt(float cx, float cy, float dirX, float dirY)
{
	ensureSystem();
	if (!TheWaterTracksRenderSystem)
		return;

	// Prefer the shore-facing direction (toward land); fall back to the stroke direction.
	float shoreX, shoreY;
	if (computeShoreDirection(cx, cy, shoreX, shoreY))
	{
		dirX = shoreX;
		dirY = shoreY;
	}

	Vector2 center(cx, cy);
	Vector2 dir(dirX, dirY);

	// New wave appends to the bottom of the list, so its index = the old count.
	Int newIndex = TheWaterTracksRenderSystem->getWaveCount();
	TheWaterTracksRenderSystem->addWaveByDirection(center, dir, m_currentType);
	pushUndo(UNDO_CREATE, newIndex, 0.0f, 0.0f, 0.0f, 0.0f);

	m_paintLastX   = cx;
	m_paintLastY   = cy;
	m_paintHasLast = true;
	m_paintCount++;
}

/** Remove the live preview wave (when leaving Create mode, deactivating, or after a
	wave is actually placed). */
void WaveEditorTool::clearPreviewWave(void)
{
	if (TheWaterTracksRenderSystem)
		TheWaterTracksRenderSystem->clearPreviewWave();
	m_ghostActive = false;
}

/** Mouse up: apply the pending drag.  Create -> add a new wave; move/rotate ->
	transform the grabbed wave in place (keeping its slot in the list). */
void WaveEditorTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L || m_dragMode == DRAG_NONE) return;

	// Paint stroke: the waves were already committed as the cursor moved. Just end the
	// stroke and report how many were laid.
	if (m_dragMode == DRAG_PAINT)
	{
		m_dragMode     = DRAG_NONE;
		m_paintHasLast = false;
		CString msg;
		msg.Format("Painted %d wave%s.", m_paintCount, (m_paintCount == 1 ? "" : "s"));
		CMainFrame::GetMainFrame()->SetMessageText(msg);
		pView->Invalidate();
		pDoc->updateAllViews();
		WaveEditorOptions::refresh();
		return;
	}

	DragMode mode = m_dragMode;
	Bool wasClickOnly = m_dragArmed;	// never crossed the dead-zone = a plain click
	m_dragMode  = DRAG_NONE;
	m_dragArmed = false;
	m_ghostActive = false;	// stop drawing the ghost; the committed wave takes over

	// A click that never moved should NOT alter anything: for an edit it just leaves
	// the wave selected (already done at mouseDown); for a create it places nothing.
	if (wasClickOnly)
	{
		m_grabCount = 0;	// no drag happened; nothing to commit
		pView->Invalidate();
		pDoc->updateAllViews();
		return;
	}

	ensureSystem();
	if (TheWaterTracksRenderSystem)
	{
		Vector2 center(m_ghostCenterX, m_ghostCenterY);
		Vector2 dir(m_ghostDirX, m_ghostDirY);

		if (mode == DRAG_CREATE)
		{
			// Drop the live preview wave first so it isn't counted, then add the real one.
			TheWaterTracksRenderSystem->clearPreviewWave();

			// New wave appends to the bottom of the list, so its index = old count.
			Int newIndex = TheWaterTracksRenderSystem->getWaveCount();
			TheWaterTracksRenderSystem->addWaveByDirection(center, dir, m_currentType);

			// Undo for a create = remove that wave (no transform to restore).
			pushUndo(UNDO_CREATE, newIndex, 0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (m_grabCount > 0)	// DRAG_MOVE or DRAG_ROTATE (group already moved live)
		{
			// The group's new transforms were applied live during the drag.  Record one
			// undo per wave, all sharing a groupId so a single Undo restores the whole
			// group to its pre-drag transform (m_grabSet holds the OLD values).
			Int groupId = (m_grabCount > 1) ? m_undoNextGroup++ : 0;
			for (Int i = 0; i < m_grabCount; ++i)
			{
				const GrabWave &g = m_grabSet[i];
				pushUndo(UNDO_TRANSFORM, g.wave,
								 g.centerX, g.centerY, g.dirX, g.dirY, groupId);
			}
		}
	}
	m_grabCount = 0;	// end the group drag

	pView->Invalidate();
	pDoc->updateAllViews();
	WaveEditorOptions::refresh();	// reflect any new/edited wave in the list
}

//-----------------------------------------------------------------------------
// Static helpers called by the WaveEditorOptions panel.
//-----------------------------------------------------------------------------

void WaveEditorTool::setEditorMode(EditorMode mode)
{
	m_editorMode = mode;

	// Leaving Manipulate mode shouldn't strand an in-progress grab.
	if (m_staticThis)
		m_staticThis->m_dragMode = DRAG_NONE;

	// The animated preview belongs to Create-mode hover; drop it when switching modes
	// (it resumes on the next hover if we're back in Create).
	clearPreviewWave();

	const char *msg;
	switch (mode) {
		case MODE_CREATE:
			msg = "Create mode: hover to preview, drag to place a wave.";
			break;
		case MODE_PAINT:
			msg = "Paint mode: hold the left button and drag to lay a trail of waves.";
			break;
		default:
			msg = "Manipulate mode: drag a wave to move it, Ctrl+drag to rotate, Shift+click to multi-select.";
			break;
	}
	CMainFrame::GetMainFrame()->SetMessageText(msg);
}

WaveEditorTool::EditorMode WaveEditorTool::getEditorMode(void)
{
	return m_editorMode;
}

void WaveEditorTool::cycleWaveType(void)
{
	if (!m_staticThis) return;
	ensureSystem();

	Int count = TheWaterTracksRenderSystem ? TheWaterTracksRenderSystem->getEditableWaveTypeCount() : 1;
	if (count < 1) count = 1;
	m_staticThis->m_currentType = (m_staticThis->m_currentType + 1) % count;

	// Re-push the live preview so its breaking-wave animation switches to the new type
	// right away, instead of waiting for the next mouse move to call updatePreviewWave().
	// Only do this while hovering a brand-new wave (Create/Paint); when editing an
	// existing wave the ghost mirrors that wave's own type, not the Cycle-Type pick.
	if (m_ghostActive && m_staticThis->m_editWave < 0 &&
			(m_editorMode == MODE_CREATE || m_editorMode == MODE_PAINT))
	{
		updatePreviewWave();
		WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
		if (p3View)
			p3View->Invalidate();
	}
}

Int WaveEditorTool::getWaveType(void)
{
	return m_staticThis ? m_staticThis->m_currentType : 0;
}

const char *WaveEditorTool::getWaveTypeName(void)
{
	if (!TheWaterTracksRenderSystem)
		return "";
	Int type = m_staticThis ? m_staticThis->m_currentType : 0;
	return TheWaterTracksRenderSystem->getWaveTypeName(type);
}

void WaveEditorTool::pushUndo(UndoKind kind, Int wave,
														 float cx, float cy, float dx, float dy,
														 Int groupId)
{
	if (m_undoTop >= WAVE_UNDO_MAX)
	{
		// Stack full: drop the oldest record to make room (shift down by one).
		for (Int i = 1; i < WAVE_UNDO_MAX; ++i)
			m_undoStack[i - 1] = m_undoStack[i];
		m_undoTop = WAVE_UNDO_MAX - 1;
	}

	WaveUndo &u = m_undoStack[m_undoTop++];
	u.kind = kind; u.wave = wave;
	u.centerX = cx; u.centerY = cy; u.dirX = dx; u.dirY = dy;
	u.groupId = groupId;
}

Bool WaveEditorTool::hasUndo(void)
{
	return (m_undoTop > 0);
}

void WaveEditorTool::undoLast(void)
{
	ensureSystem();
	if (!TheWaterTracksRenderSystem)
		return;

	if (m_undoTop <= 0)
	{
		// Nothing recorded - fall back to the legacy create-undo so the very first
		// Undo after a fresh load still removes the last placed wave.
		TheWaterTracksRenderSystem->removeLastWaveSegment();
	}
	else
	{
		// A group move/rotate pushes one record per wave sharing a groupId; undo the
		// whole group at once so the gesture reverses as a unit.  groupId 0 = standalone.
		Int topGroup = m_undoStack[m_undoTop - 1].groupId;
		do
		{
			WaveUndo u = m_undoStack[--m_undoTop];	// pop newest
			switch (u.kind)
			{
				case UNDO_CREATE:
					// Reverse a placement by removing the wave that was added.
					TheWaterTracksRenderSystem->removeWaveAt(u.wave);
					if (m_selectedWave == u.wave)
						m_selectedWave = -1;
					break;

				case UNDO_TRANSFORM:
					// Reverse a move/rotate by restoring the pre-edit center + direction.
					TheWaterTracksRenderSystem->setWaveTransform(u.wave,
						Vector2(u.centerX, u.centerY),
						Vector2(u.dirX, u.dirY));
					break;

				default:
					break;
			}
		}
		while (topGroup != 0 && m_undoTop > 0 && m_undoStack[m_undoTop - 1].groupId == topGroup);
	}

	WaveEditorOptions::refresh();	// keep the list in sync after the undo

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::saveTracks(CWorldBuilderDoc *pDoc)
{
	char path[256];
	if (!getWakPath(pDoc, path, sizeof(path))) {
		AfxMessageBox("Save the map first - the .wak file is written next to it.", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	ensureSystem();
	if (TheWaterTracksRenderSystem) {
		TheWaterTracksRenderSystem->saveTracksTo(path);
		CMainFrame::GetMainFrame()->SetMessageText("Saved waves to .wak file.");
	}
}

void WaveEditorTool::loadTracks(CWorldBuilderDoc *pDoc, Bool announce)
{
	char path[256];
	if (!getWakPath(pDoc, path, sizeof(path))) {
		// Unsaved map - there is no .wak to load yet.
		if (announce)
			AfxMessageBox("Save the map first - the .wak file lives next to it.", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	ensureSystem();

	m_selectedWave = -1;	// indices change on reload
	clearSelectionInternal();
	m_grabCount = 0;
	m_undoTop = 0;			// stored undo indices are stale after a reload

	Int count = -1;
	if (TheWaterTracksRenderSystem) {
		TheWaterTracksRenderSystem->reset();		// clear current waves (and editor undo stack)
		count = TheWaterTracksRenderSystem->loadTracksFrom(path);
	}

	// Pull just the file name out of the full path for the message.
	const char *wakName = path;
	for (const char *p = path; *p; ++p)
		if (*p == '\\' || *p == '/')
			wakName = p + 1;

	CString msg;
	if (count < 0)
		msg.Format("No .wak file found for this map (%s).", wakName);
	else if (count == 0)
		msg.Format("%s loaded - it contains no waves.", wakName);
	else
		msg.Format("Loaded %d wave%s from %s.", count, (count == 1 ? "" : "s"), wakName);

	// Always show the outcome on the status bar; only pop a dialog when the user
	// asked for it explicitly (Reload), not on the silent auto-load at activate.
	CMainFrame::GetMainFrame()->SetMessageText(msg);
	if (announce)
		AfxMessageBox(msg, (count > 0 ? MB_ICONINFORMATION : MB_ICONEXCLAMATION) | MB_OK);

	WaveEditorOptions::refresh();	// repopulate the list with the loaded waves

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

//-----------------------------------------------------------------------------
// Wave list
//-----------------------------------------------------------------------------

Int WaveEditorTool::getWaveCount(void)
{
	return TheWaterTracksRenderSystem ? TheWaterTracksRenderSystem->getWaveCount() : 0;
}

Bool WaveEditorTool::getWaveRow(Int index, float &startX, float &startY,
								float &endX, float &endY, const char *&typeName)
{
	if (!TheWaterTracksRenderSystem)
		return false;

	Vector2 s, e;
	Int typeIndex = 0;
	if (!TheWaterTracksRenderSystem->getWaveInfo(index, s, e, typeIndex))
		return false;

	startX = s.X; startY = s.Y;
	endX   = e.X; endY   = e.Y;
	typeName = TheWaterTracksRenderSystem->getWaveTypeName(typeIndex);
	return true;
}

Int WaveEditorTool::getSelectedWave(void)
{
	return m_selectedWave;	// the anchor (last clicked)
}

//-----------------------------------------------------------------------------
// Multi-selection set
//-----------------------------------------------------------------------------

void WaveEditorTool::clearSelectionInternal(void)
{
	m_selCount = 0;
}

void WaveEditorTool::addToSelectionInternal(Int index)
{
	if (index < 0)
		return;
	for (Int i = 0; i < m_selCount; ++i)
		if (m_selSet[i] == index)
			return;	// already selected
	if (m_selCount < WAVE_SEL_MAX)
		m_selSet[m_selCount++] = index;
}

Bool WaveEditorTool::isWaveSelected(Int index)
{
	for (Int i = 0; i < m_selCount; ++i)
		if (m_selSet[i] == index)
			return true;
	return false;
}

Int WaveEditorTool::getSelectionCount(void)
{
	return m_selCount;
}

/// Center the camera on a wave's midpoint (used when a list click selects an off-screen
/// wave).  No-op if the index or view is unavailable.
static void centerCameraOnWave(Int index)
{
	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (index < 0 || !TheWaterTracksRenderSystem || !p3View)
		return;
	Vector2 s, e;
	if (TheWaterTracksRenderSystem->getWaveSegment(index, s, e))
	{
		// Waves are stored in raw world units; setCenterInView() wants heightmap CELL
		// units (it multiplies by MAP_XY_FACTOR internally), so convert.
		Real cx = ((s.X + e.X) * 0.5f) / MAP_XY_FACTOR;
		Real cy = ((s.Y + e.Y) * 0.5f) / MAP_XY_FACTOR;
		p3View->setCenterInView(cx, cy);
	}
}

void WaveEditorTool::selectWave(Int index)
{
	// List single-click (no Ctrl): replace the selection with just this wave and center
	// the camera on it.
	clearSelectionInternal();
	addToSelectionInternal(index);
	m_selectedWave = index;

	centerCameraOnWave(index);

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::selectWaveNoCenter(Int index)
{
	// Replace the selection with just this wave (or clear when index < 0), keeping the
	// camera put.  Used by the in-view single grab and by panel-hide cleanup.
	clearSelectionInternal();
	addToSelectionInternal(index);
	m_selectedWave = index;

	WaveEditorOptions::refresh();	// keep the list row in sync

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::toggleWaveSelection(Int index)
{
	// Ctrl-click in the 3D view: add the wave if it's not selected, remove it if it is.
	if (index < 0)
		return;

	if (isWaveSelected(index))
	{
		// Remove it (compact the set).
		Int w = 0;
		for (Int i = 0; i < m_selCount; ++i)
			if (m_selSet[i] != index)
				m_selSet[w++] = m_selSet[i];
		m_selCount = w;
		// Move the anchor to another remaining member, or clear it.
		if (m_selectedWave == index)
			m_selectedWave = (m_selCount > 0) ? m_selSet[m_selCount - 1] : -1;
	}
	else
	{
		addToSelectionInternal(index);
		m_selectedWave = index;	// newly added wave becomes the anchor
	}

	WaveEditorOptions::refresh();

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::beginListSelection(void)
{
	// The wave list is the source of truth for this update; start from an empty set and
	// let the panel re-add every selected row.
	clearSelectionInternal();
}

void WaveEditorTool::addListSelection(Int index)
{
	addToSelectionInternal(index);
}

void WaveEditorTool::endListSelection(Int anchorIndex)
{
	// Finish a list-driven selection update: set the anchor and center the camera on it
	// (the wave may be off-screen).  Does NOT call WaveEditorOptions::refresh() - the list
	// already holds the correct highlights, and refreshing mid-notification would recurse.
	m_selectedWave = (anchorIndex >= 0 && isWaveSelected(anchorIndex))
		? anchorIndex
		: ((m_selCount > 0) ? m_selSet[m_selCount - 1] : -1);

	centerCameraOnWave(m_selectedWave);

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::deleteSelectedWave(void)
{
	if (!TheWaterTracksRenderSystem || m_selCount <= 0)
		return;

	// Removing a wave shifts the indices of every wave after it, so delete from the
	// highest index down to keep the remaining selection indices valid as we go.
	// Sort a local copy of the selection descending (simple insertion sort; small set).
	Int idx[WAVE_SEL_MAX];
	Int n = m_selCount;
	for (Int i = 0; i < n; ++i)
		idx[i] = m_selSet[i];
	for (Int i = 1; i < n; ++i)
	{
		Int key = idx[i], j = i - 1;
		while (j >= 0 && idx[j] < key) { idx[j + 1] = idx[j]; --j; }
		idx[j + 1] = key;
	}
	for (Int i = 0; i < n; ++i)
		TheWaterTracksRenderSystem->removeWaveAt(idx[i]);

	clearSelectionInternal();
	m_selectedWave = -1;
	m_undoTop = 0;	// stored undo indices are stale once waves are removed/reindexed

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

Bool WaveEditorTool::getGhostWave(float &centerX, float &centerY,
																	float &dirX, float &dirY, Int &typeIndex)
{
	if (!m_ghostActive)
		return false;

	centerX   = m_ghostCenterX;
	centerY   = m_ghostCenterY;
	dirX      = m_ghostDirX;
	dirY      = m_ghostDirY;

	// When editing an existing wave (move/rotate), the ghost must use THAT wave's
	// type so it matches its real size.  Only a brand-new wave uses the current
	// Cycle-Type selection.
	typeIndex = m_staticThis ? m_staticThis->m_currentType : 0;
	if (m_staticThis && m_staticThis->m_editWave >= 0 && TheWaterTracksRenderSystem)
	{
		Vector2 s, e;
		Int wt = 0;
		if (TheWaterTracksRenderSystem->getWaveInfo(m_staticThis->m_editWave, s, e, wt))
			typeIndex = wt;
	}
	return true;
}
