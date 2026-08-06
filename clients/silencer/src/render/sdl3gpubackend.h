#ifndef SDL3GPUBACKEND_H
#define SDL3GPUBACKEND_H

#include "renderdevice.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

// SDL3 GPU API backend — Metal on macOS (MSL), D3D12 on Windows (DXIL).
// HLSL in clients/silencer/shaders/ is the shader source-of-truth; MSL is
// hand-translated and embedded, DXIL is dxc-generated into shaders/generated/.

struct ShaderBundle {
	const char         *msl_src;     // null-terminated MSL
	const unsigned char *dxil;
	size_t              dxil_size;
	const char         *entrypoint;
};
class SDL3GPUBackend : public RenderDevice {
public:
	SDL3GPUBackend();
	~SDL3GPUBackend() override;

	bool Init(SDL_Window *window) override;
	void Shutdown() override;

	void SetPalette(const SDL_Color *colors, int count) override;
	void UploadFrame(const Uint8 *indexed_pixels, int w, int h) override;
	void UploadUiFrame(const Uint8 *rgba, int w, int h, float global_alpha = 1.0f) override;
	bool SupportsUiGeometry() const override { return true; }
	void SubmitUiFrame(const silencer::cppx_ui::GpuUiProgram &program,
	                   float global_alpha = 1.0f) override;
	void RequestCapture() override;
	bool TakeCapturedFrame(std::vector<Uint8> &rgba, int &w, int &h) override;
	void Present() override;
	const char *GpuDriverName() const override;
	void SetScaleFilter(bool linear) override;
	void BeginLobbyPanelBorderBlur(int virtualWidth, int virtualHeight, float uiScale) override;
	void AddLobbyPanelBorderBlurRect(const SDL_Rect & rect) override;

	void BeginLighting() override;
	void AddPointLight(float x, float y, float radius,
	                   SDL_Color color, float intensity) override;
	void EndLighting() override;

	int  AllocParticleBuffer(Uint32 count) override;
	void FreeParticleBuffer(int handle) override;
	void DispatchParticleUpdate(int handle, Uint32 count, float dt) override;
	void DrawParticles(int handle, Uint32 count) override;

private:
	bool CreatePipelines();
	bool CreateLightPipeline();
	bool CreateParticlePipelines();
	bool CreateLobbyPanelBlurPipeline();
	bool CreateUiGeometryPipelines();
	bool EnsureUiLayerResources(int w, int h);
	bool EnsureUiWhiteTexture(SDL_GPUCopyPass *copy);
	SDL_GPUTexture *EnsureUiTexture(SDL_GPUCopyPass *copy, uint64_t key,
	                                const uint8_t *rgba, int w, int h);
	void ReleaseUiTextureCache();
	SDL_GPUShader *LoadShader(SDL_GPUShaderStage stage,
	                          const ShaderBundle &b,
	                          Uint32 num_samplers,
	                          Uint32 num_uniform_buffers = 0,
	                          Uint32 num_storage_buffers = 0);

	SDL_Window         *window        = nullptr;
	SDL_GPUDevice      *device        = nullptr;
	SDL_GPUShaderFormat chosen_format = 0; // picked at Init()

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
	// Remap writes here; effects (lights, particles) composite additively here;
	// the upscale pass reads from here.
	SDL_GPUTexture *scene_tex   = nullptr;
	int             scene_tex_w = 0;
	int             scene_tex_h = 0;

	// --- cppx UI overlay (UploadUiFrame path): premultiplied RGBA composited
	// over the swapchain after upscale ---
	SDL_GPUTexture        *ui_tex      = nullptr;
	int                    ui_tex_w    = 0;
	int                    ui_tex_h    = 0;
	SDL_GPUTransferBuffer *ui_tbuf     = nullptr;
	Uint32                 ui_tbuf_sz  = 0;

	// --- GPU UI geometry path (SubmitUiFrame) ---
	SDL_GPUGraphicsPipeline *ui_geom_pipeline      = nullptr;
	SDL_GPUGraphicsPipeline *ui_scene_composite_pipeline = nullptr;
	SDL_GPUTexture        *ui_scene_tex   = nullptr; // RGBA, UI device res, COLOR_TARGET|SAMPLER
	int                    ui_scene_w     = 0;
	int                    ui_scene_h     = 0;
	SDL_GPUBuffer         *ui_vbuf        = nullptr;
	Uint32                 ui_vbuf_cap    = 0;       // vertices
	SDL_GPUTransferBuffer *ui_vbuf_tbuf   = nullptr;
	Uint32                 ui_vbuf_tbuf_sz = 0;      // bytes
	SDL_GPUTexture        *ui_white_tex   = nullptr; // 1x1 white for solid batches
	bool                   ui_white_ready = false;
	SDL_GPUTransferBuffer *ui_texup_tbuf  = nullptr;
	Uint32                 ui_texup_tbuf_sz = 0;
	std::unordered_map<uint64_t, SDL_GPUTexture *> ui_tex_cache;
	uint64_t               ui_tex_generation = 0;    // last seen program generation; flushes cache on change
	// Program owned by the cppx host, valid until Present consumes it.
	const silencer::cppx_ui::GpuUiProgram *pending_ui_program = nullptr;
	bool                   ui_geom_present = false;
	Uint8                  ui_geom_fade    = 255;     // 0..255

