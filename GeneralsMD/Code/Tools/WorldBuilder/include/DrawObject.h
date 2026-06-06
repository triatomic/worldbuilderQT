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


#ifndef __DRAW_OBJECT_H_
#define __DRAW_OBJECT_H_

#include "always.h"
#include "rendobj.h"
#include "w3d_file.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "shader.h"
#include "vertmaterial.h"
#include "Lib/BaseType.h"
#include "Common/AsciiString.h"

// The draw objects draw a circle of diameter 1.0 cells.
#define THE_RADIUS (0.8f*MAP_XY_FACTOR) 

class MeshClass;
class PolygonTrigger;
class WaterRenderObjClass;
class MapObject;
class Render2DClass;
class TextureClass;
//
// DrawObject: Draws 3d feedback for tools & objects.
//
//
class DrawObject : public RenderObjClass
{	

public:

	DrawObject(void);
	DrawObject(const DrawObject & src);
	DrawObject & operator = (const DrawObject &);
	~DrawObject(void);

	/////////////////////////////////////////////////////////////////////////////
	// Render Object Interface 
	/////////////////////////////////////////////////////////////////////////////
	virtual RenderObjClass *	Clone(void) const;
	virtual int						Class_ID(void) const;
	virtual void					Render(RenderInfoClass & rinfo);
//	virtual void					Special_Render(SpecialRenderInfoClass & rinfo);
//	virtual void 					Set_Transform(const Matrix3D &m); 
//	virtual void 					Set_Position(const Vector3 &v);
//TODO: MW: do these later - only needed for collision detection
	virtual Bool					Cast_Ray(RayCollisionTestClass & raytest);
//	virtual Bool					Cast_AABox(AABoxCollisionTestClass & boxtest);
//	virtual Bool					Cast_OBBox(OBBoxCollisionTestClass & boxtest);
//	virtual Bool					Intersect_AABox(AABoxIntersectionTestClass & boxtest);
//	virtual Bool					Intersect_OBBox(OBBoxIntersectionTestClass & boxtest);

	virtual void					Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const;
    virtual void					Get_Obj_Space_Bounding_Box(AABoxClass & aabox) const;


//	virtual int					 	Get_Num_Polys(void) const;
//	virtual const char *		 	Get_Name(void) const;
//	virtual void				 	Set_Name(const char * name);

//	unsigned int					Get_Flags(void)  { return Flags; }
//	void								Set_Flags(unsigned int flags) { Flags = flags; }
//	void								Set_Flag(unsigned int flag, Bool onoff) { Flags &= (~flag); if (onoff) Flags |= flag; }

	Int freeMapResources(void);
	int initData(void);

    void setDrawObjects(Bool val, Bool waypoints, Bool poly, Bool bounding, Bool sight, Bool weapon, Bool sound, Bool testart, Bool letterbox, Bool water, Bool iconssel, Bool fixedColorsW) { 
		m_drawObjects = val; 
		m_drawWaypoints=waypoints; 
		m_drawPolygonAreas = poly; 
		m_drawBoundingBoxes = bounding; 
		m_drawSightRanges = sight; 
		m_drawWeaponRanges = weapon; 
		m_drawSoundRanges = sound; 
		m_drawTestArtHighlight = testart, 
		m_drawLetterbox = letterbox;
		m_showWater = water;
		m_drawObjectsSelected = iconssel;
		m_useFixedColoredWaypoints = fixedColorsW;
	}

	static void setDoBrushFeedback(Bool val) { m_toolWantsFeedback = val; m_meshFeedback=false;}	
	static void setDoMeshFeedback(Bool val) { m_meshFeedback = val; }	
	static void setDoRampFeedback(Bool val) { m_rampFeedback = val; }
	static void setDoBoundaryFeedback(Bool val) { m_boundaryFeedback = val; }
	static void setDoWaveFeedback(Bool val) { m_waveFeedback = val; }
	static Bool getDoWaveFeedback(void) { return m_waveFeedback; }
	static void setDoGridFeedback(Bool val) { m_rulerGridFeedback = val; }
	static void setDoTracingOverlayFeedback(Bool val) { m_showTracingOverlay = val; }
	// Tracing overlay appearance (set from the TracingOverlayOptions dialog).
	// Opacity is 0..255 alpha; filter is TracingOverlayOptions::FILTER_* (0=default,
	// 1=nearest). Changing the filter invalidates the cached PNG so it re-decodes.
	static void setTracingOverlayOpacity(Int alpha) { m_tracingOverlayOpacity = alpha; }
	static void setTracingOverlayFilter(Int filter) { m_tracingOverlayFilter = filter; }

