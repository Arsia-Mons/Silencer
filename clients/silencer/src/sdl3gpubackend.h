#ifndef SDL3GPUBACKEND_H
#define SDL3GPUBACKEND_H

#include "renderdevice.h"

// SDL3 GPU API backend — Metal on macOS, Vulkan on Linux/Windows.
// Implements the indexed-color palette remap shader:
//   frame_tex (R8_UNORM, 640×480) + palette_tex (RGBA8, 256×1)
//   → fullscreen triangle → swapchain
//
// Shader strategy (Phase 2): MSL source strings for Metal. Future platforms
// add SPIR-V / DXIL paths selected at runtime via SDL_GetGPUShaderFormats().
class SDL3GPUBackend : public RenderDevice {
public:
	SDL3GPUBackend();
	~SDL3GPUBackend() override;

	bool Init(SDL_Window *window) override;
	void Shutdown() override;

	void SetPalette(const SDL_Color *colors, int count) override;
	void UploadFrame(const Uint8 *indexed_pixels, int w, int h) override;
	void Present() override;
	void SetScaleFilter(bool linear) override;

private:
	bool CreatePipeline();
	SDL_GPUShader *LoadShader(SDL_GPUShaderStage stage,
	                          const char *msl_source,
	                          const char *entrypoint,
	                          Uint32 num_samplers);

	SDL_Window              *window       = nullptr;
	SDL_GPUDevice           *device       = nullptr;
	SDL_GPUTexture          *frame_tex    = nullptr;
	SDL_GPUTexture          *palette_tex  = nullptr;
	SDL_GPUGraphicsPipeline *pipeline     = nullptr;
	SDL_GPUSampler          *sampler      = nullptr;

	// Persistent transfer buffers — reused every frame, resized if needed.
	SDL_GPUTransferBuffer *frame_tbuf    = nullptr;
	SDL_GPUTransferBuffer *palette_tbuf  = nullptr;
	Uint32                 frame_tbuf_sz = 0;
	int                    frame_tex_w   = 0;
	int                    frame_tex_h   = 0;

	// Palette CPU copy (for dirty tracking and ffmpeg replay path in game.cpp).
	SDL_Color palette_colors[256] = {};
	bool      palette_dirty       = true;

	// Pending frame upload state.
	const Uint8 *pending_pixels  = nullptr;
	int          pending_w       = 0;
	int          pending_h       = 0;
	bool         frame_dirty     = false;
};

#endif
