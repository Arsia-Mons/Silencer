#include "sdl3gpubackend.h"
#include <string.h>

// ---------------------------------------------------------------------------
// MSL shaders (Metal Shading Language — macOS/Metal backend).
// For Vulkan/D3D12: add SPIR-V / DXIL paths selected via SDL_GetGPUShaderFormats().
// ---------------------------------------------------------------------------

// Fullscreen triangle generated from vertex_id — no vertex buffer required.
// NDC coords: (-1,-1), (3,-1), (-1,3) cover the entire clip space with one triangle.
// UV coords:  (0,0), (2,0), (0,2) — hardware clips the excess to [0,1].
static const char *kVertMSL = R"(
#include <metal_stdlib>
using namespace metal;
struct VertexOut { float4 position [[position]]; float2 uv; };
vertex VertexOut vert(uint vid [[vertex_id]]) {
    const float2 pos[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    const float2 uv[3]  = {float2(0,1),  float2(2,1),  float2(0,-1)};
    VertexOut out;
    out.position = float4(pos[vid], 0, 1);
    out.uv = uv[vid];
    return out;
}
)";

// Palette remap: sample indexed (R8) texture, map the 0-1 value to a palette
// texel centre, sample the RGBA palette texture.
// UV math: R8_UNORM stores byte n as n/255.0.  Palette is 256 texels wide, so
// texel n sits at (n+0.5)/256.0.  Substituting: u = idx*(255/256) + 0.5/256.
static const char *kFragMSL = R"(
#include <metal_stdlib>
using namespace metal;
struct VertexOut { float4 position [[position]]; float2 uv; };
fragment float4 frag(VertexOut in [[stage_in]],
    texture2d<float> frame   [[texture(0)]],
    texture2d<float> palette [[texture(1)]],
    sampler frame_s   [[sampler(0)]],
    sampler palette_s [[sampler(1)]]) {
    float idx = frame.sample(frame_s, in.uv).r;
    float u   = idx * (255.0 / 256.0) + 0.5 / 256.0;
    return palette.sample(palette_s, float2(u, 0.5));
}
)";

// ---------------------------------------------------------------------------
SDL3GPUBackend::SDL3GPUBackend() = default;
SDL3GPUBackend::~SDL3GPUBackend() { Shutdown(); }

bool SDL3GPUBackend::Init(SDL_Window *win) {
	window = win;

	// Phase 2 targets Metal on macOS.  Future: pass format_flags based on
	// SDL_GetGPUShaderFormats() to support Vulkan (SPIRV) / D3D12 (DXIL).
	device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, NULL);
	if (!device) {
		SDL_Log("SDL3GPUBackend: SDL_CreateGPUDevice failed: %s", SDL_GetError());
		return false;
	}

	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		SDL_Log("SDL3GPUBackend: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
		return false;
	}

	if (!CreatePipeline()) return false;

	SDL_GPUSamplerCreateInfo si = {};
	si.min_filter    = SDL_GPU_FILTER_NEAREST;
	si.mag_filter    = SDL_GPU_FILTER_NEAREST;
	si.mipmap_mode   = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler = SDL_CreateGPUSampler(device, &si);
	if (!sampler) {
		SDL_Log("SDL3GPUBackend: SDL_CreateGPUSampler failed: %s", SDL_GetError());
		return false;
	}

	return true;
}

void SDL3GPUBackend::Shutdown() {
	if (!device) return;
	SDL_WaitForGPUIdle(device);
	if (frame_tex)    { SDL_ReleaseGPUTexture(device, frame_tex);   frame_tex   = nullptr; }
	if (palette_tex)  { SDL_ReleaseGPUTexture(device, palette_tex); palette_tex = nullptr; }
	if (pipeline)     { SDL_ReleaseGPUGraphicsPipeline(device, pipeline); pipeline = nullptr; }
	if (sampler)      { SDL_ReleaseGPUSampler(device, sampler);     sampler     = nullptr; }
	if (frame_tbuf)   { SDL_ReleaseGPUTransferBuffer(device, frame_tbuf);   frame_tbuf   = nullptr; }
	if (palette_tbuf) { SDL_ReleaseGPUTransferBuffer(device, palette_tbuf); palette_tbuf = nullptr; }
	SDL_ReleaseWindowFromGPUDevice(device, window);
	SDL_DestroyGPUDevice(device);
	device = nullptr;
}

