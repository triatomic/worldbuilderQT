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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////


#pragma once

#ifndef __W3DWaterTracks_H_
#define __W3DWaterTracks_H_

enum waveType CPP_11(: Int);	//forward reference

/// Custom render object that draws animated tracks/waves on the water.
/**
	This is an object which draws a small breaking wave or splash animation.  These objects are
	to be managed/accessed only by the WaterTracksRenderObjClassSystem
*/
class WaterTracksObj
{	
	friend class WaterTracksRenderSystem;

public:

	WaterTracksObj(void);
	~WaterTracksObj(void);

	virtual void					Render(void) {};	///<draw this object
	virtual void					Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const;	///<bounding sphere of this object
    virtual void					Get_Obj_Space_Bounding_Box(AABoxClass & aabox) const;		///<bounding box of this object

	Int freeWaterTracksResources(void);	///<free W3D assets used for this track
	void init( Real width, Real length, const Vector2 &start, const Vector2 &end, const Char *texturename, Int waveTimeOffset);	///<allocate W3D resources and set size
	void init( Real width, const Vector2 &start, const Vector2 &end, const Char *texturename);	///<allocate W3D resources and set size
	Int	update(Int msElapsed);	///< update animation state
	Int render(DX8VertexBufferClass	*vertexBuffer, Int batchStart);	///<draw this object

protected:
	TextureClass *m_stageZeroTexture;	///<primary texture
	SphereClass	m_boundingSphere;		///<bounding sphere of WaterTracks
	AABoxClass	m_boundingBox;			///<bounding box of WaterTracks

	waveType	m_type;					///<used for render state sorting (set this to texture pointer for now).
	Int			m_x;					///<vertex count
	Int			m_y;					///<vertex count
	Bool		m_bound;				///<object is bound to owner and accepts new edges
	Vector2		m_startPos;				///<starting position of wave
//	Vector2		m_endPos;				///<ending position of wave
	Vector2		m_waveDir;				///<direction of wave travel
	Vector2		m_perpDir;				///<direction perpendicular to wave travel
	Vector2		m_initStartPos;			///<original settings used to create wave
	Vector2		m_initEndPos;			///<original settings used to create wave
	Int			m_initTimeOffset;		///<time offset when wave is added into the system
	Int		m_fadeMs;				///<time for wave to fade out after it stops moving
	Int		m_totalMs;				///<amount of time to complete full motion
	Int			m_elapsedMs;			///<amount of time since start of motion

	//New version
	Real	m_waveInitialWidth;				///<width of wave segment when it first appears
	Real	m_waveInitialHeight;			///<height of wave segment when it first appears
	Real	m_waveFinalWidth;				///<width of wave segment at full size
	Real	m_waveFinalWidthPeakFrac;		///<fraction along path when wave reaches full width
	Real	m_waveFinalHeight;				///<final height of unstretched wave

	Real m_initialVelocity;	//initial velocity in world units per ms.
	Real m_waveDistance;		//<total distance traveled by wave front.
	Real m_timeToReachBeach;
	Real m_frontSlowDownAcc;
	Real m_timeToStop;
	Real m_timeToRetreat;
	Real m_backSlowDownAcc;
	Real m_timeToCompress;
	Real m_flipU;			///<force uv coordinates to flip

	WaterTracksObj	*m_nextSystem;			///<next track in system
	WaterTracksObj *m_prevSystem;			///<previous track in system
};

/// System for drawing, updating, and re-using water mark render objects.
/**
This system keeps track of all the active track mark objects and reuses them
when they expire.  It also renders all the track marks that were submitted in
this frame.
*/

class WaterTracksRenderSystem
{
	friend class WaterTracksObj;

public:

	WaterTracksRenderSystem( void );
	~WaterTracksRenderSystem( void );

	void ReleaseResources(void);	///< Release all dx8 resources so the device can be reset.
	void ReAcquireResources(void);  ///< Reacquire all resources after device reset.

	void flush (RenderInfoClass & rinfo);	///<draw all tracks that were requested for rendering.
	void update(void);	///<update the state of all edges (fade alpha, remove old, etc.)

	void init(void);	///< pre-allocate track objects
	void shutdown( void );		///< release all pre-allocated track objects, called by destructor
	void reset(void);			///< free all map dependent items.

	WaterTracksObj *bindTrack(waveType type);	///<track object to be controlled by owner
	void unbindTrack( WaterTracksObj *mod );	///<releases control of track object
	void saveTracks(void);									///<save all used tracks to disk
	void loadTracks(void);									///<load tracks from disk
	void saveTracksTo(const char *wakPath);	///<save all used tracks to an explicit .wak path
	Int loadTracksFrom(const char *wakPath);	///<load tracks from an explicit .wak path; returns track count, or -1 if the file could not be opened
	WaterTracksObj *findTrack(Vector2 &start, Vector2 &end, waveType type);

