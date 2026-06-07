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
#include "MainFrm.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"
#include "DrawObject.h"

#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/W3DWaterTracks.h"
#include "Lib/BaseType.h"
#include "vector2.h"

// Saved off so that static functions (panel handlers) can reach instance state.
WaveEditorTool*	WaveEditorTool::m_staticThis = NULL;
Bool			WaveEditorTool::m_savedShowSoftWaterEdge = false;
Bool			WaveEditorTool::m_softWaterEdgeSaved = false;
Int				WaveEditorTool::m_selectedWave = -1;
WaveEditorTool::EditorMode	WaveEditorTool::m_editorMode = WaveEditorTool::MODE_CREATE;

Bool	WaveEditorTool::m_ghostActive  = false;
float	WaveEditorTool::m_ghostCenterX = 0.0f;
float	WaveEditorTool::m_ghostCenterY = 0.0f;
float	WaveEditorTool::m_ghostDirX    = 0.0f;
float	WaveEditorTool::m_ghostDirY    = 1.0f;

WaveEditorTool::WaveUndo	WaveEditorTool::m_undoStack[WaveEditorTool::WAVE_UNDO_MAX];
Int		WaveEditorTool::m_undoTop = 0;
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
	m_staticThis = this;
}

/// Destructor
WaveEditorTool::~WaveEditorTool(void)
{
}

/// Make sure the wave system exists and the globals that gate its rendering are on.
void WaveEditorTool::ensureSystem(void)
{
	// flush() draws nothing unless soft water edges are enabled and the water has
	// a non-zero transparent depth; turn them on for the editor.
	if (TheWritableGlobalData) {
		// Snapshot the prior soft-water-edge state once, so deactivate() can put it back
		// (and the wave render system stops doing per-frame work when the editor closes).
		if (!m_softWaterEdgeSaved) {
			m_savedShowSoftWaterEdge = TheWritableGlobalData->m_showSoftWaterEdge;
			m_softWaterEdgeSaved = true;
		}
		TheWritableGlobalData->m_showSoftWaterEdge = true;
		// Never let the legacy in-game GetAsyncKeyState editor run inside WB.
		TheWritableGlobalData->m_usingWaterTrackEditor = false;
	}

	if (!TheWaterTracksRenderSystem) {
		TheWaterTracksRenderSystem = NEW WaterTracksRenderSystem;
		TheWaterTracksRenderSystem->init();
	}
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

// Activate.
void WaveEditorTool::activate()
{
	Tool::activate();
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_WAVE_EDITOR_OPTIONS);

	ensureSystem();
	m_dragMode = DRAG_NONE;

	// Auto-load any existing waves for this map so they are visible and editable.
	loadTracks(CWorldBuilderDoc::GetActiveDoc());
}