	// Group-opacity layers: PushLayer redirects the subtree into a transient
	// target, composited back at the layer opacity at PopLayer. Allocated only
	// when a program carries layers, so the common no-layer path is untouched.
	static constexpr int   kMaxUiLayers = 4;
	SDL_GPUGraphicsPipeline *ui_layer_composite_pipeline = nullptr;
	SDL_GPUTexture        *ui_layer_tex[kMaxUiLayers] = {};
	int                    ui_layer_w   = 0;
	int                    ui_layer_h   = 0;

	// --- Swapchain capture (screenshot): armed by RequestCapture(), filled during Present() ---
	bool                   capture_pending = false;
	SDL_GPUTransferBuffer *capture_tbuf    = nullptr;
	Uint32                 capture_tbuf_sz = 0;
	std::vector<Uint8>     captured_rgba;
	int                    captured_w      = 0;
	int                    captured_h      = 0;
	bool                   captured_valid  = false;

	// --- Lobby panel-border blur ---
	SDL_GPUTexture *lobby_panel_source_tex = nullptr; // full-res scene copy
	SDL_GPUTexture *lobby_panel_mask_tex = nullptr; // border pixels only
	int             lobby_panel_source_w   = 0;
	int             lobby_panel_source_h   = 0;
	int             lobby_panel_mask_w     = 0;
	int             lobby_panel_mask_h     = 0;

	// --- Pipelines ---
	SDL_GPUGraphicsPipeline *remap_pipeline    = nullptr;
	SDL_GPUGraphicsPipeline *upscale_pipeline  = nullptr;
	SDL_GPUGraphicsPipeline *ui_composite_pipeline = nullptr;
	SDL_GPUGraphicsPipeline *lobby_panel_blur_pipeline = nullptr;
	SDL_GPUGraphicsPipeline *lobby_panel_copy_pipeline = nullptr;
	SDL_GPUGraphicsPipeline *light_pipeline    = nullptr;
	SDL_GPUGraphicsPipeline *particle_pipeline = nullptr;
	SDL_GPUComputePipeline  *particle_compute  = nullptr;

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

	// --- Pending UI overlay upload ---
	const Uint8 *pending_ui_pixels = nullptr;
	int          pending_ui_w      = 0;
	int          pending_ui_h      = 0;
	bool         ui_dirty          = false;
	bool         ui_present        = false;
	Uint8        ui_global_alpha   = 255;   // FADEOUT dim, 255 = no change
	bool         frame_dirty    = false;

	// --- Pending lighting ---
	static const int kMaxLights = 64;
	struct LightEntry { float x, y, radius, intensity, r, g, b; };
	LightEntry pending_lights[kMaxLights];
	int        pending_light_count = 0;
	bool       lighting_active     = false;

	// --- Pending particle operations ---
	static const int kMaxParticleBuffers = 8;

	// Must match the MSL Particle layout (4-byte aligned, 32 bytes).
	struct GPUParticle { float x, y, vx, vy, life, max_life; Uint32 color_idx, flags; };

	struct ParticleBuffer { SDL_GPUBuffer *buf = nullptr; Uint32 capacity = 0; };
	ParticleBuffer particle_bufs[kMaxParticleBuffers];

	struct PendingParticleUpdate { int handle; Uint32 count; float dt; };
	struct PendingParticleDraw   { int handle; Uint32 count; };
	PendingParticleUpdate pending_particle_updates[kMaxParticleBuffers];
	PendingParticleDraw   pending_particle_draws[kMaxParticleBuffers];
	int pending_particle_update_count = 0;
	int pending_particle_draw_count   = 0;

	std::vector<SDL_Rect> pending_lobby_panel_border_blur_rects;
	int pending_lobby_panel_blur_virtual_w = 0;
	int pending_lobby_panel_blur_virtual_h = 0;
	float pending_lobby_panel_blur_scale = 1.0f;
};

#endif
