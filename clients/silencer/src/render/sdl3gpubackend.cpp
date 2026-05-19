#include "sdl3gpubackend.h"
#include <string.h>
#include <vector>

// DXIL bytecode generated from the HLSL sources in clients/silencer/shaders/
// at build time by dxc. The headers define `static const unsigned char
// k<Name>DXIL[]`; size comes from `sizeof()` — dxc -Fh doesn't emit one.
#include "vert_screen.dxil.h"
#include "frag_remap.dxil.h"
#include "frag_upscale.dxil.h"
#include "frag_light.dxil.h"
#include "vert_particle.dxil.h"
#include "frag_particle.dxil.h"
#include "comp_particle.dxil.h"

// ---------------------------------------------------------------------------
// MSL Shaders (Metal Shading Language — macOS/Metal backend).
// HLSL counterparts live in clients/silencer/shaders/*.hlsl and produce DXIL
// for Windows/D3D12. Keep these two in sync — they share entry-point names.
// ---------------------------------------------------------------------------

// Shared fullscreen-triangle vertex shader — generates 3 vertices from vertex_id
// (no VBO). Y-flipped UVs to match Metal's top-down pixel convention.
static const char *kVertScreenMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
vertex VOut vert_screen(uint vid [[vertex_id]]) {
    const float2 pos[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };
    const float2 uv[3]  = { float2(0,1),  float2(2,1),  float2(0,-1) };
    VOut o; o.pos = float4(pos[vid],0,1); o.uv = uv[vid]; return o;
}
)msl";

// Palette remap: R8_UNORM indexed frame + RGBA palette → scene_tex (RGBA8).
// UV math: R8 stores byte n as n/255.  Palette texel n sits at (n+0.5)/256.
static const char *kFragRemapMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
fragment float4 frag_remap(VOut in [[stage_in]],
    texture2d<float> frame   [[texture(0)]],
    texture2d<float> palette [[texture(1)]],
    sampler samp [[sampler(0)]]) {
    float idx = frame.sample(samp, in.uv).r;
    float u   = idx * (255.0/256.0) + 0.5/256.0;
    return palette.sample(samp, float2(u, 0.5));
}
)msl";

// Upscale: sample scene_tex with the chosen filter (nearest or bilinear)
// and write to swapchain. Sampler is selected at bind time in Present().
static const char *kFragUpscaleMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
fragment float4 frag_upscale(VOut in [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    sampler samp [[sampler(0)]]) {
    return scene.sample(samp, in.uv);
}
)msl";

