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

// wbview3d.cpp : implementation file
//

// #include "Common/GameLOD.h"
#include "StdAfx.h"
#include "resource.h"
#include "wwmath.h"
#include "ww3d.h"
#include "WBParallel.h"		// parallel fork-join pool for label projection
#include <vector>
#include "texturefilter.h"
#include "scene.h"
#include "rendobj.h"
#include "camera.h"
#include "intersec.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "W3DDevice/GameClient/Module/W3DTreeDraw.h"
#include "W3DDevice/GameClient/W3DBridgeBuffer.h"
#include "agg_def.h"
#include "part_ldr.h"
#include "rendobj.h"
#include "hanim.h"
#include "dx8wrapper.h"
#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "dx8renderer.h"
#include "dx8fvf.h"
#include "vertmaterial.h"
#include "font3d.h"
#include "render2d.h"
#include "rddesc.h"
#include "textdraw.h"
#include "rect.h"
#include "mesh.h"
#include "meshmdl.h"
#include "line3d.h"
#include "dynamesh.h"
#include "sphereobj.h"
#include "ringobj.h"
#include "surfaceclass.h"
#include "vector2i.h"
#include "bmp2d.h"
#include "decalsys.h"
#include "shattersystem.h"
#include "light.h"
#include "texproject.h"
#include "MapSettings.h"
#include "predlod.h"
#include "SelectMacrotexture.h"
#include "WorldBuilderView.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "MainFrm.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "WBHeightMap.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/Common/W3DConvert.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "DrawObject.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/MapObject.h"
#include "Common/GlobalData.h"
#include "ShadowOptions.h"
#include "WorldBuilder.h"
#include "wbview3d.h"
#include "Common/Debug.h"
#include "Common/ThingFactory.h"
#include "GameClient/Water.h"
#include "Common/WellKnownKeys.h"
#include "Common/ThingTemplate.h"
#include "Common/Language.h"
#include "Common/FileSystem.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/View.h"
#include "GlobalLightOptions.h"
#include "LayersList.h"
#include "MinimapDialog.h"
#include "ImpassableOptions.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
// #include "CUndoable.h"


#include <d3dx8.h>

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

// ----------------------------------------------------------------------------
// Misc. Forward Declarations
// ----------------------------------------------------------------------------
class SkeletonSceneClass;

// ----------------------------------------------------------------------------
// Constants:
// ----------------------------------------------------------------------------
#define MAX_LOADSTRING			100
#define WINDOW_WIDTH				640
#define WINDOW_HEIGHT				480
#define UPDATE_TIME					16  /* 10 frames a second */
#define MOUSE_WHEEL_FACTOR	32

#define ICON_COLOR_SECTION "EntityIconColor"
#define BRIDGE_FLOAT_AMT (0.25f)

#define SAMPLE_DYNAMIC_LIGHT	1
#ifdef SAMPLE_DYNAMIC_LIGHT
static W3DDynamicLight * theDynamicLight = NULL;
static Real theLightXOffset = 0.1f;
static Real theLightYOffset = 0.07f;
static Int theFlashCount = 0;
static Bool g_alreadyHintedTraceOverlay = false;
#endif

// ----------------------------------------------------------------------------
// Global Variables:
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Static Functions
// ----------------------------------------------------------------------------

static void		WWDebug_Message_Callback(DebugType type, const char * message);
static void		WWAssert_Callback(const char * message);
static void		Debug_Refs(void);

// ----------------------------------------------------------------------------
static void WWDebug_Message_Callback(DebugType type, const char * message)
{
#ifdef _DEBUG
	::OutputDebugString(message);
#endif
}

// ----------------------------------------------------------------------------
static void WWAssert_Callback(const char * message)
{
#ifdef _DEBUG
	::OutputDebugString(message);
	::DebugBreak();
#endif
}


// The W3DShadowManager accesses TheTacticalView, so we have to create
// a stub class & object in Worldbuilder for it to access.
class PlaceholderView : public View
{
protected:
	Int m_width, m_height;																			///< Dimensions of the view
	Int m_originX, m_originY;																		///< Location of top/left view corner

protected:
	virtual View *prependViewToList( View *list ) {return NULL;};		///< Prepend this view to the given list, return the new list
	virtual View *getNextView( void ) { return NULL; }				///< Return next view in the set
public:

	virtual void init( void ){};

	virtual UnsignedInt getID( void ) { return 1; }

	virtual Drawable *pickDrawable( const ICoord2D *screen, Bool forceAttack, PickType pickType ){return NULL;};			///< pick drawable given the screen pixel coords

	/// all drawables in the 2D screen region will call the 'callback'
	virtual Int iterateDrawablesInRegion( IRegion2D *screenRegion,
																				Bool (*callback)( Drawable *draw, void *userData ),
																				void *userData ) {return 0;};
  virtual WorldToScreenReturn worldToScreenTriReturn( const Coord3D *w, ICoord2D *s ) { return WTS_INVALID; };	///< Transform world coordinate "w" into screen coordinate "s"
	virtual void screenToWorld( const ICoord2D *s, Coord3D *w ) {};	///< Transform screen coordinate "s" into world coordinate "w"
	virtual void screenToTerrain( const ICoord2D *screen, Coord3D *world ) {};  ///< transform screen coord to a point on the 3D terrain
	virtual void screenToWorldAtZ( const ICoord2D *s, Coord3D *w, Real z ) {};  ///< transform screen point to world point at the specified world Z value
	virtual void getScreenCornerWorldPointsAtZ( Coord3D *topLeft, Coord3D *topRight,
																							Coord3D *bottomLeft, Coord3D *bottomRight,
																							Real z ){};

	virtual void drawView( void ) {};															///< Render the world visible in this view.
	virtual void updateView( void ) {};															///< Render the world visible in this view.

	virtual void setZoomLimited( Bool limit ) {}			///< limit the zoom height
	virtual Bool isZoomLimited( void ) { return TRUE; }							///< get status of zoom limit

	virtual void setWidth( Int width ) { m_width = width; }
	virtual Int getWidth( void ) { return m_width; }
	virtual void setHeight( Int height ) { m_height = height; }
	virtual Int getHeight( void ) { return m_height; }
	virtual void setOrigin( Int x, Int y) { m_originX=x; m_originY=y;}				///< Sets location of top-left view corner on display 
	virtual void getOrigin( Int *x, Int *y) { *x=m_originX; *y=m_originY;}			///< Return location of top-left view corner on display

	virtual void forceRedraw() { }

	virtual void lookAt( const Coord3D *o ){};														///< Center the view on the given coordinate
	virtual void initHeightForMap( void ) {};														///<  Init the camera height for the map at the current position.
	virtual void scrollBy( Coord2D *delta ){};														///< Shift the view by the given delta
	virtual void moveCameraTo(const Coord3D *o, Int frames, Int shutter, 
														Bool orient, Real easeIn, Real easeOut) {lookAt(o);};
	virtual void moveCameraAlongWaypointPath(Waypoint *way, Int frames, Int shutter, 
														Bool orient, Real easeIn, Real easeOut) {};
	virtual Bool isCameraMovementFinished(void) {return true;}; 
 	virtual void resetCamera(const Coord3D *location, Int frames, Real easeIn, Real easeOut) {}; ///< Move camera to location, and reset to default angle & zoom.
 	virtual void rotateCamera(Real rotations, Int frames, Real easeIn, Real easeOut) {}; ///< Rotate camera about current viewpoint.
	virtual void rotateCameraTowardObject(ObjectID id, Int milliseconds, Int holdMilliseconds, Real easeIn, Real easeOut) {};	///< Rotate camera to face an object, and hold on it
	virtual void cameraModFinalZoom(Real finalZoom, Real easeIn, Real easeOut){};			 ///< Final zoom for current camera movement.
	virtual void cameraModRollingAverage(Int framesToAverage){}; ///< Number of frames to average movement for current camera movement.
	virtual void cameraModFinalTimeMultiplier(Int finalMultiplier){}; ///< Final time multiplier for current camera movement.
	virtual void cameraModFinalPitch(Real finalPitch, Real easeIn, Real easeOut){};		 ///< Final pitch for current camera movement.
	virtual void cameraModFreezeTime(void){}					///< Freezes time during the next camera movement.
	virtual void cameraModFreezeAngle(void){}					///< Freezes time during the next camera movement.
	virtual void cameraModLookToward(Coord3D *pLoc){}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalLookToward(Coord3D *pLoc){}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalMoveTo(Coord3D *pLoc){ };			///< Sets a final move to.
	virtual Bool isTimeFrozen(void){ return false;}					///< Freezes time during the next camera movement.
	virtual Int	 getTimeMultiplier(void) {return 1;};				///< Get the time multiplier.
	virtual void setTimeMultiplier(Int multiple) {}; ///< Set the time multiplier.
	virtual void setDefaultView(Real pitch, Real angle, Real maxHeight) {};
	virtual void zoomCamera( Real finalZoom, Int milliseconds, Real easeIn, Real easeOut ) {};
	virtual void pitchCamera( Real finalPitch, Int milliseconds, Real easeIn, Real easeOut ) {};
															
	virtual void setAngle( Real angle ){};																///< Rotate the view around the up axis to the given angle
	virtual Real getAngle( void ) { return 0; }
	virtual void setPitch( Real angle ){};																	///< Rotate the view around the horizontal axis to the given angle
	virtual Real getPitch( void ) { return 0; }							///< Return current camera pitch
	virtual void setAngleAndPitchToDefault( void ){};													///< Set the view angle back to default 
	virtual void getPosition(Coord3D *pos)	{ ;}											///< Return camera position

	virtual Real getHeightAboveGround() { return 1; }
	virtual void setHeightAboveGround(Real z) { }
	virtual Real getZoom() { return 1; }
	virtual void setZoom(Real z) { }
	virtual void zoomIn( void ) {  }																			///< Zoom in, closer to the ground, limit to min
	virtual void zoomOut( void ) {  }																		///< Zoom out, farther away from the ground, limit to max
	virtual void setZoomToDefault( void ) { }														///< Set zoom to default value
	virtual Real getMaxZoom( void ) { return 0.0f; }
	virtual void setOkToAdjustHeight( Bool val ) { }						///< Set this to adjust camera height

	virtual Real getTerrainHeightUnderCamera() { return 0.0f; }
	virtual void setTerrainHeightUnderCamera(Real z) { }
	virtual Real getCurrentHeightAboveGround() { return 0.0f; }
	virtual void setCurrentHeightAboveGround(Real z) { }

	virtual void getLocation ( ViewLocation *location ) {};								///< write the view's current location in to the view location object
	virtual void setLocation ( const ViewLocation *location ){};								///< set the view's current location from to the view location object

	virtual ObjectID getCameraLock() const { return INVALID_ID; }
	virtual void setCameraLock(ObjectID id) {  }
	virtual void snapToCameraLock( void ) {  }
	virtual void setSnapMode( CameraLockType lockType, Real lockDist ) {  }

	virtual Drawable *getCameraLockDrawable() const { return NULL; }
	virtual void setCameraLockDrawable(Drawable *drawable) { }

	virtual void setMouseLock( Bool mouseLocked ) {}					///< lock/unlock the mouse input to the tactical view
	virtual Bool isMouseLocked( void ) { return FALSE; }			///< is the mouse input locked to the tactical view?

	virtual void setFieldOfView( Real angle ) {};							///< Set the horizontal field of view angle
	virtual Real getFieldOfView( void ) {return 0;};										///< Get the horizontal field of view angle

	virtual Bool setViewFilterMode(enum FilterModes) {return FALSE;}	///<stub
	virtual void setViewFilterPos(const Coord3D *pos) {};	///<stub
	virtual void setFadeParameters(Int fadeFrames, Int direction) {};	///<stub
	virtual void set3DWireFrameMode(Bool enable) { }; ///<stub
	virtual Bool setViewFilter(		enum FilterTypes m_viewFilterMode) { return FALSE;}	///<stub
	virtual enum FilterModes	 getViewFilterMode(void) {return (enum FilterModes)0;}			///< Turns on viewport special effect (black & white mode)
	virtual enum FilterTypes	 getViewFilterType(void) {return (enum FilterTypes)0;}			///< Turns on viewport special effect (black & white mode)

	virtual void shake( const Coord3D *epicenter, CameraShakeType shakeType ) {};
	
	virtual Real getFXPitch( void ) const { return 1.0f; }
	virtual void forceCameraConstraintRecalc(void) { }
	virtual void rotateCameraTowardPosition(const Coord3D *pLoc, Int milliseconds, Real easeIn, Real easeOut, Bool reverseRotation) {};	///< Rotate camera to face an object, and hold on it

	virtual const Coord3D& get3DCameraPosition() const { static Coord3D dummy; return dummy; }							///< Returns the actual camera position

	virtual void setGuardBandBias( const Coord2D *gb ) {};

};

PlaceholderView bogusTacticalView;



// ----------------------------------------------------------------------------
// Customized scene for worldbuilder preview window.
// ----------------------------------------------------------------------------

class SkeletonSceneClass : public RTS3DScene
{
public:
	SkeletonSceneClass(void) : m_testPass(NULL) { }
	~SkeletonSceneClass(void) { REF_PTR_RELEASE(m_testPass); }

	void					Set_Material_Pass(MaterialPassClass * pass)	{ REF_PTR_SET(m_testPass, pass); }	
	virtual void Remove_Render_Object(RenderObjClass * obj);

	Bool safeContains(RenderObjClass *obj);

protected:
	MaterialPassClass *m_testPass;
};


Bool SkeletonSceneClass::safeContains(RenderObjClass *obj)
{
	Bool found = false;
	SceneIterator *sceneIter = Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		if (obj==sceneIter->Current_Item()) {
			found = true;
			break;
		}
		sceneIter->Next();
	}
	Destroy_Iterator(sceneIter);
	return found;
}

// ----------------------------------------------------------------------------

void SkeletonSceneClass::Remove_Render_Object(RenderObjClass * obj)
{
	if (RenderList.Contains(obj)) {
		RenderObjClass *refPtr = NULL;
		REF_PTR_SET(refPtr, obj); // ref it, as when it gets removed from the scene, may get deleted otherwise.
		RTS3DScene::Remove_Render_Object(obj);
		REF_PTR_RELEASE(refPtr);
	}
}

// Bool pleaseHelpMeIamUnderTheWata( Real x, Real y)
// {
// 	ICoord3D iLoc;
// 	iLoc.x = (floor(x+0.5f));
// 	iLoc.y = (floor(y+0.5f));
// 	iLoc.z = 0;
// 	// Look for water areas.
// 	for (PolygonTrigger *pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
// 		if (!pTrig->isWaterArea()) {
// 			continue;
// 		}
// 		// See if point is in a water area
// 		if (pTrig->pointInTrigger(iLoc)) {
// 			Real wZ = pTrig->getPoint(0)->z;
// 			// See if the ground height is less than the water level.
// 			Real curHeight = TheTerrainRenderObject->getHeightMapHeight(x, y, NULL);
// 			return (curHeight<wZ);
// 		}
// 	}
// 	return false;
// }

/**
 * Adriane [Deathscythe]
 * Non computationaly expensive suggested check :P 
 */
Bool isInsideMapStructureObject(Real x, Real y)
{
	MapObject* pObj = MapObject::getFirstMapObject();

	while (pObj) {
		const ThingTemplate* t = pObj->getThingTemplate();
		if (!t || !t->isKindOf(KINDOF_STRUCTURE)) {
			pObj = pObj->getNext();
			continue;
		}

		const Coord3D* pos = pObj->getLocation();
		const GeometryInfo& geom = t->getTemplateGeometryInfo();

		Real halfSizeX = geom.getMajorRadius();
		Real halfSizeY = geom.getMinorRadius();

		Real angle = pObj->getAngle(); // Assuming this gives rotation in radians around Z
		Real cosA = (Real)cos(-angle); // Inverse rotate
		Real sinA = (Real)sin(-angle);

		// Translate point to object-local space
		Real dx = x - pos->x;
		Real dy = y - pos->y;

		Real localX = dx * cosA - dy * sinA;
		Real localY = dx * sinA + dy * cosA;

		// Check if point is within the axis-aligned bounding box in local space
		if (fabs(localX) <= halfSizeX && fabs(localY) <= halfSizeY) {
			return true;
		}

		pObj = pObj->getNext();
	}

	return false;
}


void WbView3d::setObjTracking(MapObject *pMapObj,  Coord3D pos, Real angle, Bool show)
{
	m_showObjToolTrackingObj = show;
	if (!show) return;
	Real scale;
	AsciiString modelName = getModelNameAndScale(pMapObj, &scale, BODY_PRISTINE);

	// Adriane [Deathscythe] The worldbuilder's scale change is very off for infantry -- adjust properly
	if (scale > 2.0 && pMapObj->getThingTemplate()->isKindOf(KINDOF_INFANTRY)) {
		scale *= 4.0f;  // scale up to 350% 
	}

	if (modelName != m_objectToolTrackingModelName) {
		m_objectToolTrackingModelName = modelName;
		REF_PTR_RELEASE(m_objectToolTrackingObj);
		m_objectToolTrackingObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, 0);
	}
	if (m_objectToolTrackingObj == NULL) {
		return;
	}
	

	m_validTerrain = true; // always true
	if(getShowBuildZoneFeedBack()){
		const Coord3D *loc = pMapObj->getLocation();
		const ThingTemplate *t = pMapObj->getThingTemplate();
		Real objectAngle = pMapObj->getAngle();

		Real halfSizeX = 1.0f;
		Real halfSizeY = 1.0f;
		
		if (t) {
			const GeometryInfo &geom = t->getTemplateGeometryInfo();
			halfSizeX = geom.getMajorRadius();
			halfSizeY = geom.getMinorRadius();
		}

		// outer margin for the bounding box of the object 
		// halfSizeY += 10.0f;
		// halfSizeX += 10.0f;
		
		const int numSamples = 5;
		Bool terrainValid = true;
		
		Real cosA = (Real)cos(angle);
		Real sinA = (Real)sin(angle);
		bool anyCliff = false;
		bool anyBadBuild = false;

		Real objectSize = max(halfSizeX, halfSizeY); // or use a better approximation

		for (int i = -numSamples; i <= numSamples && !anyCliff; ++i) {
			for (int j = -numSamples; j <= numSamples && !anyCliff; ++j) {

				// skip outer circle if desired
				Real fx = (Real)i / (Real)numSamples;
				Real fy = (Real)j / (Real)numSamples;
				if ((fx * fx + fy * fy) > 1.0f)
					continue;

				Real offsetX = fx * halfSizeX;
				Real offsetY = fy * halfSizeY;

				Real rotatedX = offsetX * cosA - offsetY * sinA;
				Real rotatedY = offsetX * sinA + offsetY * cosA;

				Real sampleX = pos.x + rotatedX;
				Real sampleY = pos.y + rotatedY;

				if (m_heightMapRenderObj->isCliffCell(sampleX, sampleY)) {
					// DEBUG_LOG(("Cliff terrain at sample (%.2f, %.2f)\n", sampleX, sampleY));
					anyCliff = true;
					break;
				}

				if (i == 0 && j == 0 && m_heightMapRenderObj->isBadBuildLocation(pos.x, pos.y, objectAngle, halfSizeX, halfSizeY) || 
					isInsideMapStructureObject(sampleX, sampleY)) {
					// DEBUG_LOG(("Bad build location at (%.2f, %.2f) for object size %.2f\n", sampleX, sampleY, objectSize));
					anyBadBuild = true;
				}

				
			}
		}

	
		m_validTerrain = !anyCliff && !anyBadBuild;
	}

	bool usedBridgeHeight = false;
	MapObject *prevBridge = NULL; 
	if (getUseWaterHeight()) {
		for (MapObject *cur = MapObject::getFirstMapObject(); cur; cur = cur->getNext()) {
			if (cur->getFlag(FLAG_BRIDGE_POINT1)) {
				prevBridge = cur;
				continue;
			}
			
			if (cur->getFlag(FLAG_BRIDGE_POINT2) && prevBridge) {
				Vector3 pt1, pt2;

				pt1.Set(prevBridge->getLocation()->x, prevBridge->getLocation()->y, 0);
				pt2.Set(cur->getLocation()->x, cur->getLocation()->y, 0);

				pt1.Z = TheTerrainRenderObject->getHeightMapHeight(pt1.X, pt1.Y, NULL) + BRIDGE_FLOAT_AMT;
				pt2.Z = TheTerrainRenderObject->getHeightMapHeight(pt2.X, pt2.Y, NULL) + BRIDGE_FLOAT_AMT;

				Vector3 bridgeVec = pt2 - pt1;
				Vector3 toObj = Vector3(pos.x, pos.y, 0) - pt1;

				float bridgeLenSq = bridgeVec.X * bridgeVec.X + bridgeVec.Y * bridgeVec.Y;
				float t = (bridgeLenSq > 0.0001f) ? ((toObj.X * bridgeVec.X + toObj.Y * bridgeVec.Y) / bridgeLenSq) : 0.0f;

				Vector3 closestPt = pt1 + bridgeVec * clamp(t, 0.0f, 1.0f);
				Vector3 delta = Vector3(pos.x, pos.y, 0) - closestPt;
				float distSq = delta.X * delta.X + delta.Y * delta.Y;

				W3DBridgeBuffer* bridgeBuffer = m_heightMapRenderObj->getBridgeBuffer();
				BridgeInfo info;

				if (bridgeBuffer) {
					info = bridgeBuffer->getBridgeInfoFromMapObject(prevBridge, cur);
					// DEBUG_LOG(("Bridge Width: %.0f", info.bridgeWidth));
				}

				const float maxBridgeHalfWidth = info.bridgeWidth * 0.5f;

				// Only apply height if point is within bridge bounds AND width
				if (t >= 0.0f && t <= 1.0f && distSq <= maxBridgeHalfWidth * maxBridgeHalfWidth) {
					float bridgeZ = pt1.Z + t * (pt2.Z - pt1.Z);
					pos.z = bridgeZ;
					m_lastTrackingZ = pos.z - TheTerrainRenderObject->getHeightMapHeight(pos.x, pos.y, NULL);
					m_lastTrackingZIsFromHighElev = true;
					usedBridgeHeight = true;
					break;  // Stop after first valid match
				}

				prevBridge = NULL;  // Only pair once
			} else {
				prevBridge = NULL;
			}
		}
	}
	if (!usedBridgeHeight) {
		Real terrainZ = m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, NULL);
		Real waterZ = m_heightMapRenderObj->getWaterHeightIfUnderwater(pos.x, pos.y);

		if (waterZ != -FLT_MAX && getUseWaterHeight()) {
			pos.z = waterZ;
			m_lastTrackingZ = pos.z - terrainZ;
			m_lastTrackingZIsFromHighElev = true;
		} else {
			pos.z = terrainZ;
			m_lastTrackingZIsFromHighElev = false;
		}
	}
	Matrix3D renderObjPos(true);	// init to identity
	renderObjPos.Translate(pos.x, pos.y, pos.z);
	renderObjPos.Rotate_Z(angle);
	m_objectToolTrackingObj->Set_Transform( renderObjPos );
}

// ----------------------------------------------------------------------------
// WbView3d
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(WbView3d, WbView)

