// WBParticleRuntime.cpp -- see WBParticleRuntime.h.
#include "StdAfx.h"
#include "WBParticleRuntime.h"

#include <map>
#include <vector>

#include "Common/GlobalData.h"
#include "Common/ThingTemplate.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Dict.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/GameClient.h"
#include "GameClient/ParticleSys.h"
#include "Common/GameLOD.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "assetmgr.h"
#include "rendobj.h"		// RenderObjClass::Get_Bone_Transform
#include "matrix3d.h"
#include "Common/MapObject.h"

namespace
{
	// ------------------------------------------------------------------------------------------
	// Frame-clock shims. ParticleSystemManager::update() early-returns unless
	// TheGameLogic->getFrame() ADVANCES each tick, and Particle::update() reads
	// TheGameClient->getFrame() for keyframe timing. WB has neither global, so we assign these
	// tiny subclasses whose getFrame() returns a counter we bump once per WB frame. No real
	// simulation is stood up -- init() is never called, so no Display/UI/input is created.
	// ------------------------------------------------------------------------------------------
	// The shim logic frame. It must ADVANCE for the manager to tick, and particle aging is
	// frame-counted -- so if we bumped it once per repaint, particles would animate faster
	// whenever WB repaints more often (e.g. while the cursor moves). Instead we derive it from
	// wall-clock at the game's fixed logic rate, so a "frame" is a fixed slice of real time
	// regardless of how often the viewport actually redraws.
	UnsignedInt s_wbFrame = 1;			// starts non-zero so the first tick differs from the guard's 0
	unsigned long s_wbStartTick = 0;	// GetTickCount() at enable; 0 until set
	const UnsignedInt WB_LOGIC_FPS = 30;	// == the game's LOGICFRAMES_PER_SECOND

	// Editor particle budget when [ObjectOptionPanel] MaxParticleCount isn't set. Much higher than
	// the game's 2500 (the editor previews whole scenes and isn't framerate-critical); the LOD
	// limiter still culls once this is exceeded, so it also bounds worst-case cost.
	const int WB_DEFAULT_MAX_PARTICLES = 20000;

	// Previewed GAME particle templates trip data-quality DEBUG_ASSERTCRASHes that the retail
	// build compiles out (e.g. "A particle has an infinite lifetime" for a 0-lifetime particle).
	// Those aren't WB bugs and mustn't crash the editor, so we ignore asserts ONLY across the
	// particle create/tick calls, restoring the user's setting immediately after. Guarded on
	// DEBUG_CRASHING because m_debugIgnoreAsserts only exists there (retail: this is a no-op).
	struct AssertQuiet
	{
#ifdef DEBUG_CRASHING
		Bool saved;
		AssertQuiet()
		{
			saved = (TheWritableGlobalData != NULL) ? TheWritableGlobalData->m_debugIgnoreAsserts : FALSE;
			if (TheWritableGlobalData != NULL)
			{
				TheWritableGlobalData->m_debugIgnoreAsserts = TRUE;
			}
		}
		~AssertQuiet()
		{
			if (TheWritableGlobalData != NULL)
			{
				TheWritableGlobalData->m_debugIgnoreAsserts = saved;
			}
		}
#endif
	};

	class WBParticleGameLogic : public GameLogic
	{
	public:
		WBParticleGameLogic() : GameLogic() {}
		virtual UnsignedInt getFrame( void ) { return s_wbFrame; }
	};

	class WBParticleGameClient : public GameClient
	{
	public:
		WBParticleGameClient() : GameClient() {}
		virtual UnsignedInt getFrame( void ) { return s_wbFrame; }

		// --- pure-virtual stubs (never reached: we never call init(), never create drawables) ---
		virtual void createRayEffectByTemplate( const Coord3D *, const Coord3D *, const ThingTemplate * ) {}
		virtual void addScorch( const Coord3D *, Real, Scorches ) {}
		virtual Drawable *friend_createDrawable( const ThingTemplate *, DrawableStatus = DRAWABLE_STATUS_NONE ) { return NULL; }
		virtual void setTeamColor( Int, Int, Int ) {}
		virtual void adjustLOD( Int ) {}
		virtual void notifyTerrainObjectMoved( Object * ) {}

