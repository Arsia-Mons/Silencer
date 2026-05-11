#ifndef SDLRENDERERBACKEND_H
#define SDLRENDERERBACKEND_H

#include "renderdevice.h"
#include <vector>

// CPU-side palette remap + SDL_Renderer streaming texture upload. Used on
// Emscripten where SDL3 (3.4.2 in the emcc port) has no WebGPU backend for
// SDL_GPU — the SDL_Renderer 2D API still works because it's backed by GLES2
// (WebGL2 in the browser).
//
// Stage 1 of the WASM build only needs to render the main menu, which means:
//   - Upload an 8-bit indexed framebuffer
//   - Apply palette remap → RGBA
//   - Blit/upscale to the window
//
// Particle compute, additive lighting, etc. (Phase 3 of the GPU backend) are
// left as the RenderDevice base-class no-ops. Stage 5+ (in-game spectating)
// will revisit how to do those without compute shaders in the browser —
// likely a CPU port for lights and CPU particle simulation for the modest
// counts the game emits.
class SDLRendererBackend : public RenderDevice {
public:
	SDLRendererBackend();
	~SDLRendererBackend() override;

	bool Init(SDL_Window *window) override;
	void Shutdown() override;

	void SetPalette(const SDL_Color *colors, int count) override;
	void UploadFrame(const Uint8 *indexed_pixels, int w, int h) override;
	void Present() override;
	void SetScaleFilter(bool linear) override;

private:
	bool EnsureSceneTexture(int w, int h);

	SDL_Window   *window   = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Texture  *scene    = nullptr; // RGBA8888 streaming, game-resolution
	int           scene_w  = 0;
	int           scene_h  = 0;

	// Local palette copy; alpha forced to 255 at SetPalette time.
	SDL_Color palette_colors[256] = {};

	// Pending frame upload. UploadFrame stashes the pointer; Present remaps
	// + uploads. Caller keeps the pixel buffer alive between the two.
	const Uint8 *pending_pixels = nullptr;
	int          pending_w      = 0;
	int          pending_h      = 0;
	bool         frame_dirty    = false;

	// Pre-allocated scratch RGBA8 row for the texture lock/unlock path.
	// Resized lazily in EnsureSceneTexture.
	std::vector<Uint8> scratch_rgba;

	bool use_linear = false;
};

#endif