// ----------------------------------------------------------------------------
WbView3d::WbView3d() :
	m_assetManager(NULL),
	m_scene(NULL),
	m_overlayScene(NULL),
	m_transparentObjectsScene(NULL),
	m_baseBuildScene(NULL),	 
	m_objectToolTrackingObj(NULL),
	m_lastTrackingZIsFromHighElev(false),
	m_lastTrackingZ(0.0),
	m_showObjToolTrackingObj(false),
	m_camera(NULL),
	m_heightMapRenderObj(NULL),
	m_mouseWheelOffset(0),
	m_actualWinSize(0, 0),
	m_cameraAngle(0.0),
	m_cameraAngleRaw(0.0),
	m_snapCameraAngle45(false),
	m_FXPitch(1.0f),
	m_actualHeightAboveGround(0.0f),
	m_cameraGroundZ(0.0f),
	m_cameraBorderWorld(0.0f),
	m_doPitch(false),
	m_theta(0.0),
	m_time(0),
	m_updateCount(0),
	m_haveLabelCache(false),
	m_labelEpoch(0),
	m_labelAnchorMode(0),
	m_labelRenderer(0),
	m_haveGdiPaintKey(false),
	m_needToLoadRoads(0),
	m_timer(NULL),
	m_drawObject(NULL),
	m_layer(NULL),
	m_buildLayer(NULL),
	m_intersector(NULL),
	m_showEntireMap(false),
	m_partialMapSize(129),
	m_showWireframe(false),
	m_projection(false),
	m_showShadows(false),
	m_firstPaint(true),
	m_groundLevel(10),
	m_curTrackingZ(10),
	m_ww3dInited(false),
	m_showLayersList(false),
	m_showMapBoundaries(false),
	m_showBoundingBoxes(false),
	m_showSightRanges(false),
	m_showWeaponRanges(false),
	m_highlightTestArt(false),
	m_showLetterbox(false),
	m_validTerrain(true),
	m_showBuildZoneFeedback(false),
	m_showSoundCircles(false),
    m_editStartTime(0),
    m_totalEditTime(0),
    m_isTimerRunning(false)
{
	TheTacticalView = &bogusTacticalView;
	// D3DX font handle for viewport labels. The release-guards in
	// createLabelFont()/shutdownWW3D() test it, so it MUST start NULL. Without this,
	// a _DEBUG build leaves it as 0xcdcdcdcd and the first createLabelFont() call
	// dereferences that bogus pointer -> access violation.
	m3DFont = NULL;
	m_actualWinSize.x = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Width", THREE_D_VIEW_WIDTH);
	m_actualWinSize.y = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Height", THREE_D_VIEW_HEIGHT);

	
	m_lod = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "LODMode", 2);
	m_textShadow = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "TextShadow", 1) != 0;
	m_textAntialias = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "TextAntialias", 1) != 0;
	m_labelAnchorMode = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "LabelAnchorMode", 0);
	m_labelRenderer = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "LabelRenderer", 0);
	m_snapCameraAngle45 = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "SnapCameraAngle45", 0) != 0);

	int msaaMode = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "MSAAMode", 0);
	DX8Wrapper::Set_Multi_Sample_Type((D3DMULTISAMPLE_TYPE)msaaMode);

	m_cameraOffset.x = m_cameraOffset.y = m_cameraOffset.z = 1;

	for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{
		m_globalLight[i] = NEW_REF( LightClass, (LightClass::DIRECTIONAL) );
		m_lightFeedbackMesh[i]=NULL;
	}

	m_showWireframe = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowWireframe", 0) != 0);
	m_showEntireMap = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowEntireMap", 1) != 0);
	m_projection = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowTopDownView", 0) != 0);
	m_showShadows = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowShadows", 1) != 0);
	TheWritableGlobalData->m_useShadowDecals = m_showShadows;
	TheWritableGlobalData->m_useShadowVolumes = m_showShadows;
	TheWritableGlobalData->m_showSoftWaterEdge = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", 1) != 0);
	TheWritableGlobalData->m_use3WayTerrainBlends = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowExtraBlends", 1) > 1 ? 2 : 1);
	setShowModels(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowModels", 1) != 0);
	setShowBoundingBoxes(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowBoundingBoxes", 0) != 0);
	setShowSightRanges(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowSightRanges", 0) != 0);
	setShowWeaponRanges(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowWeaponRanges", 0) != 0);
	setShowGarrisoned(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowGarrisoned", 0) != 0);
	setHighlightTestArt(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "HighlightTestArt", 0) != 0);
	setShowLetterbox(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowLetterbox", 0) != 0);
}

// ----------------------------------------------------------------------------
WbView3d::~WbView3d()
{
	for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{
		if (m_lightFeedbackMesh[i] != NULL)
		{	m_lightFeedbackMesh[i]->Remove();
			REF_PTR_RELEASE(m_lightFeedbackMesh[i]);
		}
	}
	REF_PTR_RELEASE(m_drawObject) ;
	REF_PTR_RELEASE(m_heightMapRenderObj);
	W3DShaderManager::shutdown();
	shutdownWW3D();
}
// ----------------------------------------------------------------------------
void WbView3d::shutdownWW3D(void)
{
	if (m_intersector) {
		delete m_intersector;
		m_intersector = NULL;
	}

	if (m_layer) {
		delete m_layer;
		m_layer = NULL;
	}
	if (m_buildLayer) {
		delete m_buildLayer;
		m_buildLayer = NULL;
	}
	if (m3DFont) {
		m3DFont->Release();
		m3DFont = NULL;
	}
	if (m_ww3dInited) {
		m_lightList.Reset_List();

		if (m_assetManager) {
			PredictiveLODOptimizerClass::Free();	/// @todo: where does this need to be done?
			m_assetManager->Free_Assets();
			delete m_assetManager;
			m_assetManager = NULL;
		}

		if (TheW3DShadowManager)
		{	
			TheW3DShadowManager->removeAllShadows();
			delete TheW3DShadowManager;
			TheW3DShadowManager=NULL;
		}
		REF_PTR_RELEASE(m_transparentObjectsScene);
		REF_PTR_RELEASE(m_overlayScene);
		REF_PTR_RELEASE(m_baseBuildScene);	
		REF_PTR_RELEASE(m_objectToolTrackingObj);
		REF_PTR_RELEASE(m_scene);
		REF_PTR_RELEASE(m_camera);
		REF_PTR_RELEASE(m_heightMapRenderObj);
		REF_PTR_RELEASE(m_drawObject);
		for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
			REF_PTR_RELEASE(m_globalLight[i]);
#ifdef SAMPLE_DYNAMIC_LIGHT
		REF_PTR_RELEASE(theDynamicLight);
#endif
		WW3D::Shutdown();

		WWMath::Shutdown();
	}
	m_ww3dInited = false;
}

//=============================================================================
// WbView3d::ReleaseResources
//=============================================================================
/** Releases all w3d assets, to prepare for Reset device call. */
//=============================================================================
void WbView3d::ReleaseResources(void)
{
	if (TheTerrainRenderObject) {
		TheTerrainRenderObject->ReleaseResources();
	}
	if (m3DFont) {
		m3DFont->Release();
	}
	m3DFont = NULL;
	if (m_drawObject) {
		m_drawObject->freeMapResources();
	}
}

//=============================================================================
// WbView3d::ReAcquireResources
//=============================================================================
/** Reallocates all W3D assets after a reset.. */
//=============================================================================
void WbView3d::ReAcquireResources(void)
{
	if (TheTerrainRenderObject) {
		TheTerrainRenderObject->ReAcquireResources();
		TheTerrainRenderObject->loadRoadsAndBridges(NULL,FALSE);
		// TheTerrainRenderObject->worldBuilderUpdateBridgeTowers( m_assetManager, m_scene );
	}
	m_drawObject->initData();
	createLabelFont();

}

// ----------------------------------------------------------------------------
void WbView3d::killTheTimer(void) 
{
	if (m_timer != NULL) {
		KillTimer(m_timer);
		m_timer = NULL;
	}
}

// ----------------------------------------------------------------------------
void WbView3d::reset3dEngineDisplaySize(Int width, Int height) 
{
	if (m_actualWinSize.x == width && 
		m_actualWinSize.y == height) {
		return;
	}
	bogusTacticalView.setWidth(width);
	bogusTacticalView.setHeight(height);
	bogusTacticalView.setOrigin(0,0);
	m_actualWinSize.x = width;
	m_actualWinSize.y = height;
	if (m_ww3dInited) {
		WW3D::Set_Device_Resolution(m_actualWinSize.x, m_actualWinSize.y, true);
	}

	// Update camera FOV instead of stretching -- Preserves ratio
	if (m_camera) {
		float newAspectRatio = (float)width / (float)height;
		m_camera->Set_Aspect_Ratio_HackedForWB(newAspectRatio);
	}
}

// ----------------------------------------------------------------------------
void WbView3d::initAssets()
{

	m_assetManager = new W3DAssetManager;	
	m_assetManager->Register_Prototype_Loader(&_ParticleEmitterLoader );
	m_assetManager->Register_Prototype_Loader(&_AggregateLoader);
	m_assetManager->Set_WW3D_Load_On_Demand(true);
}
	
// ----------------------------------------------------------------------------
#define TERRAIN_SAMPLE_SIZE 40.0f
static Real getHeightAroundPos(WBHeightMap *heightMap, Real x, Real y)
{
	Real terrainHeight = heightMap->getHeightMapHeight(x, y, NULL);

	// find best approximation of max terrain height we can see
	Real terrainHeightMax = terrainHeight;
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x+TERRAIN_SAMPLE_SIZE, y-TERRAIN_SAMPLE_SIZE, NULL));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x-TERRAIN_SAMPLE_SIZE, y-TERRAIN_SAMPLE_SIZE, NULL));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x+TERRAIN_SAMPLE_SIZE, y+TERRAIN_SAMPLE_SIZE, NULL));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x-TERRAIN_SAMPLE_SIZE, y+TERRAIN_SAMPLE_SIZE, NULL));

	return terrainHeightMax;
}

// ----------------------------------------------------------------------------
void WbView3d::setupCamera()
{
	Matrix3D camtransform(1);
	float zOffset = - m_mouseWheelOffset / 1200;
	Real zoom = 1.0f;
	if (zOffset != 0) {
		Real zPos = (m_cameraOffset.length()-m_groundLevel)/m_cameraOffset.length();
		Real zAbs = zOffset + zPos;
		if (zAbs<0) zAbs = -zAbs;
		if (zAbs<0.01) zAbs = 0.01f;
		//DEBUG_LOG(("zOffset = %.2f, zAbs = %.2f, zPos = %.2f\n", zOffset, zAbs, zPos));	
		if (zOffset > 0) {
			zOffset *= zAbs;
		}	else if (zOffset < -0.3f) {
			zOffset = -0.15f + zOffset/2.0f;
		}
		if (zOffset < -0.6f) {
			zOffset = -0.3f + zOffset/2.0f;
		}
		//DEBUG_LOG(("zOffset = %.2f\n", zOffset));
		zoom = zAbs;
	}

/////////////////////////////////////////////////////////////////
	Vector3 sourcePos, targetPos;

	Real angle = m_cameraAngle;
	Real pitch = 0;
	Coord3D pos;
	pos.x = m_centerPt.X* MAP_XY_FACTOR;
	pos.y = m_centerPt.Y* MAP_XY_FACTOR;
	pos.z = m_centerPt.Z* MAP_XY_FACTOR;

	Real groundLevel = m_heightMapRenderObj?getHeightAroundPos(m_heightMapRenderObj, pos.x, pos.y) : 0;
	// Cache the terrain height under the camera so the minimap view box can intersect
	// the frustum against the real ground (getViewFrustumGroundCorners). m_centerPt.Z is
	// always 0, so the old "groundZ = m_centerPt.Z * MAP_XY_FACTOR" was a flat z=0 plane,
	// which drifted the box on non-flat / non-square maps.
	m_cameraGroundZ = groundLevel;

	// Cache the border offset in world units. The camera/eye is in ABSOLUTE cell-index
	// world (m_centerPt includes the border), but minimap dots, terrain, and
	// MapObject::getLocation() are all BORDER-RELATIVE. getViewFrustumGroundCorners
	// subtracts this so its corners share the object world space (so the drawn box AND
	// the isInViewFrustum cull line up with the blips instead of being shifted by the
	// border).
	{
		WorldHeightMapEdit *pMapForBorder = WbDoc() ? WbDoc()->GetHeightMap() : NULL;
		m_cameraBorderWorld = pMapForBorder ? (pMapForBorder->getBorderSize() * MAP_XY_FACTOR) : 0.0f;
	}

	// set position of camera itself
	/*
	sourcePos.X = m_cameraOffset.x;
	sourcePos.Y = m_cameraOffset.y;
	sourcePos.Z = m_cameraOffset.z;
	*/
	sourcePos.X = m_cameraOffset.x * zoom;
	sourcePos.Y = m_cameraOffset.y * zoom;
	sourcePos.Z = m_cameraOffset.z * zoom;

	// camera looking at origin
	targetPos.X = 0;
	targetPos.Y = 0;
	targetPos.Z = 0;


	Real factor = 1.0 - (groundLevel/sourcePos.Z );

	// construct a matrix to rotate around the up vector by the given angle
	Matrix3D angleTransform( Vector3( 0.0f, 0.0f, 1.0f ), angle );

	// construct a matrix to rotate around the horizontal vector by the given angle
	Matrix3D pitchTransform( Vector3( 1.0f, 0.0f, 0.0f ), pitch );

	// rotate camera position (pitch, then angle)
#ifdef ALLOW_TEMPORARIES
	sourcePos = pitchTransform * sourcePos;
	sourcePos = angleTransform * sourcePos;
#else
	pitchTransform.mulVector3(sourcePos);
	angleTransform.mulVector3(sourcePos);
#endif
	//sourcePos *= factor+zOffset;
	sourcePos *= factor;

	// translate to current XY position
	sourcePos.X += pos.x;
	sourcePos.Y += pos.y;
	sourcePos.Z += pos.z+groundLevel;
	
	targetPos.X += pos.x;
	targetPos.Y += pos.y;
	targetPos.Z += pos.z+groundLevel;

	// do fxPitch adjustment
	Real height = sourcePos.Z - targetPos.Z;
	height *= m_FXPitch;
	targetPos.Z = sourcePos.Z - height;

	// Just for kicks, lets see how high we are above the ground
	m_actualHeightAboveGround = m_cameraOffset.z * zoom - groundLevel;
	m_cameraSource = sourcePos;
	m_cameraTarget = targetPos;
	/*
	DEBUG_LOG(("Camera: pos=(%g,%g) height=%g pitch=%g FXPitch=%g yaw=%g groundLevel=%g\n",
		targetPos.X, targetPos.Y,
		m_actualHeightAboveGround,
		pitch,
		m_FXPitch,
		angle, m_groundLevel));
		*/

	// build new camera transform
	camtransform.Make_Identity();
	if (factor < 0) { //WST 11/11/02. Fix camera flipping over when near the ground too early 
		targetPos = sourcePos + (sourcePos-targetPos);
	}
	camtransform.Look_At( sourcePos, targetPos, 0 );
	/////////////////////////////////////////////////////////////
	targetPos.Z = 0;
	Real lookDistance = (targetPos-sourcePos).Length();
	Real nearZ, farZ;
	if (lookDistance < 300) lookDistance = 300;
	m_camera->Get_Clip_Planes(nearZ, farZ);
	m_camera->Set_Clip_Planes(lookDistance/200, lookDistance*3);

	if (m_heightMapRenderObj) {
		if (m_projection) {
			camtransform.Make_Identity();
			camtransform.Set_Translation(Vector3(targetPos.X, targetPos.Y, lookDistance));
			// m_heightMapRenderObj->setFlattenHeights(true);
			//m_camera->Set_Projection_Type(CameraClass::ORTHO);
		} else {
			// m_heightMapRenderObj->setFlattenHeights(false);
			//m_camera->Set_Projection_Type(CameraClass::PERSPECTIVE);
		}
	}
	m_camera->Set_Transform( camtransform );
	if (m_heightMapRenderObj) {
		m_heightMapRenderObj->setDrawEntireMap(m_showEntireMap);
	}

	m_camera->Set_Aspect_Ratio_HackedForWB((float)m_actualWinSize.x/(float)m_actualWinSize.y);
}

// ----------------------------------------------------------------------------
void WbView3d::init3dScene()
{
	// build scene	
	REF_PTR_RELEASE(m_overlayScene);
	REF_PTR_RELEASE(m_transparentObjectsScene);
	REF_PTR_RELEASE(m_baseBuildScene); 
	REF_PTR_RELEASE(m_scene);
	REF_PTR_RELEASE(m_camera);
	REF_PTR_RELEASE(m_heightMapRenderObj);

	m_scene = NEW_REF(SkeletonSceneClass,());
#ifdef SAMPLE_DYNAMIC_LIGHT
	REF_PTR_RELEASE(theDynamicLight);
	theDynamicLight = NEW_REF(W3DDynamicLight, ());
	Real red = 1;
	Real green = 1;
	Real blue = 0;
	if(red==0 && blue==0 && green==0) {
		red = green = blue = 1;
	}
	theDynamicLight->Set_Ambient( Vector3( red, green, blue ) );
	theDynamicLight->Set_Diffuse( Vector3( red, green, blue) );
	theDynamicLight->Set_Position(Vector3(211, 363, 10));
	theDynamicLight->Set_Far_Attenuation_Range(5, 15);
	// Note: Don't Add_Render_Object dynamic lights. 
	m_scene->addDynamicLight( theDynamicLight );
#endif
	m_overlayScene = NEW_REF(SkeletonSceneClass,());
	m_baseBuildScene = NEW_REF(SkeletonSceneClass,());
	m_transparentObjectsScene = NEW_REF(SkeletonSceneClass,());
//	m_scene->Set_Polygon_Mode(SceneClass::LINE);	
	m_scene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));
	m_overlayScene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));
	m_baseBuildScene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));

	// Scene needs camera to be rendered with ----------------------------------
	m_camera = NEW_REF(CameraClass,());

}

// ----------------------------------------------------------------------------
void WbView3d::resetRenderObjects()
{
	if (!m_scene) return;
	if (TheW3DShadowManager) {	
		TheW3DShadowManager->removeAllShadows();
	}
	
	SceneIterator *sceneIter = m_scene->Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		RenderObjClass * robj = sceneIter->Current_Item();
		robj->Add_Ref();
		m_scene->Remove_Render_Object(robj);
		robj->Release_Ref();
		sceneIter->Next();
	}
	m_scene->Destroy_Iterator(sceneIter);
	sceneIter = m_baseBuildScene->Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		RenderObjClass * robj = sceneIter->Current_Item();
		robj->Add_Ref();
		m_baseBuildScene->Remove_Render_Object(robj);
		robj->Release_Ref();
		sceneIter->Next();
	}
	m_baseBuildScene->Destroy_Iterator(sceneIter);
	MapObject *pMapObj = MapObject::getFirstMapObject();
	// Erase references to render objs that have been removed.
	while (pMapObj) 
	{
		pMapObj->setRenderObj(NULL);
		pMapObj->setShadowObj(NULL);
		pMapObj = pMapObj->getNext();
	}

	Int i;
	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i); 
		BuildListInfo *pBuild = pSide->getBuildList();
		while (pBuild) {
			pBuild->setRenderObj(NULL);
			pBuild = pBuild->getNext();
		}
	}

	m_needToLoadRoads = true; // load roads next time we redraw.

	if (TheW3DShadowManager)
		TheW3DShadowManager->Reset();

	updateLights();
	if (m_heightMapRenderObj) {
		m_scene->Add_Render_Object(m_heightMapRenderObj);
		m_heightMapRenderObj->removeAllTrees();
		m_heightMapRenderObj->removeAllProps();
	}
}

// ----------------------------------------------------------------------------
void WbView3d::stepTimeOfDay()
{
	TheWritableGlobalData->m_timeOfDay = (TimeOfDay)(TheGlobalData->m_timeOfDay+1);
	if (TheGlobalData->m_timeOfDay >= TIME_OF_DAY_COUNT) {
		TheWritableGlobalData->m_timeOfDay = TIME_OF_DAY_FIRST;
	}
	resetRenderObjects();
	invalObjectInView(NULL);

	// Time-of-day changes the terrain tint, so the minimap needs a full resample.
	// This is an explicit user action (like load/toggle), so rebuild immediately
	// rather than via the throttle (which honors "Refresh Rate: Off").
	if (TheMinimapDialog && TheMinimapDialog->IsWindowVisible())
		TheMinimapDialog->rebuildTerrain();
}

// ----------------------------------------------------------------------------
void WbView3d::setLighting(const GlobalData::TerrainLighting *tl, Int whichLighting, Int whichLight)
{
	if (whichLighting == GlobalLightOptions::K_TERRAIN) {
		TheWritableGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][whichLight]= *tl;
	} else if (whichLighting == GlobalLightOptions::K_OBJECTS) { 
		TheWritableGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
	} else if (whichLighting == GlobalLightOptions::K_BOTH) { 
		TheWritableGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
		TheWritableGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
	} 
	const GlobalData::TerrainLighting *ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight];
	TheWritableGlobalData->setTimeOfDay(TheGlobalData->m_timeOfDay);
	if( m_globalLight ) {
		m_globalLight[whichLight]->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
		m_globalLight[whichLight]->Set_Diffuse( Vector3(ol->diffuse.red, ol->diffuse.green, ol->diffuse.blue ) );
		m_globalLight[whichLight]->Set_Specular( Vector3(0,0,0) );
		Matrix3D mtx;
		mtx.Set(Vector3(1,0,0), Vector3(0,1,0), Vector3(ol->lightPos.x, ol->lightPos.y, ol->lightPos.z), Vector3(0,0,0));
		m_globalLight[whichLight]->Set_Transform(mtx);
		if( m_scene && whichLight == 0) {	//only let the first light contribute to ambient
			m_scene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
			m_baseBuildScene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
		}	
	}
	if(TheTerrainRenderObject) {
		TheTerrainRenderObject->setTimeOfDay(TheGlobalData->m_timeOfDay);
	}
	if (TheW3DShadowManager) {	
		TheW3DShadowManager->setTimeOfDay(TheGlobalData->m_timeOfDay);
	}
	m_needToLoadRoads = true; // load roads next time we redraw.
	Invalidate(false);
}

// ----------------------------------------------------------------------------
void WbView3d::updateLights() 
{
	++m_updateCount;

	// Update lights list.
	m_lightList.Reset_List();

	{
		TheWritableGlobalData->setTimeOfDay(TheGlobalData->m_timeOfDay);
		const GlobalData::TerrainLighting *ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][0];

		if( m_scene )
		{
			m_scene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
			m_baseBuildScene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
		}

		if (TheW3DShadowManager) {	
			TheW3DShadowManager->setTimeOfDay(TheGlobalData->m_timeOfDay);
		}

		for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
		{

			if( m_globalLight[i] )
			{
				ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][i];
				m_globalLight[i]->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
				m_globalLight[i]->Set_Diffuse( Vector3(ol->diffuse.red, ol->diffuse.green, ol->diffuse.blue ) );
				m_globalLight[i]->Set_Specular( Vector3(0,0,0) );
				Matrix3D mtx;
				mtx.Set(Vector3(1,0,0), Vector3(0,1,0), Vector3(ol->lightPos.x, ol->lightPos.y, ol->lightPos.z), Vector3(0,0,0));
				m_globalLight[i]->Set_Transform(mtx);
 				m_scene->setGlobalLight(m_globalLight[i],i);
 				m_baseBuildScene->setGlobalLight(m_globalLight[i],i);
			}
		}
		if(TheTerrainRenderObject) {
			TheTerrainRenderObject->setTimeOfDay(TheGlobalData->m_timeOfDay);
		}


	}

	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj && m_heightMapRenderObj) {
		if (pMapObj->isLight()) { 
			Coord3D loc = *pMapObj->getLocation();
			loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);
			RenderObjClass *renderObj= pMapObj->getRenderObj();
			if (renderObj) {
				m_scene->Remove_Render_Object(renderObj);
				pMapObj->setRenderObj(NULL);
			}
			// It is a light, and handled at the device level.  jba.
			LightClass* lightP = NEW_REF(LightClass, (LightClass::POINT));

			Dict *props = pMapObj->getProperties();

			Real lightHeightAboveTerrain, lightInnerRadius, lightOuterRadius;
			RGBColor lightAmbientColor, lightDiffuseColor;
			lightHeightAboveTerrain = props->getReal(TheKey_lightHeightAboveTerrain);
			lightInnerRadius = props->getReal(TheKey_lightInnerRadius);
			lightOuterRadius = props->getReal(TheKey_lightOuterRadius);
			lightAmbientColor.setFromInt(props->getInt(TheKey_lightAmbientColor));
			lightDiffuseColor.setFromInt(props->getInt(TheKey_lightDiffuseColor));

			lightP->Set_Ambient( Vector3( lightAmbientColor.red, lightAmbientColor.green, lightAmbientColor.blue ) );
			lightP->Set_Diffuse( Vector3(  lightDiffuseColor.red, lightDiffuseColor.green, lightDiffuseColor.blue) );

			lightP->Set_Position(Vector3(loc.x, loc.y, loc.z+lightHeightAboveTerrain));

			lightP->Set_Far_Attenuation_Range(lightInnerRadius, lightOuterRadius);

			m_lightList.Add(lightP);
 			m_scene->Add_Render_Object(lightP);
			pMapObj->setRenderObj(lightP);
			REF_PTR_RELEASE( lightP );
		}
		pMapObj = pMapObj->getNext();
	}

	--m_updateCount;
}

// ----------------------------------------------------------------------------
void WbView3d::updateScorches(void)
{
	TheTerrainRenderObject->clearAllScorches();
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->isScorch()) 
		{
			const Coord3D *pos = pMapObj->getLocation();
			Real radius = pMapObj->getProperties()->getReal(TheKey_objectRadius);
			Scorches type = (Scorches) pMapObj->getProperties()->getInt(TheKey_scorchType);

			Vector3 loc(pos->x, pos->y, pos->z);
			TheTerrainRenderObject->addScorch(loc, radius, type);
		}
	}
}