// Lobby panel-border post-process. This intentionally samples the rendered
// scene texture, spreading the existing border pixels outward without tying
// the effect to any palette index.
static const char *kFragLobbyPanelBlurMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
fragment float4 frag_upscale(VOut in [[stage_in]],
    texture2d<float> scene  [[texture(0)]],
    texture2d<float> border [[texture(1)]],
    sampler samp [[sampler(0)]]) {
    const float2 texel = 1.0 / float2(scene.get_width(), scene.get_height());
    float4 base = scene.sample(samp, in.uv);
    float3 accum = float3(0.0);
    float alpha = 0.0;
    const float radiusLimit = 10.0;

    for (int i = 1; i <= 10; ++i) {
        float radius = float(i) - 0.35;
        float diagonal = radius * 0.70710678;
        float falloff = 1.0 - (float(i) / (radiusLimit + 1.0));
        float weight = falloff * falloff * 0.18;

        float4 tap = border.sample(samp, in.uv + float2( radius, 0.0) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2(-radius, 0.0) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2(0.0,  radius) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2(0.0, -radius) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2( diagonal,  diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2(-diagonal,  diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2( diagonal, -diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.sample(samp, in.uv + float2(-diagonal, -diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
    }

    if (alpha <= 0.0001) {
        return base;
    }
    float3 blurred = accum / alpha;
    return float4(mix(base.rgb, blurred, saturate(alpha)), base.a);
}
)msl";

// Additive emissive light disc rendered into scene_tex.
// Pixel-space distance avoids aspect-ratio distortion.
// Uniform buffer 0 layout (48 bytes = 3 × float4):
//   float4[0]: cx, cy, radius, intensity
//   float4[1]: r,  g,  b,     game_w
//   float4[2]: game_h, (pad x3)
static const char *kFragLightMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
struct LightParams {
    float cx, cy, radius, intensity;
    float r, g, b, gw;
    float gh, _p0, _p1, _p2;
};
fragment float4 frag_light(VOut in [[stage_in]],
    constant LightParams& p [[buffer(0)]]) {
    float2 px = in.uv * float2(p.gw, p.gh);
    float  d  = length(px - float2(p.cx, p.cy));
    float  f  = 1.0 - smoothstep(0.0, p.radius, d);
    f = f * f; // quadratic falloff
    float3 contrib = float3(p.r, p.g, p.b) * f * p.intensity;
    return float4(contrib, f * p.intensity);
}
)msl";

// Particle vertex shader: derives quad corners from a GPU storage buffer.
// 6 vertices per particle (2 triangles). Dead particles (life<=0) produce
// a degenerate quad outside clip space and are silently discarded.
// Uniform buffer 0: float4(game_w, game_h, 0, 0).
static const char *kVertParticleMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct Particle { float x,y,vx,vy,life,max_life; uint color_idx,flags; };
struct PVert { float4 pos [[position]]; float pal_u; };
vertex PVert vert_particle(uint vid [[vertex_id]],
    device const Particle* parts [[buffer(0)]],
    constant float4& fi [[buffer(1)]]) {
    const float2 off[6] = {
        float2(-1.5,-1.5), float2(1.5,-1.5), float2(-1.5,1.5),
        float2(-1.5, 1.5), float2(1.5,-1.5), float2( 1.5,1.5)
    };
    uint pid = vid / 6;
    uint cor = vid % 6;
    Particle p = parts[pid];
    float2 ndc = (float2(p.x, p.y) + off[cor]) / fi.xy * 2.0 - 1.0;
    ndc.y = -ndc.y;
    PVert o;
    o.pos   = p.life > 0.0 ? float4(ndc, 0, 1) : float4(2, 2, 0, 0);
    o.pal_u = float(p.color_idx) * (255.0/256.0) + 0.5/256.0;
    return o;
}
)msl";

// Particle fragment: palette lookup using the index baked into PVert.pal_u.
static const char *kFragParticleMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct PVert { float4 pos [[position]]; float pal_u; };
fragment float4 frag_particle(PVert in [[stage_in]],
    texture2d<float> palette [[texture(0)]],
    sampler samp [[sampler(0)]]) {
    return palette.sample(samp, float2(in.pal_u, 0.5));
}
)msl";

// Compute kernel: advance particle positions by dt, decay life.
// Dispatch with threadcount_x=64; caller rounds count up to 64.
static const char *kComputeParticleMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct Particle { float x,y,vx,vy,life,max_life; uint color_idx,flags; };
kernel void update_particles(
    device Particle* parts [[buffer(0)]],
    constant float&  dt    [[buffer(1)]],
    uint id [[thread_position_in_grid]]) {
    Particle p = parts[id];
    if (p.life <= 0.0f) return;
    p.x    += p.vx * dt;
    p.y    += p.vy * dt;
    p.life -= dt;
    if (p.life < 0.0f) p.life = 0.0f;
    parts[id] = p;
}
)msl";

// ---------------------------------------------------------------------------
// Per-shader bundles: every Init/CreatePipeline call picks msl_src vs dxil
// based on `chosen_format` set at backend Init.
// ---------------------------------------------------------------------------
static const ShaderBundle kVertScreen   = { kVertScreenMSL,   kVertScreenDXIL,   sizeof(kVertScreenDXIL),   "vert_screen"      };
static const ShaderBundle kFragRemap    = { kFragRemapMSL,    kFragRemapDXIL,    sizeof(kFragRemapDXIL),    "frag_remap"       };
static const ShaderBundle kFragUpscale  = { kFragUpscaleMSL,  kFragUpscaleDXIL,  sizeof(kFragUpscaleDXIL),  "frag_upscale"     };
static const ShaderBundle kFragLobbyPanelBlur = { kFragLobbyPanelBlurMSL, kFragUpscaleDXIL, sizeof(kFragUpscaleDXIL), "frag_upscale" };
static const ShaderBundle kFragLight    = { kFragLightMSL,    kFragLightDXIL,    sizeof(kFragLightDXIL),    "frag_light"       };
static const ShaderBundle kVertParticle = { kVertParticleMSL, kVertParticleDXIL, sizeof(kVertParticleDXIL), "vert_particle"    };
static const ShaderBundle kFragParticle = { kFragParticleMSL, kFragParticleDXIL, sizeof(kFragParticleDXIL), "frag_particle"    };
static const ShaderBundle kCompParticle = { kComputeParticleMSL, kCompParticleDXIL, sizeof(kCompParticleDXIL), "update_particles" };

namespace {

constexpr int kLobbyPanelBlurRadius = 10;

struct LobbyPanelBlurRects {
	std::vector<SDL_Rect> blur;
	std::vector<SDL_Rect> restore;
};

SDL_Rect ClampRect(SDL_Rect r, int w, int h) {
	if (r.x < 0) {
		r.w += r.x;
		r.x = 0;
	}
	if (r.y < 0) {
		r.h += r.y;
		r.y = 0;
	}
	if (r.x + r.w > w) r.w = w - r.x;
	if (r.y + r.h > h) r.h = h - r.y;
	if (r.w < 0) r.w = 0;
	if (r.h < 0) r.h = 0;
	return r;
}

int CeilScaled(float value) {
	return static_cast<int>(value + 0.9999f);
}

SDL_Rect ScaleLobbyPanelRect(SDL_Rect rect,
                             int virtualW,
                             int virtualH,
                             float scale,
                             int targetW,
                             int targetH) {
	if (virtualW <= 0 || virtualH <= 0 || scale <= 0.0f) {
		return ClampRect(rect, targetW, targetH);
	}
	const int scaledW = static_cast<int>((float)virtualW * scale + 0.5f);
	const int scaledH = static_cast<int>((float)virtualH * scale + 0.5f);
	const int offsetX = scaledW < targetW ? (targetW - scaledW) / 2 : 0;
	const int offsetY = scaledH < targetH ? (targetH - scaledH) / 2 : 0;
	const int x0 = offsetX + static_cast<int>((float)rect.x * scale);
	const int y0 = offsetY + static_cast<int>((float)rect.y * scale);
	const int x1 = offsetX + CeilScaled((float)(rect.x + rect.w) * scale);
	const int y1 = offsetY + CeilScaled((float)(rect.y + rect.h) * scale);
	return ClampRect(SDL_Rect{ x0, y0, x1 - x0, y1 - y0 }, targetW, targetH);
}

void AddLobbyPanelBlurRect(LobbyPanelBlurRects &rects,
                           SDL_Rect exact,
                           int blurRadius,
                           int w,
                           int h) {
	exact = ClampRect(exact, w, h);
	if (exact.w <= 0 || exact.h <= 0) return;
	rects.restore.push_back(exact);

	SDL_Rect blur = {
		exact.x - blurRadius,
		exact.y - blurRadius,
		exact.w + blurRadius * 2,
		exact.h + blurRadius * 2,
	};
	blur = ClampRect(blur, w, h);
	if (blur.w > 0 && blur.h > 0) rects.blur.push_back(blur);
}

LobbyPanelBlurRects BuildLobbyPanelBlurRects(const std::vector<SDL_Rect> & source,
                                             int w,
                                             int h,
                                             int virtualW,
                                             int virtualH,
                                             float scale,
                                             int blurRadius) {
	LobbyPanelBlurRects rects;
	if (w <= 0 || h <= 0) return rects;
	for (const SDL_Rect & rect : source) {
		AddLobbyPanelBlurRect(
			rects,
			ScaleLobbyPanelRect(rect, virtualW, virtualH, scale, w, h),
			blurRadius,
			w,
			h);
	}
	return rects;
}

}  // namespace

// ---------------------------------------------------------------------------
SDL3GPUBackend::SDL3GPUBackend() = default;
SDL3GPUBackend::~SDL3GPUBackend() { Shutdown(); }

bool SDL3GPUBackend::Init(SDL_Window *win) {
	window = win;

	const SDL_GPUShaderFormat formats =
	    SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;
	device = SDL_CreateGPUDevice(formats, false, NULL);
	if (!device) {
		SDL_Log("SDL3GPUBackend: SDL_CreateGPUDevice failed: %s", SDL_GetError());
		return false;
	}

	// Reduce the device's reported format set to the one we actually feed
	// shaders in. Prefer DXIL on Windows/D3D12, MSL on Apple/Metal.
	const SDL_GPUShaderFormat avail = SDL_GetGPUShaderFormats(device);
	if (avail & SDL_GPU_SHADERFORMAT_DXIL)      chosen_format = SDL_GPU_SHADERFORMAT_DXIL;
	else if (avail & SDL_GPU_SHADERFORMAT_MSL)  chosen_format = SDL_GPU_SHADERFORMAT_MSL;
	else {
		SDL_Log("SDL3GPUBackend: no compatible shader format (driver reports 0x%x)",
		        (unsigned)avail);
		return false;
	}
	SDL_Log("SDL3GPUBackend: chosen shader format=%s",
	        chosen_format == SDL_GPU_SHADERFORMAT_DXIL ? "DXIL" : "MSL");

	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		SDL_Log("SDL3GPUBackend: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
		return false;
	}

	SDL_GPUSamplerCreateInfo si = {};
	si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	si.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

	si.min_filter = SDL_GPU_FILTER_NEAREST;
	si.mag_filter = SDL_GPU_FILTER_NEAREST;
	nearest_sampler = SDL_CreateGPUSampler(device, &si);
	if (!nearest_sampler) {
		SDL_Log("SDL3GPUBackend: nearest sampler failed: %s", SDL_GetError());
		return false;
	}

	si.min_filter = SDL_GPU_FILTER_LINEAR;
	si.mag_filter = SDL_GPU_FILTER_LINEAR;
	linear_sampler = SDL_CreateGPUSampler(device, &si);
	if (!linear_sampler) {
		SDL_Log("SDL3GPUBackend: linear sampler failed: %s", SDL_GetError());
		return false;
	}

	return CreatePipelines();
}

void SDL3GPUBackend::Shutdown() {
	if (!device) return;
	SDL_WaitForGPUIdle(device);

	if (frame_tex)         { SDL_ReleaseGPUTexture(device, frame_tex);          frame_tex         = nullptr; }
	if (palette_tex)       { SDL_ReleaseGPUTexture(device, palette_tex);        palette_tex       = nullptr; }
	if (scene_tex)         { SDL_ReleaseGPUTexture(device, scene_tex);          scene_tex         = nullptr; }
	if (lobby_panel_source_tex) { SDL_ReleaseGPUTexture(device, lobby_panel_source_tex); lobby_panel_source_tex = nullptr; }
	if (lobby_panel_mask_tex) { SDL_ReleaseGPUTexture(device, lobby_panel_mask_tex); lobby_panel_mask_tex = nullptr; }
	if (remap_pipeline)    { SDL_ReleaseGPUGraphicsPipeline(device, remap_pipeline);   remap_pipeline   = nullptr; }
	if (upscale_pipeline)  { SDL_ReleaseGPUGraphicsPipeline(device, upscale_pipeline); upscale_pipeline = nullptr; }
	if (lobby_panel_blur_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, lobby_panel_blur_pipeline); lobby_panel_blur_pipeline = nullptr; }
	if (lobby_panel_copy_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, lobby_panel_copy_pipeline); lobby_panel_copy_pipeline = nullptr; }
	if (light_pipeline)    { SDL_ReleaseGPUGraphicsPipeline(device, light_pipeline);   light_pipeline   = nullptr; }
	if (particle_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, particle_pipeline); particle_pipeline = nullptr; }
	if (particle_compute)  { SDL_ReleaseGPUComputePipeline(device, particle_compute);  particle_compute  = nullptr; }
	if (nearest_sampler)   { SDL_ReleaseGPUSampler(device, nearest_sampler);    nearest_sampler   = nullptr; }
	if (linear_sampler)    { SDL_ReleaseGPUSampler(device, linear_sampler);     linear_sampler    = nullptr; }
	if (frame_tbuf)        { SDL_ReleaseGPUTransferBuffer(device, frame_tbuf);  frame_tbuf        = nullptr; }
	if (palette_tbuf)      { SDL_ReleaseGPUTransferBuffer(device, palette_tbuf); palette_tbuf     = nullptr; }

	for (int i = 0; i < kMaxParticleBuffers; i++) {
		if (particle_bufs[i].buf) {
			SDL_ReleaseGPUBuffer(device, particle_bufs[i].buf);
			particle_bufs[i] = {};
		}
	}

	SDL_ReleaseWindowFromGPUDevice(device, window);
	SDL_DestroyGPUDevice(device);
	device = nullptr;
}

