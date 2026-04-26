#ifndef RENDERDEVICE_H
#define RENDERDEVICE_H

#include "shared.h"

// Abstract rendering interface. All game code talks through this; GPU-specific
// types never leak into game code. SDL3GPUBackend is the Phase 2 implementation.
// Console backends (NVN, GNM, D3D12) slot in without touching game code.
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

	// Flush pending uploads, run the palette-remap shader, and swap buffers.
	// No-op (skips render pass) when the window is minimized.
	virtual void Present() = 0;

	// Nearest-pixel is always used for the indexed→palette step.
	// Phase 3 will implement bilinear post-remap upscaling via a second pass.
	virtual void SetScaleFilter(bool linear) = 0;
};

#endif
