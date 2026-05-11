#include "sdlrendererbackend.h"
#include <cstring>
#include <vector>

SDLRendererBackend::SDLRendererBackend() = default;
SDLRendererBackend::~SDLRendererBackend() { Shutdown(); }

bool SDLRendererBackend::Init(SDL_Window *win) {
	window = win;

	// SDL_CreateRenderer with NULL driver name lets SDL pick: on emscripten it
	// resolves to "opengles2" → WebGL2 canvas context. The renderer owns the
	// canvas/back-buffer; we draw into it each frame via SDL_RenderTexture.
	renderer = SDL_CreateRenderer(window, NULL);
	if (!renderer) {
		SDL_Log("SDLRendererBackend: SDL_CreateRenderer failed: %s", SDL_GetError());
		return false;
	}

	const char *driver = SDL_GetRendererName(renderer);
	SDL_Log("SDLRendererBackend: using SDL_Renderer driver=%s", driver ? driver : "?");

	// Default scale: nearest. Game::SetupRenderDevice flips to linear when the
	// user's config wants it.
	SDL_SetRenderVSync(renderer, 1);
	return true;
}

void SDLRendererBackend::Shutdown() {
	if (scene)    { SDL_DestroyTexture(scene);  scene = nullptr; }
	if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
	window = nullptr;
}

void SDLRendererBackend::SetPalette(const SDL_Color *colors, int count) {
	if (count > 256) count = 256;
	for (int i = 0; i < count; i++) {
		palette_colors[i] = colors[i];
		palette_colors[i].a = 255;
	}
}

void SDLRendererBackend::UploadFrame(const Uint8 *indexed_pixels, int w, int h) {
	pending_pixels = indexed_pixels;
	pending_w      = w;
	pending_h      = h;
	frame_dirty    = true;
}

bool SDLRendererBackend::EnsureSceneTexture(int w, int h) {
	if (scene && scene_w == w && scene_h == h) return true;
	if (scene) { SDL_DestroyTexture(scene); scene = nullptr; }
	scene = SDL_CreateTexture(renderer,
	                          SDL_PIXELFORMAT_RGBA32,
	                          SDL_TEXTUREACCESS_STREAMING,
	                          w, h);
	if (!scene) {
		SDL_Log("SDLRendererBackend: SDL_CreateTexture(%dx%d) failed: %s", w, h, SDL_GetError());
		return false;
	}
	SDL_SetTextureScaleMode(scene, use_linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	scene_w = w;
	scene_h = h;
	scratch_rgba.assign(size_t(w) * size_t(h) * 4, 0);
	return true;
}

void SDLRendererBackend::Present() {
	if (!renderer) return;
	if (frame_dirty && pending_pixels && pending_w > 0 && pending_h > 0) {
		if (!EnsureSceneTexture(pending_w, pending_h)) return;

		// CPU palette remap → scratch_rgba. ~640*480 = 307k pixel lookups
		// per frame, ~1ms on modern hardware. SDL_GPU's fragment-shader
		// remap would be faster but isn't available on emscripten yet.
		const Uint8 *src = pending_pixels;
		Uint8       *dst = scratch_rgba.data();
		const size_t n   = size_t(pending_w) * size_t(pending_h);
		for (size_t i = 0; i < n; i++) {
			const SDL_Color &c = palette_colors[src[i]];
			dst[i*4 + 0] = c.r;
			dst[i*4 + 1] = c.g;
			dst[i*4 + 2] = c.b;
			dst[i*4 + 3] = c.a;
		}

		// SDL_UpdateTexture vs SDL_LockTexture: emscripten/WebGL2 paths
		// behave the same; UpdateTexture is the simpler call.
		SDL_UpdateTexture(scene, NULL, scratch_rgba.data(), pending_w * 4);
		frame_dirty = false;
	}

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	if (scene) {
		// Stretch to the full window — the upscale filter is set on the
		// texture itself via SDL_SetTextureScaleMode.
		SDL_RenderTexture(renderer, scene, NULL, NULL);
	}
	SDL_RenderPresent(renderer);
}

void SDLRendererBackend::SetScaleFilter(bool linear) {
	use_linear = linear;
	if (scene) {
		SDL_SetTextureScaleMode(scene, linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	}
}