SDL_GPUShader *SDL3GPUBackend::LoadShader(SDL_GPUShaderStage stage,
                                           const ShaderBundle &b,
                                           Uint32 num_samplers,
                                           Uint32 num_uniform_buffers,
                                           Uint32 num_storage_buffers) {
	SDL_GPUShaderCreateInfo info = {};
	if (chosen_format == SDL_GPU_SHADERFORMAT_DXIL) {
		info.code      = b.dxil;
		info.code_size = b.dxil_size;
		info.format    = SDL_GPU_SHADERFORMAT_DXIL;
	} else {
		info.code      = (const Uint8 *)b.msl_src;
		info.code_size = strlen(b.msl_src);
		info.format    = SDL_GPU_SHADERFORMAT_MSL;
	}
	info.stage               = stage;
	info.entrypoint          = b.entrypoint;
	info.num_samplers        = num_samplers;
	info.num_uniform_buffers = num_uniform_buffers;
	info.num_storage_buffers = num_storage_buffers;
	return SDL_CreateGPUShader(device, &info);
}

bool SDL3GPUBackend::CreatePipelines() {
	// --- Remap pipeline: R8 indexed frame → scene_tex (RGBA8) ---
	{
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen, 0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragRemap,  2);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: remap shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}

		SDL_GPUColorTargetDescription ct = {};
		ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;

		remap_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!remap_pipeline) {
			SDL_Log("SDL3GPUBackend: remap pipeline failed: %s", SDL_GetError());
			return false;
		}
	}

	// --- Upscale pipeline: scene_tex → swapchain (swapchain format) ---
	{
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen,  0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUpscale, 1);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: upscale shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}

		SDL_GPUColorTargetDescription ct = {};
		ct.format = SDL_GetGPUSwapchainTextureFormat(device, window);

		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;

		upscale_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!upscale_pipeline) {
			SDL_Log("SDL3GPUBackend: upscale pipeline failed: %s", SDL_GetError());
			return false;
		}
	}

	return CreateLobbyPanelBlurPipeline();
}

