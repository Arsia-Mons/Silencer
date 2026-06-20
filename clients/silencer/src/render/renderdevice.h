#ifndef RENDERDEVICE_H
#define RENDERDEVICE_H

#include "shared.h"

#include <vector>

namespace silencer {
namespace cppx_ui {
struct GpuUiProgram;
}
} // namespace silencer

// Abstract rendering interface; GPU-specific types never leak into game code.
class RenderDevice {
public:
	virtual ~RenderDevice() = default;

	// The window must NOT have SDL_WINDOW_OPENGL set.
	virtual bool Init(SDL_Window *window) = 0;
	virtual void Shutdown() = 0;

	// Alpha is forced to 255. Upload deferred to Present().
	virtual void SetPalette(const SDL_Color *colors, int count) = 0;

	// Upload deferred to Present(); pixels must remain valid until then.
	virtual void UploadFrame(const Uint8 *indexed_pixels, int w, int h) = 0;

	// Queue a premultiplied-RGBA8 UI overlay composited over the final frame.
	// `w*h*4` tightly-packed bytes; deferred to Present(), must stay valid until
	// then. Null/0 clears the overlay. `global_alpha` dims in lockstep with the
	// FADEOUT palette fade; PREMULTIPLIED, so it scales all four channels.
	virtual void UploadUiFrame(const Uint8 * /*rgba*/, int /*w*/, int /*h*/,
	                           float /*global_alpha*/ = 1.0f) {}

	// GPU-geometry alternative to UploadUiFrame. `global_alpha` is the FADEOUT
	// fraction applied as a GPU multiply at composite. Program owned by the cppx
	// host, must stay valid until the next Present().
	virtual bool SupportsUiGeometry() const { return false; }
	virtual void SubmitUiFrame(const silencer::cppx_ui::GpuUiProgram & /*program*/,
	                           float /*global_alpha*/ = 1.0f) {}

	// Arm with RequestCapture() before a Present(); afterwards TakeCapturedFrame()
	// yields tightly-packed RGBA8 (out = w*h*4 bytes), false if none available.
	virtual void RequestCapture() {}
	virtual bool TakeCapturedFrame(std::vector<Uint8> & /*rgba*/, int & /*w*/,
	                               int & /*h*/) { return false; }

	// Skips the render pass silently when the window is minimized.
	virtual void Present() = 0;

	// Backend driver name for telemetry (e.g. "metal", "vulkan", "direct3d12").
	// Empty when not applicable (TUI). Best-effort device identity.
	virtual const char * GpuDriverName() const { return ""; }

	virtual void SetScaleFilter(bool linear) = 0;

	virtual void BeginLobbyPanelBorderBlur(int /*virtualWidth*/,
	                                       int /*virtualHeight*/,
	                                       float /*uiScale*/) {}
	virtual void AddLobbyPanelBorderBlurRect(const SDL_Rect & /*rect*/) {}

	// Returns false if the backend has lost its presentation target (e.g. TUI
	// frame socket closed by the frontend); the game loop exits cleanly instead
	// of spinning on broken writes.
	virtual bool IsAlive() const { return true; }

	// Lights are composited additively AFTER palette remap, so they don't
	// conflict with CPU palette lighting already baked into pixels.
	virtual void BeginLighting() {}

	// Soft point light at pixel (x,y), radius in pixels, intensity [0,1].
	// Clamped to an internal max per frame.
	virtual void AddPointLight(float /*x*/, float /*y*/, float /*radius*/,
	                           SDL_Color /*color*/, float /*intensity*/) {}

	virtual void EndLighting() {}

	// Returns an opaque handle >= 0, or -1 on failure.
	virtual int AllocParticleBuffer(Uint32 /*count*/) { return -1; }

	virtual void FreeParticleBuffer(int /*handle*/) {}

	virtual void DispatchParticleUpdate(int /*handle*/, Uint32 /*count*/, float /*dt*/) {}

	// Must be called after DispatchParticleUpdate and before Present().
	virtual void DrawParticles(int /*handle*/, Uint32 /*count*/) {}
};

#endif
