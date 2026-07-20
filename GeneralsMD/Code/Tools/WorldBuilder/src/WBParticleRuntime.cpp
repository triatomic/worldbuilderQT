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
	};

	// ------------------------------------------------------------------------------------------
	// Runtime state.
	// ------------------------------------------------------------------------------------------
	bool s_enabled = false;
	WBParticleTerrainLogic *s_wbTerrain = NULL;	// installed into TheTerrainLogic while enabled

	// Created once on first enable, then kept for the process life (their base destructors assume
	// init() ran, so we must never delete them). Installed into TheGameLogic/TheGameClient and
	// left there.
	WBParticleGameLogic  *s_wbLogic  = NULL;
	WBParticleGameClient *s_wbClient = NULL;
	Bool s_savedUseFX = FALSE;
	Int  s_savedMaxParticleCount = 0;
	W3DAssetManager *s_savedDisplayAssetMgr = NULL;	// restore W3DDisplay::m_assetManager on disable

	// The live emitter IDs created per placed object, for teardown.
	typedef std::vector<ParticleSystemID> IDList;
	typedef std::map<MapObject *, IDList> EmitterMap;
	EmitterMap s_emitters;

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

	// Spawn one emitter from a template at a world position; record its ID under obj.
	void spawnEmitter( MapObject *obj, const ParticleSystemTemplate *tmpl, const Coord3D &pos )
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
		s_emitters[obj].push_back( sys->getSystemID() );
	}

	// Collect the always-on particle-system templates a ThingTemplate carries: the ones in its
	// DEFAULT draw state (the base/idle ModelConditionInfo). Condition-gated states (damage,
	// combat, upgrades) are skipped -- WB can't evaluate their triggers, so showing them would be
	// wrong. The default state is m_conditionStates[0] (built from MODELCONDITION_NONE).
	// One always-on attached emitter: its template + the bone it hangs off (empty = object origin).
	struct AttachedEmitter
	{
		const ParticleSystemTemplate *tmpl;
		AsciiString bone;
	};

	// A template's always-on emitter set is immutable for the process life, but
	// createEmittersForObject runs per placed object on every render rebuild (invalObjectInView(NULL)
	// walks every object on ~20 events). Compute it once per distinct template and cache it, so
	// repeat instances are a map lookup instead of a fresh module walk + vector build each time.
	typedef std::vector<AttachedEmitter> AttachedList;
	typedef std::map<const ThingTemplate *, AttachedList> AttachedCache;
	AttachedCache s_attachedCache;

	const AttachedList &collectAlwaysOnTemplates( const ThingTemplate *tt )
	{
		static const AttachedList empty;
		if (tt == NULL)
		{
			return empty;
		}
		AttachedCache::iterator cached = s_attachedCache.find( tt );
		if (cached != s_attachedCache.end())
		{
			return cached->second;
		}

		AttachedList &out = s_attachedCache[tt];
		const ModuleInfo &draws = tt->getDrawModuleInfo();
		for (int i = 0; i < draws.getCount(); ++i)
		{
			const W3DModelDrawModuleData *md = draws.getNthData( i )->getAsW3DModelDrawModuleData();
			if (md == NULL || md->m_conditionStates.empty())
			{
				continue;
			}
			const ModelConditionInfo &base = md->m_conditionStates[0];
			for (size_t b = 0; b < base.m_particleSysBones.size(); ++b)
			{
				if (base.m_particleSysBones[b].particleSystemTemplate != NULL)
				{
					AttachedEmitter e;
					e.tmpl = base.m_particleSysBones[b].particleSystemTemplate;
					e.bone = base.m_particleSysBones[b].boneName;
					out.push_back( e );
				}
			}
		}
		return out;
	}

	// One emitter to place: its template + the world position it should sit at. Both the initial
	// spawn (createEmittersForObject) and the drag-move reposition build this SAME ordered list from
	// one source, so the systems recorded per object stay index-aligned with it.
	struct Placement
	{
		const ParticleSystemTemplate *tmpl;
		Coord3D pos;
	};

	// Build the ordered emitter placement list for an object: (a) its standalone particle-system
	// marker (if any), then (b) each always-on attached emitter at its bone world position. renderObj
	// (may be NULL) is the positioned render object attached emitters read their bone transform from;
	// origin is the terrain-adjusted object origin used for the marker and for unresolved bones.
	void computeEmitterPlacements( MapObject *obj, RenderObjClass *renderObj, const Coord3D &origin,
		std::vector<Placement> &out )
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
					out.push_back( p );
				}
			}
		}

		// (b) Always-on emitters attached to the object's template's default draw state, placed at
		// their bone. Get_Bone_Transform gives the bone's WORLD matrix (the render obj is already
		// positioned); an unknown bone falls back to Get_Transform (the object origin), so a bad
		// bone name still shows the emitter rather than dropping it.
		const AttachedList &attached = collectAlwaysOnTemplates( obj->getThingTemplate() );
		for (size_t i = 0; i < attached.size(); ++i)
		{
			Placement p;
			p.tmpl = attached[i].tmpl;
			p.pos = origin;
			if (renderObj != NULL && !attached[i].bone.isEmpty() && !attached[i].bone.isNone())
			{
				const Matrix3D &bx = renderObj->Get_Bone_Transform( attached[i].bone.str() );
				Vector3 t = bx.Get_Translation();
				p.pos.x = t.X;
				p.pos.y = t.Y;
				p.pos.z = t.Z;
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

		// A ground-emit particle system reads TheTerrainLogic->getGroundHeight(); WB has no
		// TheTerrainLogic. Install the heightmap-backed shim (kept for the process life).
		if (TheTerrainLogic == NULL)
		{
			s_wbTerrain = new WBParticleTerrainLogic();
			TheTerrainLogic = s_wbTerrain;
		}

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

		// Leave the shim globals installed. They are harmless (only serve getFrame()), and any
		// particle still draining during the next WW3D flush would read TheGameClient -- so
		// un-pointing it to NULL here could crash the draw path. WB had no GameClient/GameLogic
		// of its own, so keeping the shims in place changes nothing else. tick() is gated on
		// s_enabled, so the manager stops advancing once disabled.
		if (TheWritableGlobalData != NULL)
		{
			TheWritableGlobalData->m_useFX = s_savedUseFX;
			TheWritableGlobalData->m_maxParticleCount = s_savedMaxParticleCount;
		}
		W3DDisplay::m_assetManager = s_savedDisplayAssetMgr;	// restore (normally NULL)
	}
}

void placeEmittersForObject(MapObject *obj, RenderObjClass *renderObj,
	float worldX, float worldY, float worldZ)
{
	if (!s_enabled || obj == NULL)
	{
		return;
	}

	Coord3D origin;		// the object origin: standalone-marker position + bone fallback
	origin.x = worldX;
	origin.y = worldY;
	origin.z = worldZ;

	std::vector<Placement> places;
	computeEmitterPlacements( obj, renderObj, origin, places );

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
		spawnEmitter( obj, places[i].tmpl, places[i].pos );
	}
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
}

void destroyAllEmitters()
{
	for (EmitterMap::iterator it = s_emitters.begin(); it != s_emitters.end(); ++it)
	{
		destroyIDList( it->second );
	}
	s_emitters.clear();
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
	TheParticleSystemManager->update();
	TheParticleSystemManager->queueParticleRender();
}

bool hasActiveEmitters()
{
	return s_enabled && !s_emitters.empty();
}

}	// namespace WBParticleRuntime