bool SDL3GPUBackend::CreateLobbyPanelBlurPipeline() {
	const Uint32 blurSamplers =
		chosen_format == SDL_GPU_SHADERFORMAT_MSL ? 2u : 1u;
	SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen,  0);
	SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT,
	                               kFragLobbyPanelBlur,
	                               blurSamplers);
	if (!vs || !fs) {
		SDL_Log("SDL3GPUBackend: lobby panel blur shaders failed: %s", SDL_GetError());
		if (vs) SDL_ReleaseGPUShader(device, vs);
		if (fs) SDL_ReleaseGPUShader(device, fs);
		return false;
	}

	SDL_GPUColorTargetDescription ct = {};
	ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

	SDL_GPUGraphicsPipelineCreateInfo pi = {};
	pi.vertex_shader   = vs;
	pi.fragment_shader = fs;
	pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pi.target_info.color_target_descriptions = &ct;
	pi.target_info.num_color_targets         = 1;

	lobby_panel_blur_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
	SDL_ReleaseGPUShader(device, vs);
	SDL_ReleaseGPUShader(device, fs);
	if (!lobby_panel_blur_pipeline) {
		SDL_Log("SDL3GPUBackend: lobby panel blur pipeline failed: %s", SDL_GetError());
		return false;
	}

	vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen,  0);
	fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUpscale, 1);
	if (!vs || !fs) {
		SDL_Log("SDL3GPUBackend: lobby panel copy shaders failed: %s", SDL_GetError());
		if (vs) SDL_ReleaseGPUShader(device, vs);
		if (fs) SDL_ReleaseGPUShader(device, fs);
		return false;
	}

	pi = {};
	pi.vertex_shader   = vs;
	pi.fragment_shader = fs;
	pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pi.target_info.color_target_descriptions = &ct;
	pi.target_info.num_color_targets         = 1;

	lobby_panel_copy_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
	SDL_ReleaseGPUShader(device, vs);
	SDL_ReleaseGPUShader(device, fs);
	if (!lobby_panel_copy_pipeline) {
		SDL_Log("SDL3GPUBackend: lobby panel copy pipeline failed: %s", SDL_GetError());
		return false;
	}
	return true;
}