// ----------------------------------------------------------------------------
void WbView3d::updateTrees(void)
{
	/** 
	 * Adriane [Deathscythe] -- Bug fix 
	 * Do not render them trees when we dont need too
	 */
	TheTerrainRenderObject->removeAllTrees();
	if (!m_showModels) {
        return;
    }

	TheTerrainRenderObject->removeAllProps();
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{

		// Skip objects that shouldn't render (hidden by layer or flag)
		if (pMapObj->getFlags() & FLAG_DONT_RENDER)
			continue;

		const ThingTemplate *tTemplate;
		
		tTemplate = pMapObj->getThingTemplate();
		if (tTemplate && tTemplate->isKindOf(KINDOF_OPTIMIZED_TREE) ) 
		{
			Real scale = tTemplate->getAssetScale();
			const ModuleInfo& mi = tTemplate->getDrawModuleInfo();
			if (mi.getCount() > 0)
			{
				const ModuleData* mdd = mi.getNthData(0);
				AsciiString name = KEYNAME(mdd->getModuleTagNameKey());
				const W3DTreeDrawModuleData* md = mdd ? mdd->getAsW3DTreeDrawModuleData(): NULL;
				if (md)
				{
					Coord3D pos = *pMapObj->getLocation();
					if (m_heightMapRenderObj) {
						pos.z += m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, NULL);
						TheTerrainRenderObject->addTree((DrawableID)(Int)pMapObj, pos, scale, pMapObj->getAngle(), 
							0.0f /*no random scaling*/, md);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------
void WbView3d::invalidateCellInView(int xIndex, int yIndex) 
{
	Invalidate(false);	/// @todo be smarter about invaling the area
}

// ----------------------------------------------------------------------------
void WbView3d::updateFenceListObjects(MapObject *pObject)
{
	MapObject *pMapObj;
	for (pMapObj = pObject; pMapObj; pMapObj = pMapObj->getNext())
	{

		Coord3D loc = *pMapObj->getLocation();
		loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);

		RenderObjClass *renderObj=NULL;
		REF_PTR_SET( renderObj, pMapObj->getRenderObj() );
		if (!renderObj) {
			Real scale = 1.0; 
			AsciiString modelName = getModelNameAndScale(pMapObj, &scale, BODY_PRISTINE);
			// set render object, or create if we need to
			if( renderObj == NULL && modelName.isEmpty() == FALSE && 
					strncmp( modelName.str(), "No ", 3 ) ) 
			{

				renderObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, 0);

			}  // end if
		}

		if (renderObj) {
			pMapObj->setRenderObj(renderObj);

			// set item's position to loc, and get scale from item and apply it.

			Matrix3D renderObjPos(true);	// init to identity
			renderObjPos.Translate(loc.x, loc.y, loc.z);
			renderObjPos.Rotate_Z(pMapObj->getAngle());
			renderObj->Set_Transform( renderObjPos );


			m_scene->Add_Render_Object(renderObj);

			REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
		}
	}

	Invalidate(false);
}


// ----------------------------------------------------------------------------
void WbView3d::removeFenceListObjects(MapObject *pObject)
{
	MapObject *pMapObj;
	for (pMapObj = pObject; pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->getRenderObj()) {
			m_scene->Remove_Render_Object(pMapObj->getRenderObj());
			pMapObj->setRenderObj(NULL);
		}
	}

	Invalidate(false);
}

/**
 * Adriane [Deathscythe]
 * Very nasty hack incoming, good lord — some necessary functions are already exposed for tree drawing,
 * and I can fix the preview bug using those.
 * 
 * Do note though: this is a temporary fix —
 * WorldBuilder originally only checked for W3DModelDraw. However, due to
 * optimizations made by the Zero Hour developers, many tree objects were
 * converted to use W3DTreeDraw instead. As a result, previews for these
 * objects were broken. This fallback restores support for tree previews.
 * 
 * Extra: the original getBestModelName() is also synonymous with the shadow system.
 * I had to revert edits to the original and create a new function,
 * since there's a weird bug I haven't been able to fix where models are drawn twice
 * when trees are rendered.
 */
AsciiString WbView3d::getBestModelNameWBPrev(const ThingTemplate* tt, const ModelConditionFlags& c)
{
	if (tt)
	{
		const ModuleInfo& mi = tt->getDrawModuleInfo();
		if (mi.getCount() > 0)
		{
			const ModuleData* mdd = mi.getNthData(0);

			// Try W3DModelDraw first
			const W3DModelDrawModuleData* md = mdd ? mdd->getAsW3DModelDrawModuleData() : NULL;
			if (md)
			{
				return md->getBestModelNameForWB(c);
			}

			/*
			* Adriane [Deathscythe] 
			* Fallback: Try W3DTreeDraw -- now supports preview model in WorldBuilder.
			*/
			const W3DTreeDrawModuleData* td = mdd ? mdd->getAsW3DTreeDrawModuleData() : NULL;
			if (td)
			{
				return td->m_modelName;
			}
		}
	}
	return AsciiString::TheEmptyString;
}

// ----------------------------------------------------------------------------
/// @todo srj -- this is a terrible hack, since things can have multiple models, and it's private info. fix later.
AsciiString WbView3d::getBestModelName(const ThingTemplate* tt, const ModelConditionFlags& c)
{
	if (tt)
	{
		const ModuleInfo& mi = tt->getDrawModuleInfo();
		if (mi.getCount() > 0)
		{
//		const W3DModelDrawModuleData* md = dynamic_cast<const W3DModelDrawModuleData*>(mi->getNthData(0));
			const ModuleData* mdd = mi.getNthData(0);
			const W3DModelDrawModuleData* md = mdd ? mdd->getAsW3DModelDrawModuleData() : NULL;
			if (md)
			{
				return md->getBestModelNameForWB(c);
			}
		}
	}
	// removing this crash as sounds can (and should) have no model - jkmcd
	return AsciiString::TheEmptyString;
}

// ----------------------------------------------------------------------------
void WbView3d::invalBuildListItemInView(BuildListInfo *pBuildToInval)
{
	Int i;
	Bool found = false;
	
	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i); 

		// find which player color we should use
		Int playerColor = 0xFFFFFF;
		const Dict *sideDict = pSide->getDict();
		if (sideDict)
		{
			Bool exists = false;
			Int color = sideDict->getInt(TheKey_playerColor, &exists);
			if (exists)
			{
				playerColor = color;
			}
		}


		for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
			if (pBuildToInval == pBuild) {
				found = true;
			}
			if (!found && pBuildToInval) { 
				continue;
			}
			if (!BuildListTool::isActive() && !pBuild->isInitiallyBuilt() && !getShowBuildListObjects()) {
				continue;
			}
			// Update.
			Coord3D loc = *pBuild->getLocation();
			loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);
			RenderObjClass *renderObj=NULL;
			Shadow			*shadowObj=NULL;
			// Build list render obj is not refcounted, so check & make sure it exists in the scene.
			if (pBuild->getRenderObj()) {
				if (!m_baseBuildScene->safeContains(pBuild->getRenderObj())) {
					pBuild->setRenderObj(NULL);
				}
			}
				
			REF_PTR_SET(renderObj, pBuild->getRenderObj());
			if (!renderObj) {
				Real scale = 1.0; 
				AsciiString thingName = pBuild->getTemplateName();
				const ThingTemplate *tTemplate = TheThingFactory->findTemplate(thingName);

				AsciiString modelName = "No Model Name";
				if (tTemplate) {
					ModelConditionFlags state;
					state.clear();
					modelName = getBestModelName(tTemplate, state);
					scale = tTemplate->getAssetScale();
				} 
				// set render object, or create if we need to
				if( renderObj == NULL && modelName.isEmpty() == FALSE && 
						strncmp( modelName.str(), "No ", 3 ) ) 
				{

					renderObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, playerColor);
					if( m_showShadows  && tTemplate->getShadowType() != SHADOW_NONE)
					{
						//add correct type of shadow
						Shadow::ShadowTypeInfo shadowInfo;
						shadowInfo.allowUpdates=FALSE;	//shadow image will never update
						shadowInfo.allowWorldAlign=TRUE;	//shadow image will wrap around world objects
						strcpy(shadowInfo.m_ShadowName,tTemplate->getShadowTextureName().str());
						DEBUG_ASSERTCRASH(shadowInfo.m_ShadowName[0] != '\0', ("this should be validated in ThingTemplate now"));
						shadowInfo.m_type=(ShadowType)tTemplate->getShadowType();
						shadowInfo.m_sizeX=tTemplate->getShadowSizeX();
						shadowInfo.m_sizeY=tTemplate->getShadowSizeY();
						shadowInfo.m_offsetX=tTemplate->getShadowOffsetX();
						shadowInfo.m_offsetY=tTemplate->getShadowOffsetY();
						shadowObj=TheW3DShadowManager->addShadow(renderObj, &shadowInfo);
					}
				}  // end if
			}
			if (renderObj) {
				pBuild->setRenderObj(renderObj);
				pBuild->setShadowObj(shadowObj);
				// set item's position to loc.
				Matrix3D renderObjPos(true);	// init to identity
				renderObjPos.Translate(loc.x, loc.y, loc.z);
				renderObjPos.Rotate_Z(pBuild->getAngle());
				renderObj->Set_Transform( renderObjPos );

				m_baseBuildScene->Add_Render_Object(renderObj);

				REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
			}
			if (found) break;
		}
	}

	// Build list render obj is not refcounted, so check & make sure it exists in the scene.
	if (!found && pBuildToInval && pBuildToInval->getRenderObj()) {
		if (!m_baseBuildScene->safeContains(pBuildToInval->getRenderObj())) {
			pBuildToInval->setRenderObj(NULL);
		}
	}
	if (!found && pBuildToInval && pBuildToInval->getRenderObj()) {
		m_baseBuildScene->Remove_Render_Object(pBuildToInval->getRenderObj());
		pBuildToInval->setRenderObj(NULL);
	}
	Invalidate(false);
}


AsciiString WbView3d::getModelNameAndScale(MapObject *pMapObj, Real *scale, BodyDamageType curDamageState)
{
	ModelConditionFlags state;
	switch (curDamageState) 
	{
			case BODY_PRISTINE:
			default:
				state.clear();
				break;

			case BODY_DAMAGED:
				state.set(MODELCONDITION_DAMAGED);
				break;

			case BODY_REALLYDAMAGED:
				state.set(MODELCONDITION_REALLY_DAMAGED);
				break;

			case BODY_RUBBLE:
				state.set(MODELCONDITION_RUBBLE);
				break;
	}

	if (getShowGarrisoned())
	{
		state.set(MODELCONDITION_GARRISONED);
	}
	Int objWeather = 0;
	Bool exists;
	if (pMapObj && pMapObj->getProperties()) 
	{
		objWeather = pMapObj->getProperties()->getInt(TheKey_objectWeather, &exists);
	}
	switch (objWeather)
	{
		default:
		case 0:
			if (TheGlobalData->m_weather == WEATHER_SNOWY)
			{
				state.set(MODELCONDITION_SNOW);
			}
			break;
		case 2:
			state.set(MODELCONDITION_SNOW);
			break;
	}

	Int objTime = 0;
	if (pMapObj && pMapObj->getProperties()) 
	{
		objTime = pMapObj->getProperties()->getInt(TheKey_objectTime, &exists);
	}
	switch (objTime)
	{
		default:
		case 0:
			if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
			{
				state.set(MODELCONDITION_NIGHT);
			}
			break;
		case 2:
			state.set(MODELCONDITION_NIGHT);
			break;
	}

	AsciiString modelName("No Model Name");
	*scale = 1.0f;
	Int i;
	char buffer[ _MAX_PATH ];
	if (strncmp(TEST_STRING, pMapObj->getName().str(), strlen(TEST_STRING)) == 0) 
	{
		/* Handle test art models here */
		strcpy(buffer, pMapObj->getName().str());

		for (i=0; buffer[i]; i++) {
			if (buffer[i] == '/') {
				i++;
				break;
			}
		}
		modelName = buffer+i;
	}	
	else 
	{
		modelName = "No Model Name"; // must be this while GDF exists (it's the default)
		const ThingTemplate *tTemplate;
		
		tTemplate = pMapObj->getThingTemplate();
		if( tTemplate && !(pMapObj->getFlags() & FLAG_DONT_RENDER))
		{

			// get visual data from the thing template
			modelName = getBestModelNameWBPrev(tTemplate, state);
			*scale = tTemplate->getAssetScale();

		}  // end if
	}  // end else
	return modelName;
}

static AsciiString CleanSubObjName(const AsciiString& in)
{
    const char* raw = in.str();
    if (!raw) return AsciiString::TheEmptyString;

    // Trim leading whitespace
    while (*raw && isspace((unsigned char)*raw)) raw++;

    // Trim trailing whitespace
    const char* end = raw + strlen(raw);
    while (end > raw && isspace((unsigned char)*(end - 1))) end--;

    // Copy into a buffer we can edit
    int len = (int)(end - raw);
    if (len <= 0) return AsciiString::TheEmptyString;

    char buf[256]; // plenty big for subobject names
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    strncpy(buf, raw, len);
    buf[len] = '\0';

    // Normalize to uppercase
    for (int i = 0; i < len; i++) {
        buf[i] = toupper((unsigned char)buf[i]);
    }

    return AsciiString(buf);
}


static void DumpSubObjects(RenderObjClass* obj, const char* modelName)
{
    if (!obj) return;

    int count = obj->Get_Num_Sub_Objects();
    DEBUG_LOG(("--- SubObjects for %s (count=%d) ---\n", modelName, count));

    for (int i = 0; i < count; i++) {
        RenderObjClass* sub = obj->Get_Sub_Object(i);
        const char* subName = sub ? sub->Get_Name() : NULL; // <-- use Get_Name()
        if (subName && *subName) {
            DEBUG_LOG(("   [%d] '%s'\n", i, subName));
        } else {
            DEBUG_LOG(("   [%d] (no name)\n", i));
        }
    }
}


// ----------------------------------------------------------------------------
void WbView3d::invalObjectInView(MapObject *pMapObjIn)
{
	++m_updateCount;
	++m_labelEpoch;		// objects/properties may have changed -> rebuild label cache
	Bool updateAllTrees = false;
	if (m_heightMapRenderObj == NULL) {
		m_heightMapRenderObj = NEW_REF(WBHeightMap,());

		m_scene->Add_Render_Object(m_heightMapRenderObj);
	}
	if (pMapObjIn == NULL) {
		invalBuildListItemInView(NULL);
	}
	Bool found = false;
	Bool isRoad = false;
	Bool isLight = false;
	Bool isScorch = false;
	if (pMapObjIn == NULL)
		isScorch = true;
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (found) break;
		if (pMapObjIn == pMapObj)
			found = true;
		if (pMapObjIn != NULL && !found) {
			continue;
		}
		if (pMapObj->getFlags() & (FLAG_ROAD_FLAGS|FLAG_BRIDGE_FLAGS)) {
			isRoad = true;
			continue; // Roads don't create drawable objects.
		}
		if (pMapObj->isLight() ) {
			isLight = true;
			continue; // Lights don't create drawable objects.
		}
		if (pMapObj->isScorch()) {
			if (pMapObj == pMapObjIn) {
				isScorch = true;
			}
			continue;
		}


		Coord3D loc = *pMapObj->getLocation();
		loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);

		const ThingTemplate *tTemplate = pMapObj->getThingTemplate();
		if (tTemplate && tTemplate->isKindOf(KINDOF_OPTIMIZED_TREE)) {
			if (!m_heightMapRenderObj->updateTreePosition((DrawableID)(Int)pMapObj, loc, pMapObj->getAngle())) {
				// Couldn't find it, so update them all. [5/27/2003]
				updateAllTrees = true;
			}
			if (found) break; 
		}

		RenderObjClass *renderObj=NULL;
		Shadow		   *shadowObj=NULL;

		REF_PTR_SET( renderObj, pMapObj->getRenderObj() );
		Int playerColor = 0xFFFFFF;
		BodyDamageType curDamageState = BODY_PRISTINE;
		Bool isVehicle = false;

		Bool exists;
		if (tTemplate && !(pMapObj->getFlags() & FLAG_DONT_RENDER))
		{
			isVehicle = tTemplate->isKindOf(KINDOF_VEHICLE);
			AsciiString objectTeamName = pMapObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
			if (exists) {
				TeamsInfo *teamInfo = TheSidesList->findTeamInfo(objectTeamName);
				if (teamInfo) {
					AsciiString teamOwner = teamInfo->getDict()->getAsciiString(TheKey_teamOwner);
					SidesInfo* pSide = TheSidesList->findSideInfo(teamOwner);
					if (pSide) {
						Bool hasColor = false;
						Int color = pSide->getDict()->getInt(TheKey_playerColor, &hasColor);
						if (hasColor) {
							playerColor = color;
						} else {
							AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
							const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
							if (pt) {
								playerColor = pt->getPreferredColor()->getAsInt();
							}
						}
					}
				}
			}
		}
		Int health = 100;
		health = pMapObj->getProperties()->getInt(TheKey_objectInitialHealth, &exists);
		Real ratio = health/100.0;
		if (ratio > TheGlobalData->m_unitDamagedThresh)
		{
			curDamageState = BODY_PRISTINE;
		}
		else if (ratio > TheGlobalData->m_unitReallyDamagedThresh)
		{
			curDamageState = BODY_DAMAGED;
		}
		else if (ratio > 0.0f)
		{
			curDamageState = BODY_REALLYDAMAGED;
		}
		else
		{
			curDamageState = BODY_RUBBLE;
		}


		if (!renderObj) {
			Real scale = 1.0;
		
			const ThingTemplate* tTemplate = pMapObj->getThingTemplate();
			if (tTemplate) {
				Bool hasPostCollapseState = false;
				const ModuleInfo& modelinfo = tTemplate->getBehaviorModuleInfo();
				for (Int modIdx = 0; modIdx < modelinfo.getCount(); ++modIdx) {
					if (modelinfo.getNthName(modIdx).compare("StructureCollapseUpdate") == 0 || 
						modelinfo.getNthName(modIdx).compare("StructureToppleUpdate")   == 0
					) {
						// DEBUG_LOG(("HAS POST_COLLAPSE\n"));
						hasPostCollapseState = true;
						break;
					}
				}

			// 	const ModuleData* moduleData = NULL;
			// 	const ModuleInfo& modInfo = tTemplate->getBehaviorModuleInfo();

			// 	for (int i = 0; i < modInfo.getCount(); ++i) {
			// 		if (modInfo.getNthName(i).compare("SupplyWarehouseDockUpdate") == 0) {
			// 			moduleData = modInfo.getNthData(i);
			// 			break;
			// 		}
			// 	}

			// if (moduleData) {
			// 	const SupplyWarehouseDockUpdateModuleData* dockData =
			// 		static_cast<const SupplyWarehouseDockUpdateModuleData*>(moduleData);

			// 	int startingBoxes = dockData->m_startingBoxesData;

			// 	// Compute cash value
			// 	int cashValue = static_cast<int>(startingBoxes * TheGlobalData->m_baseValuePerSupplyBox);

			// 	DEBUG_LOG(("starting boxes: %d, cash value: %d\n", startingBoxes, cashValue));
			// }

				scale = tTemplate->getAssetScale();
				// Adriane [Deathscythe] The worldbuilder's scale change is very off for infantry -- adjust properly
				if (scale > 2.0 && pMapObj->getThingTemplate()->isKindOf(KINDOF_INFANTRY)) {
					scale *= 4.0f;  // scale up to 350% 
				}
		
				// Setup model condition flags from the current damage state
				ModelConditionFlags state;
				switch (curDamageState) {
					case BODY_PRISTINE:
					default:
						state.clear();
						break;
					case BODY_DAMAGED:
						state.set(MODELCONDITION_DAMAGED);
						break;
					case BODY_REALLYDAMAGED:
						state.set(MODELCONDITION_REALLY_DAMAGED);
						break;
					case BODY_RUBBLE:
						if(hasPostCollapseState){
							state.set(MODELCONDITION_POST_COLLAPSE);
						} else {
							state.set(MODELCONDITION_RUBBLE);
						}
						break;
				}
		
				if (getShowGarrisoned()) {
					state.set(MODELCONDITION_GARRISONED);
				}
		
				// Check weather conditions
				Int objWeather = 0;
				Bool exists = FALSE;
				if (pMapObj && pMapObj->getProperties()) {
					objWeather = pMapObj->getProperties()->getInt(TheKey_objectWeather, &exists);
				}
				switch (objWeather) {
					default:
					case 0:
						if (TheGlobalData->m_weather == WEATHER_SNOWY) {
							state.set(MODELCONDITION_SNOW);
						}
						break;
					case 2:
						state.set(MODELCONDITION_SNOW);
						break;
				}
		
				// Check time of day
				Int objTime = 0;
				if (pMapObj && pMapObj->getProperties()) {
					objTime = pMapObj->getProperties()->getInt(TheKey_objectTime, &exists);
				}
				switch (objTime) {
					default:
					case 0:
						if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT) {
							state.set(MODELCONDITION_NIGHT);
						}
						break;
					case 2:
						state.set(MODELCONDITION_NIGHT);
						break;
				}
		
				const ModuleInfo& mi = tTemplate->getDrawModuleInfo();
				// DEBUG_LOG(("Draw Module Count: %d \n", mi.getCount()));
				if (mi.getCount() > 0) {
					for (int i = 0; i < mi.getCount(); ++i) {
						const ModuleData* mdd = mi.getNthData(i);
						// DEBUG_LOG(("Processing Module Number: %d \n", i));
						const W3DModelDrawModuleData* md = mdd ? mdd->getAsW3DModelDrawModuleData() : NULL;
						if (md) {
							AsciiString modelName = md->getBestModelNameForWB(state);
							// DEBUG_LOG(("Processing ModelName: %s\n", modelName.str()));
							if (!modelName.isEmpty() && strncmp(modelName.str(), "No ", 3) != 0) {
								if (!getShowModels()) continue;
		
								// Create the sub-model render object
								RenderObjClass* subRenderObj = m_assetManager->Create_Render_Obj(modelName.str(), scale, playerColor);
								if (subRenderObj) {

									// DumpSubObjects(subRenderObj, modelName.str()); // <-- dump list once

									const ModelConditionInfo* info = md->findBestInfo(state);
									if (info && !info->m_hideShowVec.empty()) {
										for (size_t i = 0; i < info->m_hideShowVec.size(); ++i) {
											const ModelConditionInfo::HideShowSubObjInfo& h = info->m_hideShowVec[i];

											// AsciiString cleanName = CleanSubObjName(h.subObjName);
											AsciiString cleanName = h.subObjName;
											Int objIndex;
											RenderObjClass* subObj = subRenderObj->Get_Sub_Object_By_Name(cleanName.str(), &objIndex);
											if (subObj) {
												// DEBUG_LOG(("*** SubObject to clean: '%s'\n", cleanName.str()));
												subObj->Set_Hidden(h.hide);
												subObj->Release_Ref();
											} 
											// else {
												// DEBUG_LOG(("*** ASSET ERROR: SubObject '%s' not found in %s!\n",
												// 		cleanName.str(), modelName.str()));
												// DumpSubObjects(subRenderObj, modelName.str()); // <-- dump list once
											// }
										}
									}

									Bool isNight = state.test(MODELCONDITION_NIGHT);

									for (Int subIndex = 0; subIndex < subRenderObj->Get_Num_Sub_Objects(); ++subIndex)
									{
										RenderObjClass* test = subRenderObj->Get_Sub_Object(subIndex);
										if (!test) continue;

										const char* name = test->Get_Name();
										if (!name) continue;

										if (strstr(name, "HEADLIGHT") && !isNight)
											test->Set_Hidden(true);

										if (strstr(name, "MUZZLE") || strstr(name, "TURRETFX") || strstr(name, "WARHEAD"))
											test->Set_Hidden(true);

										test->Release_Ref();
									}
									

									// Handle shadow for each sub model
									if (m_showShadows) {
										Shadow::ShadowTypeInfo shadowInfo;
										shadowInfo.allowUpdates = FALSE;    // Shadow image will never update
										shadowInfo.allowWorldAlign = TRUE;  // Shadow image will wrap around world objects
										
										if (tTemplate->getShadowType() != SHADOW_NONE && !(pMapObj->getFlags() & FLAG_DONT_RENDER)) {
											strcpy(shadowInfo.m_ShadowName, tTemplate->getShadowTextureName().str());
											DEBUG_ASSERTCRASH(shadowInfo.m_ShadowName[0] != '\0', ("this should be validated in ThingTemplate now"));
											shadowInfo.m_type = (ShadowType)tTemplate->getShadowType();
											shadowInfo.m_sizeX = tTemplate->getShadowSizeX();
											shadowInfo.m_sizeY = tTemplate->getShadowSizeY();
											shadowInfo.m_offsetX = tTemplate->getShadowOffsetX();
											shadowInfo.m_offsetY = tTemplate->getShadowOffsetY();
											// DEBUG_LOG(("processing shadow"));
											shadowObj = TheW3DShadowManager->addShadow(subRenderObj, &shadowInfo); // <-- CAPTURE
										} else {
											shadowInfo.m_type = (ShadowType)SHADOW_VOLUME;
											shadowObj = TheW3DShadowManager->addShadow(subRenderObj, &shadowInfo); // <-- CAPTURE
										}
									}
		
									// Attach submodels properly
									if (!renderObj) {
										renderObj = subRenderObj;  // First model is the main renderObj

										if (m_lod == 1 || !m_showSubDraw) {
											break;  // Only use the first model
										}
									} else {
										renderObj->Add_Sub_Object_To_Bone(subRenderObj, 0);  // Attach to root
										subRenderObj->Release_Ref();  // Release ref after attaching
									}
								}
							}
						}
					}
				}
			}
		}
		if (renderObj && !(pMapObj->getFlags() & FLAG_DONT_RENDER)) {
			pMapObj->setRenderObj(renderObj);

			if (pMapObj->getShadowObj() == NULL) {
				pMapObj->setShadowObj(shadowObj);
			}

			// set item's position to loc, and get scale from item and apply it.

			Matrix3D renderObjPos(true);	// init to identity
			renderObjPos.Translate(loc.x, loc.y, loc.z);
			renderObjPos.Rotate_Z(pMapObj->getAngle());
			renderObj->Set_Transform( renderObjPos );

			if (isVehicle) {
				// note that this affects our orientation, but NOT our position... specifically,
				// it does NOT force us to "stick" to the ground!
				Matrix3D mtx;
				Coord3D terrainNormal;
				m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, &terrainNormal );
				makeAlignToNormalMatrix(pMapObj->getAngle(), loc, terrainNormal, mtx);
				renderObj->Set_Transform( mtx );
			}

			m_scene->Add_Render_Object(renderObj);

			REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
		} else if (renderObj) {
			m_scene->Remove_Render_Object(renderObj);
		}
		if (found) break;
	}
	if (!found && pMapObjIn) {
		if (pMapObjIn->getFlags() & (FLAG_ROAD_FLAGS|FLAG_BRIDGE_FLAGS)) {
			isRoad = true;
		}
		const ThingTemplate *tTemplate = pMapObjIn->getThingTemplate();
		if (tTemplate && tTemplate->isKindOf(KINDOF_OPTIMIZED_TREE)) {
			updateAllTrees = true;
		}
	}
	if (!found && pMapObjIn && pMapObjIn->getRenderObj()) {
		if( m_showShadows ) {
			resetRenderObjects();
			invalObjectInView(NULL);
			--m_updateCount;
			return;
		}
		m_scene->Remove_Render_Object(pMapObjIn->getRenderObj());
		pMapObjIn->setRenderObj(NULL);
	}

	if (isRoad) {
		if(!m_showRoads){
			TheTerrainRenderObject->removeAllRoads();
		} else {
			m_needToLoadRoads = true; // load roads next time we redraw.
		}
	}
	if (updateAllTrees) {
		updateTrees();	
	}
	if (isLight) {
		updateLights(); 
	}
	if (isScorch) {
		updateScorches();
	}
	Invalidate(false);

	// Objects added/moved/deleted/modified all funnel through here (both the doc's
	// invalObject path and direct p3View->invalObjectInView(NULL) callers), so this
	// is the single place to refresh the minimap's object overlay. Pass terrainChanged
	// = false so it re-composites the cached terrain instead of resampling (cheap).
	//
	// But while the mouse is held down we are in an active drag (moving/rotating an
	// object), and this fires on every mouse-move -- re-compositing the whole minimap
	// buffer per move tanks the framerate when the minimap is open. Skip the refresh
	// during the drag; the mouse-up funnels through here once more with the button
	// released, giving a single refresh when the move finishes.
	if (TheMinimapDialog && TheMinimapDialog->IsWindowVisible() && !PointerTool::isMouseDown())
		TheMinimapDialog->requestRebuild(false);

	--m_updateCount;
}