	private:
		virtual Display *createGameDisplay( void ) { return NULL; }
		virtual InGameUI *createInGameUI( void ) { return NULL; }
		virtual GameWindowManager *createWindowManager( void ) { return NULL; }
		virtual FontLibrary *createFontLibrary( void ) { return NULL; }
		virtual DisplayStringManager *createDisplayStringManager( void ) { return NULL; }
		virtual VideoPlayerInterface *createVideoPlayer( void ) { return NULL; }
		virtual TerrainVisual *createTerrainVisual( void ) { return NULL; }
		virtual Keyboard *createKeyboard( void ) { return NULL; }
		virtual Mouse *createMouse( void ) { return NULL; }
		virtual SnowManager *createSnowManager( void ) { return NULL; }
		virtual void setFrameRate( Real ) {}
	};

	// A ground-emit particle system reads TheTerrainLogic->getGroundHeight() during update(). WB
	// never creates TheTerrainLogic (NULL -> crash). getGroundHeight is virtual and TerrainLogic
	// has a public ctor with no pure virtuals of its own, so a tiny subclass overriding just that
	// getter -- backed by WB's live heightmap (TheTerrainRenderObject) -- is a clean shim. We
	// never call init()/update(), and never delete it (same lifetime rule as the other shims).
	class WBParticleTerrainLogic : public TerrainLogic
	{
	public:
		WBParticleTerrainLogic() : TerrainLogic() {}
		virtual Real getGroundHeight( Real x, Real y, Coord3D *normal = NULL ) const
		{
			if (TheTerrainRenderObject != NULL)
			{
				return TheTerrainRenderObject->getHeightMapHeight( x, y, normal );
			}
			return 0.0f;
		}
		// The wave editor's render path calls TheTerrainLogic->isUnderwater() whenever a
		// TheTerrainLogic exists; the base implementation walks polygon-trigger / water-grid state
		// WB never fully stands up, so it crashes here. WB was designed to run that path with
		// TheTerrainLogic == NULL (it then seats waves via the wave system's own water-height hook),
		// but installing this shim for particles makes TheTerrainLogic non-NULL and reroutes the
		// wave editor into the crashing base path. Override it to report the flat global water level
		// (the same fallback the wave renderer uses when no editor water hook is set), which is
		// crash-free and needs no game water state. Per-area water seating is unaffected when
		// particles are off (TheTerrainLogic stays NULL and the editor hook is used as before).
		virtual Bool isUnderwater( Real x, Real y, Real *waterZ = NULL, Real *terrainZ = NULL )
		{
			const Real wh = TheGlobalData->m_waterPositionZ;
			const Real gh = getGroundHeight( x, y );
			if (waterZ != NULL)
			{
				*waterZ = wh;
			}
			if (terrainZ != NULL)
			{
				*terrainZ = gh;
			}
			return gh < wh;
		}
	};

	// ------------------------------------------------------------------------------------------
	// Runtime state.
	// ------------------------------------------------------------------------------------------
	bool s_enabled = false;
	WBParticleTerrainLogic *s_wbTerrain = NULL;	// installed into TheTerrainLogic across update() only

	// Created once on first enable, then kept for the process life (their base destructors assume
	// init() ran, so we must never delete them). Installed into TheGameLogic/TheGameClient and
	// left there.
	WBParticleGameLogic  *s_wbLogic  = NULL;
	WBParticleGameClient *s_wbClient = NULL;
	Bool s_savedUseFX = FALSE;
	Int  s_savedMaxParticleCount = 0;
	W3DAssetManager *s_savedDisplayAssetMgr = NULL;	// restore W3DDisplay::m_assetManager on disable

	// Publishes the TerrainLogic shim into the global for the lifetime of the scope, then puts
	// back whatever was there (normally NULL). Only ParticleSystem::update() needs it; leaving it
	// installed any longer makes shared engine code think the game is running -- see the comment
	// at the use site in tick(). Restores rather than blindly NULLing so this stays correct if WB
	// ever grows a real TheTerrainLogic.
	struct ScopedTerrainLogic
	{
		TerrainLogic *m_saved;
		ScopedTerrainLogic() : m_saved(TheTerrainLogic)
		{
			if (s_wbTerrain == NULL)
			{
				s_wbTerrain = new WBParticleTerrainLogic();
			}
			TheTerrainLogic = s_wbTerrain;
		}
		~ScopedTerrainLogic()
		{
			TheTerrainLogic = m_saved;
		}
	};