	// Tracing overlay file resolution. The overlay is per-map: it lives in
	// data\editor named after the current map, and may be a .png (decoded via
	// D3DX) or a .dds (loaded via the asset manager). Falls back to the legacy
	// "trace_overlay" base name when no map is loaded/saved yet.
	//   getTracingOverlayBaseName -> e.g. "data\editor\MyMap" (no extension)
	//   resolveTracingOverlayPath -> the existing .png/.dds file, or empty string
	static AsciiString getTracingOverlayBaseName(void);
	static AsciiString resolveTracingOverlayPath(void);
	// Recommended overlay texture size for the active map. PNG can be any size, so
	// it gets the exact map cell extents (outPngW/outPngH); DDS wants power-of-two,
	// so it gets those extents rounded up (outDdsW/outDdsH). Returns false if no
	// map is available; the out params are then left untouched.
	static Bool getTracingOverlayRecommendedSize(Int &outPngW, Int &outPngH,
																							 Int &outDdsW, Int &outDdsH);

	static void setDoAmbientSoundFeedback(Bool val) { m_ambientSoundFeedback = val; }
	static void setDoBaseRadiusFeedback(Bool val) { m_baseRadiusFeedback = val; }
	static void setForceDrawArrow(Bool val) { m_forceDrawArrow = val; }
	
	static void setBrushFeedbackParms(Bool square, Int width, Int featherWidth, Int height = 0) 
														{ m_squareFeedback = square; m_brushWidth=width;
															m_meshFeedback = false; m_brushFeatherWidth = featherWidth; m_brushHeight = height;}
	static void disableFeedback(void) {m_disableFeedback = true;};
	static void enableFeedback(void) {m_disableFeedback = false;};
	static Bool isFeedbackEnabled(void) { return !m_disableFeedback;};
	static void setFeedbackPos(Coord3D pos);

	static void setWaypointDragFeedback(const Coord3D &start, const Coord3D &end);
	static void setRampFeedbackParms(const Coord3D *start, const Coord3D *end, Real rampWidth);
	static void stopWaypointDragFeedback();

	static void setRoadIconColor(int val) { m_roadIconColor = val; }
	static void setWaypointIconColor(int val) { m_waypointIconColor = val; }
	static void setUnitIconColor(int val) { m_unitIconColor = val; }
	static void setDefaultIconColor(int val) { m_defaultIconColor = val; }
	static void setTreeIconColor(int val) { m_treeIconColor = val; }

	MeshClass *peekMesh(void) {return m_moldMesh;};
	void getMeshBounds(SphereClass *pSphere) {*pSphere = m_moldMeshBounds;};

	static Bool m_terrainPasteFeedback;
	static Coord3D m_terrainPasteCenter;
	static Int m_terrainPasteFeedbackRotation;	

protected:
	enum {MAX_RADIUS = 50, NUM_FEEDBACK_VERTEX = 201*201, NUM_FEEDBACK_INDEX = 101*101*6};
	Int	m_numTriangles;	//dimensions of list 

	DX8IndexBufferClass				*m_indexBuffer;	///< indices defining a object icon
	ShaderClass								m_shaderClass; ///< shader or rendering state for heightmap
	VertexMaterialClass	  	  *m_vertexMaterialClass;
	DX8VertexBufferClass			*m_vertexBufferTile1;	///< First vertex buffer.
	DX8VertexBufferClass			*m_vertexBufferTile2;	///< Second vertex buffer.

	DX8VertexBufferClass			*m_vertexBufferWater;	///< Vertex buffer for the water plane.
	DX8IndexBufferClass				*m_indexWater;	///< indices defining a triangle strip for the water on terrain
	Int												m_waterVertexCount;

	WaterRenderObjClass				*m_waterDrawObject;

	Bool											m_drawObjectsSelected;
	Bool											m_drawObjects;
	Bool											m_drawWaypoints;
	Bool											m_drawPolygonAreas;
	Bool											m_drawBoundingBoxes;
	Bool											m_drawSightRanges;
	Bool											m_drawWeaponRanges;
	Bool											m_useFixedColoredWaypoints;
  Bool                      m_drawSoundRanges;
	Bool											m_drawTestArtHighlight;
	Bool											m_drawLetterbox;
	Bool                                            m_showWater;

	DX8VertexBufferClass			*m_vertexFeedback;	///< Vertex buffer for brush feedback.
	DX8IndexBufferClass				*m_indexFeedback;	///< indices defining a triangle strip for the feedback on terrain
	Int												m_feedbackIndexCount;
	Int												m_feedbackVertexCount;