// ----------------------------------------------------------------------------
void WbView3d::updateHeightMapInView(WorldHeightMap *htMap, Bool partial, const IRegion2D &partialRange)
{
	if (htMap == NULL)
		return;
	++m_updateCount;
	m_haveLabelCache = false;	// new/changed terrain or map -> don't reuse a stale label batch

	if (m_heightMapRenderObj == NULL) {
		m_heightMapRenderObj = NEW_REF(WBHeightMap,());
		m_scene->Add_Render_Object(m_heightMapRenderObj);
		partial = false;
	}


	if (m_heightMapRenderObj) {

		Int curTicks = ::GetTickCount();

		RefRenderObjListIterator lightListIt(&m_lightList);	
		if (partial) {
			m_heightMapRenderObj->doPartialUpdate(partialRange, htMap, &lightListIt);
		} else {
			if (m_showEntireMap) {
				htMap->setDrawOrg(0, 0);
				htMap->setDrawWidth(htMap->getXExtent());
				htMap->setDrawHeight(htMap->getYExtent());
				m_heightMapRenderObj->initHeightData(htMap->getXExtent(), htMap->getYExtent(), htMap, &lightListIt);
			} else {	
				htMap->setDrawWidth(m_partialMapSize);
				htMap->setDrawHeight(m_partialMapSize);
				m_heightMapRenderObj->initHeightData(htMap->getDrawWidth(), htMap->getDrawHeight(), htMap, &lightListIt);
			}
			m_heightMapRenderObj->updateViewImpassableAreas();
		}
		curTicks = GetTickCount() - curTicks;
		if (curTicks < 1) curTicks = 1;
	} 

	invalObjectInView(NULL);	// update all the map objects, to account for ground changes

	--m_updateCount;
}

// ----------------------------------------------------------------------------
void WbView3d::setCenterInView(Real x, Real y)
{
	if (x != m_centerPt.X || y != m_centerPt.Y) {
		m_centerPt.X = x;
		m_centerPt.Y = y;
		constrainCenterPt();
		redraw();
		updateHysteresis();
		drawLabels();
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
}

void WbView3d::setCenterInViewDeferred(Real x, Real y)
{
	if (x != m_centerPt.X || y != m_centerPt.Y) {
		m_centerPt.X = x;
		m_centerPt.Y = y;
		constrainCenterPt();
		updateHysteresis();
		// Rebuild the camera transform NOW (no D3D present -- setupCamera only sets
		// m_camera's matrix) so the minimap view box, which projects the frustum via
		// getViewFrustumGroundCorners(), reflects the new center on this same click.
		// Without this the box reads the stale transform and lags one click behind
		// (the first click appears to do nothing).
		setupCamera();
		// Do NOT render here. Let the 3D view's own OnPaint/OnTimer loop do the
		// D3D present on its own thread/window context. Rendering from the Minimap
		// dialog's message handler corrupts the device and blanks the viewport.
		Invalidate(FALSE);
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
}

Bool WbView3d::getViewFrustumGroundCorners(Coord3D corners[4])
{
	if (!m_camera)
		return FALSE;

	// Cast a ray from the camera through each viewport corner (normalized device
	// space, -1..1) and intersect the ground plane z = groundZ. groundZ is the actual
	// terrain height under the camera center, cached by setupCamera. (Do NOT use
	// m_centerPt.Z here -- it is permanently 0, which intersected a flat z=0 plane and
	// drifted the box off the camera on non-flat / non-square maps, worst along +Y.)
	const Real groundZ = m_cameraGroundZ;
	const Vector3 eye = m_camera->Get_Position();

	// NDC corners in view order: top-left, top-right, bottom-right, bottom-left.
	// (+Y is up in view space, so top = +1.)
	static const Vector2 ndc[4] = {
		Vector2(-1.0f,  1.0f),
		Vector2( 1.0f,  1.0f),
		Vector2( 1.0f, -1.0f),
		Vector2(-1.0f, -1.0f)
	};

	for (int i = 0; i < 4; ++i)
	{
		Vector3 onPlane;
		m_camera->Un_Project(onPlane, ndc[i]);
		Vector3 dir = onPlane - eye;

		// Intersect ray eye + t*dir with plane z = groundZ.
		Real denom = dir.Z;
		Real t;
		if (denom > -1.0e-6f && denom < 1.0e-6f)
			t = 1.0f;					// ray parallel to ground; degenerate, just use the plane pt
		else
			t = (groundZ - eye.Z) / denom;

		// If the corner points away from the ground (t<=0, looking at the horizon),
		// push it far out along the ray so the box edge still spans the view.
		if (t <= 0.0f)
			t = 100000.0f;

		Vector3 hit = eye + dir * t;
		// Convert from absolute (border-included) world to BORDER-RELATIVE world so the
		// corners match MapObject::getLocation() -- the space the minimap dots, terrain,
		// and the isInViewFrustum cull all use. Without this the box and cull are shifted
		// by border*MAP_XY_FACTOR (the box lands over the wrong part of the map).
		corners[i].x = hit.X - m_cameraBorderWorld;
		corners[i].y = hit.Y - m_cameraBorderWorld;
		corners[i].z = hit.Z;
	}
	return TRUE;
}

//=============================================================================
// WbView3d::picked3dObjectInView
//=============================================================================
/** Returns true if the pixel location picks the object. */
//=============================================================================
MapObject *WbView3d::picked3dObjectInView(CPoint viewPt)
{
	// This code picks on all 3d objects.
	if (m_intersector && m_layer) {
		CRect client;
		this->GetClientRect(&client);
		float logX = (Real)viewPt.x / (Real)client.Width();
		float logY = (Real)viewPt.y / (Real)client.Height();
		//m_intersector->Result.CollisionType = COLLISION_TYPE_0|COLLISION_TYPE_1;
		// do the intersection using W3D intersector class
		Bool hit = m_intersector->Intersect_Screen_Point_Layer( logX, logY, *m_layer );
		if( hit )
		{
			MapObject *pObj;
			for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				if (pObj->getRenderObj() == m_intersector->Result.IntersectedRenderObject) {
					return pObj;
				}
			}
		}
	}

	return NULL;
}

//=============================================================================
// WbView3d::pickedBuildObjectInView
//=============================================================================
/** Returns true if the pixel location picks the object. */
//=============================================================================
BuildListInfo *WbView3d::pickedBuildObjectInView(CPoint viewPt)
{
	Coord3D cpt;
	Int i;
	viewToDocCoords(viewPt, &cpt, false);
 	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i); 
		for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
			Coord3D center = *pBuild->getLocation();
			center.x -= cpt.x;
			center.y -= cpt.y;
			center.z = 0;
			Real len = center.length();
			// Check and see if we are within 1.5 cell size of the center.
			if (len < 1.5f*MAP_XY_FACTOR) {
				return pBuild;
			}
		}
	}
	// This code picks on all 3d build objects.
	if (m_intersector && m_buildLayer) {
		CRect client;
		this->GetClientRect(&client);
		float logX = (Real)viewPt.x / (Real)client.Width();
		float logY = (Real)viewPt.y / (Real)client.Height();

		// do the intersection using W3D intersector class
		Bool hit = m_intersector->Intersect_Screen_Point_Layer( logX, logY, *m_buildLayer );
		if( hit ) {
 			for (i=0; i<TheSidesList->getNumSides(); i++) {
				SidesInfo *pSide = TheSidesList->getSideInfo(i); 
				for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
					if (pBuild->getRenderObj() == m_intersector->Result.IntersectedRenderObject) {
						return pBuild;
					}
				}
			}
		}
	}

	return NULL;
}

// ----------------------------------------------------------------------------
Bool WbView3d::viewToDocCoords(CPoint curPt, Coord3D *newPt, Bool constrain)
{
	DEBUG_ASSERTCRASH((this),("oops"));
//	const Int VIEW_BORDER = 3000;  // keeps you from falling off the edge of the world.
	Bool result = false;
	CRect client;
	this->GetClientRect(&client);

	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();
	
	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location; 
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirection.Normalize();
	float MaxDistance =  m_camera->Get_Depth()*MAP_XY_FACTOR;
	rayDirectionPt = rayLocation + rayDirection*MaxDistance;

	LineSegClass ray(rayLocation, rayDirectionPt);

	// Note - there are 2 ways to track.  One is for tools (like paint texture)
	// that follow the terrain.  They want to track the terrain, so the texturing
	// follows the cursor.  Most tools, however, don't want to jump up & down clifs
	// and such.  So they use a fixed z plane when tracking, so things don't move 
	// depending what you move over.
	Bool followTerrain = true;
	if (WbApp()->isCurToolLocked()) {
		followTerrain = WbApp()->getCurTool()->followsTerrain();
	}
	if (followTerrain && TheTerrainRenderObject) {
		CastResultStruct castResult;
		RayCollisionTestClass rayCollide(ray, &castResult) ;
		if( TheTerrainRenderObject->Cast_Ray(rayCollide) )
		{
			// get the point of intersection according to W3D
			intersection = castResult.ContactPoint;
			m_curTrackingZ = intersection.Z;
			result = true;
		}  // end if
	} 
	if (!result) {
		intersection.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
		intersection.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
		result = true;
	}
	newPt->x = intersection.X;
	newPt->y = intersection.Y;
	newPt->z = MAGIC_GROUND_Z;
	if (constrain) {
#if 1
		if (m_doLockAngle) {
			Real dy = fabs(m_mouseDownDocPoint.y - newPt->y);
			Real dx = fabs(m_mouseDownDocPoint.x - newPt->x);
			if (dx>2*dy) {
				// lock to dx.
				newPt->y = m_mouseDownDocPoint.y;
			} else if (dy>2*dx) {
				//lock to dy.
				newPt->x = m_mouseDownDocPoint.x;
			} else {
				// Lock to 45 degree.
				dx = (dx+dy)/2;
				dy = dx;
				if (newPt->x < m_mouseDownDocPoint.x) dx = -dx;
				if (newPt->y < m_mouseDownDocPoint.y) dy = -dy;
				newPt->x = m_mouseDownDocPoint.x+dx;
				newPt->y = m_mouseDownDocPoint.y+dy;
			}
		}
#else
		if (m_cameraAngle > PI) {
			m_cameraAngle -= 2*PI;
		}
		if (m_cameraAngle < -PI) {
			m_cameraAngle += 2*PI;
		}
		Bool flip = false;
		// If we are looking sideways, flip the locks.
		if (PI/4<m_cameraAngle && m_cameraAngle < 3*PI/4) {
			flip = true;
		}
		if (-PI/4>m_cameraAngle && m_cameraAngle > -3*PI/4) {
			flip = true;
		}

		if (flip) {
			if (m_doLockVertical) {
				newPt->y = m_mouseDownDocPoint.y;
			} else if (m_doLockHorizontal) {
				newPt->x = m_mouseDownDocPoint.x;
			}
		} else {
			if (m_doLockHorizontal) {
				newPt->y = m_mouseDownDocPoint.y;
			} else if (m_doLockVertical) {
				newPt->x = m_mouseDownDocPoint.x;
			}
		}
#endif
	}
	return result;
}

// ----------------------------------------------------------------------------
Bool WbView3d::viewToDocCoordZ(CPoint curPt, Coord3D *newPt, Real theZ)
{
	DEBUG_ASSERTCRASH((this),("oops"));
	CRect client;
	this->GetClientRect(&client);

	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();
	
	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location; 
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirection.Normalize();
	float MaxDistance =  m_camera->Get_Depth()*MAP_XY_FACTOR;
	rayDirectionPt = rayLocation + rayDirection*MaxDistance;

	LineSegClass ray(rayLocation, rayDirectionPt);

	intersection.X = Vector3::Find_X_At_Z(theZ, rayLocation, rayDirectionPt);
	intersection.Y = Vector3::Find_Y_At_Z(theZ, rayLocation, rayDirectionPt);

	newPt->x = intersection.X;
	newPt->y = intersection.Y;
	newPt->z = theZ;
	return true;
}

// ----------------------------------------------------------------------------
void WbView3d::updateHysteresis(void)
{
	CRect client;
	GetClientRect(&client);
	CPoint curPt;
	curPt.x = (client.left+client.right)/2;
	curPt.y = (client.bottom+client.top)/2;
	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();
	
	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location; 
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;

	intersection.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	intersection.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);

	// Calculate the point offset by 3 pixels.
	logX = (Real)(curPt.x+3) / (Real)client.Width();
	Vector3 offset(0,0,0);

	xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];
	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;
	offset.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset = offset - intersection;
	m_hysteresis = offset.Length();

	logX = (Real)(curPt.x) / (Real)client.Width();
	logY = (Real)(curPt.y+3) / (Real)client.Height();

	xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];
	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;
	offset.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset = offset - intersection;
	if (m_hysteresis < offset.Length()) m_hysteresis = offset.Length();

	CPoint pt1, pt2;
	Coord3D loc;
	loc.x = intersection.X;
	loc.y = intersection.Y;
	loc.z = intersection.Z;
	this->docToViewCoords(loc, &pt1);
	loc.x += MAP_XY_FACTOR*0.4f;
	loc.y += MAP_XY_FACTOR*0.4f;
	this->docToViewCoords(loc, &pt2);
	Int dx = pt1.x-pt2.x;
	if (dx<0) dx = -dx;
	Int dy = pt1.y-pt2.y;
	if (dy<0) dy = -dy;
	if (dx<dy) dx = dy;
	if (dx<4) dx = 3;
	m_pickPixels = dx+3;

}

// ----------------------------------------------------------------------------
Bool WbView3d::docToViewCoords(Coord3D curPt, CPoint* newPt)
{
	Bool coordInsideFrustrum = true;
	Vector3 world;
	Vector3 screen;
	newPt->x = -1000;
	newPt->y = -1000;
	if (m_heightMapRenderObj) {
		curPt.z += m_heightMapRenderObj->getHeightMapHeight(curPt.x, curPt.y, NULL);
	}

	world.Set( curPt.x, curPt.y, curPt.z );
	if (m_camera->Project( screen, world ) != CameraClass::INSIDE_FRUSTUM) {
		coordInsideFrustrum = false;
	} else {
		coordInsideFrustrum = true;
	}

	CRect rClient;
	GetClientRect(&rClient);

	//
	// note that the screen coord returned from the project W3D camera 
	// gave us a screen coords that range from (-1,-1) bottom left to
	// (1,1) top right ... we are turning that into (0,0) upper left
	// coords now
	//
	Int sx, sy;
	W3DLogicalScreenToPixelScreen( screen.X, screen.Y,
																 &sx, &sy,
																 rClient.right-rClient.left, rClient.bottom-rClient.top );

	newPt->x = rClient.left + sx;
	newPt->y = rClient.top + sy;

	return coordInsideFrustrum;
}


Int WbView3d::parseHexColorFromProfile(const char* section, const char* key, const char* defaultHex)
{
    CString str = AfxGetApp()->GetProfileString(section, key, defaultHex);

#ifdef _UNICODE
    char buffer[16];
    WideCharToMultiByte(CP_ACP, 0, str, -1, buffer, sizeof(buffer), NULL, NULL);
    unsigned int color = 0;
    sscanf(buffer, "%x", &color);
#else
    unsigned int color = 0;
    sscanf(str, "%x", &color);
#endif

    color &= 0xFFFFFF;

    // 🔥 Swap Red and Blue to match Windows COLORREF
    unsigned int r = (color >> 16) & 0xFF;
    unsigned int g = (color >> 8) & 0xFF;
    unsigned int b = (color >> 0) & 0xFF;
    color = (b << 16) | (g << 8) | (r << 0);

    return (Int)color;
}


// void WbView3d::addMapObjectIfVisible(MapObject *pMapObj)
// {
//     if (!pMapObj) return;

//     const Coord3D* loc = pMapObj->getLocation();
//     SphereClass bounds(Vector3(loc->x, loc->y, loc->z), THE_RADIUS);
//     Bool isCulled = m_camera->Cull_Sphere(bounds);

//     if (isCulled) {
//         return;
//     }

//     RenderObjClass* renderObj = NULL;
//     Real scale = 1.0;
//     AsciiString modelName = getModelNameAndScale(pMapObj, &scale, BODY_PRISTINE);
//     if (!modelName.isEmpty() && strncmp(modelName.str(), "No ", 3) != 0) {
//         renderObj = m_assetManager->Create_Render_Obj(modelName.str(), scale, 0);

//         if (renderObj) {
//             pMapObj->setRenderObj(renderObj);

//             // 🛠 Fix: adjust z by terrain height
//             Coord3D finalLoc = *loc;
//             if (m_heightMapRenderObj) {
//                 finalLoc.z += m_heightMapRenderObj->getHeightMapHeight(finalLoc.x, finalLoc.y, NULL);
//             }

//             Matrix3D renderObjPos(true); // Identity
//             renderObjPos.Translate(finalLoc.x, finalLoc.y, finalLoc.z);
//             renderObjPos.Rotate_Z(pMapObj->getAngle());
//             renderObj->Set_Transform(renderObjPos);

//             m_scene->Add_Render_Object(renderObj);
//             REF_PTR_RELEASE(renderObj); // Scene owns it now
//         }
//     }
// }


// void WbView3d::updateVisibleMapObjects()
// {
//     // Step 1: Clean up previous render objects
//     resetRenderObjects();

//     // Step 2: Loop through ALL MapObjects and add if visible
//     MapObject* pMapObj = MapObject::getFirstMapObject();
//     while (pMapObj)
//     {
//         addMapObjectIfVisible(pMapObj);
//         pMapObj = pMapObj->getNext();
//     }
// }

// ----------------------------------------------------------------------------
void WbView3d::redraw(void) 
{
	if (m_updateCount > 0) {
		return;
	}
	if (IsIconic()) {
		return;
	}
	if (!IsWindowVisible()) {
		return;
	}
	if (!m_ww3dInited) {
		return;
	}
	
	setupCamera();

	DEBUG_ASSERTCRASH((m_heightMapRenderObj),("oops"));
	if (m_heightMapRenderObj) {
		if (m_needToLoadRoads && m_showRoads) {
			m_heightMapRenderObj->loadRoadsAndBridges(NULL,FALSE);
			// m_heightMapRenderObj->worldBuilderUpdateBridgeTowers( m_assetManager, m_scene );
			m_needToLoadRoads = false;
		}
		++m_updateCount;
		Int curTicks = GetTickCount();
		RefRenderObjListIterator lightListIt(&m_lightList);	
		m_heightMapRenderObj->updateCenter(m_camera, &lightListIt);
		m_heightMapRenderObj->On_Frame_Update();
		--m_updateCount;

		curTicks = GetTickCount()-curTicks;
//		if (curTicks>2) {
//			WWDEBUG_SAY(("%d ms for updateCenter, %d FPS\n", curTicks, 1000/curTicks));
//		}
	}

	// const Int COLOR_GREN = 0x00FF00; // Reserved for waypoint path
	// const Int COLOR_YLLW = 0xFFFF00; // Reserved for roads
	// const Int COLOR_PINK = 0xFF00FF; // Reserved for units
	// const Int COLOR_CYAN = 0x00FFFF; // Reserved for anything else
	// AfxGetApp()->GetProfileString(APP_SECTION, "Color16", "0");

	if (m_drawObject) {
		Int roadIconColor = parseHexColorFromProfile(ICON_COLOR_SECTION, "Roads", "FFFF00"); 
		m_drawObject->setRoadIconColor(roadIconColor);
		Int waypointIconColor  = parseHexColorFromProfile(ICON_COLOR_SECTION, "Waypoints", "00FF00");
		m_drawObject->setWaypointIconColor(waypointIconColor);
		Int unitIconColor  = parseHexColorFromProfile(ICON_COLOR_SECTION, "Units", "FF00FF");
		m_drawObject->setUnitIconColor(unitIconColor);
		Int treeIconColor  = parseHexColorFromProfile(ICON_COLOR_SECTION, "Trees", "00FF00");
		m_drawObject->setTreeIconColor(treeIconColor);

		Int defaultIconColor  = parseHexColorFromProfile(ICON_COLOR_SECTION, "Default", "00FFFF");
		m_drawObject->setDefaultIconColor(defaultIconColor);

		m_drawObject->setDrawObjects(
			m_showObjects, 
			m_showWaypoints || WaypointTool::isActive(),
			m_showPolygonTriggers || PolygonTool::isActive() || WaterTool::isActive(),
			m_showBoundingBoxes, 
			m_showSightRanges, 
			m_showWeaponRanges, 
			m_showSoundCircles, 
			m_highlightTestArt, 
			m_showLetterbox,
			m_showWater,
			m_showObjectsSelected,
			m_useFixedColoredWaypoints
		);
	}

	WW3D::Sync( GetTickCount() );
	m_buildRedMultiplier += (GetTickCount()-m_time)/500.0f;
	if (m_buildRedMultiplier>4.0f || m_buildRedMultiplier<0) {
		m_buildRedMultiplier = 0;
	}

	// updateVisibleMapObjects();
	render();
	m_time = ::GetTickCount();
}

