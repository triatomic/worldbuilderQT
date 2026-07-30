// WBParticleRuntime.h -- live particle-system preview for the WorldBuilder editor.
//
// Particle systems render in-game but not in WB, because WB never stands up the runtime half of
// the particle subsystem: it creates TheParticleSystemManager (WorldBuilder.cpp) but never ticks
// it, never provides the frame clock ParticleSystemManager::update() depends on, and never creates
// a live emitter. This module supplies that runtime, gated behind the "Render Particles" toggle so
// the default WB behaviour is byte-unchanged.
//
// It does three things while enabled:
//   1. Stands up a frame clock -- ParticleSystemManager::update() reads TheGameLogic->getFrame()
//      (a per-frame dedup guard that must ADVANCE) and Particle::update() reads
//      TheGameClient->getFrame() (keyframe timing). WB has neither global, so we assign tiny
//      WB-local subclasses whose getFrame() returns our own counter (GameLogic::getFrame() was
//      made virtual for exactly this). No real simulation is created.
//   2. Creates/destroys live emitters for placed objects -- standalone particle-system markers and
//      the emitters attached to the draw state the viewport is showing (structure fire, chimney
//      smoke, steam vents). The state comes from the caller so it always matches the model drawn,
//      including the NIGHT / SNOW / GARRISONED / damaged states WB can select.
//   3. Ticks + queues the manager each WB frame so particles animate and draw during the WW3D flush.
//
// The whole module is a no-op unless setEnabled(true) has been called. All calls are safe when
// disabled or when the particle subsystem is unavailable.
#ifndef WB_PARTICLE_RUNTIME_H
#define WB_PARTICLE_RUNTIME_H

#include "Common/ModelState.h"	// ModelConditionFlags (a BitFlags<> typedef, can't be forward-declared)

class MapObject;
class RenderObjClass;

namespace WBParticleRuntime
{
	// The "Render Particles" toggle. Turning it on stands up the frame-clock shims; turning it
	// off destroys every emitter and tears the shims back down (globals restored to their prior
	// values). Idempotent.
	void setEnabled(bool on);
	bool isEnabled();

	// Place (or re-place) the emitters a placed object should show: its standalone particle-system
	// marker plus the emitters attached to the draw state the viewport is DRAWING. renderObj (may
	// be NULL) is the object's positioned W3D render object -- attached emitters read their bone's
	// world transform from it, so this MUST be called AFTER renderObj->Set_Transform. worldX/Y/Z
	// (terrain-adjusted object origin) is the fallback position for the marker and for any bone that
	// can't be resolved.
	//
	// flags MUST be the condition state the caller selected the model with (WbView3d applies NIGHT,
	// SNOW, GARRISONED and the object's damage level). Emitters are declared per condition state, so
	// passing the wrong one shows another state's FX -- or, far more often, none at all, since a
	// building's chimney smoke is typically declared on its night/garrisoned/damaged states while the
	// pristine daytime state has those lines commented out. NULL means "no model was drawn", which
	// falls back to the default state.
	//
	// If the object already has live emitters that still match its template, they are MOVED in place
	// (keeping their in-flight particles, so a drag-move doesn't visibly reset them); otherwise the
	// set is rebuilt from scratch. Callers don't choose -- just call this whenever the object's
	// position/state may have changed. No-op when disabled or the object carries no particle systems.
	void placeEmittersForObject(MapObject *obj, RenderObjClass *renderObj,
		float worldX, float worldY, float worldZ, const ModelConditionFlags *flags);

	// Destroy every emitter tracked for this object (called when it moves/changes/deletes).
	void destroyEmittersForObject(MapObject *obj);

	// Destroy every emitter for every object (called from the full scene teardown).
	void destroyAllEmitters();

	/// Drop the cached per-template emitter sets. Call when the loaded game data changes (a
	/// map.ini load), since an override gives the template a different set.
	void clearTemplateCache();

	// Advance the frame clock and tick + queue the manager for this WB frame. Call once per redraw
	// after WW3D::Sync and before the scene render. No-op when disabled.
	void tick();

	// True while at least one live emitter exists -- lets the redraw timer keep animating a static
	// view (mirrors the wavesActive exception).
	bool hasActiveEmitters();
}

#endif // WB_PARTICLE_RUNTIME_H