	// The live emitter IDs created per placed object, for teardown.
	typedef std::vector<ParticleSystemID> IDList;
	typedef std::map<MapObject *, IDList> EmitterMap;
	EmitterMap s_emitters;

	// The bone Z rotation each tracked emitter was SPAWNED with, index-aligned with s_emitters.
	// rotateLocalTransformZ is relative with no absolute setter, so an in-place move cannot fix a
	// changed orientation; this lets tryRepositionInPlace detect a rotation and force a respawn.
	typedef std::vector<Real> RotationList;
	typedef std::map<MapObject *, RotationList> RotationMap;
	RotationMap s_spawnRotations;

	// Destroy the manager's systems for one tracked ID list (the manager isn't in m_scene, so it
	// isn't auto-cleaned). Guards on the manager being present.
	void destroyIDList( const IDList &ids )
	{
		if (TheParticleSystemManager == NULL)
		{
			return;
		}
		for (size_t i = 0; i < ids.size(); ++i)
		{
			TheParticleSystemManager->destroyParticleSystemByID( ids[i] );
		}
	}

	// Spawn one emitter from a template at a world position; record its ID under obj. rotationZ is
	// the bone's Z rotation, so a directional system (a spout, an exhaust) points the way the bone
	// does -- the engine does the same via rotateLocalTransformZ after setPosition.
	void spawnEmitter( MapObject *obj, const ParticleSystemTemplate *tmpl, const Coord3D &pos,
		Real rotationZ )
	{
		if (tmpl == NULL || TheParticleSystemManager == NULL)
		{
			return;
		}
		AssertQuiet quiet;	// previewed templates may trip data-quality asserts
		ParticleSystem *sys = TheParticleSystemManager->createParticleSystem( tmpl );
		if (sys == NULL)
		{
			return;
		}
		sys->setPosition( &pos );
		if (rotationZ != 0.0f)
		{
			sys->rotateLocalTransformZ( rotationZ );
		}
		s_emitters[obj].push_back( sys->getSystemID() );
		s_spawnRotations[obj].push_back( rotationZ );
	}

	// Collect the particle-system templates a ThingTemplate shows in the condition state WB is
	// DRAWING. Emitters are declared per ModelConditionInfo, so the set depends on the state -- and
	// the state WB draws is not necessarily the default one: the viewport applies NIGHT, SNOW,
	// GARRISONED and the object's damage level (see WbView3d::getModelNameAndScale). Resolving from
	// a hardcoded empty flag set instead meant reading the DEFAULT state's emitters while the
	// viewport drew a different state's model, so a building whose chimney smoke is declared on its
	// NIGHT / GARRISONED / damaged states (very common -- the pristine daytime state often has the
	// ParticleSysBone lines commented out) showed no FX at all.
	// One attached emitter: its template + the bone it hangs off (empty = object origin).
	struct AttachedEmitter
	{
		const ParticleSystemTemplate *tmpl;
		AsciiString bone;
	};

	// createEmittersForObject runs per placed object on every render rebuild (invalObjectInView(NULL)
	// walks every object on ~20 events), so compute a state's set once and cache it -- repeat
	// instances become a map lookup instead of a fresh bone walk + vector build.
	//
	// Keyed on the RESOLVED ModelConditionInfo rather than the ThingTemplate + flags: findBestInfo
	// collapses many flag combinations onto the same state, so this both keys correctly per state
	// and hits more often than caching per flag set would. (ModelConditionFlags has no operator<,
	// so it cannot key a std::map anyway.)
	//
	// NOT immutable for the process life: a map.ini load appends an override to a template's chain
	// whose states are new objects, so the cache has to be dropped when the data changes -- see
	// clearTemplateCache, called from the map.ini refresh. A stale entry is only wasted memory,
	// never a wrong answer, since the new state is a distinct pointer.
	typedef std::vector<AttachedEmitter> AttachedList;
	typedef std::map<const ModelConditionInfo *, AttachedList> AttachedCache;
	AttachedCache s_attachedCache;