bool SDL3GPUBackend::CreateLightPipeline() {
	// Additive blend: src×srcAlpha + dst×1 (classic emissive additive)
	SDL_GPUColorTargetBlendState blend = {};
	blend.enable_blend             = true;
	blend.src_color_blendfactor    = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	blend.dst_color_blendfactor    = SDL_GPU_BLENDFACTOR_ONE;
	blend.color_blend_op           = SDL_GPU_BLENDOP_ADD;
	blend.src_alpha_blendfactor    = SDL_GPU_BLENDFACTOR_ZERO;
	blend.dst_alpha_blendfactor    = SDL_GPU_BLENDFACTOR_ONE;
	blend.alpha_blend_op           = SDL_GPU_BLENDOP_ADD;

	SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen, 0);
	SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragLight,
	                               /*num_samplers=*/0, /*num_uniform_buffers=*/1);
	if (!vs || !fs) {
		SDL_Log("SDL3GPUBackend: light shaders failed: %s", SDL_GetError());
		if (vs) SDL_ReleaseGPUShader(device, vs);
		if (fs) SDL_ReleaseGPUShader(device, fs);
		return false;
	}

	SDL_GPUColorTargetDescription ct = {};
	ct.format      = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	ct.blend_state = blend;

	SDL_GPUGraphicsPipelineCreateInfo pi = {};
	pi.vertex_shader   = vs;
	pi.fragment_shader = fs;
	pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pi.target_info.color_target_descriptions = &ct;
	pi.target_info.num_color_targets         = 1;

	light_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
	SDL_ReleaseGPUShader(device, vs);
	SDL_ReleaseGPUShader(device, fs);
	if (!light_pipeline) {
		SDL_Log("SDL3GPUBackend: light pipeline failed: %s", SDL_GetError());
		return false;
	}
	return true;
}

