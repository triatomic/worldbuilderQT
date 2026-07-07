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


// WbView3d.h
// Class to encapsulate height map.
// Author: Steven Johnson, Aug 2001

#if !defined(AFX_WBVIEW3D_H__832D8241_87F6_11D5_8CE0_00010297BBAC__INCLUDED_)
#define AFX_WBVIEW3D_H__832D8241_87F6_11D5_8CE0_00010297BBAC__INCLUDED_

#define OBJECT_OPTION_PANEL "ObjectOptionPanel"
#define BUILDLIST_OPTION_PANEL "BuildListOptionPanel"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// wbview3d.h : header file
//

#include "Lib/BaseType.h"
#include "rendobj.h"
#include "robjlist.h"
#include "wbview.h"
#include "Common/GameType.h"
#include "Common/GlobalData.h"
#include "Common/ModelState.h"
#include "dx8wrapper.h"
#include "WBFontAtlas.h"

//#include "GameLogic/Module/BodyModule.h" -- Yikes... not necessary to include this! (KM)
enum BodyDamageType; //Ahhhh much better!

class WorldHeightMap;
class LayerClass;
class IntersectionClass;
class W3DAssetManager;
class SkeletonSceneClass;
class CameraClass;
class WBHeightMap;
class LightClass;
class MapObject;
class DrawObject;
class CWorldBuilderView;
class BuildListInfo;
class TransRenderObj;
struct ID3DXFont;

/////////////////////////////////////////////////////////////////////////////
// WbView3d view

class WbView3d : public WbView, public DX8_CleanupHook
{
protected:
	WbView3d();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(WbView3d)

// Attributes
public:

