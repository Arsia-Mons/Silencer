#ifndef RENDERDEVICE_H
#define RENDERDEVICE_H

#include "shared.h"

// Abstract rendering interface. All game code talks through this; GPU-specific
// types never leak into game code. SDL3GPUBackend is the Phase 2/3 implementation.
// Console backends (NVN, GNM, D3D12) slot in without touching game code.
//
// Render order each frame:
//   UploadFrame()                     — queue 8-bit indexed pixels
//   [DispatchParticleUpdate() ...]     — optional: compute-simulate GPU particles
//   [DrawParticles() ...]              — optional: queue particle draw (additive, into scene)
//   [BeginLighting() + AddPointLight() + EndLighting()]  — optional: additive emissive lights
//   Present()                          — execute all queued work, swap buffers
class RenderDevice {
public:
	virtual ~RenderDevice() = default;

	// Initialise the device against an existing SDL window.
	// The window must NOT have SDL_WINDOW_OPENGL set.
	virtual bool Init(SDL_Window *window) = 0;
	virtual void Shutdown() = 0;

	// Upload the 256-entry palette. Alpha is forced to 255.
	// Call whenever the palette changes; the upload is deferred to Present().
	virtual void SetPalette(const SDL_Color *colors, int count) = 0;

	// Copy the 8-bit indexed pixel buffer for the current frame.
	// Upload is deferred to Present(); pixels must remain valid until then.
	virtual void UploadFrame(const Uint8 *indexed_pixels, int w, int h) = 0;

	// Flush all pending work: uploads, compute, render passes, swap buffers.
	// Skips the render pass silently when the window is minimized.
	virtual void Present() = 0;

	// Control upscale filter applied when scene texture is stretched to the window.
	// Nearest-pixel is always used for the indexed→palette remap step regardless.
	virtual void SetScaleFilter(bool linear) = 0;

	// -------------------------------------------------------------------------
	// Phase 3 — Lighting (additive emissive overlay)
	// Lights are composited additively onto the scene AFTER palette remap, so
	// they do not conflict with CPU palette lighting already baked into pixels.
	// Default: no-ops (backends that don't implement lighting just skip it).
	// -------------------------------------------------------------------------

	// Begin accumulating lights for this frame. Call before AddPointLight().
	virtual void BeginLighting() {}

	// Submit a soft point light at pixel position (x,y) with the given radius
	// (pixels), color, and intensity [0,1]. Clamped to an internal max per frame.
	virtual void AddPointLight(float /*x*/, float /*y*/, float /*radius*/,
	                           SDL_Color /*color*/, float /*intensity*/) {}

	// Finalize lighting for this frame. Lights are rendered in Present().
	virtual void EndLighting() {}

	// -------------------------------------------------------------------------
	// Phase 3 — GPU compute particles (feeds VFX Tool #36)
	// Particle positions are simulated on the GPU via a compute kernel and drawn
	// additively into the scene texture before upscaling.
	// Default: no-ops that return -1 (unsupported on backends without compute).
	// -------------------------------------------------------------------------

	// Allocate a GPU particle buffer for up to `count` particles.
	// Returns an opaque handle >= 0, or -1 on failure.
	virtual int AllocParticleBuffer(Uint32 /*count*/) { return -1; }

	// Release a previously allocated particle buffer.
	virtual void FreeParticleBuffer(int /*handle*/) {}

	// Dispatch the compute kernel to advance particle positions by dt seconds.
	// Particles with life <= 0 are skipped.
	virtual void DispatchParticleUpdate(int /*handle*/, Uint32 /*count*/, float /*dt*/) {}

	// Queue a draw of `count` particles from the buffer into the scene.
	// Must be called after DispatchParticleUpdate and before Present().
	virtual void DrawParticles(int /*handle*/, Uint32 /*count*/) {}
};

#endif