	// The emitters declared on one resolved condition state (cached).
	const AttachedList &collectStateEmitters( const ModelConditionInfo *state )
	{
		static const AttachedList empty;
		if (state == NULL)
		{
			return empty;
		}
		AttachedCache::iterator cached = s_attachedCache.find( state );
		if (cached != s_attachedCache.end())
		{
			return cached->second;
		}

		AttachedList &out = s_attachedCache[state];
		for (size_t b = 0; b < state->m_particleSysBones.size(); ++b)
		{
			if (state->m_particleSysBones[b].particleSystemTemplate != NULL)
			{
				AttachedEmitter e;
				e.tmpl = state->m_particleSysBones[b].particleSystemTemplate;
				e.bone = state->m_particleSysBones[b].boneName;
				out.push_back( e );
			}
		}
		return out;
	}

	// Append the emitters this template shows in condition state `flags`, resolving each draw
	// module's state the way the engine does (findBestInfo, as W3DModelDraw's ctor does) rather
	// than indexing m_conditionStates[0] -- a DefaultConditionState is not necessarily the first
	// entry, and taking the wrong one means reading another state's emitters (or none).
	void collectEmittersForState( const ThingTemplate *tt, const ModelConditionFlags &flags,
		AttachedList &out )
	{
		if (tt == NULL)
		{
			return;
		}
		const ModuleInfo &draws = tt->getDrawModuleInfo();
		for (int i = 0; i < draws.getCount(); ++i)
		{
			const W3DModelDrawModuleData *md = draws.getNthData( i )->getAsW3DModelDrawModuleData();
			if (md == NULL || md->m_conditionStates.empty())
			{
				continue;
			}
			const ModelConditionInfo *base = md->findBestInfo( flags );
			if (base == NULL)
			{
				continue;
			}
			const AttachedList &mine = collectStateEmitters( base );
			for (size_t b = 0; b < mine.size(); ++b)
			{
				out.push_back( mine[b] );
			}
		}
	}

	// One emitter to place: its template + the world position it should sit at. Both the initial
	// spawn (createEmittersForObject) and the drag-move reposition build this SAME ordered list from
	// one source, so the systems recorded per object stay index-aligned with it.
	struct Placement
	{
		const ParticleSystemTemplate *tmpl;
		Coord3D pos;
		Real rotationZ;		// bone's Z rotation (0 = unoriented / object origin)
	};

	// Build the ordered emitter placement list for an object: (a) its standalone particle-system
	// marker (if any), then (b) each attached emitter at its bone world position. renderObj
	// (may be NULL) is the positioned render object attached emitters read their bone transform from;
	// origin is the terrain-adjusted object origin used for the marker and for unresolved bones.
	// flags is the condition state the VIEWPORT is drawing, so the emitters match the model shown.
	void computeEmitterPlacements( MapObject *obj, RenderObjClass *renderObj, const Coord3D &origin,
		const ModelConditionFlags &flags, std::vector<Placement> &out )
	{
		// (a) Standalone placed particle system: a marker whose dict names a particle template.
		Dict *props = obj->getProperties();
		if (props != NULL)
		{
			static const NameKeyType key = TheNameKeyGenerator->nameToKey( "particleSystemName" );
			Bool exists = FALSE;
			AsciiString name = props->getAsciiString( key, &exists );
			if (exists && !name.isEmpty() && TheParticleSystemManager != NULL)
			{
				const ParticleSystemTemplate *tmpl = TheParticleSystemManager->findTemplate( name );
				if (tmpl != NULL)
				{
					Placement p;
					p.tmpl = tmpl;
					p.pos = origin;
					p.rotationZ = 0.0f;	// standalone marker: no bone to take an orientation from
					out.push_back( p );
				}
			}
		}

		// (b) Emitters attached to the draw state the viewport is showing, placed at their bone.
		// Get_Bone_Transform gives the bone's WORLD matrix (the render obj is already positioned).
		//
		// Resolve the name through Get_Bone_Index FIRST and require a non-zero result, exactly as
		// the engine does (W3DModelDraw's recalc of bone particle systems). HTreeClass returns 0
		// for "no such bone", which is ALSO the valid index of the root bone -- so the by-name
		// Get_Bone_Transform overload cannot tell the two apart and quietly hands back the root
		// bone's transform. That put emitters for a mistyped/missing bone on the model's root
		// instead of at the object origin, which reads as "one object's FX are off" while every
		// object whose bones all resolve looks correct.
		AttachedList attached;
		collectEmittersForState( obj->getThingTemplate(), flags, attached );
		for (size_t i = 0; i < attached.size(); ++i)
		{
			Placement p;
			p.tmpl = attached[i].tmpl;
			p.pos = origin;
			p.rotationZ = 0.0f;
			if (renderObj != NULL && !attached[i].bone.isEmpty() && !attached[i].bone.isNone())
			{
				const int boneIndex = renderObj->Get_Bone_Index( attached[i].bone.str() );
				if (boneIndex != 0)
				{
					const Matrix3D &bx = renderObj->Get_Bone_Transform( boneIndex );
					Vector3 t = bx.Get_Translation();
					p.pos.x = t.X;
					p.pos.y = t.Y;
					p.pos.z = t.Z;
					// Orient the system like the bone, as the engine does. Without this a
					// directional emitter always fired along +X no matter how the object was
					// rotated, so it only lined up at one angle -- the rest of the time the FX
					// pointed the wrong way relative to the building.
					p.rotationZ = bx.Get_Z_Rotation();
				}
			}
			out.push_back( p );
		}
	}