	// DX8_CleanupHook methods
	virtual void ReleaseResources(void);	///< Release all dx8 resources so the device can be reset.
	virtual void ReAcquireResources(void);  ///< Reacquire all resources after device reset.

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(WbView3d)
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~WbView3d();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(WbView3d)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnViewShowwireframe();
	afx_msg void OnUpdateViewShowwireframe(CCmdUI* pCmdUI);
	afx_msg void OnViewShowfullwireframe();
	afx_msg void OnUpdateViewShowfullwireframe(CCmdUI* pCmdUI);
	afx_msg void OnViewShowselectionoverlay();
	afx_msg void OnUpdateViewShowselectionoverlay(CCmdUI* pCmdUI);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnViewShowentire3dmap();
	afx_msg void OnUpdateViewShowentire3dmap(CCmdUI* pCmdUI);
	afx_msg void OnViewShowtopdownview();
	afx_msg void OnUpdateViewShowtopdownview(CCmdUI* pCmdUI);
	afx_msg void OnViewShowclouds();
	afx_msg void OnUpdateViewShowclouds(CCmdUI* pCmdUI);
	afx_msg void OnViewShowmacrotexture();
	afx_msg void OnUpdateViewShowmacrotexture(CCmdUI* pCmdUI);
	afx_msg void OnEditSelectmacrotexture();
	afx_msg void OnViewShowshadows();
	afx_msg void OnUpdateViewShowshadows(CCmdUI* pCmdUI);
	afx_msg void OnViewShowSoftWater();
	afx_msg void OnUpdateViewShowSoftWater(CCmdUI* pCmdUI);
	afx_msg void OnViewExtraBlends();
	afx_msg void OnUpdateViewShowExtraBlends(CCmdUI* pCmdUI);
	afx_msg void OnEditShadows();
	afx_msg void OnEditMapSettings();
	afx_msg void OnClearAllExtraBoundaries();
	afx_msg void OnViewShowimpassableareas();
	afx_msg void OnUpdateViewShowimpassableareas(CCmdUI* pCmdUI);
	afx_msg void OnImpassableAreaOptions();
	afx_msg void OnViewPartialmapsize96x96();
	afx_msg void OnUpdateViewPartialmapsize96x96(CCmdUI* pCmdUI);
	afx_msg void OnViewPartialmapsize192x192();
	afx_msg void OnUpdateViewPartialmapsize192x192(CCmdUI* pCmdUI);
	afx_msg void OnViewPartialmapsize160x160();
	afx_msg void OnUpdateViewPartialmapsize160x160(CCmdUI* pCmdUI);
	afx_msg void OnViewPartialmapsize128x128();
	afx_msg void OnUpdateViewPartialmapsize128x128(CCmdUI* pCmdUI);
	afx_msg void OnViewShowModels();
	afx_msg void OnUpdateViewShowModels(CCmdUI* pCmdUI);
	afx_msg void OnViewBoundingBoxes();
	afx_msg void OnUpdateViewBoundingBoxes(CCmdUI* pCmdUI);
	afx_msg void OnViewSightRanges();
	afx_msg void OnUpdateViewSightRanges(CCmdUI* pCmdUI);
	afx_msg void OnViewWeaponRanges();
	afx_msg void OnUpdateViewWeaponRanges(CCmdUI* pCmdUI);
	afx_msg void OnHighlightTestArt();
	afx_msg void OnUpdateHighlightTestArt(CCmdUI* pCmdUI);
	afx_msg void OnShowLetterbox();
	afx_msg void OnUpdateShowLetterbox(CCmdUI* pCmdUI);
	afx_msg void OnViewLayersList();
	afx_msg void OnUpdateViewLayersList(CCmdUI* pCmdUI);
	afx_msg void OnViewMinimap();
	afx_msg void OnUpdateViewMinimap(CCmdUI* pCmdUI);
#ifdef RTS_HAS_QT
public:
	// Startup restore: reopen the Qt Layers / Minimap tool windows if they were open last
	// session (== the old MFC OnCreate ShowWindow-from-profile, deferred to InitInstance's
	// tail where the Qt owner window exists). Open-only, no toggle.
	void qtRestoreStartupWindows();
protected:
#endif
	afx_msg void OnMinimapShowObjects();
	afx_msg void OnUpdateMinimapShowObjects(CCmdUI* pCmdUI);
	afx_msg void OnMinimapShowRoads();
	afx_msg void OnUpdateMinimapShowRoads(CCmdUI* pCmdUI);
	afx_msg void OnMinimapShowBorder();
	afx_msg void OnUpdateMinimapShowBorder(CCmdUI* pCmdUI);
	afx_msg void OnMinimapFullExtent();
	afx_msg void OnUpdateMinimapFullExtent(CCmdUI* pCmdUI);
	afx_msg void OnMinimapCullObjects();
	afx_msg void OnUpdateMinimapCullObjects(CCmdUI* pCmdUI);
	afx_msg void OnMinimapSnap45();
	afx_msg void OnUpdateMinimapSnap45(CCmdUI* pCmdUI);
	afx_msg void OnMinimapRefreshOff();
	afx_msg void OnMinimapRefresh16();
	afx_msg void OnMinimapRefresh33();
	afx_msg void OnMinimapRefresh100();
	afx_msg void OnMinimapRefresh250();
	afx_msg void OnMinimapRefresh1000();
	afx_msg void OnUpdateMinimapRefreshOff(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRefresh16(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRefresh33(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRefresh100(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRefresh250(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRefresh1000(CCmdUI* pCmdUI);
	afx_msg void OnMinimapRes256();
	afx_msg void OnMinimapRes512();
	afx_msg void OnMinimapRes1024();
	afx_msg void OnMinimapRes2048();
	afx_msg void OnUpdateMinimapRes256(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRes512(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRes1024(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMinimapRes2048(CCmdUI* pCmdUI);
	afx_msg void OnViewGarrisoned();
	afx_msg void OnUpdateViewGarrisoned(CCmdUI* pCmdUI);
	afx_msg void OnViewShowMapBoundaries();
	afx_msg void OnUpdateViewShowMapBoundaries(CCmdUI* pCmdUI);
	afx_msg void OnViewShowWaveLines();
	afx_msg void OnUpdateViewShowWaveLines(CCmdUI* pCmdUI);
	afx_msg void OnViewShowRulerGrid();
	afx_msg void OnUpdateViewShowRulerGrid(CCmdUI* pCmdUI);
	afx_msg void OnViewShowTracingOverlay();
	afx_msg void OnUpdateViewShowTracingOverlay(CCmdUI* pCmdUI);
	afx_msg void OnViewShowAmbientSounds();
	afx_msg void OnUpdateViewShowAmbientSounds(CCmdUI* pCmdUI);
	afx_msg void OnViewShowSoundCircles();
	afx_msg void OnUpdateViewShowSoundCircles(CCmdUI* pCmdUI);
	afx_msg void OnWindowLODMode1();
	afx_msg void OnUpdateOnWindowLODMode1(CCmdUI* pCmdUI);
	afx_msg void OnWindowLODMode2();
	afx_msg void OnUpdateOnWindowLODMode2(CCmdUI* pCmdUI);
	afx_msg void OnWindowLODMode3();
	afx_msg void OnUpdateOnWindowLODMode3(CCmdUI* pCmdUI);
	afx_msg void OnMSAANone();
	afx_msg void OnMSAA2X();
	afx_msg void OnMSAA4X();
	afx_msg void OnMSAA8X();
	afx_msg void OnUpdateMSAANone(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMSAA2X(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMSAA4X(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMSAA8X(CCmdUI* pCmdUI);
	afx_msg void OnResetDevice();
	afx_msg void OnTexFilterDefault();
	afx_msg void OnTexFilterAniso16X();
	afx_msg void OnUpdateTexFilterDefault(CCmdUI* pCmdUI);
	afx_msg void OnUpdateTexFilterAniso16X(CCmdUI* pCmdUI);
	afx_msg void OnTextShadow();
	afx_msg void OnUpdateTextShadow(CCmdUI* pCmdUI);
	afx_msg void OnTextAntialias();
	afx_msg void OnUpdateTextAntialias(CCmdUI* pCmdUI);
	afx_msg void OnTextAnchorDefault();
	afx_msg void OnUpdateTextAnchorDefault(CCmdUI* pCmdUI);
	afx_msg void OnTextAnchorNew();
	afx_msg void OnUpdateTextAnchorNew(CCmdUI* pCmdUI);
	afx_msg void OnTextRendererOld();
	afx_msg void OnUpdateTextRendererOld(CCmdUI* pCmdUI);
	afx_msg void OnTextRendererNew();
	afx_msg void OnUpdateTextRendererNew(CCmdUI* pCmdUI);
	afx_msg void OnTextRendererAtlas();
	afx_msg void OnUpdateTextRendererAtlas(CCmdUI* pCmdUI);
	afx_msg void OnTextLabelCullOff();
	afx_msg void OnUpdateTextLabelCullOff(CCmdUI* pCmdUI);
	afx_msg void OnTextLabelCullNear();
	afx_msg void OnUpdateTextLabelCullNear(CCmdUI* pCmdUI);
	afx_msg void OnTextLabelCullMedium();
	afx_msg void OnUpdateTextLabelCullMedium(CCmdUI* pCmdUI);
	afx_msg void OnTextLabelCullFar();
	afx_msg void OnUpdateTextLabelCullFar(CCmdUI* pCmdUI);
	afx_msg void OnViewShowSubDraw();
	afx_msg void OnUpdateViewShowSubDraw(CCmdUI* pCmdUI);
	afx_msg void OnViewShowBaseRadius();
	afx_msg void OnUpdateViewShowBaseRadius(CCmdUI* pCmdUI);
	afx_msg void OnRefreshSceneObjects();

	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);

  //}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	HINSTANCE								m_hInst;
	W3DAssetManager					*m_assetManager;
	SkeletonSceneClass			*m_scene;
	SkeletonSceneClass			*m_overlayScene;
	SkeletonSceneClass			*m_baseBuildScene;
	SkeletonSceneClass			*m_transparentObjectsScene;
	CameraClass							*m_camera;
	WBHeightMap							*m_heightMapRenderObj;
	Bool m_validTerrain;

	RenderObjClass					*m_objectToolTrackingObj;
	Bool										m_showObjToolTrackingObj;
	AsciiString							m_objectToolTrackingModelName;

	Real										m_mouseWheelOffset;
	Real										m_groundLevel;
	Coord3D									m_cameraOffset;
	CPoint									m_actualWinSize;
	Real										m_theta;
	Real										m_cameraAngle;
	Real										m_cameraAngleRaw;						// unsnapped accumulated rotation; m_cameraAngle is snapped from this when Angle Snap Lock is on
	Bool										m_snapCameraAngle45;					// Angle Snap Lock: constrain camera rotation to 45-degree steps
	Real										m_FXPitch;
	Bool										m_doPitch;
	Real										m_actualHeightAboveGround;	// for camera tool display only
	Vector3									m_cameraSource;							// for camera tool display only
	Vector3									m_cameraTarget;							// for camera tool display only
	Real										m_cameraGroundZ;						// terrain height (world Z) under the camera center; for the minimap view-box projection
	Real										m_cameraBorderWorld;					// border size * MAP_XY_FACTOR; subtracted to put frustum corners in border-relative (object) world
	Int											m_time;
	Int											m_updateCount;
	UINT										m_timer;
	DrawObject							*m_drawObject;
	RefRenderObjListClass		m_lightList;
	LayerClass							*m_layer;
	LayerClass							*m_buildLayer;
	IntersectionClass				*m_intersector;
	Bool										m_showWireframe;
	Bool										m_showFullWireframe;	///< true => render whole scene in LINE mode (no solid pass)
	Bool										m_showSelectionOverlay;	///< true => tint selected objects with a highlight color
	Bool										m_ww3dInited;
#ifdef RTS_HAS_QT
	// Set when a device reset (WW3D::Set_Device_Resolution) FAILED: Reset_Device releases
	// every DX8 resource before attempting the reset and does NOT re-acquire them on
	// failure, so rendering would deref freed buffers. redraw() retries while this is set.
	Bool										m_deviceResetFailed;
#endif
	Bool										m_needToLoadRoads;
	LightClass							*m_globalLight[MAX_GLOBAL_LIGHTS];
	RenderObjClass						*m_lightFeedbackMesh[MAX_GLOBAL_LIGHTS];

	Real										m_buildRedMultiplier;

	Real										m_curTrackingZ;


	Bool										m_projection; ///< True if top down projection instead of "isometric" perspective.
	Bool										m_showEntireMap; ///< True if drawing entire map instead of cached fast subset.
	Bool										m_showShadows; ///< True if drawing shadows.
	Bool										m_firstPaint;  ///< True if we haven't painted yet.
	Bool										m_showLayersList;	///< Flag whether the layers list is visible or not.
	Bool										m_showMapBoundaries;	///< Flag whether to show all the map boundaries or not
	Bool										m_showWaveLines;	///< Flag whether to draw wave start->end overlay lines
	Bool										m_showAmbientSounds;	///< Flag whether to show all the ambient sounds or not
  Bool										m_showSoundCircles;	///< Flag whether to show the minimum and maximum radii of the ambient sounds attached to the selected object
	Bool										m_showBoundingBoxes;
	Bool										m_showSightRanges;
	Bool										m_showWeaponRanges;
	Bool										m_highlightTestArt;
	Bool										m_showLetterbox;
	Bool										m_showRulerGrid;
	Bool										m_showTracingOverlay; ///< Flag whether to show the tracing overlay or not
	Bool										m_showBaseRadius; ///< Flag whether to show the base radius or not
	Bool										m_showSubDraw; ///< Flag whether to show the sub models

	Bool m_showBuildZoneFeedback;
	Int m_lod;
	Bool m_textShadow;
	Bool m_textAntialias;					///< grayscale antialiasing for viewport labels
	Int  m_labelAnchorMode;					///< 0 = Default (ground), 1 = New (object center-height)
	Int  m_labelRenderer;					///< 0 = Old (D3DX m3DFont, in-frame), 1 = New (raw GDI TextOut, strobes), 2 = Atlas (batched glyph quads, in-frame)
	Int  m_labelCull;						///< viewport-label cull: 0 = Off, 1 = Near, 2 = Medium, 3 = Far (ground distance from look-at target; zoom-independent)
	void setMSAA(D3DMULTISAMPLE_TYPE type);
	void setTextureFilter(int mode);
	void createLabelFont();					///< (re)create m3DFont honoring m_textAntialias


	ID3DXFont*							m3DFont;		// in-frame label font (Old renderer mode)
	WBFontAtlas							m_fontAtlas;	// GDI-built glyph atlas -> D3D quads

	// --- viewport-label change-detection key --------------------------------
	// Snapshot of everything that affects label geometry/colour. Built per frame
	// by buildLabelKey(); used by the GDI-mode repaint coalescing below to detect
	// "nothing changed" frames.
	struct LabelCacheKey
	{
		Real camXform[12];		// camera transform (3x4) signature
		Int  winW, winH;
		Int  lod;
		Bool showNames, showModels, showWaypoints, showNamesExtra, showPolygonTriggers;
		Bool lightFeedback;
		Int  timeOfDay;
		Int  labelAnchorMode;	// 0 = ground, 1 = center-height
		UnsignedInt epoch;		// bumped on object/property edits

		Bool operator==(const LabelCacheKey &o) const;
	};
	UnsignedInt		m_labelEpoch;		// bumped whenever labels may have changed

	// --- GDI-mode (Label Renderer: New) flicker coalescing -----------------
	// Raw GDI ::TextOut on the window strobes because every repaint flips the D3D
	// back buffer (no text) over the whole window before TextOut redraws the text.
	// On a STATIC view we therefore skip the OnTimer repaint entirely, so the last
	// presented frame + its GDI text persist on screen (no flip = no wipe = no
	// strobe). We resume repainting when buildLabelKey() changes, while a mouse
	// drag / camera track is active, or after a low-rate fallback interval (so
	// anything that changes without bumping the key still updates within ~5s).
	// Only consulted when m_labelRenderer == 1; D3DX mode keeps free-running.
	LabelCacheKey	m_lastGdiPaintKey;	// buildLabelKey() snapshot at the last GDI repaint
	Bool			m_haveGdiPaintKey;	// m_lastGdiPaintKey is valid
	LabelCacheKey	buildLabelKey();
	Int											m_pickPixels;
	Int											m_partialMapSize;
	Real m_lastTrackingZ;     // stores last used ghost placement height
	Bool m_lastTrackingZIsFromHighElev;

    DWORD m_editStartTime;      // Timestamp when editing started
    DWORD m_totalEditTime;      // Total accumulated edit time in milliseconds
    Bool m_isTimerRunning;      // Whether the timer is currently running
protected:

	UINT getLastDrawTime();
	void init3dScene();
	void initAssets();
	void initWW3D();
	void drawCircle( HDC hdc, const Coord3D & centerPoint, Real radius, COLORREF color );
	void drawLabels(HDC hdc);
	void drawLabels(void);
	void drawStatusLabels(CPoint basePt, int offset, const char* text, void* m3DFont, HDC hdc);
	void shutdownWW3D();
	void killTheTimer();
	void render();
	void setupCamera();
	void updateHysteresis(void);
	void updateLights();
	void updateScorches();
	void updateTrees();
	int parseHexColorFromProfile(const char* section, const char* key, const char* defaultHex);
	// Read the 5 EntityIconColor profile values once and push them into DrawObject's
	// (static) icon-color slots. Call once at init, and again whenever those keys change
	// -- NOT every redraw (each parse is a registry/INI hit). See redraw().
	void reloadIconColors();
	// void addMapObjectIfVisible(MapObject *pMapObj);
	// void updateVisibleMapObjects();
	

public:

	/// In-memory copy of the View > Show Object Selection Overlay toggle (authoritative;
	/// the menu handler keeps it and the registry in sync). Lets the click path test the
	/// overlay state without an uncached GetProfileInt registry read per click.
	Bool getShowSelectionOverlay() const { return m_showSelectionOverlay; }

	void startEditTimer();
	void pauseEditTimer();
	void resetEditTimer();
	void setEditTime(DWORD second);
	DWORD getEditTimeInSeconds();
	AsciiString formatEditTime();

	virtual Bool viewToDocCoords(CPoint curPt, Coord3D *newPt, Bool constrain=true);
	virtual Bool docToViewCoords(Coord3D curPt, CPoint* newPt);

	virtual void updateHeightMapInView(WorldHeightMap *htMap, Bool partial, const IRegion2D &partialRange);

	/// Invalidates an object. Pass NULL to inval all objects.
	virtual void invalObjectInView(MapObject *pObj);

	// find the best model for an object
	AsciiString getBestModelName(const ThingTemplate* tt, const ModelConditionFlags& c);

	// Adriane [Deathscythe] - Nasty dogshit hack
	AsciiString getBestModelNameWBPrev(const ThingTemplate* tt, const ModelConditionFlags& c);
	
	/// Invalidates an build list object.
	void invalBuildListItemInView(BuildListInfo *pBuild);

	/// Invalidates the area of one height map cell in the 2d view.
	virtual void invalidateCellInView(int xIndex, int yIndex);

	/// Scrolls the window by this amount.
	virtual void scrollInView(Real x, Real y, Bool end);

	virtual void setDefaultCamera();
	virtual void rotateCamera(Real delta);	 
	virtual void pitchCamera(Real delta);
	void setCameraPitch(Real absolutePitch);
	Real getCameraPitch(void);
	Real getCurrentZoom(void); //WST 10/17/2002
	Real getHeightAboveGround(void) { return m_actualHeightAboveGround; }
	Vector3 getCameraSource(void) { return m_cameraSource; }
	Vector3 getCameraTarget(void) { return m_cameraTarget; }
	Real getCameraAngle(void) { return m_cameraAngle; }
	CPoint getActualWinSize(void) {return m_actualWinSize;}

	/// Fill corners[4] with the world-space ground-plane points of the view frustum
	/// (the 4 viewport corners cast to the ground), for drawing a minimap view box.
	/// Order: top-left, top-right, bottom-right, bottom-left. Returns false if no camera.
	Bool getViewFrustumGroundCorners(Coord3D corners[4]);

	Real getLastTrackingZ(void) { return m_lastTrackingZ; }
	Bool getLastTrackingZIsFromHighElev(void) { return m_lastTrackingZIsFromHighElev; }
	virtual MapObject *picked3dObjectInView(CPoint viewPt);
	virtual BuildListInfo *pickedBuildObjectInView(CPoint viewPt);

	void removeFenceListObjects(MapObject *pObject);
	void updateFenceListObjects(MapObject *pObject);

	/// Removes all render objects.  Call when swithing to a new map.
	void resetRenderObjects();

	void stepTimeOfDay(void);

	void reset3dEngineDisplaySize(Int width, Int height); ///< Closes & reinitializes w3d.
	void setLighting(const GlobalData::TerrainLighting *tl, Int whichLighting, Int whichLight=0);

	DrawObject *getDrawObject(void) {return m_drawObject;};

	AsciiString getModelNameAndScale(MapObject *pMapObj, Real *scale, BodyDamageType curDamageState);

	virtual Int getPickPixels(void) {return m_pickPixels;}
	virtual Bool viewToDocCoordZ(CPoint curPt, Coord3D *newPt, Real Z); 
public:

//	void init(CWorldBuilderView *pMainView, HINSTANCE hInstance, CWnd* parent);
	void redraw();

	virtual void setCenterInView(Real x, Real y);

	// Like setCenterInView, but does NOT render synchronously. Updates the camera
	// center and invalidates the view so its own paint loop renders. Safe to call
	// from another window's message handler (e.g. the Minimap dialog) without
	// driving the D3D device from a foreign context.
	void setCenterInViewDeferred(Real x, Real y);

	Bool getShowTerrain();
	Bool getShowWireframe();

	// Wave-line overlay toggle, shared by the View menu and the Wave Editor panel
	// checkbox so the two stay in sync (flag + registry + DrawObject + redraw).
	Bool getShowWaveLines(void) { return m_showWaveLines; }
	void setShowWaveLines(Bool show);

	// void setShowBuildZoneFeedBack(Bool toggle) {m_showBuildZoneFeedback = toggle;}
	Bool getShowBuildZoneFeedBack(void) { return ::AfxGetApp()->GetProfileInt(OBJECT_OPTION_PANEL, "PreviewBuildZone", 1);}
	Bool getUseWaterHeight(void) { return ::AfxGetApp()->GetProfileInt(OBJECT_OPTION_PANEL, "UseWaterHeight", 0);}
	Bool getShowBuildListObjects(void) { return ::AfxGetApp()->GetProfileInt(BUILDLIST_OPTION_PANEL, "ForceShowBuildListObjects", 0);}

	void setObjTracking(MapObject *pMapObj, Coord3D pos, Real angle, Bool show);
	void setViewLayersList(Bool showLayersList) { m_showLayersList = showLayersList; }

	Bool getShowMapBoundaryFeedback(void) const { return m_showMapBoundaries; }
	Bool getShowAmbientSoundsFeedback(void) const { return m_showAmbientSounds; }

	Bool getShowGridFeedback(void) const { return m_showRulerGrid; }
	Bool getShowTracingOverlay(void) const { return m_showTracingOverlay; }

	void togglePitchAndRotation( void ) { m_doPitch = !m_doPitch; }
	virtual Bool isDoingPitch( void ) { return m_doPitch; }
	void setShowBoundingBoxes(Bool toggle) {m_showBoundingBoxes = toggle;}
	Bool getShowBoundingBoxes(void) { return m_showBoundingBoxes;}
	void setShowSightRanges(Bool toggle) {m_showSightRanges = toggle;}
	Bool getShowSightRanges(void) { return m_showSightRanges;}
	void setShowWeaponRanges(Bool toggle) {m_showWeaponRanges = toggle;}
	Bool getShowWeaponRanges(void) { return m_showWeaponRanges;}
	void setHighlightTestArt(Bool toggle) {m_highlightTestArt = toggle;}
	Bool getHighlightTestArt(void) { return m_highlightTestArt;}
	void setShowLetterbox(Bool toggle) {m_showLetterbox = toggle;}
	Bool getShowLetterbox(void) { return m_showLetterbox;}
};

inline UINT WbView3d::getLastDrawTime() { return m_time; }
inline Bool WbView3d::getShowWireframe() { return m_showWireframe; }


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WBVIEW3D_H__832D8241_87F6_11D5_8CE0_00010297BBAC__INCLUDED_)
