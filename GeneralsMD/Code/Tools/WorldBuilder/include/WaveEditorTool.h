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

// WaveEditorTool.h
// Place water-track "waves" directly in the WorldBuilder 3D view and save them
// to the map's .wak file (the same format generals_wave.exe reads/writes).

#pragma once

#ifndef WAVE_EDITOR_TOOL_H
#define WAVE_EDITOR_TOOL_H

#include "Tool.h"

class WbView;
class CWorldBuilderDoc;

class WaveEditorTool : public Tool
{
public:
	/// What a left-drag is currently doing.
	enum DragMode {
		DRAG_NONE,
		DRAG_CREATE,	///< empty water: dragging out a new wave's direction
		DRAG_MOVE,		///< on a wave body: sliding it around
		DRAG_ROTATE		///< on a wave arrow: re-aiming its travel direction
	};

	/// Toggle that gates what a press does: place new waves vs. edit existing ones.
	enum EditorMode {
		MODE_CREATE,		///< every press drags out a new wave
		MODE_MANIPULATE		///< presses only select/move/rotate existing waves
	};

protected:
	Coord3D		m_centerPt3d;	///< wave center: drop point (create) or grabbed wave's center (edit)
	DragMode	m_dragMode;		///< active drag mode, DRAG_NONE when not dragging
	Int			m_editWave;		///< editor index of the wave being moved/rotated (-1 if creating)
	Int			m_currentType;	///< 0-based placeable wave-type index
	WbView*		m_View;			///< view that owns the rubber-band feedback

	// Drag dead-zone: a press only becomes an active drag once the cursor moves past
	// a few pixels, so a plain click selects without nudging the wave.
	Bool		m_dragArmed;		///< true between mouseDown and crossing the threshold
	CPoint		m_pressViewPt;		///< screen point where the press started
	// Grab offset (move only): cursor-world minus wave-center at grab time, so the
	// wave slides WITH the cursor instead of snapping its center under the cursor.
	float		m_grabOffsetX;
	float		m_grabOffsetY;
	// Rotate anchors: the wave's direction and the cursor's angle at grab time, so a
	// rotate turns the wave by the cursor's ANGLE DELTA (smooth from its current
	// orientation) instead of snapping to point at the cursor.
	float		m_rotStartDirX;		///< wave travel dir at grab (world)
	float		m_rotStartDirY;
	float		m_rotStartCursorAng;	///< atan2 of (cursor - center) at grab, radians

	static WaveEditorTool*	m_staticThis;

	/// Ensure TheWaterTracksRenderSystem exists and the editor globals are set.
	static void ensureSystem(void);
	/// Push the current ghost transform into the engine's live animated preview wave.
	static void updatePreviewWave(void);
	/// Remove the live animated preview wave.
	static void clearPreviewWave(void);
	/// Derive the map's .wak path into buffer; returns false if the map is unsaved.
	static Bool getWakPath(CWorldBuilderDoc *pDoc, char *buffer, Int bufLen);

public:
	WaveEditorTool(void);
	~WaveEditorTool(void);

	virtual void activate();
	virtual void deactivate();

	virtual void setCursor(void);
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual Bool followsTerrain(void) {return false;};

	/// True when the wave editor is the SELECTED palette tool (ignores transient
	/// Space/Alt/Ctrl tool swaps). The wave overlay + animated tracks gate on this so
	/// they only show while the editor is in use, regardless of the View toggle.
	static Bool isEditorActive(void);

	// Called by the WaveEditorOptions panel.
	static void cycleWaveType(void);
	static Int  getWaveType(void);
	static const char *getWaveTypeName(void);

	// Create/Manipulate mode (driven by the options-panel toggle buttons).
	static void setEditorMode(EditorMode mode);	///< choose place-new vs. edit-existing
	static EditorMode getEditorMode(void);			///< current mode (default MODE_CREATE)
	static void undoLast(void);
	static Bool hasUndo(void);			///< true if there's a wave action to undo (for Ctrl+Z routing)
	static void saveTracks(CWorldBuilderDoc *pDoc);
	static void loadTracks(CWorldBuilderDoc *pDoc, Bool announce = false);

	// Wave list (driven by the options panel's list control).  Plain-typed so the
	// MFC panel never has to include the W3D water header.
	static Int  getWaveCount(void);				///< number of waves in the system
	static Bool getWaveRow(Int index, float &startX, float &startY,
												 float &endX, float &endY, const char *&typeName);	///< per-wave row data
	static void selectWave(Int index);			///< highlight a wave + center the camera on it (list use); -1 clears
	static void selectWaveNoCenter(Int index);	///< highlight a wave WITHOUT moving the camera (in-view grab)
	static Int  getSelectedWave(void);			///< currently highlighted wave index, or -1
	static void deleteSelectedWave(void);		///< remove the selected wave from the system

	// Ghost preview (read by DrawObject while the user drags out a new wave).
	static Bool getGhostWave(float &centerX, float &centerY,
													 float &dirX, float &dirY, Int &typeIndex);	///< true + ghost params while dragging

protected:
	static Int	m_selectedWave;	///< index of the wave highlighted in the list/overlay, or -1
	static EditorMode	m_editorMode;	///< create vs. manipulate (panel toggle)

	// Live ghost-preview state, set during a drag so the overlay can draw it.
	static Bool		m_ghostActive;	///< true while a wave is being dragged out
	static float	m_ghostCenterX;	///< drop point (wave center), world X
	static float	m_ghostCenterY;	///< drop point (wave center), world Y
	static float	m_ghostDirX;	///< current travel direction, world X (unnormalized)
	static float	m_ghostDirY;	///< current travel direction, world Y (unnormalized)

	// Multi-step undo covering BOTH modes (create and move/rotate), so the panel's
	// Undo button and Ctrl+Z reverse actions repeatedly, newest first.
	enum UndoKind { UNDO_NONE, UNDO_CREATE, UNDO_TRANSFORM };
	struct WaveUndo {
		UndoKind	kind;		///< what the action was
		Int			wave;		///< editor index the action touched
		float		centerX;	///< pre-edit center (UNDO_TRANSFORM)
		float		centerY;
		float		dirX;		///< pre-edit travel direction (UNDO_TRANSFORM)
		float		dirY;
	};
	enum { WAVE_UNDO_MAX = 64 };
	static WaveUndo	m_undoStack[WAVE_UNDO_MAX];
	static Int		m_undoTop;	///< number of records on the stack

	// Pre-edit transform captured at mouseDown (pushed onto the stack at commit time).
	static float	m_pendCenterX;
	static float	m_pendCenterY;
	static float	m_pendDirX;
	static float	m_pendDirY;

	static void pushUndo(UndoKind kind, Int wave,
											 float cx, float cy, float dx, float dy);	///< record an undoable action
};

#endif //WAVE_EDITOR_TOOL_H