	// Try to MOVE this object's existing emitters to the given placements instead of respawning.
	// Succeeds only when the tracked set is present, matches the new placement count, and every
	// tracked system is still alive -- a finite-lifetime emitter that expired mid-drag has been
	// removed by the manager, so its ID is stale and repositioning can't bring it back (that would
	// leave it dead after the drag). On any of those, returns false so the caller does a full
	// rebuild that respawns the whole set. Placement order matches spawn order, so IDs align.
	bool tryRepositionInPlace( MapObject *obj, const std::vector<Placement> &places )
	{
		if (TheParticleSystemManager == NULL)
		{
			return false;
		}
		EmitterMap::iterator it = s_emitters.find( obj );
		if (it == s_emitters.end() || places.empty() || it->second.size() != places.size())
		{
			return false;
		}

		// Only a MOVE can be done in place. ParticleSystem exposes rotateLocalTransformZ (a
		// RELATIVE rotate) with no absolute setter, so a system whose bone orientation changed
		// cannot be corrected here -- re-applying would compound onto the rotation it already
		// carries. Rotating the object changes its bones' Z rotation, so bail out and let the
		// caller respawn the whole set with the new orientation baked in.
		RotationMap::iterator rot = s_spawnRotations.find( obj );
		if (rot == s_spawnRotations.end() || rot->second.size() != places.size())
		{
			return false;
		}
		for (size_t r = 0; r < places.size(); ++r)
		{
			if (places[r].rotationZ != rot->second[r])
			{
				return false;
			}
		}

		// Resolve every tracked system first: only move once we know the whole set is alive (a
		// partial move into a stale set can't be aligned). findParticleSystem is an O(N) scan, so
		// cache the resolved pointers here rather than looking each up twice.
		std::vector<ParticleSystem *> live;
		live.reserve( places.size() );
		for (size_t i = 0; i < it->second.size(); ++i)
		{
			ParticleSystem *sys = TheParticleSystemManager->findParticleSystem( it->second[i] );
			if (sys == NULL || sys->isDestroyed())
			{
				return false;
			}
			live.push_back( sys );
		}

		for (size_t i = 0; i < live.size(); ++i)
		{
			live[i]->setPosition( &places[i].pos );
		}
		return true;
	}
}

