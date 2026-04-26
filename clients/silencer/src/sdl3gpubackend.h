#ifndef SDL3GPUBACKEND_H
#define SDL3GPUBACKEND_H

#include "renderdevice.h"

// SDL3 GPU API backend — Metal on macOS (auto-selects Vulkan on Linux/Windows).
//
// Phase 2 — palette remap shader: frame_tex (R8_UNORM) → scene_tex (RGBA8)
// Phase 3 — offscreen scene + bilinear upscale + additive lights + GPU particles
//
// Render pass ordering inside Present():
//   copy pass   — upload frame_tex and palette_tex (dirty-flagged)
//   compute pass — advance particle positions (one dispatch per pending update)
//   remap pass  — indexed frame → scene_tex  (CLEAR, remap_pipeline)
//   effects pass — particles + lights → scene_tex  (LOAD, additive blend)
//   upscale pass — scene_tex → swapchain  (CLEAR, nearest or bilinear)
//
// Shader strategy: MSL source strings (Metal). SDL_GetGPUShaderFormats() is the
// hook for adding SPIR-V (Vulkan) / DXIL (D3D12) paths on other platforms.
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

	// Phase 3 — lighting
	void BeginLighting() override;
	void AddPointLight(float x, float y, float radius,
	                   SDL_Color color, float intensity) override;
	void EndLighting() override;

	// Phase 3 — GPU compute particles
	int  AllocParticleBuffer(Uint32 count) override;
	void FreeParticleBuffer(int handle) override;
	void DispatchParticleUpdate(int handle, Uint32 count, float dt) override;
	void DrawParticles(int handle, Uint32 count) override;

private:
	bool CreatePipelines();
	bool CreateLightPipeline();
	bool CreateParticlePipelines();
	SDL_GPUShader *LoadShader(SDL_GPUShaderStage stage,
	                          const char *msl_source,
	                          const char *entrypoint,
	                          Uint32 num_samplers,
	                          Uint32 num_uniform_buffers = 0,
	                          Uint32 num_storage_buffers = 0);

	SDL_Window    *window = nullptr;
	SDL_GPUDevice *device = nullptr;

	// --- Indexed frame texture (R8_UNORM, game resolution) ---
	SDL_GPUTexture        *frame_tex    = nullptr;
	SDL_GPUTransferBuffer *frame_tbuf   = nullptr;
	Uint32                 frame_tbuf_sz = 0;
	int                    frame_tex_w  = 0;
	int                    frame_tex_h  = 0;

	// --- Palette texture (RGBA8, 256×1) ---
	SDL_GPUTexture        *palette_tex  = nullptr;
	SDL_GPUTransferBuffer *palette_tbuf = nullptr;

	// --- Scene texture (RGBA8, game resolution, COLOR_TARGET|SAMPLER) ---
	// Remap writes here; effects (lights, particles) composite additively here.
	// Upscale pass reads from here to produce the final swapchain image.
	SDL_GPUTexture *scene_tex   = nullptr;
	int             scene_tex_w = 0;
	int             scene_tex_h = 0;

	// --- Pipelines ---
	SDL_GPUGraphicsPipeline *remap_pipeline    = nullptr; // indexed → scene_tex
	SDL_GPUGraphicsPipeline *upscale_pipeline  = nullptr; // scene_tex → swapchain
	SDL_GPUGraphicsPipeline *light_pipeline    = nullptr; // additive disc → scene_tex
	SDL_GPUGraphicsPipeline *particle_pipeline = nullptr; // particles → scene_tex
	SDL_GPUComputePipeline  *particle_compute  = nullptr; // update particle positions

	// --- Samplers ---
	SDL_GPUSampler *nearest_sampler = nullptr;
	SDL_GPUSampler *linear_sampler  = nullptr;
	bool            use_linear      = false;

	// --- Palette CPU copy ---
	SDL_Color palette_colors[256] = {};
	bool      palette_dirty       = true;

	// --- Pending frame upload ---
	const Uint8 *pending_pixels = nullptr;
	int          pending_w      = 0;
	int          pending_h      = 0;
	bool         frame_dirty    = false;

	// --- Pending lighting (Phase 3) ---
	// Additive emissive — no conflict with CPU palette lighting.
	static const int kMaxLights = 64;
	struct LightEntry { float x, y, radius, intensity, r, g, b; };
	LightEntry pending_lights[kMaxLights];
	int        pending_light_count = 0;
	bool       lighting_active     = false; // BeginLighting called this frame

	// --- Pending particle operations (Phase 3) ---
	static const int kMaxParticleBuffers = 8;

	// GPU particle struct — must match MSL layout (naturally 4-byte aligned, 32 bytes).
	struct GPUParticle { float x, y, vx, vy, life, max_life; Uint32 color_idx, flags; };

	struct ParticleBuffer { SDL_GPUBuffer *buf = nullptr; Uint32 capacity = 0; };
	ParticleBuffer particle_bufs[kMaxParticleBuffers];

	struct PendingParticleUpdate { int handle; Uint32 count; float dt; };
	struct PendingParticleDraw   { int handle; Uint32 count; };
	PendingParticleUpdate pending_particle_updates[kMaxParticleBuffers];
	PendingParticleDraw   pending_particle_draws[kMaxParticleBuffers];
	int pending_particle_update_count = 0;
	int pending_particle_draw_count   = 0;
};

#endif
