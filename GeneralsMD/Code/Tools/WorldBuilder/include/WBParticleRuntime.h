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
//      the always-on emitters a ThingTemplate's DEFAULT draw state carries (structure fire,
//      ambient smoke). Condition-gated emitters (damage/combat/upgrade) are intentionally skipped.
//   3. Ticks + queues the manager each WB frame so particles animate and draw during the WW3D flush.
//
// The whole module is a no-op unless setEnabled(true) has been called. All calls are safe when
// disabled or when the particle subsystem is unavailable.
#ifndef WB_PARTICLE_RUNTIME_H
#define WB_PARTICLE_RUNTIME_H

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
	// marker plus the always-on emitters its template's DEFAULT draw state carries. renderObj (may
	// be NULL) is the object's positioned W3D render object -- attached emitters read their bone's
	// world transform from it, so this MUST be called AFTER renderObj->Set_Transform. worldX/Y/Z
	// (terrain-adjusted object origin) is the fallback position for the marker and for any bone that
	// can't be resolved.
	//
	// If the object already has live emitters that still match its template, they are MOVED in place
	// (keeping their in-flight particles, so a drag-move doesn't visibly reset them); otherwise the
	// set is rebuilt from scratch. Callers don't choose -- just call this whenever the object's
	// position/state may have changed. No-op when disabled or the object carries no particle systems.
	void placeEmittersForObject(MapObject *obj, RenderObjClass *renderObj,
		float worldX, float worldY, float worldZ);

	// Destroy every emitter tracked for this object (called when it moves/changes/deletes).
	void destroyEmittersForObject(MapObject *obj);

	// Destroy every emitter for every object (called from the full scene teardown).
	void destroyAllEmitters();

	// Advance the frame clock and tick + queue the manager for this WB frame. Call once per redraw
	// after WW3D::Sync and before the scene render. No-op when disabled.
	void tick();

	// True while at least one live emitter exists -- lets the redraw timer keep animating a static
	// view (mirrors the wavesActive exception).
	bool hasActiveEmitters();
}

#endif // WB_PARTICLE_RUNTIME_H