namespace WBParticleRuntime
{

bool isEnabled()
{
	return s_enabled;
}

void setEnabled(bool on)
{
	if (on == s_enabled)
	{
		return;
	}

	if (on)
	{
		if (TheParticleSystemManager == NULL)
		{
			return;	// no particle subsystem -- can't enable
		}
		// Install the frame-clock shims. These are created ONCE and never destroyed: the base
		// GameClient/GameLogic destructors assume init() ran (they tear down subsystems we never
		// created), so deleting a never-init'd instance is a NULL-deref crash. WB has no real
		// GameClient/GameLogic, so leaking two tiny frame-counter objects for the session is the
		// safe choice. On re-enable we just re-point the globals at the surviving shims.
		if (s_wbLogic == NULL)
		{
			s_wbLogic = new WBParticleGameLogic();
		}
		if (s_wbClient == NULL)
		{
			s_wbClient = new WBParticleGameClient();
		}
		TheGameLogic  = s_wbLogic;
		TheGameClient = s_wbClient;

		// ParticleSystem::createParticle() (the emit path) dereferences TheGameLODManager for its
		// LOD skip/priority checks. WB never creates it (NULL -> crash on the first emitted
		// particle). Stand up a real one. We must NOT call init() -- it loads INIs and runs
		// hardware benchmarks -- but createParticle only reads the dynamic-LOD skip mask +
		// min-priority fields, which the ctor leaves unset. applyDynamicLODLevel() copies them
		// from m_dynamicGameLODInfo[level] (an inline array whose elements default-construct to
		// PARTICLE_PRIORITY_LOWEST / skip-mask 0 -- valid without init()). The ctor already sets
		// m_currentDynamicLOD = HIGH so setDynamicLODLevel(HIGH) would early-return; step through
		// LOW first to force the apply. Net effect: nothing is LOD-culled (full-detail preview).
		// Kept for the process life (no init() means the dtor path is unsafe, like the shims).
		if (TheGameLODManager == NULL)
		{
			TheGameLODManager = new GameLODManager();
			TheGameLODManager->setDynamicLODLevel(DYNAMIC_GAME_LOD_LOW);
			TheGameLODManager->setDynamicLODLevel(DYNAMIC_GAME_LOD_HIGH);
		}

		// NOTE: the TerrainLogic shim is deliberately NOT installed here. It goes in only around
		// ParticleSystem::update() via ScopedTerrainLogic -- see the comment in tick().

		// The particle DRAW path (W3DParticleSystemManager::doParticles) reads the static
		// W3DDisplay::m_assetManager to fetch sprite textures -- WB never sets that static (it uses
		// its own WbView3d::m_assetManager, reachable via WW3DAssetManager::Get_Instance()). Point
		// the static at the live instance so the draw can resolve textures. Restored on disable.
		s_savedDisplayAssetMgr = W3DDisplay::m_assetManager;
		W3DDisplay::m_assetManager = (W3DAssetManager *)WW3DAssetManager::Get_Instance();

		// update() hard-gates on m_useFX; force it on while we preview. Also give a real particle
		// budget: WB never runs GameLODManager::init(), so m_maxParticleCount is still its 0
		// default -- and createParticle() treats 0 as "over budget on EVERY particle", so it
		// thrashes removeOldestParticles() and rejects new particles (the choking + skipping).
		// The editor isn't framerate-critical like the game, so use a much larger cap than the
		// game's 2500 default, and let it be tuned via [ObjectOptionPanel] MaxParticleCount in
		// WorldBuilder.ini (0/absent -> the default below).
		if (TheWritableGlobalData != NULL)
		{
			s_savedUseFX = TheWritableGlobalData->m_useFX;
			TheWritableGlobalData->m_useFX = TRUE;
			s_savedMaxParticleCount = TheWritableGlobalData->m_maxParticleCount;
			int cap = ::AfxGetApp()->GetProfileInt("ObjectOptionPanel", "MaxParticleCount", 0);
			if (cap <= 0)
			{
				cap = WB_DEFAULT_MAX_PARTICLES;
			}
			TheWritableGlobalData->m_maxParticleCount = cap;
		}
		s_wbStartTick = 0;	// (re)start the wall-clock frame counter on first tick
		s_enabled = true;
	}
	else
	{
		destroyAllEmitters();
		s_enabled = false;

		// Leave the GameLogic/GameClient shims installed. They are harmless (only serve
		// getFrame()), and any particle still draining during the next WW3D flush would read
		// TheGameClient -- so un-pointing it to NULL here could crash the draw path. WB had no
		// GameClient/GameLogic of its own, so keeping those in place changes nothing else.
		// tick() is gated on s_enabled, so the manager stops advancing once disabled.
		// (TheTerrainLogic needs no teardown here -- ScopedTerrainLogic already removed it the
		// moment update() returned, so it is never installed outside that call.)
		if (TheWritableGlobalData != NULL)
		{
			TheWritableGlobalData->m_useFX = s_savedUseFX;
			TheWritableGlobalData->m_maxParticleCount = s_savedMaxParticleCount;
		}
		W3DDisplay::m_assetManager = s_savedDisplayAssetMgr;	// restore (normally NULL)
	}
}

void placeEmittersForObject(MapObject *obj, RenderObjClass *renderObj,
	float worldX, float worldY, float worldZ, const ModelConditionFlags *flags)
{
	if (!s_enabled || obj == NULL)
	{
		return;
	}

	Coord3D origin;		// the object origin: standalone-marker position + bone fallback
	origin.x = worldX;
	origin.y = worldY;
	origin.z = worldZ;

	// No state supplied (a caller with no model, so nothing to match): fall back to the default
	// state, which is what a model-less FX marker declares its emitters on anyway.
	ModelConditionFlags none;
	none.clear();

	std::vector<Placement> places;
	computeEmitterPlacements( obj, renderObj, origin, (flags != NULL) ? *flags : none, places );

	// If this object already has a live emitter set that still matches its template, MOVE the
	// systems in place instead of respawning -- so a drag-move keeps its in-flight particles and
	// doesn't visibly reset every mouse-tick. Otherwise (first placement, changed set, or any
	// tracked system expired) fall through to a full rebuild. Deciding here keeps callers from
	// having to know the move-vs-rebuild policy.
	if (tryRepositionInPlace( obj, places ))
	{
		return;
	}

	// Full rebuild: drop any stale emitters and respawn the whole set fresh.
	destroyEmittersForObject( obj );
	for (size_t i = 0; i < places.size(); ++i)
	{
		spawnEmitter( obj, places[i].tmpl, places[i].pos, places[i].rotationZ );
	}
}

void clearTemplateCache()
{
	// Called when the loaded game data changes (a map.ini load), which replaces the condition
	// states the cache is keyed on. Cheap: the sets are rebuilt lazily on the next placement.
	s_attachedCache.clear();
}

void destroyEmittersForObject(MapObject *obj)
{
	EmitterMap::iterator it = s_emitters.find( obj );
	if (it == s_emitters.end())
	{
		return;
	}
	destroyIDList( it->second );
	s_emitters.erase( it );
	s_spawnRotations.erase( obj );
}

void destroyAllEmitters()
{
	for (EmitterMap::iterator it = s_emitters.begin(); it != s_emitters.end(); ++it)
	{
		destroyIDList( it->second );
	}
	s_emitters.clear();
	s_spawnRotations.clear();
}

void tick()
{
	if (!s_enabled || TheParticleSystemManager == NULL)
	{
		return;
	}
	// Advance the logic frame by REAL elapsed time (30 logic fps), not once per repaint, so the
	// animation rate is independent of how often WB redraws (cursor movement used to speed it up).
	if (s_wbStartTick == 0)
	{
		s_wbStartTick = ::GetTickCount();
	}
	unsigned long elapsedMs = ::GetTickCount() - s_wbStartTick;
	UnsignedInt want = 1 + (UnsignedInt)((unsigned long long)elapsedMs * WB_LOGIC_FPS / 1000UL);
	if (want <= s_wbFrame)
	{
		return;	// same logic frame -- don't re-tick (also what the manager's own guard enforces)
	}
	s_wbFrame = want;
	AssertQuiet quiet;	// per-particle aging can trip data-quality asserts on game templates
	{
		// Install the TerrainLogic shim ONLY across update(), then take it straight back out.
		//
		// ParticleSystem::update() is the one and only reader (ParticleSys.cpp, the
		// m_isEmitAboveGroundOnly ground-height test); the particle DRAW path never touches
		// TheTerrainLogic. Meanwhile a lot of shared engine code treats "TheTerrainLogic exists"
		// as "the game is running, not the editor" and switches behaviour on it:
		//   - W3DBridgeBuffer::drawBridges() disables every bridge and re-enables only the ones
		//     in TheTerrainLogic's bridge list -- which our shim never populates, so every
		//     bridge disappears. Its else branch is commented "In wb, all are enabled."
		//   - W3DWaterTracks prefers TheTerrainLogic->isUnderwater() over the editor's
		//     per-polygon m_editorWaterHeightFunc, so waves reseat to the flat global water
		//     level and sink below a map's higher water areas.
		// Overriding those getters on the shim does NOT help: the engine branches on the
		// POINTER, not on what it returns. Keeping the global NULL outside update() is the only
		// thing that keeps the editor on its intended paths.
		ScopedTerrainLogic terrainForUpdate;
		TheParticleSystemManager->update();
	}
	TheParticleSystemManager->queueParticleRender();
}

bool hasActiveEmitters()
{
	return s_enabled && !s_emitters.empty();
}

}	// namespace WBParticleRuntime