// ----------------------------------------------------------------------------
void WbView3d::render()
{
	++m_updateCount;

	if (WW3D::Begin_Render(true,true,Vector3(0.5f,0.5f,0.5f), TheWaterTransparency->m_minWaterOpacity) == WW3D_ERROR_OK)
	{
		
		DEBUG_ASSERTCRASH((m_heightMapRenderObj),("oops"));

		
		if (m_heightMapRenderObj) {
			m_heightMapRenderObj->Set_Hidden((m_showTerrain ? 0 : 1));
			m_heightMapRenderObj->doTextures(true);
		}
		m_scene->Set_Polygon_Mode(SceneClass::FILL);
		// Render 3D scene

		try {
			// === Per-frame MapObject-based culling ===
			if ((m_showModels || m_showShadows) &&
				m_scene && m_heightMapRenderObj && m_camera && TheW3DShadowManager && m_lod != 3)
			{
				MapObject *pObj = MapObject::getFirstMapObject();
				Vector3 camPos = m_camera->Get_Position();
				
				// Default
				float generalCullDistance;
				float maxShadowDist;
				float propCullDistance;

				if (m_lod == 1) {
					generalCullDistance = 2000.0f;
					maxShadowDist       = 1500.0f;
					propCullDistance    = 1500.0f;
				} else {
					generalCullDistance = 2500.0f;
					maxShadowDist       = 2500.0f;
					propCullDistance    = 2000.0f;
				}

				
				const float generalCullDistSq = generalCullDistance * generalCullDistance;
				const float propCullDistSq    = propCullDistance * propCullDistance;
				const float maxShadowDistSq   = maxShadowDist * maxShadowDist;

				while (pObj)
				{
					const ThingTemplate *t = pObj->getThingTemplate();
					if (!t) { 
						pObj = pObj->getNext();
						continue; 
					}

					Coord3D loc = *pObj->getLocation();
					loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);

					float radius = max(max(t->getTemplateGeometryInfo().getMajorRadius(),
										t->getTemplateGeometryInfo().getMinorRadius()), 20.0f);

					SphereClass bounds(Vector3(loc.x, loc.y, loc.z), radius);
					bool culled = m_camera->Cull_Sphere(bounds);

					// === Global distance culling (applies to everything) ===
					float dx = camPos.X - loc.x;
					float dy = camPos.Y - loc.y;
					float dz = camPos.Z - loc.z;
					float distSq = dx*dx + dy*dy + dz*dz;

					if (!culled && distSq > generalCullDistSq) {
						culled = true;
					}

					// === Extra distance-based culling for misc props ===
					if (!culled && t->getEditorSorting() == ES_MISC_MAN_MADE || t->getEditorSorting() == ES_MISC_NATURAL) {
						if (distSq > propCullDistSq) {
							culled = true;
						}
					}

					if (RenderObjClass *robj = pObj->getRenderObj()) {
						// bool farLOD = (distSq > (doodooDistance * doodooDistance)); // or whatever threshold
						// robj->Force_Degraded_Render(farLOD);
						robj->Set_Hidden(culled);
					}

					if (m_showShadows) {
						if (Shadow *shadow = pObj->getShadowObj()) {
							if (shadow) {
								shadow->enableShadowRender(distSq <= maxShadowDistSq && !culled);
							}
						}
					}

					pObj = pObj->getNext();
				}
			}
		}
		catch (...) {
			DEBUG_LOG(("Culling pass threw an exception — skipping this frame.\n"));
		}

		WW3D::Render(m_scene,m_camera);	
		Vector3 amb = m_baseBuildScene->Get_Ambient_Light();
		Vector3 newAmb(amb);
		Real mul = m_buildRedMultiplier;
		if (mul>2.0f) mul = 4.0f-mul;
		Real gMul = 2.0-mul;
		newAmb.X *= mul;
		newAmb.Y *= gMul;
		if (newAmb.X>1) newAmb.X = 1;
		m_baseBuildScene->Set_Ambient_Light(newAmb); 
		WW3D::Render(m_baseBuildScene,m_camera);	
		m_baseBuildScene->Set_Ambient_Light(amb); 

		if (m_showWireframe) {
			if (m_heightMapRenderObj) {
				m_heightMapRenderObj->doTextures(false);
				m_scene->Set_Polygon_Mode(SceneClass::POINT);
				// Render 3D scene
				WW3D::Render(m_scene,m_camera);	
				WW3D::Render(m_baseBuildScene,m_camera);	
				m_heightMapRenderObj->doTextures(true);
			}
		} 
		if (m_showObjToolTrackingObj && m_objectToolTrackingObj) {
			m_transparentObjectsScene->Add_Render_Object(m_objectToolTrackingObj);
			DX8TextureCategoryClass::SetForceMultiply(true);
			TheDX8MeshRenderer.Enable_Lighting(false);
			Real lightLevel = 1.0f;
			if(m_validTerrain){
				m_transparentObjectsScene->Set_Ambient_Light(Vector3(lightLevel,lightLevel,lightLevel)); 
			} else {
				m_transparentObjectsScene->Set_Ambient_Light(Vector3(1.0f, 0.0f, 0.0f)); // Red light
			}
			WW3D::Render(m_transparentObjectsScene, m_camera);
			TheDX8MeshRenderer.Enable_Lighting(true);
			DX8TextureCategoryClass::SetForceMultiply(false);
			m_transparentObjectsScene->Remove_Render_Object(m_objectToolTrackingObj);
		}
		
		// Draw the 3d obj icons on top of the rest of the data.
		WW3D::Render(m_overlayScene,m_camera);

		// Viewport labels, Old (D3DX) mode: draw directly with m3DFont inside the
		// frame (flicker-free). drawLabels(NULL) takes the m3DFont->DrawText path.
		// In New (GDI) mode the labels are drawn instead in OnPaint() via ::TextOut.
		if (m3DFont && m_labelRenderer == 0) {
			drawLabels(NULL);
		}

		WW3D::End_Render();
	}
	--m_updateCount;
}

// ----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(WbView3d, WbView)
	//{{AFX_MSG_MAP(WbView3d)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_SHOWWINDOW()
	ON_COMMAND(ID_VIEW_SHOWWIREFRAME, OnViewShowwireframe)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWWIREFRAME, OnUpdateViewShowwireframe)
	ON_WM_ERASEBKGND()
	ON_COMMAND(ID_VIEW_SHOWENTIRE3DMAP, OnViewShowentire3dmap)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWENTIRE3DMAP, OnUpdateViewShowentire3dmap)
	ON_COMMAND(ID_VIEW_SHOWTOPDOWNVIEW, OnViewShowtopdownview)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTOPDOWNVIEW, OnUpdateViewShowtopdownview)
	ON_COMMAND(ID_VIEW_SHOWCLOUDS, OnViewShowclouds)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWCLOUDS, OnUpdateViewShowclouds)
	ON_COMMAND(ID_VIEW_SHOWMACROTEXTURE, OnViewShowmacrotexture)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMACROTEXTURE, OnUpdateViewShowmacrotexture)
	ON_COMMAND(ID_EDIT_SELECTMACROTEXTURE, OnEditSelectmacrotexture)
	ON_COMMAND(ID_VIEW_SHOWSOFTWATER, OnViewShowSoftWater)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWEXTRABLENDS, OnUpdateViewShowExtraBlends)
	ON_COMMAND(ID_VIEW_SHOWEXTRABLENDS, OnViewExtraBlends)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWSOFTWATER, OnUpdateViewShowSoftWater)
	ON_COMMAND(ID_VIEW_SHOWSHADOWS, OnViewShowshadows)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWSHADOWS, OnUpdateViewShowshadows)
	ON_COMMAND(ID_EDIT_SHADOWS, OnEditShadows)
	ON_COMMAND(ID_EDIT_MAPSETTINGS, OnEditMapSettings)
	ON_COMMAND(ID_REMOVEBOUNDARIES, OnClearAllExtraBoundaries)
	ON_COMMAND(ID_VIEW_SHOWIMPASSABLEAREAS, OnViewShowimpassableareas)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWIMPASSABLEAREAS, OnUpdateViewShowimpassableareas)
	ON_COMMAND(ID_VIEW_IMPASSABLEAREAOPTIONS, OnImpassableAreaOptions)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_96X96, OnViewPartialmapsize96x96)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_96X96, OnUpdateViewPartialmapsize96x96)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_192X192, OnViewPartialmapsize192x192)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_192X192, OnUpdateViewPartialmapsize192x192)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_160X160, OnViewPartialmapsize160x160)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_160X160, OnUpdateViewPartialmapsize160x160)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_128X128, OnViewPartialmapsize128x128)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_128X128, OnUpdateViewPartialmapsize128x128)
	ON_COMMAND(ID_VIEW_SHOWMODELS, OnViewShowModels)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMODELS, OnUpdateViewShowModels)
	ON_COMMAND(ID_VIEW_BOUNDINGBOXES, OnViewBoundingBoxes)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BOUNDINGBOXES, OnUpdateViewBoundingBoxes)
	ON_COMMAND(ID_VIEW_SIGHTRANGES, OnViewSightRanges)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SIGHTRANGES, OnUpdateViewSightRanges)
	ON_COMMAND(ID_VIEW_WEAPONRANGES, OnViewWeaponRanges)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WEAPONRANGES, OnUpdateViewWeaponRanges)
	ON_COMMAND(ID_HIGHLIGHT_TESTART, OnHighlightTestArt)
	ON_UPDATE_COMMAND_UI(ID_HIGHLIGHT_TESTART, OnUpdateHighlightTestArt)
	ON_COMMAND(ID_SHOW_LETTERBOX, OnShowLetterbox)
	ON_UPDATE_COMMAND_UI(ID_SHOW_LETTERBOX, OnUpdateShowLetterbox)
	ON_COMMAND(ID_VIEW_GARRISONED, OnViewGarrisoned)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GARRISONED, OnUpdateViewGarrisoned)
	ON_COMMAND(ID_VIEW_LAYERS_LIST, OnViewLayersList)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LAYERS_LIST, OnUpdateViewLayersList)
	ON_COMMAND(ID_VIEW_MINIMAP, OnViewMinimap)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MINIMAP, OnUpdateViewMinimap)
	ON_COMMAND(ID_MINIMAP_SHOWOBJECTS, OnMinimapShowObjects)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_SHOWOBJECTS, OnUpdateMinimapShowObjects)
	ON_COMMAND(ID_MINIMAP_SHOWROADS, OnMinimapShowRoads)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_SHOWROADS, OnUpdateMinimapShowRoads)
	ON_COMMAND(ID_MINIMAP_CULLOBJECTS, OnMinimapCullObjects)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_CULLOBJECTS, OnUpdateMinimapCullObjects)
	ON_COMMAND(ID_MINIMAP_SNAP45, OnMinimapSnap45)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_SNAP45, OnUpdateMinimapSnap45)
	ON_COMMAND(ID_MINIMAP_REFRESH_OFF, OnMinimapRefreshOff)
	ON_COMMAND(ID_MINIMAP_REFRESH_16, OnMinimapRefresh16)
	ON_COMMAND(ID_MINIMAP_REFRESH_33, OnMinimapRefresh33)
	ON_COMMAND(ID_MINIMAP_REFRESH_100, OnMinimapRefresh100)
	ON_COMMAND(ID_MINIMAP_REFRESH_250, OnMinimapRefresh250)
	ON_COMMAND(ID_MINIMAP_REFRESH_1000, OnMinimapRefresh1000)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_OFF, OnUpdateMinimapRefreshOff)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_16, OnUpdateMinimapRefresh16)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_33, OnUpdateMinimapRefresh33)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_100, OnUpdateMinimapRefresh100)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_250, OnUpdateMinimapRefresh250)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_REFRESH_1000, OnUpdateMinimapRefresh1000)
	ON_COMMAND(ID_MINIMAP_RES_256, OnMinimapRes256)
	ON_COMMAND(ID_MINIMAP_RES_512, OnMinimapRes512)
	ON_COMMAND(ID_MINIMAP_RES_1024, OnMinimapRes1024)
	ON_COMMAND(ID_MINIMAP_RES_2048, OnMinimapRes2048)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_RES_256, OnUpdateMinimapRes256)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_RES_512, OnUpdateMinimapRes512)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_RES_1024, OnUpdateMinimapRes1024)
	ON_UPDATE_COMMAND_UI(ID_MINIMAP_RES_2048, OnUpdateMinimapRes2048)
	ON_COMMAND(ID_VIEW_SHOWMAPBOUNDARIES, OnViewShowMapBoundaries)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMAPBOUNDARIES, OnUpdateViewShowMapBoundaries)
	ON_COMMAND(ID_VIEW_RULERGRID, OnViewShowRulerGrid)
	ON_UPDATE_COMMAND_UI(ID_VIEW_RULERGRID, OnUpdateViewShowRulerGrid)
	ON_COMMAND(ID_VIEW_SHOWTRACINGOVERLAY, OnViewShowTracingOverlay)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTRACINGOVERLAY, OnUpdateViewShowTracingOverlay)
	ON_COMMAND(ID_VIEW_SHOWSUBDRAW, OnViewShowSubDraw)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWSUBDRAW, OnUpdateViewShowSubDraw)
	ON_COMMAND(ID_VIEW_SHOWBASERADIUS, OnViewShowBaseRadius)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWBASERADIUS, OnUpdateViewShowBaseRadius)
	ON_COMMAND(ID_VIEW_SHOWAMBIENTSOUNDS, OnViewShowAmbientSounds)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWAMBIENTSOUNDS, OnUpdateViewShowAmbientSounds)
	ON_COMMAND(ID_VIEW_SHOW_SOUND_CIRCLES, OnViewShowSoundCircles)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOW_SOUND_CIRCLES, OnUpdateViewShowSoundCircles)


	ON_COMMAND(ID_LOD_MODE_1, OnWindowLODMode1)
	ON_UPDATE_COMMAND_UI(ID_LOD_MODE_1, OnUpdateOnWindowLODMode1)
	ON_COMMAND(ID_LOD_MODE_2, OnWindowLODMode2)
	ON_UPDATE_COMMAND_UI(ID_LOD_MODE_2, OnUpdateOnWindowLODMode2)
	ON_COMMAND(ID_LOD_MODE_3, OnWindowLODMode3)
	ON_UPDATE_COMMAND_UI(ID_LOD_MODE_3, OnUpdateOnWindowLODMode3)

	ON_COMMAND(ID_MSAA_NONE, OnMSAANone)
	ON_UPDATE_COMMAND_UI(ID_MSAA_NONE, OnUpdateMSAANone)
	ON_COMMAND(ID_MSAA_2X, OnMSAA2X)
	ON_UPDATE_COMMAND_UI(ID_MSAA_2X, OnUpdateMSAA2X)
	ON_COMMAND(ID_MSAA_4X, OnMSAA4X)
	ON_UPDATE_COMMAND_UI(ID_MSAA_4X, OnUpdateMSAA4X)
	ON_COMMAND(ID_MSAA_8X, OnMSAA8X)
	ON_UPDATE_COMMAND_UI(ID_MSAA_8X, OnUpdateMSAA8X)
	ON_COMMAND(ID_TEXFILTER_DEFAULT, OnTexFilterDefault)
	ON_UPDATE_COMMAND_UI(ID_TEXFILTER_DEFAULT, OnUpdateTexFilterDefault)
	ON_COMMAND(ID_TEXFILTER_ANISO16X, OnTexFilterAniso16X)
	ON_UPDATE_COMMAND_UI(ID_TEXFILTER_ANISO16X, OnUpdateTexFilterAniso16X)
	ON_COMMAND(ID_TEXT_SHADOW, OnTextShadow)
	ON_UPDATE_COMMAND_UI(ID_TEXT_SHADOW, OnUpdateTextShadow)
	ON_COMMAND(ID_TEXT_ANTIALIAS, OnTextAntialias)
	ON_UPDATE_COMMAND_UI(ID_TEXT_ANTIALIAS, OnUpdateTextAntialias)
	ON_COMMAND(ID_TEXT_ANCHOR_DEFAULT, OnTextAnchorDefault)
	ON_UPDATE_COMMAND_UI(ID_TEXT_ANCHOR_DEFAULT, OnUpdateTextAnchorDefault)
	ON_COMMAND(ID_TEXT_ANCHOR_NEW, OnTextAnchorNew)
	ON_UPDATE_COMMAND_UI(ID_TEXT_ANCHOR_NEW, OnUpdateTextAnchorNew)
	ON_COMMAND(ID_TEXT_RENDERER_OLD, OnTextRendererOld)
	ON_UPDATE_COMMAND_UI(ID_TEXT_RENDERER_OLD, OnUpdateTextRendererOld)
	ON_COMMAND(ID_TEXT_RENDERER_NEW, OnTextRendererNew)
	ON_UPDATE_COMMAND_UI(ID_TEXT_RENDERER_NEW, OnUpdateTextRendererNew)

	ON_COMMAND(ID_REVALIDATE_RENDER, OnRefreshSceneObjects)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// ----------------------------------------------------------------------------
// WbView3d drawing

void WbView3d::OnDraw(CDC* pDC)
{
	// Not used.  See OnPaint.
}

// ----------------------------------------------------------------------------
// WbView3d diagnostics

#ifdef _DEBUG
// ----------------------------------------------------------------------------
void WbView3d::AssertValid() const
{
	WbView::AssertValid();
}

// ----------------------------------------------------------------------------
void WbView3d::Dump(CDumpContext& dc) const
{
	WbView::Dump(dc);
}
#endif //_DEBUG

// ----------------------------------------------------------------------------
void WbView3d::initWW3D()
{
	// only want to do once per instance, but do lazily.
	if (!m_ww3dInited) {
		


		m_ww3dInited = true;

		WWMath::Init();

		WW3D::Set_Prelit_Mode(WW3D::PRELIT_MODE_VERTEX);

		initAssets();
		WW3D::Init(m_hWnd);	
		WW3D::Set_Prelit_Mode( WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS );
		WW3D::Set_Collision_Box_Display_Mask(0x00);	///<set to 0xff to make collision boxes visible

		bogusTacticalView.setWidth(m_actualWinSize.x);
		bogusTacticalView.setHeight(m_actualWinSize.y);
		bogusTacticalView.setOrigin(0,0);
		if (WW3D::Set_Render_Device(0, m_actualWinSize.x, m_actualWinSize.y, 32, true, true) != WW3D_ERROR_OK) 
		{
			// Getting the device at the default bit depth (32) didn't work, so try
			// getting a 16 bit display.  (Voodoo 1-3 only supported 16 bit.) jba.
			if (WW3D::Set_Render_Device(0, m_actualWinSize.x, m_actualWinSize.y, 16, true, true) != WW3D_ERROR_OK) 
			{
				DEBUG_CRASH(("Couldn't set render device."));
			}
		}

		createLabelFont();

		int texFilterMode = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "TexFilterMode", 0);
		if (texFilterMode == 1) {
			TextureFilterClass::Set_Max_Anisotropy(16);
			WW3D::Set_Texture_Filter(TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC);
		}

		WW3D::Enable_Static_Sort_Lists(true);
		WW3D::Set_Thumbnail_Enabled(false);
		WW3D::Set_Screen_UV_Bias( TRUE );  ///< this makes text look good :)

		W3DShaderManager::init();
		init3dScene();
		m_layer = new LayerClass( m_scene, m_camera );
		m_buildLayer = new LayerClass( m_baseBuildScene, m_camera );
		m_intersector = new IntersectionClass();
		m_drawObject = new DrawObject();
		m_overlayScene->Add_Render_Object(m_drawObject);

#if 1
		TheWritableGlobalData->m_useShadowVolumes = true;
		TheWritableGlobalData->m_useShadowDecals = true;
		// TheWritableGlobalData->m_useTreeSway = false;
		TheWritableGlobalData->m_enableBehindBuildingMarkers = false;	//this is only for the game.
		TheWritableGlobalData->m_textureReductionFactor = 0;
		if (TheW3DShadowManager==NULL)
		{	TheW3DShadowManager = new W3DShadowManager;
 			TheW3DShadowManager->init();			
		}
#endif
		updateLights();
		resetRenderObjects();
	}
}

// ----------------------------------------------------------------------------
// WbView3d message handlers

// ----------------------------------------------------------------------------
int WbView3d::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (WbView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// install debug callbacks
	WWDebug_Install_Message_Handler(WWDebug_Message_Callback);
	WWDebug_Install_Assert_Handler(WWAssert_Callback);

	m_timer = SetTimer(0, UPDATE_TIME, NULL);

	initWW3D();	
	TheWritableGlobalData->m_useCloudMap = AfxGetApp()->GetProfileInt("GameOptions", "cloudMap", 0);
	AfxGetApp()->WriteProfileInt("GameOptions", "cloudMap", TheGlobalData->m_useCloudMap);	// Just in case it wasn't already there
 	m_partialMapSize = AfxGetApp()->GetProfileInt("GameOptions", "partialMapSize", 97);

	m_showLayersList = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowLayersList", 0);
	m_showMapBoundaries = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowMapBoundaries", 0);
	m_showAmbientSounds = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowAmbientSounds", 0);
	m_showBaseRadius = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowBaseRadius", 1);
	m_showSubDraw = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowSubDraw", 1);
	m_showSoundCircles = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowSoundCircles", 0);
	m_showRulerGrid = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowRulerGrid", 1);
	m_showTracingOverlay = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowTracingOverlay", 0);


	CFileFind finder;
	BOOL fileExists = finder.FindFile("data\\editor\\trace_overlay.dds");
	if (!fileExists)
	{
		if(m_showTracingOverlay){
			::MessageBeep(MB_ICONERROR);
			AfxMessageBox(
				"Missing texture:\n"
				"data\\editor\\trace_overlay.dds\n\n"
				"The tracing overlay will not be displayed until this texture is restored.\n\n"
				"You little shit, did you not install the worldbuilder properly? the texture file was supposed to be on the zip file - did you delete it?.",
				MB_ICONERROR | MB_OK
			);
		}
		m_showTracingOverlay = 0;
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowTracingOverlay", m_showTracingOverlay ? 1 : 0);
	}

	if (m_showObjects &&
		::AfxGetApp()->GetProfileInt(TOOLTIP_SECTION, "ShowObjectIconsWarningShown", 0) == 0)
	{
		AfxMessageBox(
			"Showing ALL object icons can noticeably slow down rendering performance.\n\n"
			"You may want to disable this option, since the updated version now allows "
			"you to rotate objects simply by selecting them, no need to display all icons.",
			MB_OK | MB_ICONWARNING
		);

		::AfxGetApp()->WriteProfileInt(TOOLTIP_SECTION, "ShowObjectIconsWarningShown", 1);
	}


	DrawObject::setDoBoundaryFeedback(m_showMapBoundaries);
	DrawObject::setDoGridFeedback(m_showRulerGrid);
	DrawObject::setDoAmbientSoundFeedback(m_showAmbientSounds);
	DrawObject::setDoTracingOverlayFeedback(m_showTracingOverlay);
	DrawObject::setDoBaseRadiusFeedback(m_showBaseRadius);

	startEditTimer();

	return 0;
}

// ----------------------------------------------------------------------------
void WbView3d::OnPaint() 
{	

	PAINTSTRUCT ps;
	HDC hdc = ::BeginPaint(m_hWnd, &ps);
	if (!m_firstPaint) {
		redraw();
	}
	// New (GDI) mode only: draw labels with raw ::TextOut onto the window HDC, after
	// the D3D frame has been presented by redraw()/End_Render(). This is what strobes
	// (the next flip wipes it) -- accepted trade-off. Old (D3DX) mode draws labels
	// inside the frame in render(), so we must NOT also draw them here.
	if (m_labelRenderer == 1) {
		drawLabels(hdc);
		// Record the view state we just painted, so OnTimer can skip repaints (and
		// thus the strobe-inducing buffer flip) until something actually changes.
		m_lastGdiPaintKey = buildLabelKey();
		m_haveGdiPaintKey = true;
	} else if (m_doRectFeedback) {
		// Old (D3DX) mode draws labels inside the D3D frame and never passes an HDC to
		// drawLabels, so the drag-select box (a GDI ::FrameRect) would never appear. Draw
		// it here on the window HDC instead -- same GDI box as GDI mode, for both modes.
		CBrush brush;
		brush.CreateSolidBrush(RGB(255, 165, 0));
		::FrameRect(hdc, &m_feedbackBox, (HBRUSH)brush.GetSafeHandle());
	}
	::EndPaint(m_hWnd, &ps);
	if (m_firstPaint) {
		CMainFrame::GetMainFrame()->adjustWindowSize();
		m_firstPaint = false;
	}
	DX8Wrapper::SetCleanupHook(this);
	
}

