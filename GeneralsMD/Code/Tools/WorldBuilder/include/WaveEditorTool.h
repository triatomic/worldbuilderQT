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
		DRAG_ROTATE,	///< on a wave arrow: re-aiming its travel direction
		DRAG_PAINT,		///< hold + drag: drop waves along the stroke as the cursor moves
		DRAG_BUCKET		///< hold + drag: auto-fill the shoreline under the brush with waves
	};

	/// Toggle that gates what a press does: place new waves vs. edit existing ones.
	enum EditorMode {
		MODE_CREATE,		///< every press drags out a new wave
		MODE_MANIPULATE,	///< presses only select/move/rotate existing waves
		MODE_PAINT,			///< hold left button + drag to lay a trail of waves
		MODE_BUCKET			///< hold left button + drag to auto-fill the shoreline with waves
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

	// Paint-stroke state (MODE_PAINT): the last point where we dropped a wave, so the
	// next wave only drops once the cursor has travelled m_paintSpacing world units. The
	// travel direction of each painted wave follows the stroke (last-drop -> cursor).
	Bool		m_paintHasLast;		///< true once the first wave of the stroke is down
	float		m_paintLastX;		///< world X of the last painted wave's center
	float		m_paintLastY;		///< world Y of the last painted wave's center
	Int			m_paintCount;		///< waves laid this stroke (for the status read-out)

	static WaveEditorTool*	m_staticThis;

	/// Ensure TheWaterTracksRenderSystem exists and the editor globals are set.
	static void ensureSystem(void);
	/// Push the current ghost transform into the engine's live animated preview wave.
	static void updatePreviewWave(void);
	/// Drop one wave (paint stroke), aimed along dir, and record it for Undo.
	void paintWaveAt(float cx, float cy, float dirX, float dirY);
	/// Bucket fill: drop waves along every shoreline point within the brush radius of
	/// (cx,cy), centered slightly offshore, aimed toward land, auto-spaced by crest width.
	void bucketApplyAt(float cx, float cy);
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

	// Placeable wave types (for the list's right-click "change type" menu).  Plain-typed
	// so the panel never includes the W3D water header.
	static Int  getWaveTypeCount(void);					///< number of placeable wave types
	static const char *getWaveTypeNameAt(Int typeIndex);	///< display name of the Nth placeable type
	static void setSelectedWavesType(Int typeIndex);		///< change the type of every selected wave

	// Create/Manipulate mode (driven by the options-panel toggle buttons).
	static void setEditorMode(EditorMode mode);	///< choose place-new vs. edit-existing
	static EditorMode getEditorMode(void);			///< current mode (default MODE_CREATE)

	// Bucket-fill brush size (world units), driven by the panel's size slider.
	static void setBucketBrushSize(Int worldUnits);	///< brush radius for MODE_BUCKET shoreline fill
	static Int  getBucketBrushSize(void);			///< current bucket brush radius (world units)

	/// Bucket brush overlay (read by DrawObject): true only while the wave editor is the
	/// selected tool, the mode is MODE_BUCKET and the cursor is over the view; returns the
	/// cursor's world position and the brush radius so a circle can be drawn there.
	static Bool getBucketBrush(float &centerX, float &centerY, Int &radius);
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
	static Int  getSelectedWave(void);			///< the anchor (last-clicked) wave index, or -1

	// Multi-selection (Manipulate mode).  The selection is a set of wave indices; the
	// "anchor" (m_selectedWave) is the most-recently clicked one, used for camera centering
	// and list focus.  A plain click selects just one wave; Ctrl-click toggles a wave in/out.
	static Bool isWaveSelected(Int index);		///< true if 'index' is in the current selection
	static Int  getSelectionCount(void);		///< number of waves currently selected
	static void toggleWaveSelection(Int index);	///< add/remove a wave from the selection (Ctrl-click in the 3D view), keeping the camera

	// Batch selection sync from the wave-list control: begin, add each selected row, then
	// end with the anchor row.  Lets the native list own Ctrl/Shift selection and just
	// mirrors the result into the tool (end() centers the camera on the anchor).
	static void beginListSelection(void);
	static void addListSelection(Int index);
	static void endListSelection(Int anchorIndex);

	static void deleteSelectedWave(void);		///< remove ALL selected waves from the system

	// Ghost preview (read by DrawObject while the user drags out a new wave).
	static Bool getGhostWave(float &centerX, float &centerY,
													 float &dirX, float &dirY, Int &typeIndex);	///< true + ghost params while dragging

protected:
	static Int	m_selectedWave;	///< ANCHOR wave (last clicked) - drives camera + list focus, or -1
	static EditorMode	m_editorMode;	///< create vs. manipulate (panel toggle)
	static Int	m_bucketBrushSize;	///< MODE_BUCKET brush radius in world units (panel size slider)

	// Bucket brush cursor (world XY), tracked on hover/drag so DrawObject can render the
	// brush circle at the cursor like the terrain brush does.
	static Bool		m_bucketCursorValid;	///< true while the cursor is over the view in MODE_BUCKET
	static float	m_bucketCursorX;
	static float	m_bucketCursorY;

	// Auto-load guard: the .wak is loaded ONCE per map, not on every activate().  A
	// right-click pan transiently swaps to the hand-scroll tool and swaps back, which
	// re-runs activate(); reloading there would reset() the wave system and discard any
	// waves placed but not yet saved.  Keyed on the map path so opening a different map
	// still reloads, while tool swaps/pans keep the in-memory waves (like a paint tool).
	static Bool		m_tracksLoaded;		///< true once we've loaded waves for m_loadedMapPath
	static CString	m_loadedMapPath;	///< map path the current in-memory waves belong to

	// Multi-selection set (Manipulate mode).  Stored as a flat index list to avoid pulling
	// STL into this MFC tool; the wave count per map is small so linear scans are fine.
	enum { WAVE_SEL_MAX = 256 };
	static Int	m_selSet[WAVE_SEL_MAX];	///< selected wave indices
	static Int	m_selCount;				///< number of entries in m_selSet
	static void addToSelectionInternal(Int index);	///< append if not already present (no refresh)
	static void clearSelectionInternal(void);		///< empty the set (no refresh)

	// Group-drag snapshot: each selected wave's center + travel dir captured at mouseDown,
	// so a move/rotate applies the same delta to the whole group and Undo can restore them.
	struct GrabWave {
		Int		wave;		///< editor index
		float	centerX;	///< pre-edit center
		float	centerY;
		float	dirX;		///< pre-edit travel direction
		float	dirY;
	};
	static GrabWave	m_grabSet[WAVE_SEL_MAX];	///< snapshot of the selection at grab time
	static Int		m_grabCount;			///< number of waves in the active group drag
	static float	m_grabPivotX;			///< group centroid at grab (rotate pivot)
	static float	m_grabPivotY;

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
		Int			groupId;	///< records sharing a groupId undo together (group move/rotate); 0 = standalone
	};
	enum { WAVE_UNDO_MAX = 512 };
	static WaveUndo	m_undoStack[WAVE_UNDO_MAX];
	static Int		m_undoTop;	///< number of records on the stack
	static Int		m_undoNextGroup;	///< next group id to hand out (monotonic, never 0)

	// Pre-edit transform captured at mouseDown (pushed onto the stack at commit time).
	static float	m_pendCenterX;
	static float	m_pendCenterY;
	static float	m_pendDirX;
	static float	m_pendDirY;

	static void pushUndo(UndoKind kind, Int wave,
											 float cx, float cy, float dx, float dy,
											 Int groupId = 0);	///< record an undoable action (groupId!=0 ties a group together)
};

#endif //WAVE_EDITOR_TOOL_H