bool SDL3GPUBackend::CreateParticlePipelines() {
	// Compute pipeline: advance particle positions.
	{
		SDL_GPUComputePipelineCreateInfo ci = {};
		if (chosen_format == SDL_GPU_SHADERFORMAT_DXIL) {
			ci.code      = kCompParticle.dxil;
			ci.code_size = kCompParticle.dxil_size;
			ci.format    = SDL_GPU_SHADERFORMAT_DXIL;
		} else {
			ci.code      = (const Uint8 *)kCompParticle.msl_src;
			ci.code_size = strlen(kCompParticle.msl_src);
			ci.format    = SDL_GPU_SHADERFORMAT_MSL;
		}
		ci.entrypoint                    = kCompParticle.entrypoint;
		ci.num_readwrite_storage_buffers = 1;
		ci.num_uniform_buffers           = 1;
		ci.threadcount_x                 = 64;
		ci.threadcount_y                 = 1;
		ci.threadcount_z                 = 1;
		particle_compute = SDL_CreateGPUComputePipeline(device, &ci);
		if (!particle_compute) {
			SDL_Log("SDL3GPUBackend: particle compute pipeline failed: %s", SDL_GetError());
			return false;
		}
	}

	// Graphics pipeline: draw particle quads additively into scene_tex.
	{
		// Vertex reads from storage buffer (particles) + uniform (frame_info).
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,
		                               kVertParticle,
		                               /*num_samplers=*/0,
		                               /*num_uniform_buffers=*/1,
		                               /*num_storage_buffers=*/1);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT,
		                               kFragParticle,
		                               /*num_samplers=*/1);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: particle shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}

		SDL_GPUColorTargetBlendState blend = {};
		blend.enable_blend             = true;
		blend.src_color_blendfactor    = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
		blend.dst_color_blendfactor    = SDL_GPU_BLENDFACTOR_ONE;
		blend.color_blend_op           = SDL_GPU_BLENDOP_ADD;
		blend.src_alpha_blendfactor    = SDL_GPU_BLENDFACTOR_ZERO;
		blend.dst_alpha_blendfactor    = SDL_GPU_BLENDFACTOR_ONE;
		blend.alpha_blend_op           = SDL_GPU_BLENDOP_ADD;

		SDL_GPUColorTargetDescription ct = {};
		ct.format      = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ct.blend_state = blend;

		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;

		particle_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!particle_pipeline) {
			SDL_Log("SDL3GPUBackend: particle graphics pipeline failed: %s", SDL_GetError());
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SDL3GPUBackend::SetPalette(const SDL_Color *colors, int count) {
	int n = count < 256 ? count : 256;
	for (int i = 0; i < n; i++) {
		palette_colors[i]   = colors[i];
		palette_colors[i].a = 255;
	}
	palette_dirty = true;
}

void SDL3GPUBackend::UploadFrame(const Uint8 *indexed_pixels, int w, int h) {
	pending_pixels = indexed_pixels;
	pending_w      = w;
	pending_h      = h;
	frame_dirty    = true;
}

void SDL3GPUBackend::SetScaleFilter(bool linear) {
	use_linear = linear;
}

void SDL3GPUBackend::BeginLobbyPanelBorderBlur(int virtualWidth,
                                               int virtualHeight,
                                               float uiScale) {
	pending_lobby_panel_border_blur_rects.clear();
	pending_lobby_panel_blur_virtual_w = virtualWidth;
	pending_lobby_panel_blur_virtual_h = virtualHeight;
	pending_lobby_panel_blur_scale = uiScale > 0.0f ? uiScale : 1.0f;
}

void SDL3GPUBackend::AddLobbyPanelBorderBlurRect(const SDL_Rect & rect) {
	if (rect.w <= 0 || rect.h <= 0) return;
	pending_lobby_panel_border_blur_rects.push_back(rect);
}

// Phase 3 — lighting
void SDL3GPUBackend::BeginLighting() {
	pending_light_count = 0;
	lighting_active     = true;
}

void SDL3GPUBackend::AddPointLight(float x, float y, float radius,
                                    SDL_Color color, float intensity) {
	if (!lighting_active || pending_light_count >= kMaxLights) return;
	LightEntry &le = pending_lights[pending_light_count++];
	le.x         = x;
	le.y         = y;
	le.radius    = radius;
	le.intensity = intensity;
	le.r         = color.r / 255.0f;
	le.g         = color.g / 255.0f;
	le.b         = color.b / 255.0f;
}

void SDL3GPUBackend::EndLighting() {
	// Lighting is composited in Present(); nothing to flush here.
}

// Phase 3 — GPU particles
int SDL3GPUBackend::AllocParticleBuffer(Uint32 count) {
	if (!device) return -1;
	// Lazy pipeline creation on first particle buffer allocation.
	if (!particle_compute && !CreateParticlePipelines()) return -1;

	for (int i = 0; i < kMaxParticleBuffers; i++) {
		if (!particle_bufs[i].buf) {
			SDL_GPUBufferCreateInfo bi = {};
			bi.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ  |
			           SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
			           SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
			bi.size  = sizeof(GPUParticle) * count;
			particle_bufs[i].buf      = SDL_CreateGPUBuffer(device, &bi);
			particle_bufs[i].capacity = count;
			return particle_bufs[i].buf ? i : -1;
		}
	}
	return -1; // no free slots
}

void SDL3GPUBackend::FreeParticleBuffer(int handle) {
	if (handle < 0 || handle >= kMaxParticleBuffers) return;
	if (particle_bufs[handle].buf) {
		SDL_WaitForGPUIdle(device);
		SDL_ReleaseGPUBuffer(device, particle_bufs[handle].buf);
		particle_bufs[handle] = {};
	}
}

void SDL3GPUBackend::DispatchParticleUpdate(int handle, Uint32 count, float dt) {
	if (handle < 0 || handle >= kMaxParticleBuffers) return;
	if (!particle_bufs[handle].buf) return;
	if (pending_particle_update_count >= kMaxParticleBuffers) return;
	PendingParticleUpdate &u = pending_particle_updates[pending_particle_update_count++];
	u.handle = handle;
	u.count  = count;
	u.dt     = dt;
}

void SDL3GPUBackend::DrawParticles(int handle, Uint32 count) {
	if (handle < 0 || handle >= kMaxParticleBuffers) return;
	if (!particle_bufs[handle].buf) return;
	if (pending_particle_draw_count >= kMaxParticleBuffers) return;
	PendingParticleDraw &d = pending_particle_draws[pending_particle_draw_count++];
	d.handle = handle;
	d.count  = count;
}

// ---------------------------------------------------------------------------
// Present — executes all queued work in a single command buffer.
// ---------------------------------------------------------------------------
void SDL3GPUBackend::Present() {
	if (!device || !remap_pipeline || !upscale_pipeline) return;

	// Lazily create/resize frame texture.
	if (frame_dirty && pending_pixels) {
		if (!frame_tex || frame_tex_w != pending_w || frame_tex_h != pending_h) {
			if (frame_tex) SDL_ReleaseGPUTexture(device, frame_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                   = SDL_GPU_TEXTURETYPE_2D;
			ti.format                 = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
			ti.usage                  = SDL_GPU_TEXTUREUSAGE_SAMPLER;
			ti.width                  = (Uint32)pending_w;
			ti.height                 = (Uint32)pending_h;
			ti.layer_count_or_depth   = 1;
			ti.num_levels             = 1;
			frame_tex   = SDL_CreateGPUTexture(device, &ti);
			frame_tex_w = pending_w;
			frame_tex_h = pending_h;
		}
		Uint32 needed = (Uint32)(pending_w * pending_h);
		if (!frame_tbuf || frame_tbuf_sz < needed) {
			if (frame_tbuf) SDL_ReleaseGPUTransferBuffer(device, frame_tbuf);
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage     = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size      = needed;
			frame_tbuf    = SDL_CreateGPUTransferBuffer(device, &tbi);
			frame_tbuf_sz = needed;
		}

		// Create/resize scene texture (RGBA8, COLOR_TARGET + SAMPLER).
		if (!scene_tex || scene_tex_w != pending_w || scene_tex_h != pending_h) {
			if (scene_tex) SDL_ReleaseGPUTexture(device, scene_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                 = SDL_GPU_TEXTURETYPE_2D;
			ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
			                          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			ti.width                = (Uint32)pending_w;
			ti.height               = (Uint32)pending_h;
			ti.layer_count_or_depth = 1;
			ti.num_levels           = 1;
			scene_tex   = SDL_CreateGPUTexture(device, &ti);
			scene_tex_w = pending_w;
			scene_tex_h = pending_h;
		}

		if (!lobby_panel_source_tex ||
		    lobby_panel_source_w != pending_w ||
		    lobby_panel_source_h != pending_h) {
			if (lobby_panel_source_tex) SDL_ReleaseGPUTexture(device, lobby_panel_source_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                 = SDL_GPU_TEXTURETYPE_2D;
			ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
			                          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			ti.width                = (Uint32)pending_w;
			ti.height               = (Uint32)pending_h;
			ti.layer_count_or_depth = 1;
			ti.num_levels           = 1;
			lobby_panel_source_tex = SDL_CreateGPUTexture(device, &ti);
			lobby_panel_source_w = pending_w;
			lobby_panel_source_h = pending_h;
		}

		if (!lobby_panel_mask_tex ||
		    lobby_panel_mask_w != pending_w ||
		    lobby_panel_mask_h != pending_h) {
			if (lobby_panel_mask_tex) SDL_ReleaseGPUTexture(device, lobby_panel_mask_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                 = SDL_GPU_TEXTURETYPE_2D;
			ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
			                          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			ti.width                = (Uint32)pending_w;
			ti.height               = (Uint32)pending_h;
			ti.layer_count_or_depth = 1;
			ti.num_levels           = 1;
			lobby_panel_mask_tex = SDL_CreateGPUTexture(device, &ti);
			lobby_panel_mask_w = pending_w;
			lobby_panel_mask_h = pending_h;
		}
	}

	// Lazily create palette texture.
	if (!palette_tex) {
		SDL_GPUTextureCreateInfo ti = {};
		ti.type                 = SDL_GPU_TEXTURETYPE_2D;
		ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		ti.width                = 256;
		ti.height               = 1;
		ti.layer_count_or_depth = 1;
		ti.num_levels           = 1;
		palette_tex = SDL_CreateGPUTexture(device, &ti);
		if (!palette_tbuf) {
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage    = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size     = 256 * 4;
			palette_tbuf = SDL_CreateGPUTransferBuffer(device, &tbi);
		}
		palette_dirty = true;
	}

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
	if (!cmd) return;

	LobbyPanelBlurRects lobby_blur_rects =
		BuildLobbyPanelBlurRects(
			pending_lobby_panel_border_blur_rects,
			pending_w,
			pending_h,
			pending_lobby_panel_blur_virtual_w,
			pending_lobby_panel_blur_virtual_h,
			pending_lobby_panel_blur_scale,
			kLobbyPanelBlurRadius);

	// ---- 1. Copy pass: upload frame + palette ----
	bool upload_frame = frame_dirty && pending_pixels && frame_tex && frame_tbuf;
	if (upload_frame || palette_dirty) {
		SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

		if (upload_frame) {
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

	// ---- 2. Compute pass: advance particle positions ----
	if (pending_particle_update_count > 0 && particle_compute) {
		for (int i = 0; i < pending_particle_update_count; i++) {
			PendingParticleUpdate &u = pending_particle_updates[i];
			int h = u.handle;
			if (h < 0 || h >= kMaxParticleBuffers || !particle_bufs[h].buf) continue;

			SDL_GPUStorageBufferReadWriteBinding rw = {};
			rw.buffer = particle_bufs[h].buf;
			rw.cycle  = false;

			SDL_GPUComputePass *cp = SDL_BeginGPUComputePass(cmd, nullptr, 0, &rw, 1);
			SDL_BindGPUComputePipeline(cp, particle_compute);
			SDL_PushGPUComputeUniformData(cmd, 0, &u.dt, sizeof(float));
			Uint32 groups = (u.count + 63) / 64;
			SDL_DispatchGPUCompute(cp, groups, 1, 1);
			SDL_EndGPUComputePass(cp);
		}
		pending_particle_update_count = 0;
	}

	// ---- 3. Remap pass: indexed frame → scene_tex (CLEAR) ----
	if (frame_tex && palette_tex && scene_tex) {
		SDL_GPUColorTargetInfo ct = {};
		ct.texture     = scene_tex;
		ct.load_op     = SDL_GPU_LOADOP_CLEAR;
		ct.store_op    = SDL_GPU_STOREOP_STORE;
		ct.clear_color = {0, 0, 0, 1};

		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
			SDL_BindGPUGraphicsPipeline(pass, remap_pipeline);
			SDL_GPUTextureSamplerBinding binds[2] = {
				{frame_tex,   nearest_sampler},
				{palette_tex, nearest_sampler},
			};
			SDL_BindGPUFragmentSamplers(pass, 0, binds, 2);
			SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(pass);
		}
	}

	// ---- 4. Lobby panel blur pass: scene-sampled border blur → scene_tex ----
	if (!lobby_blur_rects.blur.empty() &&
	    lobby_panel_source_tex &&
	    lobby_panel_mask_tex &&
	    lobby_panel_blur_pipeline &&
	    lobby_panel_copy_pipeline &&
	    scene_tex) {
		SDL_GPUBlitInfo copyInfo = {};
		copyInfo.source.texture = scene_tex;
		copyInfo.source.w = (Uint32)scene_tex_w;
		copyInfo.source.h = (Uint32)scene_tex_h;
		copyInfo.destination.texture = lobby_panel_source_tex;
		copyInfo.destination.w = (Uint32)scene_tex_w;
		copyInfo.destination.h = (Uint32)scene_tex_h;
		copyInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;
		copyInfo.filter = SDL_GPU_FILTER_NEAREST;
		SDL_BlitGPUTexture(cmd, &copyInfo);

		SDL_GPUColorTargetInfo maskCt = {};
		maskCt.texture = lobby_panel_mask_tex;
		maskCt.load_op = SDL_GPU_LOADOP_CLEAR;
		maskCt.store_op = SDL_GPU_STOREOP_STORE;
		maskCt.clear_color = {0, 0, 0, 0};

		bool maskReady = false;
		SDL_GPURenderPass *maskPass = SDL_BeginGPURenderPass(cmd, &maskCt, 1, nullptr);
		if (maskPass) {
			SDL_BindGPUGraphicsPipeline(maskPass, lobby_panel_copy_pipeline);
			SDL_GPUTextureSamplerBinding sourceBind = {lobby_panel_source_tex, nearest_sampler};
			SDL_BindGPUFragmentSamplers(maskPass, 0, &sourceBind, 1);
			for (const SDL_Rect &rect : lobby_blur_rects.restore) {
				SDL_SetGPUScissor(maskPass, &rect);
				SDL_DrawGPUPrimitives(maskPass, 3, 1, 0, 0);
			}
			SDL_EndGPURenderPass(maskPass);
			maskReady = true;
		}

		SDL_GPUColorTargetInfo ct = {};
		ct.texture  = scene_tex;
		ct.load_op  = SDL_GPU_LOADOP_LOAD;
		ct.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass *pass = maskReady
			? SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr)
			: nullptr;
		if (pass) {
			SDL_BindGPUGraphicsPipeline(pass, lobby_panel_blur_pipeline);
			SDL_GPUTextureSamplerBinding blurBinds[2] = {
				{lobby_panel_source_tex, linear_sampler},
				{lobby_panel_mask_tex,   linear_sampler},
			};
			SDL_BindGPUFragmentSamplers(
				pass,
				0,
				blurBinds,
				chosen_format == SDL_GPU_SHADERFORMAT_MSL ? 2 : 1);
			for (const SDL_Rect &rect : lobby_blur_rects.blur) {
				SDL_SetGPUScissor(pass, &rect);
				SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			}

			SDL_BindGPUGraphicsPipeline(pass, lobby_panel_copy_pipeline);
			SDL_GPUTextureSamplerBinding sourceBind = {lobby_panel_source_tex, nearest_sampler};
			SDL_BindGPUFragmentSamplers(pass, 0, &sourceBind, 1);
			for (const SDL_Rect &rect : lobby_blur_rects.restore) {
				SDL_SetGPUScissor(pass, &rect);
				SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			}
			SDL_EndGPURenderPass(pass);
		}
	}

	// ---- 5. Effects pass: particles + lights → scene_tex (LOAD, additive) ----
	bool has_particles = (pending_particle_draw_count > 0) && particle_pipeline;
	bool has_lights    = (pending_light_count > 0) && lighting_active;

	if ((has_particles || has_lights) && scene_tex) {
		// Lazy light pipeline creation.
		if (has_lights && !light_pipeline) {
			has_lights = CreateLightPipeline();
		}

		SDL_GPUColorTargetInfo ct = {};
		ct.texture  = scene_tex;
		ct.load_op  = SDL_GPU_LOADOP_LOAD; // preserve remap output
		ct.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
			// Draw particles (additive blend baked into particle_pipeline).
			if (has_particles) {
				SDL_BindGPUGraphicsPipeline(pass, particle_pipeline);
				for (int i = 0; i < pending_particle_draw_count; i++) {
					PendingParticleDraw &d = pending_particle_draws[i];
					int h = d.handle;
					if (h < 0 || h >= kMaxParticleBuffers || !particle_bufs[h].buf) continue;

					SDL_GPUBuffer *buf = particle_bufs[h].buf;
					SDL_BindGPUVertexStorageBuffers(pass, 0, &buf, 1);

					float fi[4] = {(float)scene_tex_w, (float)scene_tex_h, 0.f, 0.f};
					SDL_PushGPUVertexUniformData(cmd, 0, fi, sizeof(fi));

					SDL_GPUTextureSamplerBinding pal_bind = {palette_tex, nearest_sampler};
					SDL_BindGPUFragmentSamplers(pass, 0, &pal_bind, 1);

					SDL_DrawGPUPrimitives(pass, d.count * 6, 1, 0, 0);
				}
			}

			// Draw lights (additive blend baked into light_pipeline).
			if (has_lights && light_pipeline) {
				SDL_BindGPUGraphicsPipeline(pass, light_pipeline);
				struct LightParams { float cx,cy,radius,intensity; float r,g,b,gw; float gh,p0,p1,p2; };
				for (int i = 0; i < pending_light_count; i++) {
					LightEntry &le = pending_lights[i];
					LightParams lp = {};
					lp.cx        = le.x;
					lp.cy        = le.y;
					lp.radius    = le.radius;
					lp.intensity = le.intensity;
					lp.r         = le.r;
					lp.g         = le.g;
					lp.b         = le.b;
					lp.gw        = (float)scene_tex_w;
					lp.gh        = (float)scene_tex_h;
					SDL_PushGPUFragmentUniformData(cmd, 0, &lp, sizeof(lp));
					SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
				}
			}

			SDL_EndGPURenderPass(pass);
		}
	}

	pending_particle_draw_count = 0;
	pending_light_count         = 0;
	lighting_active             = false;
	pending_lobby_panel_border_blur_rects.clear();

	// ---- 6. Acquire swapchain — null when minimized ----
	SDL_GPUTexture *swapchain = nullptr;
	Uint32 sw_w = 0, sw_h = 0;
	SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchain, &sw_w, &sw_h);
	if (!swapchain) {
		SDL_SubmitGPUCommandBuffer(cmd);
		return;
	}

	// ---- 7. Upscale pass: scene_tex → swapchain ----
	if (scene_tex) {
		SDL_GPUColorTargetInfo ct = {};
		ct.texture     = swapchain;
		ct.load_op     = SDL_GPU_LOADOP_CLEAR;
		ct.store_op    = SDL_GPU_STOREOP_STORE;
		ct.clear_color = {0, 0, 0, 1};

		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
			SDL_BindGPUGraphicsPipeline(pass, upscale_pipeline);
			SDL_GPUSampler *up_samp = use_linear ? linear_sampler : nearest_sampler;
			SDL_GPUTextureSamplerBinding bind = {scene_tex, up_samp};
			SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
			SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(pass);
		}
	}

	SDL_SubmitGPUCommandBuffer(cmd);
}