SDL_GPUShader *SDL3GPUBackend::LoadShader(SDL_GPUShaderStage stage,
                                           const char *msl_source,
                                           const char *entrypoint,
                                           Uint32 num_samplers) {
	SDL_GPUShaderCreateInfo info = {};
	info.code        = (const Uint8 *)msl_source;
	info.code_size   = strlen(msl_source);
	info.format      = SDL_GPU_SHADERFORMAT_MSL;
	info.stage       = stage;
	info.entrypoint  = entrypoint;
	info.num_samplers = num_samplers;
	return SDL_CreateGPUShader(device, &info);
}

bool SDL3GPUBackend::CreatePipeline() {
	SDL_GPUShader *vert = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertMSL, "vert", 0);
	SDL_GPUShader *frag = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragMSL, "frag", 2);
	if (!vert || !frag) {
		SDL_Log("SDL3GPUBackend: shader compilation failed: %s", SDL_GetError());
		if (vert) SDL_ReleaseGPUShader(device, vert);
		if (frag) SDL_ReleaseGPUShader(device, frag);
		return false;
	}

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = SDL_GetGPUSwapchainTextureFormat(device, window);

	SDL_GPUGraphicsPipelineCreateInfo pi = {};
	pi.vertex_shader        = vert;
	pi.fragment_shader      = frag;
	pi.primitive_type       = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pi.target_info.color_target_descriptions   = &color_target;
	pi.target_info.num_color_targets           = 1;
	pi.target_info.has_depth_stencil_target    = false;

	pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
	SDL_ReleaseGPUShader(device, vert);
	SDL_ReleaseGPUShader(device, frag);
	if (!pipeline) {
		SDL_Log("SDL3GPUBackend: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
		return false;
	}
	return true;
}

void SDL3GPUBackend::SetPalette(const SDL_Color *colors, int count) {
	int n = count < 256 ? count : 256;
	for (int i = 0; i < n; i++) {
		palette_colors[i] = colors[i];
		palette_colors[i].a = 255; // GPU RGBA upload requires explicit alpha
	}
	palette_dirty = true;
}

void SDL3GPUBackend::UploadFrame(const Uint8 *indexed_pixels, int w, int h) {
	pending_pixels = indexed_pixels;
	pending_w      = w;
	pending_h      = h;
	frame_dirty    = true;
}

void SDL3GPUBackend::SetScaleFilter(bool /*linear*/) {
	// Phase 2: always nearest-pixel (can't linear-filter palette indices).
	// Phase 3: second render-target pass will upscale with bilinear filter.
}