Real getWaterHeightIfUnderwaterx(Real x, Real y)
{
    ICoord3D iLoc;
    iLoc.x = (floor(x + 0.5f));
    iLoc.y = (floor(y + 0.5f));
    iLoc.z = 0;

    for (PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
        if (!pTrig->isWaterArea()) {
            continue;
        }

        if (pTrig->pointInTrigger(iLoc)) {
            Real waterZ = pTrig->getPoint(0)->z;
            Real terrainZ = TheTerrainRenderObject->getHeightMapHeight(x, y, NULL);
            if (terrainZ < waterZ) {
                return waterZ;
            }
        }
    }

    return -FLT_MAX; // Not underwater
}

void WbView3d::drawCircle(HDC hdc, const Coord3D& centerPoint, Real radius, COLORREF color)
{
    CPoint rulerPoints[2];
    Coord3D pnt;
    Real angle = 0.0f;
    Real inc = PI / 24.0f; // smoother circle

    // Create and select a correctly colored pen. Remember the old one so that it can be restored.
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN penOld = (HPEN)SelectObject(hdc, pen);

    // Get the starting point on the circumference of the circle.
    pnt.x = centerPoint.x + radius * cosf(angle);
    pnt.y = centerPoint.y + radius * sinf(angle);

    // Sample terrain height
    pnt.z = TheTerrainRenderObject->getHeightMapHeight(pnt.x, pnt.y, NULL);

    // Optional: Adjust for water if enabled
    if (m_showWater) {
        Real waterHeight = getWaterHeightIfUnderwaterx(pnt.x, pnt.y);
        if (waterHeight != -FLT_MAX) {
            pnt.z = waterHeight + 4.5f;
        }
    }

    docToViewCoords(pnt, &rulerPoints[0]);

    angle += inc;
    for (; angle <= 2.0f * PI + 0.001f; angle += inc) {
        // Calculate next point on circle
        pnt.x = centerPoint.x + radius * cosf(angle);
        pnt.y = centerPoint.y + radius * sinf(angle);

        // Sample terrain height
        pnt.z = TheTerrainRenderObject->getHeightMapHeight(pnt.x, pnt.y, NULL);

        // Optional: Adjust for water if needed
        if (m_showWater) {
            Real waterHeight = getWaterHeightIfUnderwaterx(pnt.x, pnt.y) - 30.0f;
            if (waterHeight != -FLT_MAX) {
                pnt.z = waterHeight + 4.5f;
            }
        }

        docToViewCoords(pnt, &rulerPoints[1]);

        ::Polyline(hdc, rulerPoints, 2); // draw the segment

        // Prepare next segment
        rulerPoints[0] = rulerPoints[1];
    }

    SelectObject(hdc, penOld);
    DeleteObject(pen);
}



void WbView3d::startEditTimer()
{

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		DEBUG_LOG(("Map Name: %s\n", pDoc->getMapPath()));
	}

    if (!m_isTimerRunning) {
        m_editStartTime = ::GetTickCount();
        m_isTimerRunning = true;
    }
}

void WbView3d::pauseEditTimer()
{
    if (m_isTimerRunning) {
        DWORD currentTime = ::GetTickCount();
        m_totalEditTime += (currentTime - m_editStartTime);
        m_isTimerRunning = false;
    }
}

void WbView3d::resetEditTimer()
{
    m_totalEditTime = 0;
    m_editStartTime = ::GetTickCount();
    m_isTimerRunning = true;
}

DWORD WbView3d::getEditTimeInSeconds()
{
    DWORD totalTime = m_totalEditTime;
    
    if (m_isTimerRunning) {
        DWORD currentTime = ::GetTickCount();
        totalTime += (currentTime - m_editStartTime);
    }
    
    return totalTime / 1000; // Convert milliseconds to seconds
}

AsciiString WbView3d::formatEditTime()
{
    DWORD totalSeconds = getEditTimeInSeconds();
    
    DWORD hours = totalSeconds / 3600;
    DWORD minutes = (totalSeconds % 3600) / 60;
    DWORD seconds = totalSeconds % 60;
    
    char buffer[64];
    sprintf(buffer, "Total Edit Time: %02d:%02d:%02d", hours, minutes, seconds);
    
    return AsciiString(buffer);
}

void WbView3d::setEditTime(DWORD seconds)
{
	resetEditTimer();
	m_totalEditTime = seconds * 1000; // Convert seconds to milliseconds
	startEditTimer();
}

// --- viewport-label vertex-batch cache --------------------------------------

Bool WbView3d::LabelCacheKey::operator==(const LabelCacheKey &o) const
{
	for (int i = 0; i < 12; ++i)
		if (camXform[i] != o.camXform[i]) return false;
	return winW == o.winW && winH == o.winH && lod == o.lod &&
		showNames == o.showNames && showModels == o.showModels &&
		showWaypoints == o.showWaypoints && showNamesExtra == o.showNamesExtra &&
		showPolygonTriggers == o.showPolygonTriggers &&
		lightFeedback == o.lightFeedback && timeOfDay == o.timeOfDay &&
		labelAnchorMode == o.labelAnchorMode && epoch == o.epoch;
}

// Snapshot everything drawLabels() reads to decide label geometry / positions /
// colours. If this matches the previous frame, the cached vertex batch can be
// re-issued instead of rebuilt.
WbView3d::LabelCacheKey WbView3d::buildLabelKey()
{
	LabelCacheKey k;
	if (m_camera) {
		const Matrix3D &m = m_camera->Get_Transform();
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 4; ++c)
				k.camXform[r * 4 + c] = m[r][c];
	} else {
		for (int i = 0; i < 12; ++i) k.camXform[i] = 0.0f;
	}
	k.winW = m_actualWinSize.x;
	k.winH = m_actualWinSize.y;
	k.lod = m_lod;
	k.showNames = isNamesVisible();
	k.showModels = m_showModels;
	k.showWaypoints = m_showWaypoints;
	k.showNamesExtra = m_showNamesExtra;
	k.showPolygonTriggers = m_showPolygonTriggers;
	k.lightFeedback = m_doLightFeedback;
	k.timeOfDay = (Int)TheGlobalData->m_timeOfDay;
	k.labelAnchorMode = m_labelAnchorMode;
	k.epoch = m_labelEpoch;
	return k;
}

void WbView3d::drawLabels(void)
{
	CDC * pDC = GetDC();
	drawLabels(pDC->m_hDC);
	ReleaseDC(pDC);
}

void WbView3d::drawStatusLabels(CPoint basePt, int offset, const char* text, void* m3DFont, HDC hdc) {
	CPoint labelPt = basePt;
	labelPt.y += offset * 15;

	int red = 0, green = 255, blue = 255;
	AsciiString label = text;

	// Route by the label-renderer mode (member m_labelRenderer), not by m3DFont
	// nullness: Old (0) uses the in-frame D3DX font, New (1) uses raw GDI ::TextOut.
	if (m_labelRenderer == 0 && m3DFont && !hdc) {
		if (m_textShadow) {
			RECT shadowRct = { labelPt.x + 2, labelPt.y + 1, labelPt.x + 2, labelPt.y + 1 };
			((ID3DXFont*)m3DFont)->DrawText(label.str(), label.getLength(), &shadowRct,
				DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE, 0xFF000000);
		}
		DWORD textColor = 0xFF000000 | (red << 16) | (green << 8) | blue;
		RECT rct = { labelPt.x + 1, labelPt.y, labelPt.x + 1, labelPt.y };
		((ID3DXFont*)m3DFont)->DrawText(label.str(), label.getLength(), &rct,
			DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE, textColor);
	} else if (m_labelRenderer == 1 && hdc) {
		::SetBkMode(hdc, TRANSPARENT);
		if (m_textShadow) {
			::SetTextColor(hdc, RGB(0, 0, 0));
			::TextOut(hdc, labelPt.x + 2, labelPt.y + 1, label.str(), label.getLength());
		}
		::SetTextColor(hdc, RGB(red, green, blue));
		::TextOut(hdc, labelPt.x + 1, labelPt.y, label.str(), label.getLength());
	}
}

/// This is actually draw any 2d graphics and/or feedback.
void WbView3d::drawLabels(HDC hdc)
{
	Coord3D selectedPos;	//position of selected object
	Real	selectedRadius=120.0f;	//default distance of lightfeeback model from object
	selectedPos.x=0;selectedPos.y=0;selectedPos.z=0; 

	// === TEMPORARY DEBUG LABEL: Draw "test" near the mouse cursor with white text and black outline ===
	// if (PointerTool::isMouseDown() && !PointerTool::isDragSelecting()) {
	// 	CPoint pt;
	// 	GetCursorPos(&pt);         // screen coords
	// 	ScreenToClient(&pt);       // convert to client coords

	// 	const int x = pt.x + 32;   // Increased offset to move text further right
	// 	const int y = pt.y + 8;
	// 	const CString text = _T(PointerTool::getLastPointerInfoString());
	// 	const int len = text.GetLength();

	// 	SetBkMode(hdc, TRANSPARENT);

	// 	// Draw outline (black) around the text
	// 	SetTextColor(hdc, RGB(0, 0, 0));
	// 	::TextOut(hdc, x - 1, y,     text, len);
	// 	::TextOut(hdc, x + 1, y,     text, len);
	// 	::TextOut(hdc, x,     y - 1, text, len);
	// 	::TextOut(hdc, x,     y + 1, text, len);
	// 	::TextOut(hdc, x - 1, y - 1, text, len);
	// 	::TextOut(hdc, x + 1, y - 1, text, len);
	// 	::TextOut(hdc, x - 1, y + 1, text, len);
	// 	::TextOut(hdc, x + 1, y + 1, text, len);

	// 	// Draw main text (white)
	// 	SetTextColor(hdc, RGB(255, 255, 255));
	// 	::TextOut(hdc, x, y, text, len);
	// }

	// DEBUG_LOG(("AutoEdgeOutTool::isActive() = %d\n", AutoEdgeOutTool::isActive() ? 1 : 0));
	// DEBUG_LOG(("PointerTool::isDragSelecting() = %d\n", PointerTool::isDragSelecting() ? 1 : 0));
	// DEBUG_LOG(("PointerTool::isMouseDown() = %d\n", PointerTool::isMouseDown() ? 1 : 0));


	int totalWorldCash = 0;

	// Draw labels.
	//
	// The per-object work splits into two parts: a parallelizable COMPUTE part
	// (projection, terrain-height lookup, property dictionary reads, name/color/
	// status resolution) and a serial EMIT part (m_fontAtlas.drawText /
	// drawStatusLabels) that drives the D3D8 device and must stay on the UI
	// thread. We snapshot the object list, fill one LabelRecord per object in a
	// parallel prepass, then emit serially IN THE ORIGINAL ORDER so the frame is
	// byte-identical to the single-threaded path. WBParallel falls back to a
	// serial inline run for small object counts, so light scenes pay no overhead.
	struct StatusLabel { const char *text; };	// emit text, in fixed order
	struct LabelRecord {
		bool   project;					// passed projection + LOD cull -> has name/status labels to emit
		CPoint pt;
		// up to 4 name labels (base + 3 waypoint paths), precomputed
		int          nameCount;
		AsciiString  nameText[4];
		int          nameSlot[4];		// original i (0..3) -> vertical offset
		UnsignedInt  nameArgb[4];
		// status labels (only when m_showNamesExtra), in emit order
		bool         showStatus;
		int          statusCount;
		const char  *statusText[7];
		// reductions consumed after the loop
		int          cashPartial;
		bool         feedback;			// this is the selected object for light feedback
		Coord3D      selPos;
		Real         selRadius;
	};

	// Snapshot the object list on the UI thread (it must not mutate during the
	// parallel pass).
	std::vector<MapObject*> objs;
	for (MapObject *o = MapObject::getFirstMapObject(); o; o = o->getNext())
		objs.push_back(o);
	const int objCount = (int)objs.size();

	std::vector<LabelRecord> recs(objCount);

	// Prime the camera frustum once on this thread. CameraClass::Project() lazily
	// rebuilds a mutable cached frustum on first use; doing it here means every
	// worker's Project() sees a valid frustum and only READS it (no data race on
	// the mutable cache).
	{
		Vector3 dummy;
		m_camera->Project(dummy, Vector3(0, 0, 0));
	}

	// Projection target space. The two renderers draw into DIFFERENT pixel spaces:
	//  - Old (D3DX): label quads are composited into the D3D back buffer, whose size
	//    is m_actualWinSize (a fixed/forced resolution that the driver stretches to
	//    fill the window). Project into m_actualWinSize so the quads land correctly.
	//  - New (GDI): ::TextOut draws into the window's client HDC at 1:1 client pixels.
	//    Project into the client rect (and offset by its origin) or the text lands
	//    offset/scaled relative to the stretched 3D image.
	// GetClientRect must run on the UI thread (not inside the parallel worker), so we
	// resolve the projection size/origin here, once, and the workers only read it.
	Int projW, projH, projOriginX, projOriginY;
	if (m_labelRenderer == 1) {
		CRect rClientProj;
		GetClientRect(&rClientProj);
		projW = rClientProj.Width();
		projH = rClientProj.Height();
		projOriginX = rClientProj.left;
		projOriginY = rClientProj.top;
	} else {
		projW = m_actualWinSize.x;
		projH = m_actualWinSize.y;
		projOriginX = 0;
		projOriginY = 0;
	}

	WBParallel::parallelFor(0, objCount, [&](int begin, int end)
	{
		for (int oi = begin; oi < end; ++oi) {
			MapObject *pMapObj = objs[oi];
			LabelRecord &rec = recs[oi];
			rec.project = false;
			rec.nameCount = 0;
			rec.showStatus = false;
			rec.statusCount = 0;
			rec.cashPartial = 0;
			rec.feedback = false;
			rec.selRadius = 0.0f;

			const ThingTemplate *tmpl;
			tmpl = pMapObj->getThingTemplate();
			if (tmpl && tmpl->isKindOf(KINDOF_SUPPLY_SOURCE)) {
				const ModuleInfo& modInfo = tmpl->getBehaviorModuleInfo();

				for (int i = 0; i < modInfo.getCount(); ++i) {
					if (modInfo.getNthName(i).compare("SupplyWarehouseDockUpdate") == 0) {
						const ModuleData* moduleData = modInfo.getNthData(i);
						if (!moduleData) continue;

						const SupplyWarehouseDockUpdateModuleData* dockData =
							static_cast<const SupplyWarehouseDockUpdateModuleData*>(moduleData);

						int boxes = dockData->m_startingBoxesData;

						// Add to running total (cash value)
						rec.cashPartial += static_cast<int>(boxes * TheGlobalData->m_baseValuePerSupplyBox);
					}
				}
			}

			if (!isNamesVisible()) continue;

			if (pMapObj->getFlags() & FLAG_DONT_RENDER) continue;

			Coord3D pos = *pMapObj->getLocation();
			float terrainZ = m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, NULL);
			pos.z += terrainZ;

			// Label anchor mode. Default (0): anchor at the object's ground point (legacy;
			// the label then drifts relative to a tall marker as the camera orbits).
			// New (1): anchor ON the object -- at the vertical center of its visible
			// representation -- so the label sits on the model and tracks it as the camera
			// orbits. Covers all viewport-label kinds:
			//   - objects with a loaded render model -> bounding-sphere center (mid-height);
			//   - flag-style markers with no model (ambient sounds = ES_AUDIO, and waypoints)
			//     -> mid-height of the pole drawn by DrawObject (poleHeight 20 -> ~10);
			//   - anything else flat (e.g. scorches) -> ground anchor.
			// Reads static geometry/template data only -> thread-safe in this parallel pass.
			if (m_labelAnchorMode == 1) {
				if (RenderObjClass *ro = pMapObj->getRenderObj()) {
					SphereClass s;
					ro->Get_Obj_Space_Bounding_Sphere(s);
					pos.z += s.Center.Z;		// center of the model, not its top
				} else {
					const ThingTemplate *att = pMapObj->getThingTemplate();
					Bool isAudioFlag = att && att->getEditorSorting() == ES_AUDIO;
					if (isAudioFlag || pMapObj->isWaypoint())
						pos.z += 15.0f;		// mid-pole (poleHeight 20 / 2 + slack), per DrawObject.cpp
				}
			}

			// Light feedback logic
			if (m_doLightFeedback && pMapObj->isSelected()) {
				rec.feedback = true;
				rec.selPos = pos;
				rec.selPos.z = terrainZ;
				rec.selRadius = 120.0f;

				if (RenderObjClass *selRobj = pMapObj->getRenderObj()) {
					SphereClass sphere;
					selRobj->Get_Obj_Space_Bounding_Sphere(sphere);
					rec.selRadius = sphere.Radius + sphere.Center.Length() + 20.0f;
				}
			}

			// Get base name from object properties
			Bool exists;
			AsciiString objectDictName = pMapObj->getProperties()->getAsciiString(TheKey_objectName, &exists);
			AsciiString name;

			Bool isRenderableObject = !(pMapObj->getFlags() & (FLAG_ROAD_FLAGS | FLAG_BRIDGE_FLAGS));
			if (!objectDictName.isEmpty() && isRenderableObject && (m_showModels || pMapObj->isSelected())) {
				name = objectDictName;
			} else if (pMapObj->isWaypoint() && m_showWaypoints) {
				name = pMapObj->getWaypointName();
			} else if (pMapObj->getThingTemplate() && isRenderableObject && m_showModels &&
				!pMapObj->getRenderObj() && !pMapObj->getThingTemplate()->isKindOf(KINDOF_OPTIMIZED_TREE)) {
				name = pMapObj->getThingTemplate()->getName();
			}

			// Check if any of the custom status flags are true
			bool hasStatusLabel = false;

			// Show only if opposite of default
			bool value;

			value = pMapObj->getProperties()->getBool(TheKey_objectIndestructible, &exists);
			if (exists && value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectUnsellable, &exists);
			if (exists && value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectTargetable, &exists);
			if (exists && value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectPowered, &exists);
			if (exists && !value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectRecruitableAI, &exists);
			if (exists && !value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectEnabled, &exists);
			if (exists && !value) hasStatusLabel = true;

			value = pMapObj->getProperties()->getBool(TheKey_objectSelectable, &exists);
			if (exists && !value) hasStatusLabel = true;


			if(!m_showModels && !pMapObj->isSelected()){
				hasStatusLabel = false;
			}

			// Skip label projection if completely nameless + no status
			if (name.isEmpty() && !m_showWaypoints && objectDictName.isEmpty() && !hasStatusLabel) continue;

			// Anchor to the object's true world position. (A world-space X nudge here
			// projects to a different screen direction at every camera angle, making the
			// label "float" around the object as you rotate -- so don't add one.)
			Vector3 world(pos.x, pos.y, pos.z);
			Vector3 screen;
			if (CameraClass::INSIDE_FRUSTUM != m_camera->Project(screen, world)) continue;

			// Map into the renderer's pixel space (back buffer for D3DX, client rect
			// for GDI; see projW/projH/projOrigin* resolved above).
			Int sx, sy;
			W3DLogicalScreenToPixelScreenHackedForWBLabels(
				screen.X, screen.Y,
				&sx, &sy,
				projW, projH
			);

			CPoint pt(projOriginX + sx, projOriginY + sy - 5);

			// Skip Projection if not visible to this area
			if(m_lod == 1){
				CPoint center(projOriginX + projW / 2, projOriginY + projH / 2);
				int dx = pt.x - center.x;
				int dy = pt.y - center.y;
				int distSq = dx * dx + dy * dy;
				if (distSq > 300 * 300) continue;
			}

			rec.project = true;
			rec.pt = pt;

			// Resolve all name labels: base + waypoint path labels.
			for (Int i = 0; i < 4; i++) {
				AsciiString label;
				if (i == 0) {
					label = name;
				} else if (m_showWaypoints) {
					switch (i) {
						case 1: label = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists); break;
						case 2: label = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists); break;
						case 3: label = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists); break;
					}
				}

				if (label.isEmpty()) continue;

				if (!m_showNamesExtra) {
					AsciiString lower = label;
					lower.toLower();
					if (lower.startsWith("waypoint"))
						continue; // skip clutter
				}

				Int red = 255, green = 255, blue = 255;

				if (i == 0) {
					if (!objectDictName.isEmpty()) {
						red = 255;
						green = 255;
						blue = 0; // Yellow-ish
					} else {
						red = 0;
						green = 255;
						blue = 0; // Green
					}
				}

				int n = rec.nameCount++;
				rec.nameText[n] = label;
				rec.nameSlot[n] = i;
				rec.nameArgb[n] = 0xFF000000 | (red << 16) | (green << 8) | blue;
			}

			// Resolve status labels (emitted after the name lines).
			if(m_showNamesExtra){
				rec.showStatus = true;

				if (pMapObj->getProperties()->getBool(TheKey_objectIndestructible, &exists) && exists)
					rec.statusText[rec.statusCount++] = "Indestructible";

				if (pMapObj->getProperties()->getBool(TheKey_objectUnsellable, &exists) && exists)
					rec.statusText[rec.statusCount++] = "Unsellable";

				if (pMapObj->getProperties()->getBool(TheKey_objectTargetable, &exists) && exists)
					rec.statusText[rec.statusCount++] = "Targetable";

				bool isPowered = pMapObj->getProperties()->getBool(TheKey_objectPowered, &exists);
				if (exists && !isPowered)
					rec.statusText[rec.statusCount++] = "Not Powered";

				bool isEnabled = pMapObj->getProperties()->getBool(TheKey_objectEnabled, &exists);
				if (exists && !isEnabled)
					rec.statusText[rec.statusCount++] = "Not Enabled";

				bool isAIRecruitable = pMapObj->getProperties()->getBool(TheKey_objectRecruitableAI, &exists);
				if (exists && !isAIRecruitable)
					rec.statusText[rec.statusCount++] = "Not AI Recruitable";

				bool isSelectable = pMapObj->getProperties()->getBool(TheKey_objectSelectable, &exists);
				if (exists && !isSelectable)
					rec.statusText[rec.statusCount++] = "Not Selectable";
			}
		}
	});

	// Serial emit pass (UI thread): walk records in original order so the output
	// is identical to the single-threaded path. drawText / drawStatusLabels drive
	// the D3D device and must not run concurrently.
	if (true) {
		for (int oi = 0; oi < objCount; ++oi) {
			const LabelRecord &rec = recs[oi];

			totalWorldCash += rec.cashPartial;

			if (rec.feedback) {
				selectedPos = rec.selPos;
				selectedRadius = rec.selRadius;
			}

			if (!rec.project) continue;

			for (int n = 0; n < rec.nameCount; ++n) {
				CPoint labelPt = rec.pt;
				labelPt.y += rec.nameSlot[n] * 15;

				const AsciiString &label = rec.nameText[n];
				UnsignedInt argb = rec.nameArgb[n];
				int red   = (argb >> 16) & 0xFF;
				int green = (argb >>  8) & 0xFF;
				int blue  =  argb        & 0xFF;

				if (m_labelRenderer == 0 && m3DFont && !hdc) {
					if (m_textShadow) {
						RECT shadowRct = { labelPt.x + 2, labelPt.y + 1, labelPt.x + 2, labelPt.y + 1 };
						m3DFont->DrawText(label.str(), label.getLength(), &shadowRct,
							DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE, 0xFF000000);
					}
					DWORD textColor = 0xFF000000 | (red << 16) | (green << 8) | blue;
					RECT rct = { labelPt.x + 1, labelPt.y, labelPt.x + 1, labelPt.y };
					m3DFont->DrawText(label.str(), label.getLength(), &rct,
						DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE, textColor);
				} else if (m_labelRenderer == 1 && hdc) {
					::SetBkMode(hdc, TRANSPARENT);
					if (m_textShadow) {
						::SetTextColor(hdc, RGB(0, 0, 0));
						::TextOut(hdc, labelPt.x + 2, labelPt.y + 1, label.str(), label.getLength());
					}
					::SetTextColor(hdc, RGB(red, green, blue));
					::TextOut(hdc, labelPt.x + 1, labelPt.y, label.str(), label.getLength());
				}
			}

			if (rec.showStatus) {
				int statusOffset = 1; // Start after the main 4 label lines
				for (int s = 0; s < rec.statusCount; ++s)
					drawStatusLabels(rec.pt, statusOffset++, rec.statusText[s], m3DFont, hdc);
			}
		}

		/**
		 * Adriane [Deathscythe]
		 * Lets support the labels of poly triggers cause why not
		 */
		PolygonTrigger *pTrig;
		for (pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
			AsciiString triggerName = pTrig->getTriggerName();
			if (!triggerName.isEmpty() && pTrig->getNumPoints() > 0 && m_showPolygonTriggers && isNamesVisible()) {
				// Loop through all points of the polygon
				for (Int i = 0; i < pTrig->getNumPoints(); ++i) {
					ICoord3D iLoc = *pTrig->getPoint(i); // Get each point
					Coord3D loc;
					loc.x = iLoc.x;
					loc.y = iLoc.y;
					loc.z = m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, NULL);

					Vector3 world, screen;
					CPoint pt;

					// Anchor to the true trigger-point position (no world-space X nudge,
					// which would make the label drift around the point on camera rotation).
					world.Set(loc.x, loc.y, loc.z);
					if (CameraClass::INSIDE_FRUSTUM != m_camera->Project(screen, world)) {
						continue;
					}

					// Renderer pixel space (back buffer for D3DX, client rect for GDI;
					// see projW/projH/projOrigin* resolved above).
					Int sx, sy;
					W3DLogicalScreenToPixelScreenHackedForWBLabels(screen.X, screen.Y,
												&sx, &sy,
												projW, projH);
					pt.x = projOriginX + sx;
					pt.y = projOriginY + sy;

					// Draw the label for each point
					if (m_labelRenderer == 0 && m3DFont && !hdc) {
						if (m_textShadow) {
							RECT shadowRct;
							shadowRct.top = shadowRct.bottom = pt.y + 1;
							shadowRct.left = shadowRct.right = pt.x + 1;
							m3DFont->DrawText(triggerName.str(), triggerName.getLength(), &shadowRct,
											DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE,
											0xFF000000);
						}
						RECT rct;
						rct.top = rct.bottom = pt.y;
						rct.left = rct.right = pt.x;
						m3DFont->DrawText(triggerName.str(), triggerName.getLength(), &rct,
										DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE,
										0xAFFF8800);
					} else if (m_labelRenderer == 1 && hdc) {
						::SetBkMode(hdc, TRANSPARENT);
						if (m_textShadow) {
							::SetTextColor(hdc, RGB(0, 0, 0));
							::TextOut(hdc, pt.x + 1, pt.y + 1, triggerName.str(), triggerName.getLength());
						}
						::SetTextColor(hdc, RGB(238, 130, 238));
						::TextOut(hdc, pt.x, pt.y, triggerName.str(), triggerName.getLength());
					}
				}
			}
		}
	}

	// Draw tracking box.
	if (hdc && m_doRectFeedback) {
		CBrush brush;
		// green brush for drawing the grid.
		brush.CreateSolidBrush(RGB(255, 165, 0));
		::FrameRect(hdc, &m_feedbackBox, (HBRUSH)brush.GetSafeHandle());
	}

	// DEBUG_LOG(("PointerTool::isMouseDown() = %d\n", PointerTool::isMouseDown() ? 1 : 0));
	// DEBUG_LOG(("AutoEdgeOutTool::isActive() = %d\n", AutoEdgeOutTool::isActive() ? 1 : 0));
	// DEBUG_LOG(("PointerTool::isDragSelecting() = %d\n", PointerTool::isDragSelecting() ? 1 : 0));
	//  BE WARNED -- DO NOT ENABLE THIS DUDE WHEN DRAGGING OR ELSE THE DRAG RECT WILL BE BROKEN UNDER POINTER TOOL
	if ((PointerTool::isMouseDown() || AutoEdgeOutTool::isActive() || ObjectTool::isActive()) 
			&& !PointerTool::isDragSelecting()
		) {
		const CString text = _T(PointerTool::getLastPointerInfoString());
		// DEBUG_LOG(("PointerTool::getLastPointerInfoString() returned: \"%s\"\n", (LPCTSTR)text));
		if (text.IsEmpty() || (m_labelRenderer == 0 && !m3DFont))
			return;

		// Get mouse position
		CPoint pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);

		const int offsetX = 32;
		const int offsetY = 8;
		const int width   = 300;
		const int height  = 50;

		// Base draw rect
		RECT baseRect = {
			pt.x + offsetX,
			pt.y + offsetY,
			pt.x + offsetX + width,
			pt.y + offsetY + height
		};

		// Outline color (black)
		const DWORD outlineColor = 0xFF000000;
		// Main text color (white)
		const DWORD mainColor = 0xFFFFFFFF;

		// Offsets for outline (4 directions: N/E/S/W). The 4 diagonals were dropped
		// to halve the per-label outline draw count (8 DrawText calls -> 4); the text
		// stays outlined on all sides, only the diagonal corners are slightly thinner.
		const int outlineOffsets[4][2] = {
			{-1,  0}, { 1,  0}, { 0, -1}, { 0,  1}
		};

		if (m_labelRenderer == 0 && m3DFont && !hdc) {
			// Draw outline
			for (int i = 0; i < 4; ++i) {
				RECT outlineRect = baseRect;
				OffsetRect(&outlineRect, outlineOffsets[i][0], outlineOffsets[i][1]);
				m3DFont->DrawText(
					text,
					text.GetLength(),
					&outlineRect,
					DT_LEFT | DT_TOP | DT_NOCLIP | DT_WORDBREAK,
					outlineColor
				);
			}

			// Draw main text
			m3DFont->DrawText(
				text,
				text.GetLength(),
				&baseRect,
				DT_LEFT | DT_TOP | DT_NOCLIP | DT_WORDBREAK,
				mainColor
			);
		} else if (m_labelRenderer == 1 && hdc) {
			::SetBkMode(hdc, TRANSPARENT);
			::SetTextColor(hdc, RGB(0, 0, 0));
			for (int i = 0; i < 4; ++i) {
				::TextOut(hdc, baseRect.left + outlineOffsets[i][0], baseRect.top + outlineOffsets[i][1],
					text, text.GetLength());
			}
			::SetTextColor(hdc, RGB(255, 255, 255));
			::TextOut(hdc, baseRect.left, baseRect.top, text, text.GetLength());
		}
	}

	CString text;
	text.Format(_T("Total world cash: %d"), totalWorldCash);

	const int offsetX = 10;
	const int offsetY = 10;

	if (m_labelRenderer == 0 && m3DFont && !hdc) {
		RECT rct = { offsetX, offsetY, offsetX + 400, offsetY + 30 };
		m3DFont->DrawText(
			text,
			text.GetLength(),
			&rct,
			DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE,
			0xFFFFFFFF
		);
	} else if (m_labelRenderer == 1 && hdc) {
		::SetBkMode(hdc, TRANSPARENT);
		::SetTextColor(hdc, RGB(255, 255, 255));
		::TextOut(hdc, offsetX, offsetY, text, text.GetLength());
	}

    // === EDIT TIMER DISPLAY (Bottom Left) ===
    AsciiString editTimeStr = formatEditTime();
	if(editTimeStr.isEmpty() == false){
		CRect rClient;
		GetClientRect(&rClient);

		const int offsetX = 10;
		const int offsetY = rClient.bottom + 70;

		if (m_labelRenderer == 0 && m3DFont && !hdc) {
			RECT rct = { offsetX, offsetY, offsetX + 400, offsetY + 30 };
			m3DFont->DrawText(
				editTimeStr.str(),
				editTimeStr.getLength(),
				&rct,
				DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE,
				0xFFFFFFFF // White color
			);
		} else if (m_labelRenderer == 1 && hdc) {
			::SetBkMode(hdc, TRANSPARENT);
			::SetTextColor(hdc, RGB(255, 255, 255));
			::TextOut(hdc, offsetX, offsetY, editTimeStr.str(), editTimeStr.getLength());
		}
	}

	if (CMainFrame::GetMainFrame()->showAutoSaveMessage()){
		CString autoSaveText = _T("Auto-saving in 10 seconds...");

		if (m_labelRenderer == 0 && m3DFont && !hdc) {
			RECT rct = { offsetX, offsetY + 20, offsetX + 400, offsetY + 50 };
			m3DFont->DrawText(
				autoSaveText,
				autoSaveText.GetLength(),
				&rct,
				DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE,
				0xFFFFFF00 // Yellow color
			);
		} else if (m_labelRenderer == 1 && hdc) {
			::SetBkMode(hdc, TRANSPARENT);
			::SetTextColor(hdc, RGB(255, 255, 0)); // Yellow color
			::TextOut(hdc, offsetX, offsetY + 20, autoSaveText, autoSaveText.GetLength());
		}
	}

	if (hdc && m_doRulerFeedback) {
		if (m_doRulerFeedback == RULER_LINE) {
			// Create and select a green pen. Remember the old one so that it can be restored.
			HPEN pen = CreatePen(PS_SOLID, 2, RGB(0,255,0));
			HPEN penOld = (HPEN)SelectObject(hdc, pen); 

			const Coord3D& p0 = m_rulerPoints[0];
			const Coord3D& p1 = m_rulerPoints[1];

			const int numSteps = 64; // Controls resolution of the curve
			CPoint lastPt;
			bool hasLast = false;

			for (int i = 0; i <= numSteps; ++i) {
				float t = (float)i / numSteps;

				Coord3D wp;
				wp.x = p0.x + t * (p1.x - p0.x);
				wp.y = p0.y + t * (p1.y - p0.y);

				// Get terrain height at this position
				wp.z = TheTerrainRenderObject->getHeightMapHeight(wp.x, wp.y, NULL);
				wp.z -= 30.0f;  // tune this value

				// Optional: lift above water if needed
				if (m_showWater) {
					Real waterHeight = getWaterHeightIfUnderwaterx(wp.x, wp.y);
					waterHeight -= 30.0f;  // tune this value
					if (waterHeight != -FLT_MAX) {
						wp.z = waterHeight;
					}
				}

				CPoint screenPt;
				docToViewCoords(wp, &screenPt);

				if (hasLast) {
				// Draw line segment manually
				::MoveToEx(hdc, lastPt.x, lastPt.y, NULL);
				::LineTo(hdc, screenPt.x, screenPt.y);
				}

				lastPt = screenPt;
				hasLast = true;
			}

			// Restore previous pen.
			SelectObject(hdc, penOld);
			DeleteObject(pen);
		} else if (m_doRulerFeedback == RULER_CIRCLE) {
      		drawCircle( hdc, m_rulerPoints[0], m_rulerLength, RGB( 0, 255, 0 ) );
		}  
	}

	if (hdc && m_doLightFeedback)
	{	//Draw Lines to indicate the direction of each light source
//		Int LightColors[MAX_GLOBAL_LIGHTS]={RGB(255,0,0),RGB(0,255,0),RGB(0,0,255)};

		for (Int lIndex=0; lIndex<MAX_GLOBAL_LIGHTS; lIndex++)
		{
			Vector3 worldStart, screenStart;	//start of line
			Vector3 worldEnd,	screenEnd;		//end of line

			worldStart.Set( selectedPos.x, selectedPos.y, selectedPos.z );
			worldEnd.Set(selectedPos.x - m_lightDirection[lIndex].x*selectedRadius,
						selectedPos.y - m_lightDirection[lIndex].y*selectedRadius,
						selectedPos.z - m_lightDirection[lIndex].z*selectedRadius);

			if (m_lightFeedbackMesh[lIndex] == NULL)
			{	char nameBuf[64];
				sprintf(nameBuf,"WB_LIGHT%d",lIndex+1);
				m_lightFeedbackMesh[lIndex]=WW3DAssetManager::Get_Instance()->Create_Render_Obj(nameBuf);
			}
			if (m_lightFeedbackMesh[lIndex]==NULL) {
				break;
			}
			Matrix3D lightMat;

			lightMat.Look_At(worldEnd,worldStart,0);
			lightMat.Set_Translation(worldEnd);

			m_lightFeedbackMesh[lIndex]->Add(m_scene);
			m_lightFeedbackMesh[lIndex]->Set_Transform(lightMat);
#ifdef DRAW_LIGHT_DIRECTION_RAYS
			if (CameraClass::INSIDE_FRUSTUM == m_camera->Project( screenStart, worldStart ) &&
				CameraClass::INSIDE_FRUSTUM == m_camera->Project( screenEnd, worldEnd ))
			{
				CRect rClient;
				GetClientRect(&rClient);

				//
				// note that the screen coord returned from the project W3D camera 
				// gave us a screen coords that range from (-1,-1) bottom left to
				// (1,1) top right ... we are turning that into (0,0) upper left
				// coords now
				//
				Int sxStart, syStart;
				Int sxEnd, syEnd;

				W3DLogicalScreenToPixelScreen( screenStart.X, screenStart.Y, &sxStart, &syStart,rClient.right-rClient.left, rClient.bottom-rClient.top );
				W3DLogicalScreenToPixelScreen( screenEnd.X, screenEnd.Y, &sxEnd, &syEnd,rClient.right-rClient.left, rClient.bottom-rClient.top );

				POINT rayPoints[2];

				rayPoints[0].x = rClient.left+sxStart;
				rayPoints[0].y= rClient.top+syStart;

				rayPoints[1].x = rClient.left+sxEnd;
				rayPoints[1].y= rClient.top+syEnd;

				HPEN pen=CreatePen( PS_SOLID,2, LightColors[lIndex]);
				HPEN penOld = (HPEN)SelectObject(hdc, pen); 
				Polyline(hdc,rayPoints,2);
				SelectObject(hdc, penOld);	//restore previous pen
				DeleteObject(pen);	//delete new pen
			}
#endif	//DRAW_LIGHT_DIRECTION_RAYS
		}//end for
	}
	else
	{	if (!m_doLightFeedback)
		{	//not in light feedback mode.  Make sure the temporary feeback models are gone

			for (Int lIndex=0; lIndex<MAX_GLOBAL_LIGHTS; lIndex++)
			{
				if (m_lightFeedbackMesh[lIndex] != NULL)
				{	m_lightFeedbackMesh[lIndex]->Remove();
					REF_PTR_RELEASE(m_lightFeedbackMesh[lIndex]);
				}
			}
		}
	}
}