// Deactivate.
void WaveEditorTool::deactivate()
{
	Tool::deactivate();

	m_dragMode  = DRAG_NONE;
	m_dragArmed = false;
	clearPreviewWave();	// also clears m_ghostActive
	if (m_View != NULL) {
		m_View->doRulerFeedback(RULER_NONE);
	}

	// Restore the soft-water-edge state we forced on in ensureSystem(). Once it's back to
	// its prior value, flush() short-circuits and the wave render system stops doing
	// per-frame work (update() + a D3D camera apply) on every repaint -- which otherwise
	// lingered for the rest of the session and showed up as a select/deselect hitch.
	if (m_softWaterEdgeSaved && TheWritableGlobalData) {
		TheWritableGlobalData->m_showSoftWaterEdge = m_savedShowSoftWaterEdge;
		m_softWaterEdgeSaved = false;
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

	// In Manipulate mode, presses only ever grab existing waves (never create).
	// In Create mode, presses always drag out a new wave (skip hit-testing).
	Int hitWave = -1;
	Bool hitArrow = false;
	if (m_editorMode == MODE_MANIPULATE && TheWaterTracksRenderSystem)
		hitWave = TheWaterTracksRenderSystem->pickWave(Vector2(cpt.x, cpt.y), hitArrow);

	if (m_editorMode == MODE_MANIPULATE && hitWave < 0)
	{
		// Clicked empty water while manipulating: clear the selection, do nothing else.
		m_dragMode  = DRAG_NONE;
		m_dragArmed = false;
		selectWaveNoCenter(-1);
		CMainFrame::GetMainFrame()->SetMessageText("Manipulate mode: click a wave to grab it (switch to Create to add waves).");
		pView->Invalidate();
		pDoc->updateAllViews();
		return;
	}

	if (hitWave >= 0)
	{
		// --- Editing an existing wave ---
		m_editWave = hitWave;
		selectWaveNoCenter(hitWave);	// highlight + sync list, but DON'T jump the camera (we're grabbing it in view)

		Vector2 s, e;
		TheWaterTracksRenderSystem->getWaveSegment(hitWave, s, e);	// s = center, e = center+dir
		m_centerPt3d.set(s.X, s.Y, 0.0f);

		// Remember this wave's transform BEFORE the edit so Undo can restore it.
		m_pendCenterX = s.X;
		m_pendCenterY = s.Y;
		m_pendDirX    = e.X - s.X;
		m_pendDirY    = e.Y - s.Y;

		// Hold Ctrl to ROTATE (re-aim around center), otherwise MOVE (slide).  The old
		// "grab the arrow" hit zone was too small to land on reliably.
		Bool ctrlDown = (0x8000 & ::GetAsyncKeyState(VK_CONTROL)) != 0;
		m_dragMode = ctrlDown ? DRAG_ROTATE : DRAG_MOVE;

		// Offset between where we grabbed and the wave center, so a move slides the
		// wave WITH the cursor (keeps the grab point under the mouse) rather than
		// snapping the center onto the cursor.
		m_grabOffsetX = s.X - cpt.x;
		m_grabOffsetY = s.Y - cpt.y;

		// Rotate anchors: remember the wave's direction and the cursor's angle around
		// the center at grab time.  Rotation then applies the cursor's angle DELTA to
		// the wave's starting direction, so it turns smoothly from its current angle
		// instead of snapping to face the cursor.
		m_rotStartDirX = e.X - s.X;
		m_rotStartDirY = e.Y - s.Y;
		m_rotStartCursorAng = atan2f(cpt.y - s.Y, cpt.x - s.X);

		// Seed the ghost from the wave's CURRENT transform so the preview starts
		// exactly on top of it.  It stays put until the drag passes the dead-zone.
		m_ghostActive  = true;
		m_ghostCenterX = s.X;
		m_ghostCenterY = s.Y;
		m_ghostDirX    = e.X - s.X;
		m_ghostDirY    = e.Y - s.Y;

		CMainFrame::GetMainFrame()->SetMessageText(ctrlDown
			? "Drag to rotate this wave; release to apply."
			: "Drag to move this wave (hold Ctrl to rotate); release to apply.");
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

	// HOVER (no button held) in Create mode: show a follow-the-cursor ghost + a live
	// animated preview wave, so you can see exactly what you'll place before clicking.
	if (m_dragMode == DRAG_NONE)
	{
		if (m_editorMode == MODE_CREATE && m == TRACK_NONE)
		{
			Coord3D hp;
			pView->viewToDocCoords(viewPt, &hp, false);
			pView->snapPoint(&hp);

			// No drag yet, so aim the preview "up" (+Y) by default; the user sets the
			// real direction by dragging.
			m_ghostActive  = true;
			m_ghostCenterX = hp.x;
			m_ghostCenterY = hp.y;
			m_ghostDirX    = 0.0f;
			m_ghostDirY    = 1.0f;

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
		// Slide the wave by the cursor, preserving where on the wave we grabbed it
		// (center = cursor + grab offset), so it doesn't jump to the cursor.
		m_ghostCenterX = cpt.x + m_grabOffsetX;
		m_ghostCenterY = cpt.y + m_grabOffsetY;
	}
	else if (m_dragMode == DRAG_ROTATE)
	{
		// Turn the wave by how far the cursor has swept around the center since grab,
		// applied to the wave's ORIGINAL direction.  This starts from the wave's
		// current angle and rotates smoothly - no snap to face the cursor.
		Real curAng = atan2f(cpt.y - m_ghostCenterY, cpt.x - m_ghostCenterX);
		Real delta  = curAng - m_rotStartCursorAng;
		Real cs = cosf(delta), sn = sinf(delta);
		m_ghostDirX = m_rotStartDirX * cs - m_rotStartDirY * sn;
		m_ghostDirY = m_rotStartDirX * sn + m_rotStartDirY * cs;
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

	DragMode mode = m_dragMode;
	Bool wasClickOnly = m_dragArmed;	// never crossed the dead-zone = a plain click
	m_dragMode  = DRAG_NONE;
	m_dragArmed = false;
	m_ghostActive = false;	// stop drawing the ghost; the committed wave takes over

	// A click that never moved should NOT alter anything: for an edit it just leaves
	// the wave selected (already done at mouseDown); for a create it places nothing.
	if (wasClickOnly)
	{
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
		else if (m_editWave >= 0)	// DRAG_MOVE or DRAG_ROTATE
		{
			TheWaterTracksRenderSystem->setWaveTransform(m_editWave, center, dir);

			// Undo for a move/rotate = restore the pre-edit transform captured at
			// mouseDown (m_pend* hold the OLD values).
			pushUndo(UNDO_TRANSFORM, m_editWave,
							 m_pendCenterX, m_pendCenterY, m_pendDirX, m_pendDirY);
		}
	}

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

	CMainFrame::GetMainFrame()->SetMessageText(mode == MODE_CREATE
		? "Create mode: hover to preview, drag to place a wave."
		: "Manipulate mode: drag a wave to move it, Ctrl+drag to rotate it.");
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
														 float cx, float cy, float dx, float dy)
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
	return m_selectedWave;
}

void WaveEditorTool::selectWave(Int index)
{
	m_selectedWave = index;

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();

	// Center the camera on the selected wave's midpoint (used by the list, where the
	// wave may be off-screen).
	if (index >= 0 && TheWaterTracksRenderSystem && p3View)
	{
		Vector2 s, e;
		if (TheWaterTracksRenderSystem->getWaveSegment(index, s, e))
		{
			// Waves are stored in raw world units; setCenterInView() wants heightmap
			// CELL units (it multiplies by MAP_XY_FACTOR internally), so convert -
			// the same /MAP_XY_FACTOR the "go to object" handlers use.
			Real cx = ((s.X + e.X) * 0.5f) / MAP_XY_FACTOR;
			Real cy = ((s.Y + e.Y) * 0.5f) / MAP_XY_FACTOR;
			p3View->setCenterInView(cx, cy);
		}
	}

	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::selectWaveNoCenter(Int index)
{
	// Highlight only - the user grabbed the wave in the view, so keep the camera put.
	m_selectedWave = index;

	// Keep the options-panel list row in sync with the in-view selection.
	WaveEditorOptions::refresh();

	WbView3d *p3View = CWorldBuilderDoc::GetActive3DView();
	if (p3View)
		p3View->Invalidate();
}

void WaveEditorTool::deleteSelectedWave(void)
{
	if (m_selectedWave < 0 || !TheWaterTracksRenderSystem)
		return;

	TheWaterTracksRenderSystem->removeWaveAt(m_selectedWave);
	m_selectedWave = -1;

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