void SDL3GPUBackend::Present() {
	if (!device || !pipeline) return;

	// Lazily create/resize frame texture.
	if (frame_dirty && pending_pixels) {
		if (!frame_tex ||
		    frame_tex_w != pending_w ||
		    frame_tex_h != pending_h) {
			if (frame_tex) { SDL_ReleaseGPUTexture(device, frame_tex); }
			SDL_GPUTextureCreateInfo ti = {};
			ti.type        = SDL_GPU_TEXTURETYPE_2D;
			ti.format      = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
			ti.usage       = SDL_GPU_TEXTUREUSAGE_SAMPLER;
			ti.width       = (Uint32)pending_w;
			ti.height      = (Uint32)pending_h;
			ti.layer_count_or_depth = 1;
			ti.num_levels  = 1;
			frame_tex   = SDL_CreateGPUTexture(device, &ti);
			frame_tex_w = pending_w;
			frame_tex_h = pending_h;
		}
		// Resize transfer buffer if needed.
		Uint32 needed = (Uint32)(pending_w * pending_h);
		if (!frame_tbuf || frame_tbuf_sz < needed) {
			if (frame_tbuf) SDL_ReleaseGPUTransferBuffer(device, frame_tbuf);
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size  = needed;
			frame_tbuf    = SDL_CreateGPUTransferBuffer(device, &tbi);
			frame_tbuf_sz = needed;
		}
	}

	// Lazily create palette texture.
	if (!palette_tex) {
		SDL_GPUTextureCreateInfo ti = {};
		ti.type        = SDL_GPU_TEXTURETYPE_2D;
		ti.format      = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ti.usage       = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		ti.width       = 256;
		ti.height      = 1;
		ti.layer_count_or_depth = 1;
		ti.num_levels  = 1;
		palette_tex = SDL_CreateGPUTexture(device, &ti);

		// First use — ensure transfer buffer exists.
		if (!palette_tbuf) {
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size  = 256 * 4;
			palette_tbuf = SDL_CreateGPUTransferBuffer(device, &tbi);
		}
		palette_dirty = true;
	}

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
	if (!cmd) return;

	// Upload passes (only when data changed).
	if ((frame_dirty && pending_pixels) || palette_dirty) {
		SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

		if (frame_dirty && pending_pixels && frame_tex && frame_tbuf) {
			Uint8 *dst = (Uint8 *)SDL_MapGPUTransferBuffer(device, frame_tbuf, false);
			if (dst) {
				memcpy(dst, pending_pixels, (size_t)(pending_w * pending_h));
				SDL_UnmapGPUTransferBuffer(device, frame_tbuf);
				SDL_GPUTextureTransferInfo src = {};
				src.transfer_buffer = frame_tbuf;
				src.rows_per_layer  = (Uint32)pending_h;
				src.pixels_per_row  = (Uint32)pending_w;
				SDL_GPUTextureRegion region = {};
				region.texture = frame_tex;
				region.w = (Uint32)pending_w;
				region.h = (Uint32)pending_h;
				region.d = 1;
				SDL_UploadToGPUTexture(copy, &src, &region, false);
			}
			frame_dirty = false;
		}

		if (palette_dirty && palette_tex && palette_tbuf) {
			Uint8 *dst = (Uint8 *)SDL_MapGPUTransferBuffer(device, palette_tbuf, false);
			if (dst) {
				memcpy(dst, palette_colors, 256 * 4);
				SDL_UnmapGPUTransferBuffer(device, palette_tbuf);
				SDL_GPUTextureTransferInfo src = {};
				src.transfer_buffer = palette_tbuf;
				src.rows_per_layer  = 1;
				src.pixels_per_row  = 256;
				SDL_GPUTextureRegion region = {};
				region.texture = palette_tex;
				region.w = 256;
				region.h = 1;
				region.d = 1;
				SDL_UploadToGPUTexture(copy, &src, &region, false);
			}
			palette_dirty = false;
		}

		SDL_EndGPUCopyPass(copy);
	}

	// Acquire swapchain — returns null when minimized; skip render pass.
	SDL_GPUTexture *swapchain = nullptr;
	Uint32 sw_w = 0, sw_h = 0;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchain, &sw_w, &sw_h);
	if (!swapchain) {
		SDL_SubmitGPUCommandBuffer(cmd);
		return;
	}

	SDL_GPUColorTargetInfo ct = {};
	ct.texture     = swapchain;
	ct.load_op     = SDL_GPU_LOADOP_CLEAR;
	ct.store_op    = SDL_GPU_STOREOP_STORE;
	ct.clear_color = {0, 0, 0, 1};

	SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
	if (pass && frame_tex && palette_tex) {
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		SDL_GPUTextureSamplerBinding bindings[2] = {
			{frame_tex,   sampler},
			{palette_tex, sampler},
		};
		SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
	}
	if (pass) SDL_EndGPURenderPass(pass);

	SDL_SubmitGPUCommandBuffer(cmd);
}