	// Cache for the PNG tracing overlay texture (decoded via D3DX). DDS overlays
	// go through the asset manager and are not cached here. We hold onto the
	// TextureClass and the path it was loaded from so we only reload when the
	// resolved overlay path changes (e.g. a different map is opened).
	TextureClass						*m_tracingOverlayTexture;	///< PNG overlay texture, or NULL.
	AsciiString							m_tracingOverlayLoadedPath;	///< Path m_tracingOverlayTexture was loaded from.
	Int												m_tracingOverlayLoadedFilter;	///< Filter the cached PNG was decoded with.

	AsciiString								m_curMeshModelName;  ///< Model name of m_moldMesh.

	MeshClass									*m_moldMesh;		///< W3D mesh model for the mold.
	SphereClass								m_moldMeshBounds;				///< Bounding sphere for mold mesh.
	Render2DClass							*m_lineRenderer;		//< Used to render 2D lines for bounding boxes.
	CPoint										m_winSize;				//< Holds the size of the window.

protected: // static state vars.
	static Bool								m_squareFeedback;	///< True for square brush feedback, false for round.
	static Int								m_brushWidth;	///< Width of brush feedback.
	static Int								m_brushHeight;	///< Height of brush feedback.
	static Int								m_brushFeatherWidth;	///< Width of brush feathered feedback.
	static Bool								m_toolWantsFeedback; ///< True to display brush feedback.
	static Bool								m_disableFeedback; ///< True to disable feedback.
	static Coord3D						m_feedbackPoint;	///< Current brush feedback location.
	static CPoint							m_cellCenter;		///< Cell to show feedback from.
	static Bool								m_meshFeedback;
	static Bool								m_rampFeedback;	///< should we be showing feedback for the ramp tool?
	static Bool								m_boundaryFeedback;
	static Bool								m_waveFeedback;		///< draw wave start->end overlay lines
	static Bool								m_rulerGridFeedback;
	static Bool								m_showTracingOverlay; ///< True to show tracing overlay.
	static Int								m_tracingOverlayOpacity;	///< 0..255 alpha for the overlay.
	static Int								m_tracingOverlayFilter;		///< 0=default(linear), 1=nearest(point).
	static Bool								m_ambientSoundFeedback;
	static Bool								m_baseRadiusFeedback;
	static Bool								m_forceDrawArrow;	///< True to force drawing arrow on roads/waypoints.

	static Bool								m_dragWaypointFeedback; ///< True for the waypoint tool dragging.
	static Coord3D						m_dragWayStart;///< Start drag waypoint feedback.
	static Coord3D						m_dragWayEnd; ///< End drag waypoint.

	static Coord3D						m_rampStartPoint;	///< Beginning ramp point
	static Coord3D						m_rampEndPoint;		///< End ramp point
	static Real								m_rampWidth;

	static Int								m_roadIconColor;
	static Int								m_waypointIconColor;
	static Int								m_unitIconColor;
	static Int								m_defaultIconColor;
	static Int								m_treeIconColor;

protected:
  void addCircleToLineRenderer( const Coord3D & center, Real radius, Real width, unsigned long color, CameraClass* camera );
	Int updateVB(DX8VertexBufferClass	*vertexBufferTile, Int color, Bool doArrow, Bool doDiamond, Bool disableColoring = true);
	void updatePolygonVB(PolygonTrigger *pTrig, Bool selected, Bool isOpen);
	void updateFeedbackVB(void);
	void updateMeshVB(void);
	void updateRampVB(void);
	void updateWaypointVB(RenderInfoClass & rinfo);
	void updateForWater(void);
	void updateBoundaryVB(void);
	void updateWaveVB(void);
	void updateTerrainPasteVB(void);
	void updateGridVB(void);
	void updateAmbientSoundVB(void);
	void updateVBWithBoundingBox(MapObject *pMapObj, CameraClass* camera);
	void updateVBWithSightRange(MapObject *pMapObj, CameraClass* camera);
	void updateVBWithWeaponRange(MapObject *pMapObj, CameraClass* camera);
	void updateVBWithTestArtHighlight(MapObject *pMapObj, CameraClass* camera);
  void updateVBWithSoundRanges(MapObject *pMapObj, CameraClass* camera);
	bool worldToScreen(const Coord3D *w, ICoord2D *s, CameraClass* camera);

};

void BuildRectFromSegmentAndWidth(const Coord3D* b, const Coord3D* t, Real width, 
																	Coord3D* outBL, Coord3D* outTL, Coord3D* outBR, Coord3D* outTR);

#endif  // end __DRAW_OBJECT_H_