// ----------------------------------------------------------------------------
void WbView3d::OnSize(UINT nType, int cx, int cy) 
{
	WbView::OnSize(nType, cx, cy);

}

// ----------------------------------------------------------------------------
BOOL WbView3d::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	if (m_trackingMode == TRACK_NONE) {

		//WST 11/21/02 New Triple speed camera zoom request by designers
		if (getCurrentZoom() > 2.0f)
		{
			m_mouseWheelOffset += zDelta;
		}
		else if (getCurrentZoom() > 1.0f)
		{
			m_mouseWheelOffset += zDelta/2;
		}
		else
		{
			m_mouseWheelOffset += zDelta/8;
		}

		MSG msg;
		while (::PeekMessage(&msg, m_hWnd, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE)) {
			zDelta = (short) HIWORD(msg.wParam);    // wheel rotation
			m_mouseWheelOffset += zDelta;
		}
		redraw();
		updateHysteresis();
		drawLabels();
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
	return true;
}

// ----------------------------------------------------------------------------
void WbView3d::setDefaultCamera()
{

	m_mouseWheelOffset = 0;
	m_cameraAngle = 0;
	m_cameraAngleRaw = 0;
	m_FXPitch = 1.0f;
	if (m_centerPt.X < 0) m_centerPt.X = 0;
	if (m_centerPt.Y < 0) m_centerPt.Y = 0;
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
		if (pMap) {
			if (m_centerPt.X > pMap->getXExtent()) {
				m_centerPt.X = pMap->getXExtent();
			}
			if (m_centerPt.Y > pMap->getYExtent()) {
				m_centerPt.Y = pMap->getYExtent();
			}
		}
	}
	m_groundLevel = 10.0;
	Coord3D pos;
	pos.x = m_centerPt.X;
	pos.y = m_centerPt.Y;
	pos.z = m_centerPt.Z;
	AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj) {
		if (pMapObj->isWaypoint()) {
			if (startingCamName == pMapObj->getWaypointName()) {
				pos = *pMapObj->getLocation();
			}
		}
		pMapObj = pMapObj->getNext();
	}

	if (m_heightMapRenderObj) {
		m_groundLevel = m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, NULL);
	}

	//m_cameraOffset.z = m_groundLevel+TheGlobalData->m_cameraHeight;
	m_cameraOffset.z = m_groundLevel+TheGlobalData->m_maxCameraHeight;
	m_cameraOffset.y = -(m_cameraOffset.z / tan(TheGlobalData->m_cameraPitch * (PI / 180.0)));
	m_cameraOffset.x = -(m_cameraOffset.y * tan(TheGlobalData->m_cameraYaw * (PI / 180.0)));

	m_cameraOffset.x *= 1.0f;
	m_cameraOffset.y *= 1.0f;
	m_cameraOffset.z *= 1.0f;
	/*
	m_cameraOffset.x *= TheGlobalData->m_maxZoom;
	m_cameraOffset.y *= TheGlobalData->m_maxZoom;
	m_cameraOffset.z *= TheGlobalData->m_maxZoom;
	*/

	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
// Round an angle (radians) to the nearest 45-degree (PI/4) step.
static Real snapAngleTo45(Real a)
{
	const Real step = PI / 4.0f;
	return step * floor(a / step + 0.5f);
}

void WbView3d::rotateCamera(Real delta)
{
	if (m_projection) return; // camera doesn't rotate in top down view.
	// Accumulate the raw (unsnapped) angle so a continuous drag keeps building up even
	// while the displayed angle is pinned to a 45-degree step. Then derive the actual
	// camera angle: when Angle Snap Lock is on, snap the raw to the nearest 45-degree
	// step, so the camera ratchets notch-to-notch (0,45,90,...) as you drag. Without the
	// separate raw accumulator, snapping m_cameraAngle in place would discard the drag
	// each event and the camera would never advance past the first step.
	m_cameraAngleRaw += delta;
	m_cameraAngle = m_snapCameraAngle45 ? snapAngleTo45(m_cameraAngleRaw) : m_cameraAngleRaw;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
void WbView3d::pitchCamera(Real delta)
{
	if (m_projection) return; // camera doesn't pitch in top down view.
	m_FXPitch += delta;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
void WbView3d::setCameraPitch(Real absolutePitch)
{
	if (m_projection) return; // camera doesn't pitch in top down view.
	m_FXPitch = absolutePitch;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
Real WbView3d::getCameraPitch(void)
{
	return m_FXPitch;
}


//WST 10.17.2002 ----------------------------------------------------------------------------
Real WbView3d::getCurrentZoom(void)
{
	float zOffset = - m_mouseWheelOffset / 1200; //WST 11/21/02 new triple speed camera zoom.
	Real zoom = 1.0f;
	if (zOffset != 0) {
		Real zPos = (m_cameraOffset.length()-m_groundLevel)/m_cameraOffset.length();
		Real zAbs = zOffset + zPos;
		if (zAbs<0) zAbs = -zAbs;
		if (zAbs<0.01) zAbs = 0.01f;
		//DEBUG_LOG(("zOffset = %.2f, zAbs = %.2f, zPos = %.2f\n", zOffset, zAbs, zPos));	
		if (zOffset > 0) {
			zOffset *= zAbs;
		}	else if (zOffset < -0.3f) {
			zOffset = -0.15f + zOffset/2.0f;
		}
		if (zOffset < -0.6f) {
			zOffset = -0.3f + zOffset/2.0f;
		}
		//DEBUG_LOG(("zOffset = %.2f\n", zOffset));
		zoom = zAbs;
	}
	return zoom;
}

// ----------------------------------------------------------------------------
void WbView3d::OnTimer(UINT nIDEvent)
{
	if (getLastDrawTime()+UPDATE_TIME >= ::GetTickCount())
		return;		// throttle: at most one repaint per UPDATE_TIME

	// GDI label mode coalesces idle repaints to suppress the strobe (see header).
	// Repaint only when the view actually changed, an interaction is in progress,
	// or the low-rate fallback elapsed. D3DX mode always free-runs.
	if (m_labelRenderer == 1) {
		const UINT GDI_IDLE_FALLBACK_MS = 5000;	// max stale-frame time when "idle"

		Bool interacting = (m_trackingMode != TRACK_NONE) || PointerTool::isMouseDown();
		LabelCacheKey key = buildLabelKey();
		Bool changed = !m_haveGdiPaintKey || !(key == m_lastGdiPaintKey);
		Bool fallbackDue = (getLastDrawTime() + GDI_IDLE_FALLBACK_MS) < ::GetTickCount();

		if (!interacting && !changed && !fallbackDue)
			return;		// static view: leave the last frame + its GDI text on screen
	}

	Invalidate(false);
}

// ----------------------------------------------------------------------------
void WbView3d::OnDestroy() 
{
	killTheTimer();
	WbView::OnDestroy();	
}

// ----------------------------------------------------------------------------
void WbView3d::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	WbView::OnShowWindow(bShow, nStatus);
}

//=============================================================================
// CWorldBuilderView::scroll
//=============================================================================
/** Scrolls the window. */
//=============================================================================
void WbView3d::scrollInView(Real xScroll, Real yScroll, Bool end) 
{
	m_centerPt.X += xScroll;
	m_centerPt.Y += yScroll;
	constrainCenterPt();
	redraw();
	drawLabels();
	if (end)
		WbDoc()->syncViewCenters(m_centerPt.X, m_centerPt.Y);
	CMainFrame::GetMainFrame()->handleCameraChange();
}

void WbView3d::OnViewShowwireframe() 
{
	m_showWireframe = !m_showWireframe;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowWireframe", m_showWireframe?1:0);
}

void WbView3d::OnUpdateViewShowwireframe(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_showWireframe?1:0);
	
}

BOOL WbView3d::OnEraseBkgnd(CDC* pDC) 
{
	// Never erase the background.  The 3d view always draws the entire
	// window, so erasing just makes it flicker.  jba.
	return true; // act like we erased.
}

void WbView3d::OnViewShowentire3dmap() 
{
	if(m_showEntireMap){
		::MessageBeep(MB_ICONINFORMATION);
		int response = ::AfxMessageBox(
			"You may have accidentally pressed CTRL+A. Are you sure you want to toggle the full map view?", 
			MB_YESNO | MB_ICONQUESTION
		);

		if (response == IDNO){
			return;
		}
	}

	m_showEntireMap = !m_showEntireMap;	
	IRegion2D range = {0,0,0,0};
	this->updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowEntireMap", m_showEntireMap?1:0);
}

void WbView3d::OnUpdateViewShowentire3dmap(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_showEntireMap?1:0);
}

void WbView3d::OnViewShowtopdownview() 
{
	m_projection = !m_projection;
	// m_heightMapRenderObj->setFlattenHeights(m_projection);
	invalObjectInView(NULL);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowTopDownView", m_projection?1:0);
}

void WbView3d::OnUpdateViewShowtopdownview(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_projection?1:0);
}

void WbView3d::OnViewShowclouds() 
{
	TheWritableGlobalData->m_useCloudMap = !TheGlobalData->m_useCloudMap;
	AfxGetApp()->WriteProfileInt("GameOptions", "cloudMap", TheGlobalData->m_useCloudMap);
}

void WbView3d::OnUpdateViewShowclouds(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(TheGlobalData->m_useCloudMap?1:0);
}

void WbView3d::OnViewShowmacrotexture() 
{
	Bool show = !TheGlobalData->m_useLightMap;
	TheWritableGlobalData->m_useLightMap = show;
}

void WbView3d::OnUpdateViewShowmacrotexture(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(TheGlobalData->m_useLightMap?1:0);
}

void WbView3d::OnEditSelectmacrotexture() 
{
	SelectMacrotexture dlg;

	// The macrotexture dialog sets the macrotexture in the 3d engine.
	dlg.DoModal();
	
}

void WbView3d::OnViewShowshadows() 
{
	m_showShadows = !m_showShadows;
	
    /**
     * Adriane [Deathscythe] Bug fix
     * We need to apply the setting change first before we check.
     * These values are checked when adding shadows, so they should be updated before that.
     */
	TheWritableGlobalData->m_useShadowDecals = m_showShadows;
	TheWritableGlobalData->m_useShadowVolumes = m_showShadows;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowShadows", m_showShadows?1:0);

	if (m_showShadows) {
		int w,h,bits;
		Bool windowed;
		WW3D::Get_Device_Resolution(w,h,bits,windowed);

		if (bits != 32) {
			::AfxMessageBox("Shadows require a 32 bit color desktop.", IDOK);
			m_showShadows = false;
		} else {
			resetRenderObjects();
			invalObjectInView(NULL);
		}
	} else {
		TheW3DShadowManager->removeAllShadows();
	}

}

void WbView3d::OnUpdateViewShowshadows(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_showShadows?1:0);
}

void WbView3d::OnViewShowSoftWater()
{
	TheWritableGlobalData->m_showSoftWaterEdge = !TheGlobalData->m_showSoftWaterEdge;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//we just turned it on, so recompute shoreline tiles since they may not exist.
		TheTerrainRenderObject->updateShorelineTiles(0,0,WbDoc()->GetHeightMap()->getXExtent()-1,WbDoc()->GetHeightMap()->getYExtent()-1,
			WbDoc()->GetHeightMap());
	}

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", TheGlobalData->m_showSoftWaterEdge ? 1 : 0);
}