	// Editor edit-API (int-based; no enum waveType / TheTerrainLogic needed by callers).
	Int getEditableWaveTypeCount(void) const;					///<number of placeable wave types
	const char *getWaveTypeName(Int typeIndex) const;	///<display name of a placeable wave type
	void addWaveSegmentByPoints(const Vector2 &start, const Vector2 &end, Int typeIndex);	///<place a wave from start->end (legacy: line is the crest, wave travels perpendicular)
	void addWaveByDirection(const Vector2 &center, const Vector2 &travelDir, Int typeIndex);	///<place a wave centered at 'center' that travels along 'travelDir' (length ignored)
	void removeLastWaveSegment(void);					///<undo the last placed wave segment
	Int getWaveCount(void) const;						///<number of primary wave fronts (editor overlay)
	Bool getWaveSegment(Int index, Vector2 &start, Vector2 &end) const;	///<start/end of the Nth primary wave; false if out of range
	Bool getWaveInfo(Int index, Vector2 &start, Vector2 &end, Int &typeIndex) const;	///<start/end + 0-based type index of the Nth wave
	Bool getWaveFrontLine(Int index, Vector2 &lineP0, Vector2 &lineP1, Vector2 &arrowTip) const;	///<visible wave-front line (perp to motion, m_finalWidth long) + arrow tip in motion dir
	void getWaveFrontLineForType(const Vector2 &center, const Vector2 &travelDir, Int typeIndex,
															 Vector2 &lineP0, Vector2 &lineP1, Vector2 &arrowTip) const;	///<same as getWaveFrontLine but for a hypothetical wave (ghost preview)
	void removeWaveAt(Int index);						///<remove the Nth primary wave (and its trailing second wave)
	Int  pickWave(const Vector2 &worldPt, Bool &hitArrow) const;	///<editor index of the wave under worldPt (-1 = none); hitArrow=true if the arrow/tip was hit (rotate) vs body (move)
	Bool setWaveTransform(Int index, const Vector2 &center, const Vector2 &travelDir);	///<move/re-aim an existing wave (re-inits primary + trailing wave); false if out of range

	// Live animated PREVIEW wave (editor hover/drag).  A single throwaway track that
	// animates like a real wave but is excluded from the editor count, list, save and
	// undo, so it never becomes a real wave.
	void setPreviewWave(const Vector2 &center, const Vector2 &travelDir, Int typeIndex);	///<create/reposition the preview wave
	void clearPreviewWave(void);	///<remove the preview wave (if any)
	Bool hasPreviewWave(void) const { return m_previewTrack != NULL; }	///<true while a live preview wave exists (keep repainting for its animation)

protected:
	DX8VertexBufferClass		*m_vertexBuffer;	///<vertex buffer used to draw all tracks
	DX8IndexBufferClass			*m_indexBuffer;	///<indices defining triangles in maximum length track
	VertexMaterialClass	  	  *m_vertexMaterialClass;	///< vertex lighting material
	ShaderClass m_shaderClass; ///<shader or rendering state for heightmap

	WaterTracksObj *m_usedModules;	///<active objects being rendered in the scene
	WaterTracksObj *m_freeModules;	//<unused modules that are free to use again

	Int		m_stripSizeX;			///< resolution (vertex count) of wave strip
	Int		m_stripSizeY;			///< resolution (vertex count) of wave strip
	Int		m_batchStart;			///< start of unused vertices in vertex buffer
	Real	m_level;				///< water level
	void releaseTrack( WaterTracksObj *mod );	///<returns track object to free store.

	// Editor edit-API state (undo stack for interactively placed wave segments).
	enum { EDIT_MAX_UNDOS = 64 };
	struct EditUndoEntry { WaterTracksObj *track; WaterTracksObj *track2; };
	EditUndoEntry	m_editUndoStack[EDIT_MAX_UNDOS];
	Int				m_editUndoCount;	///< number of placed segments available to undo
	Int				m_editFlipU;		///< alternating u-flip for placed waves
	WaterTracksObj *m_previewTrack;		///< live editor preview wave; excluded from count/list/save
	waveType		clampEditableType(Int typeIndex) const;	///< 0-based index -> valid placeable type
	WaterTracksObj *addWaveInternal(const Vector2 &midPoint, const Vector2 &dirMidPoint,
																	waveType type, Int flipU, WaterTracksObj **outSecond);
	// Map an editor wave index (0 = oldest placed) to its primary track.  bindTrack()
	// prepends to m_usedModules, so list-head order is newest-first; the editor wants
	// stable oldest-first numbering, so we count primaries from the tail.
	WaterTracksObj *getPrimaryByEditorIndex(Int index) const;
	// Find the trailing "second wave" that shares a primary's init segment+type
	// (non-zero time offset).  Returns NULL if the type has no trailing wave.
	WaterTracksObj *findSecondWaveFor(const WaterTracksObj *primary) const;
};  // end class WaterTracksRenderObjClassSystem

extern WaterTracksRenderSystem *TheWaterTracksRenderSystem;	///< singleton for track drawing system.

#endif  //__W3DWaterTracks_H_