void WbView3d::OnUpdateViewShowSoftWater(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_showSoftWaterEdge ? 1 : 0);
}

void WbView3d::OnViewExtraBlends()
{
	if (TheGlobalData->m_use3WayTerrainBlends==1)
		TheWritableGlobalData->m_use3WayTerrainBlends = 2;	//debug mode which draws the tiles in black
	else
		TheWritableGlobalData->m_use3WayTerrainBlends = 1;

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowExtraBlends", TheGlobalData->m_use3WayTerrainBlends > 1 ? 1 : 0);
}

void WbView3d::OnUpdateViewShowExtraBlends(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_use3WayTerrainBlends > 1 ? 1 : 0);
}

void WbView3d::OnEditMapSettings()
{
	MapSettings dlg;

	if (dlg.DoModal() == IDOK) {
		resetRenderObjects();
		invalObjectInView(NULL);
	}
}

void WbView3d::OnClearAllExtraBoundaries()
{
	::MessageBeep(MB_ICONINFORMATION);
	int response = ::AfxMessageBox(
		"You are about to clear all extra boundaries. Are you sure you want to continue? This action cannot be undone.",
		MB_YESNO | MB_ICONQUESTION
	);

	if (response == IDNO){
		return;
	}

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	pDoc->removeAllExtraBoundaries();
    // RemoveAllExtraBoundariesUndoable *pUndo = new RemoveAllExtraBoundariesUndoable(pDoc);
    // pDoc->AddAndDoUndoable(pUndo);
    // REF_PTR_RELEASE(pUndo);
}

void WbView3d::OnEditShadows() 
{
	if (!m_showShadows) {
		OnViewShowshadows(); // turn them on.
	}
	ShadowOptions dlg;
	dlg.DoModal();
}

void WbView3d::OnRefreshSceneObjects() 
{
	resetRenderObjects();
	invalObjectInView(NULL);
}

void WbView3d::OnViewShowModels() 
{
	setShowModels(!getShowModels());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowModels", getShowModels()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
void WbView3d::OnUpdateViewShowModels(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowModels()?1:0);
}

// MLL C&C3
void WbView3d::OnViewBoundingBoxes() 
{
	setShowBoundingBoxes(!getShowBoundingBoxes());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowBoundingBoxes", getShowBoundingBoxes()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
// MLL C&C3
void WbView3d::OnUpdateViewBoundingBoxes(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowBoundingBoxes()?1:0);
}


// MLL C&C3
void WbView3d::OnViewSightRanges() 
{
	setShowSightRanges(!getShowSightRanges());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSightRanges", getShowSightRanges()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
// MLL C&C3
void WbView3d::OnUpdateViewSightRanges(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowSightRanges()?1:0);
}

// MLL C&C3
void WbView3d::OnViewWeaponRanges() 
{
	setShowWeaponRanges(!getShowWeaponRanges());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowWeaponRanges", getShowWeaponRanges()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
// MLL C&C3
void WbView3d::OnUpdateViewWeaponRanges(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowWeaponRanges()?1:0);
}

// MLL C&C3
void WbView3d::OnHighlightTestArt() 
{
	setHighlightTestArt(!getHighlightTestArt());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "HighlightTestArt", getHighlightTestArt()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
// MLL C&C3
void WbView3d::OnUpdateHighlightTestArt(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getHighlightTestArt()?1:0);
}


// MLL C&C3
void WbView3d::OnShowLetterbox() 
{
	setShowLetterbox(!getShowLetterbox());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowLetterBox", getShowLetterbox()?1:0);
}
// MLL C&C3
void WbView3d::OnUpdateShowLetterbox(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowLetterbox()?1:0);
}


void WbView3d::OnViewGarrisoned() 
{
	setShowGarrisoned(!getShowGarrisoned());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowGarrisoned", getShowGarrisoned()?1:0);
	resetRenderObjects();
	invalObjectInView(NULL);
}
void WbView3d::OnUpdateViewGarrisoned(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(getShowGarrisoned()?1:0);
}

void WbView3d::OnViewShowimpassableareas() 
{
	Bool showImpassable = false;
	if (TheTerrainRenderObject) {
		showImpassable = TheTerrainRenderObject->getShowImpassableAreas();
		TheTerrainRenderObject->setShowImpassableAreas(!showImpassable);
		// Force the entire terrain mesh to be rerendered.
		IRegion2D range = {0,0,0,0};
		updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	}
}

void WbView3d::OnUpdateViewShowimpassableareas(CCmdUI* pCmdUI) 
{
	Bool showImpassable = false;
	if (TheTerrainRenderObject) {
		showImpassable = TheTerrainRenderObject->getShowImpassableAreas();
	}
	pCmdUI->SetCheck(showImpassable?1:0);
}

void WbView3d::OnImpassableAreaOptions()
{
	if (TheTerrainRenderObject) {
		{
			ImpassableOptions opts;
			opts.SetDefaultSlopeToShow(TheTerrainRenderObject->getViewImpassableAreaSlope());
			if (opts.DoModal() == IDOK) {
				TheTerrainRenderObject->setViewImpassableAreaSlope(opts.GetSlopeToShow());
			} else {
				TheTerrainRenderObject->setViewImpassableAreaSlope(opts.GetDefaultSlope());
			}
		}
		
		IRegion2D range = {0,0,0,0};
		updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	}
}

void WbView3d::OnViewPartialmapsize96x96() 
{
	m_partialMapSize = 97;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;	
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize96x96(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_partialMapSize == 97?1:0);
}

void WbView3d::OnViewPartialmapsize192x192() 
{
	m_partialMapSize = 192;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;	
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize192x192(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_partialMapSize == 192?1:0);
}

void WbView3d::OnViewPartialmapsize160x160() 
{
	m_partialMapSize = 161;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;	
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize160x160(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_partialMapSize == 161?1:0);
}

void WbView3d::OnViewPartialmapsize128x128() 
{
	m_partialMapSize = 129;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;	
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize128x128(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_partialMapSize == 129?1:0);
}

void WbView3d::OnViewLayersList()
{
	m_showLayersList = !m_showLayersList;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowLayersList", m_showLayersList ? 1 : 0);
	TheLayersList->ShowWindow(m_showLayersList ? SW_SHOW : SW_HIDE);
	if (m_showLayersList) {
		TheLayersList->enableUpdates();
	} else {
		TheLayersList->disableUpdates();
	}
}

void WbView3d::OnUpdateViewLayersList(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showLayersList ? 1 : 0);
}

void WbView3d::OnViewMinimap()
{
	if (TheMinimapDialog)
	{
		Bool visible = TheMinimapDialog->IsWindowVisible();
		TheMinimapDialog->ShowWindow(visible ? SW_HIDE : SW_SHOW);
		::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowMinimap", visible ? 0 : 1);
		if (!visible)
			TheMinimapDialog->rebuildTerrain();
	}
}

void WbView3d::OnUpdateViewMinimap(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->IsWindowVisible() ? 1 : 0);
}

// --- Minimap submenu: Show Objects ------------------------------------------
void WbView3d::OnMinimapShowObjects()
{
	if (TheMinimapDialog)
		TheMinimapDialog->setShowObjects(!TheMinimapDialog->getShowObjects());
}
void WbView3d::OnUpdateMinimapShowObjects(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getShowObjects() ? 1 : 0);
}
void WbView3d::OnMinimapShowRoads()
{
	if (TheMinimapDialog)
		TheMinimapDialog->setShowRoads(!TheMinimapDialog->getShowRoads());
}
void WbView3d::OnUpdateMinimapShowRoads(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getShowRoads() ? 1 : 0);
}
void WbView3d::OnMinimapCullObjects()
{
	if (TheMinimapDialog)
		TheMinimapDialog->setCullObjects(!TheMinimapDialog->getCullObjects());
}
void WbView3d::OnUpdateMinimapCullObjects(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getCullObjects() ? 1 : 0);
}

// --- Minimap menu: Angle Snap Lock (camera rotation -> 45-degree steps) -------
void WbView3d::OnMinimapSnap45()
{
	m_snapCameraAngle45 = !m_snapCameraAngle45;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "SnapCameraAngle45", m_snapCameraAngle45 ? 1 : 0);
	if (!m_projection)
	{
		// Re-derive the camera angle from the raw accumulator: snap to the nearest
		// 45-degree step when enabling (lands on-grid immediately), or restore the
		// free raw angle when disabling.
		m_cameraAngle = m_snapCameraAngle45 ? snapAngleTo45(m_cameraAngleRaw) : m_cameraAngleRaw;
		redraw();
		drawLabels();
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
}
void WbView3d::OnUpdateMinimapSnap45(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(m_snapCameraAngle45 ? 1 : 0);
}

// --- Minimap submenu: Refresh Rate (radio) ----------------------------------
void WbView3d::OnMinimapRefreshOff()  { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(0); }
void WbView3d::OnMinimapRefresh16()   { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(16); }
void WbView3d::OnMinimapRefresh33()   { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(33); }
void WbView3d::OnMinimapRefresh100()  { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(100); }
void WbView3d::OnMinimapRefresh250()  { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(250); }
void WbView3d::OnMinimapRefresh1000() { if (TheMinimapDialog) TheMinimapDialog->setRefreshDelayMs(1000); }
void WbView3d::OnUpdateMinimapRefreshOff(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 0 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRefresh16(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 16 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRefresh33(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 33 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRefresh100(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 100 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRefresh250(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 250 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRefresh1000(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getRefreshDelayMs() == 1000 ? 1 : 0);
}

// --- Minimap submenu: Resolution (radio) ------------------------------------
void WbView3d::OnMinimapRes256()  { if (TheMinimapDialog) TheMinimapDialog->setResolution(256); }
void WbView3d::OnMinimapRes512()  { if (TheMinimapDialog) TheMinimapDialog->setResolution(512); }
void WbView3d::OnMinimapRes1024() { if (TheMinimapDialog) TheMinimapDialog->setResolution(1024); }
void WbView3d::OnMinimapRes2048() { if (TheMinimapDialog) TheMinimapDialog->setResolution(2048); }
void WbView3d::OnUpdateMinimapRes1024(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getResolution() == 1024 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRes256(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getResolution() == 256 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRes512(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getResolution() == 512 ? 1 : 0);
}
void WbView3d::OnUpdateMinimapRes2048(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TheMinimapDialog != NULL);
	pCmdUI->SetCheck(TheMinimapDialog && TheMinimapDialog->getResolution() == 2048 ? 1 : 0);
}

void WbView3d::OnViewShowMapBoundaries()
{
	m_showMapBoundaries = !m_showMapBoundaries;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowMapBoundaries", m_showMapBoundaries ? 1 : 0);
	DrawObject::setDoBoundaryFeedback(m_showMapBoundaries);
}

void WbView3d::OnUpdateViewShowMapBoundaries(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showMapBoundaries ? 1 : 0);
}

void WbView3d::OnViewShowRulerGrid()
{
	m_showRulerGrid = !m_showRulerGrid;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowRulerGrid", m_showRulerGrid ? 1 : 0);
	DrawObject::setDoGridFeedback(m_showRulerGrid);
}

void WbView3d::OnUpdateViewShowRulerGrid(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showRulerGrid ? 1 : 0);
}

void WbView3d::OnViewShowTracingOverlay()
{
	m_showTracingOverlay = !m_showTracingOverlay;

	if (m_showTracingOverlay)
	{
		if(!g_alreadyHintedTraceOverlay){
			AfxMessageBox(
				"This feature is used to overlay a texture on the map -- it needs to be a dds file under your game directory\\data\\editor\\trace_overlay.dds",
				MB_ICONINFORMATION | MB_OK
			);
			g_alreadyHintedTraceOverlay = true;
		}
		
		CFileFind finder;
		BOOL fileExists = finder.FindFile("data\\editor\\trace_overlay.dds");
		if (!fileExists)
		{
			AfxMessageBox(
				"Missing texture:\n\n"
				"data\\editor\\trace_overlay.dds\n\n"
				"The tracing overlay will not be displayed until this texture is restored.\n\n"
				"You little shit, did you not install the worldbuilder properly? the texture file was supposed to be on the zip file - did you delete it?.",
				MB_ICONERROR | MB_OK
			);

			m_showTracingOverlay = 0;

		}
	}

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowTracingOverlay", m_showTracingOverlay ? 1 : 0);
	DrawObject::setDoTracingOverlayFeedback(m_showTracingOverlay);
}

void WbView3d::OnUpdateViewShowTracingOverlay(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showTracingOverlay ? 1 : 0);
}

void WbView3d::OnViewShowAmbientSounds()
{
	m_showAmbientSounds = !m_showAmbientSounds;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowAmbientSounds", m_showAmbientSounds ? 1 : 0);
	DrawObject::setDoAmbientSoundFeedback(m_showAmbientSounds);
}

void WbView3d::OnUpdateViewShowAmbientSounds(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showAmbientSounds ? 1 : 0);
}

void WbView3d::OnViewShowBaseRadius() {
	m_showBaseRadius = !m_showBaseRadius;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowBaseRadius", m_showBaseRadius ? 1 : 0);
	DrawObject::setDoBaseRadiusFeedback(m_showBaseRadius);
}

void WbView3d::OnUpdateViewShowBaseRadius(CCmdUI* pCmdUI) {
	pCmdUI->SetCheck(m_showBaseRadius ? 1 : 0);
}

void WbView3d::OnViewShowSubDraw()
{
	m_showSubDraw = !m_showSubDraw;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSubDraw", m_showSubDraw ? 1 : 0);
	resetRenderObjects();
	invalObjectInView(NULL);
}

void WbView3d::OnUpdateViewShowSubDraw(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showSubDraw ? 1 : 0);
}

void WbView3d::OnViewShowSoundCircles()
{
  m_showSoundCircles = !m_showSoundCircles;
  ::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoundCircles", m_showSoundCircles ? 1 : 0);
  resetRenderObjects();
  invalObjectInView(NULL);
}

void WbView3d::OnUpdateViewShowSoundCircles(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(m_showSoundCircles ? 1 : 0);
}

void WbView3d::OnWindowLODMode1() 
{
    m_lod = 1;
	m_showObjectsSelected = true;
	m_showObjects = false;
	TheWritableGlobalData->m_useLightMap = false;
	TheWritableGlobalData->m_showSoftWaterEdge = false;
	TheWritableGlobalData->m_useShadowDecals = false;
	TheWritableGlobalData->m_useShadowVolumes = false;

	// TheWritableGlobalData->m_textureReductionFactor = 4;
	// if (WW3D::Get_Texture_Reduction() != TheWritableGlobalData->m_textureReductionFactor)
	// {	WW3D::Set_Texture_Reduction(TheWritableGlobalData->m_textureReductionFactor,32);
	// 	// TheGameLODManager->setCurrentTextureReduction(TheWritableGlobalData->m_textureReductionFactor);
	// 	if( TheTerrainRenderObject ) 
  	// 		TheTerrainRenderObject->setTextureLOD( TheWritableGlobalData->m_textureReductionFactor );
	// }
	// ReleaseResources();       // Optional: free current resources first
	// ReAcquireResources();     // Re-load all textures with the new reduction factor

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowShadows", 0);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", 0);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIconsSelected", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIcons", 0);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LODMode", 1);
	resetRenderObjects();
	invalObjectInView(NULL);
}

void WbView3d::OnUpdateOnWindowLODMode1(CCmdUI* pCmdUI) 
{
    pCmdUI->SetCheck(m_lod == 1);
}

void WbView3d::OnWindowLODMode2() 
{
    m_lod = 2; 
	m_showObjectsSelected = true;
	m_showObjects = false;
	TheWritableGlobalData->m_useLightMap = true;
	TheWritableGlobalData->m_showSoftWaterEdge = true;
	TheWritableGlobalData->m_useShadowDecals = true;
	TheWritableGlobalData->m_useShadowVolumes = true;

	// TheWritableGlobalData->m_textureReductionFactor = 1;
	// if (WW3D::Get_Texture_Reduction() != TheWritableGlobalData->m_textureReductionFactor)
	// {	WW3D::Set_Texture_Reduction(TheWritableGlobalData->m_textureReductionFactor,32);
	// 	// TheGameLODManager->setCurrentTextureReduction(TheWritableGlobalData->m_textureReductionFactor);
	// 	if( TheTerrainRenderObject ) 
  	// 		TheTerrainRenderObject->setTextureLOD( TheWritableGlobalData->m_textureReductionFactor );
	// }
	// ReleaseResources();       // Optional: free current resources first
	// ReAcquireResources();     // Re-load all textures with the new reduction factor

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowShadows", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIconsSelected", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIcons", 0);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LODMode", 2);
	resetRenderObjects();
	invalObjectInView(NULL);
}

void WbView3d::OnUpdateOnWindowLODMode2(CCmdUI* pCmdUI) 
{
    pCmdUI->SetCheck(m_lod == 2);
}

void WbView3d::OnWindowLODMode3() 
{
    m_lod = 3; 
	TheWritableGlobalData->m_useLightMap = true;
	TheWritableGlobalData->m_showSoftWaterEdge = true;
	TheWritableGlobalData->m_useShadowDecals = true;
	TheWritableGlobalData->m_useShadowVolumes = true;
	// m_showObjectsSelected = false;
	// m_showObjects = true;
	// TheWritableGlobalData->m_textureReductionFactor = 0;
	// if (WW3D::Get_Texture_Reduction() != TheWritableGlobalData->m_textureReductionFactor)
	// {	WW3D::Set_Texture_Reduction(TheWritableGlobalData->m_textureReductionFactor,32);
	// 	// TheGameLODManager->setCurrentTextureReduction(TheWritableGlobalData->m_textureReductionFactor);
	// 	if( TheTerrainRenderObject ) 
  	// 		TheTerrainRenderObject->setTextureLOD( TheWritableGlobalData->m_textureReductionFactor );
	// }
	// ReleaseResources();       // Optional: free current resources first
	// ReAcquireResources();     // Re-load all textures with the new reduction factor
	
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowShadows", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", 1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIconsSelected", 0);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowObjectIcons",1);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LODMode", 3);
	resetRenderObjects();
	invalObjectInView(NULL);
}

void WbView3d::OnUpdateOnWindowLODMode3(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_lod == 3);
}

void WbView3d::setMSAA(D3DMULTISAMPLE_TYPE type)
{
	DX8Wrapper::Set_Multi_Sample_Type(type);
	DX8Wrapper::Reset_Device(true);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "MSAAMode", (int)type);
}

void WbView3d::OnMSAANone() { setMSAA(D3DMULTISAMPLE_NONE); }
void WbView3d::OnMSAA2X()   { setMSAA(D3DMULTISAMPLE_2_SAMPLES); }
void WbView3d::OnMSAA4X()   { setMSAA(D3DMULTISAMPLE_4_SAMPLES); }
void WbView3d::OnMSAA8X()   { setMSAA(D3DMULTISAMPLE_8_SAMPLES); }

void WbView3d::OnUpdateMSAANone(CCmdUI* pCmdUI) { pCmdUI->SetCheck(DX8Wrapper::Get_Multi_Sample_Type() == D3DMULTISAMPLE_NONE); }
void WbView3d::OnUpdateMSAA2X(CCmdUI* pCmdUI)   { pCmdUI->SetCheck(DX8Wrapper::Get_Multi_Sample_Type() == D3DMULTISAMPLE_2_SAMPLES); }
void WbView3d::OnUpdateMSAA4X(CCmdUI* pCmdUI)   { pCmdUI->SetCheck(DX8Wrapper::Get_Multi_Sample_Type() == D3DMULTISAMPLE_4_SAMPLES); }
void WbView3d::OnUpdateMSAA8X(CCmdUI* pCmdUI)   { pCmdUI->SetCheck(DX8Wrapper::Get_Multi_Sample_Type() == D3DMULTISAMPLE_8_SAMPLES); }

void WbView3d::setTextureFilter(int mode)
{
	if (mode == 1) {
		TextureFilterClass::Set_Max_Anisotropy(16);
		WW3D::Set_Texture_Filter(TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC);
	} else {
		TextureFilterClass::Set_Max_Anisotropy(2);
		WW3D::Set_Texture_Filter(TextureFilterClass::TEXTURE_FILTER_BILINEAR);
	}
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "TexFilterMode", mode);
}

void WbView3d::OnTexFilterDefault()  { setTextureFilter(0); }
void WbView3d::OnTexFilterAniso16X() { setTextureFilter(1); }

void WbView3d::OnUpdateTexFilterDefault(CCmdUI* pCmdUI)  { pCmdUI->SetCheck(WW3D::Get_Texture_Filter() != TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC); }
void WbView3d::OnUpdateTexFilterAniso16X(CCmdUI* pCmdUI) { pCmdUI->SetCheck(WW3D::Get_Texture_Filter() == TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC); }

void WbView3d::OnTextShadow()
{
	m_textShadow = !m_textShadow;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "TextShadow", m_textShadow ? 1 : 0);
	Invalidate();
}

void WbView3d::OnUpdateTextShadow(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_textShadow);
}

// ----------------------------------------------------------------------------
// (Re)build the viewport label glyph atlas, honoring the antialias setting.
// Viewport labels are rendered as textured quads from this atlas (see render()),
// so the text is part of the presented D3D frame and never flickers. Safe to call
// again at runtime to apply the AA toggle (build() releases the old atlas).
// m3DFont is left NULL (the legacy D3DX text path is retired).
void WbView3d::createLabelFont()
{
	if (m3DFont) {
		((ID3DXFont*)m3DFont)->Release();
		m3DFont = NULL;
	}

	IDirect3DDevice8* pDev = DX8Wrapper::_Get_D3D_Device8();
	if (!pDev)
		return;

	LOGFONT logFont;
	logFont.lfHeight = 20;
	logFont.lfWidth = 0;
	logFont.lfEscapement = 0;
	logFont.lfOrientation = 0;
	logFont.lfWeight = FW_REGULAR;
	logFont.lfItalic = FALSE;
	logFont.lfUnderline = FALSE;
	logFont.lfStrikeOut = FALSE;
	logFont.lfCharSet = ANSI_CHARSET;
	logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality = m_textAntialias ? ANTIALIASED_QUALITY : DEFAULT_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH;
	strcpy(logFont.lfFaceName, "Arial");

	HFONT hFont = CreateFontIndirect(&logFont);
	if (hFont) {
		D3DXCreateFont(pDev, hFont, &m3DFont);
		DeleteObject(hFont);
	}
}

void WbView3d::OnTextAntialias()
{
	m_textAntialias = !m_textAntialias;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "TextAntialias", m_textAntialias ? 1 : 0);
	createLabelFont();		// rebuild the font with the new quality
	Invalidate();
}

void WbView3d::OnUpdateTextAntialias(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_textAntialias);
}

// Label anchor mode: Default (ground) vs New (object center-height). Presented as a
// radio pair under Text Rendering. Clearing m_haveLabelCache forces an immediate rebuild.
void WbView3d::OnTextAnchorDefault()
{
	m_labelAnchorMode = 0;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LabelAnchorMode", 0);
	m_haveLabelCache = false;
	Invalidate();
}

void WbView3d::OnUpdateTextAnchorDefault(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_labelAnchorMode == 0);
}

void WbView3d::OnTextAnchorNew()
{
	m_labelAnchorMode = 1;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LabelAnchorMode", 1);
	m_haveLabelCache = false;
	Invalidate();
}

void WbView3d::OnUpdateTextAnchorNew(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_labelAnchorMode == 1);
}

// Label renderer: Old (D3DX m3DFont, drawn inside the D3D frame -> no flicker) vs
// New (raw GDI ::TextOut on the window HDC -> sharper but strobes, because D3D8 has no
// way to draw GDI into the presented frame). Presented as a radio pair under Text
// Rendering. Clearing m_haveLabelCache forces an immediate rebuild.
void WbView3d::OnTextRendererOld()
{
	m_labelRenderer = 0;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LabelRenderer", 0);
	m_haveLabelCache = false;
	Invalidate();
}

void WbView3d::OnUpdateTextRendererOld(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_labelRenderer == 0);
}

void WbView3d::OnTextRendererNew()
{
	m_labelRenderer = 1;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "LabelRenderer", 1);
	m_haveLabelCache = false;
	Invalidate();
}

void WbView3d::OnUpdateTextRendererNew(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_labelRenderer == 1);
}

void WbView3d::OnKillFocus(CWnd* pNewWnd)
{
	if (CMainFrame::GetMainFrame() && !CMainFrame::GetMainFrame()->isFocusedOnScripting()) {
		pauseEditTimer();
	}

    WbView::OnKillFocus(pNewWnd);
}

void WbView3d::OnSetFocus(CWnd* pOldWnd)
{
    startEditTimer();
    WbView::OnSetFocus(pOldWnd);
}