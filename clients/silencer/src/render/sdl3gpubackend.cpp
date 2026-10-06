#include "sdl3gpubackend.h"

#include "cppx_ui/ui_draw_program.h"

#include <string.h>
#include <vector>

// DXIL bytecode generated from clients/silencer/shaders/ by dxc at build time.
#include "vert_screen.dxil.h"
#include "frag_remap.dxil.h"
#include "frag_upscale.dxil.h"
#include "frag_lobby_panel_blur.dxil.h"
#include "frag_light.dxil.h"
#include "vert_particle.dxil.h"
#include "frag_particle.dxil.h"
#include "comp_particle.dxil.h"

// UI DXIL headers are absent on a machine without dxc (macOS dev uses the MSL
// strings); guard so the build stays MSL-only until dxc regenerates them.
#if __has_include("vert_ui.dxil.h")
#include "vert_ui.dxil.h"
#include "frag_ui.dxil.h"
#include "frag_ui_composite.dxil.h"
#define SILENCER_UI_HAS_DXIL 1
#else
#define SILENCER_UI_HAS_DXIL 0
#endif

// MSL shaders. HLSL twins in clients/silencer/shaders/*.hlsl share entry-point
// names; keep the two in sync.

// Fullscreen-triangle vertex shader (no VBO). Y-flipped UVs for Metal's
// top-down pixel convention.
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

// Palette remap. UV math: R8 stores byte n as n/255; palette texel n sits at (n+0.5)/256.
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

// Upscale: sample scene_tex to swapchain. Sampler selected at bind time.
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

// Lobby panel-border post-process: spreads existing border pixels outward by
// sampling the scene, not tied to any palette index.
static const char *kFragLobbyPanelBlurMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
fragment float4 frag_lobby_panel_blur(VOut in [[stage_in]],
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

// Additive emissive light disc. Distance is pixel-space to avoid aspect
// distortion. Uniform buffer 0 layout (48 bytes = 3 × float4):
//   [0] cx,cy,radius,intensity  [1] r,g,b,game_w  [2] game_h,pad×3
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
    f = f * f;
    float3 contrib = float3(p.r, p.g, p.b) * f * p.intensity;
    return float4(contrib, f * p.intensity);
}
)msl";

// Particle vertex shader: 6 verts/particle from a storage buffer. Dead
// particles (life<=0) emit a degenerate offscreen quad. UB0: float4(game_w, game_h, 0, 0).
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

// Advance particle positions by dt, decay life. threadcount_x=64; caller rounds count up to 64.
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

// cppx UI geometry vertex shader.
static const char *kVertUiMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct VOut { float4 pos [[position]]; float2 uv; float4 color; };
vertex VOut vert_ui(VIn in [[stage_in]]) {
    VOut o;
    o.pos   = float4(in.pos, 0.0, 1.0); // positions are already clip-space (baked CPU-side)
    o.uv    = in.uv;
    o.color = in.color;
    return o;
}
)msl";

// cppx UI geometry fragment shader: premultiplied vertex color * texture
// (1x1 white for solid batches).
static const char *kFragUiMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; float4 color; };
fragment float4 frag_ui(VOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]]) {
    return in.color * tex.sample(samp, in.uv);
}
)msl";

// cppx UI composite fragment shader. fade.x = fade fraction. fade.y selects mode:
//   y==0 GROUP OPACITY — scale all four premult channels (subtree turns translucent)
//   y==1 SCREEN FADE   — scale RGB toward black but KEEP coverage (opaque dim to
//                        black; world must not bleed through mid-fade)
static const char *kFragUiCompositeMSL = R"msl(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
fragment float4 frag_ui_composite(VOut in [[stage_in]],
    texture2d<float> ui [[texture(0)]],
    sampler samp [[sampler(0)]],
    constant float4& fade [[buffer(0)]]) {
    float4 c = ui.sample(samp, in.uv);
    float a = mix(c.a * fade.x, c.a, fade.y);
    return float4(c.rgb * fade.x, a);
}
)msl";

static const ShaderBundle kVertScreen   = { kVertScreenMSL,   kVertScreenDXIL,   sizeof(kVertScreenDXIL),   "vert_screen"      };
static const ShaderBundle kFragRemap    = { kFragRemapMSL,    kFragRemapDXIL,    sizeof(kFragRemapDXIL),    "frag_remap"       };
static const ShaderBundle kFragUpscale  = { kFragUpscaleMSL,  kFragUpscaleDXIL,  sizeof(kFragUpscaleDXIL),  "frag_upscale"     };
static const ShaderBundle kFragLobbyPanelBlur = { kFragLobbyPanelBlurMSL, kFragLobbyPanelBlurDXIL, sizeof(kFragLobbyPanelBlurDXIL), "frag_lobby_panel_blur" };
static const ShaderBundle kFragLight    = { kFragLightMSL,    kFragLightDXIL,    sizeof(kFragLightDXIL),    "frag_light"       };
static const ShaderBundle kVertParticle = { kVertParticleMSL, kVertParticleDXIL, sizeof(kVertParticleDXIL), "vert_particle"    };
static const ShaderBundle kFragParticle = { kFragParticleMSL, kFragParticleDXIL, sizeof(kFragParticleDXIL), "frag_particle"    };
static const ShaderBundle kCompParticle = { kComputeParticleMSL, kCompParticleDXIL, sizeof(kCompParticleDXIL), "update_particles" };

#if SILENCER_UI_HAS_DXIL
static const ShaderBundle kVertUi          = { kVertUiMSL,          kVertUiDXIL,          sizeof(kVertUiDXIL),          "vert_ui" };
static const ShaderBundle kFragUi          = { kFragUiMSL,          kFragUiDXIL,          sizeof(kFragUiDXIL),          "frag_ui" };
static const ShaderBundle kFragUiComposite = { kFragUiCompositeMSL, kFragUiCompositeDXIL, sizeof(kFragUiCompositeDXIL), "frag_ui_composite" };
#else
static const ShaderBundle kVertUi          = { kVertUiMSL,          nullptr, 0, "vert_ui" };
static const ShaderBundle kFragUi          = { kFragUiMSL,          nullptr, 0, "frag_ui" };
static const ShaderBundle kFragUiComposite = { kFragUiCompositeMSL, nullptr, 0, "frag_ui_composite" };
#endif

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

	// Prefer DXIL (Windows/D3D12), else MSL (Apple/Metal).
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
	if (ui_tex)            { SDL_ReleaseGPUTexture(device, ui_tex);             ui_tex            = nullptr; }
	if (ui_tbuf)           { SDL_ReleaseGPUTransferBuffer(device, ui_tbuf);     ui_tbuf           = nullptr; }
	if (ui_geom_pipeline)  { SDL_ReleaseGPUGraphicsPipeline(device, ui_geom_pipeline); ui_geom_pipeline = nullptr; }
	if (ui_scene_composite_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, ui_scene_composite_pipeline); ui_scene_composite_pipeline = nullptr; }
	if (ui_layer_composite_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, ui_layer_composite_pipeline); ui_layer_composite_pipeline = nullptr; }
	for (auto &lt : ui_layer_tex) if (lt) { SDL_ReleaseGPUTexture(device, lt); lt = nullptr; }
	ui_layer_w = ui_layer_h = 0;
	if (ui_scene_tex)      { SDL_ReleaseGPUTexture(device, ui_scene_tex);       ui_scene_tex      = nullptr; }
	if (ui_vbuf)           { SDL_ReleaseGPUBuffer(device, ui_vbuf);             ui_vbuf           = nullptr; }
	if (ui_vbuf_tbuf)      { SDL_ReleaseGPUTransferBuffer(device, ui_vbuf_tbuf); ui_vbuf_tbuf      = nullptr; }
	if (ui_white_tex)      { SDL_ReleaseGPUTexture(device, ui_white_tex);       ui_white_tex      = nullptr; }
	if (ui_texup_tbuf)     { SDL_ReleaseGPUTransferBuffer(device, ui_texup_tbuf); ui_texup_tbuf    = nullptr; }
	ReleaseUiTextureCache();
	if (capture_tbuf)      { SDL_ReleaseGPUTransferBuffer(device, capture_tbuf); capture_tbuf     = nullptr; }
	if (ui_composite_pipeline) { SDL_ReleaseGPUGraphicsPipeline(device, ui_composite_pipeline); ui_composite_pipeline = nullptr; }
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
		if (!b.dxil || b.dxil_size == 0) {
			SDL_Log("SDL3GPUBackend: shader '%s' has no DXIL (regenerate with dxc for D3D12)", b.entrypoint);
			return nullptr;
		}
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
	// Remap: R8 indexed frame → scene_tex (RGBA8).
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

	// Upscale: scene_tex → swapchain.
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

	// UI composite: ui_tex → swapchain. Reuses the upscale shaders; the only
	// difference is premultiplied-alpha blending over the finished scene.
	{
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX,   kVertScreen,  0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUpscale, 1);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: ui composite shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}

		SDL_GPUColorTargetDescription ct = {};
		ct.format = SDL_GetGPUSwapchainTextureFormat(device, window);
		ct.blend_state.enable_blend          = true;
		ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		ct.blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
		ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		ct.blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;

		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;

		ui_composite_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!ui_composite_pipeline) {
			SDL_Log("SDL3GPUBackend: ui composite pipeline failed: %s", SDL_GetError());
			return false;
		}
	}

	return CreateLobbyPanelBlurPipeline();
}

bool SDL3GPUBackend::CreateLobbyPanelBlurPipeline() {
	const Uint32 blurSamplers = 2u;
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

// Lazy (on first SubmitUiFrame) so a shader failure degrades the UI to "not
// drawn" rather than bricking the backend at Init.
bool SDL3GPUBackend::CreateUiGeometryPipelines() {
	if (ui_geom_pipeline && ui_scene_composite_pipeline)
		return true;

	// Premultiplied-over blend, shared by both pipelines below.
	SDL_GPUColorTargetBlendState blend = {};
	blend.enable_blend          = true;
	blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	blend.color_blend_op        = SDL_GPU_BLENDOP_ADD;
	blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;

	// UI geometry pipeline: per-vertex {clip pos, uv, premult color} -> ui_scene_tex.
	if (!ui_geom_pipeline) {
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX, kVertUi,
		                               /*num_samplers=*/0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUi,
		                               /*num_samplers=*/1);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: ui geometry shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}
		// Must match GpuUiVertex {float2 pos; float2 uv; float4 color;}.
		SDL_GPUVertexBufferDescription vbdesc = {};
		vbdesc.slot              = 0;
		vbdesc.pitch             = (Uint32)sizeof(silencer::cppx_ui::GpuUiVertex);
		vbdesc.input_rate        = SDL_GPU_VERTEXINPUTRATE_VERTEX;
		vbdesc.instance_step_rate = 0;
		SDL_GPUVertexAttribute attrs[3] = {};
		attrs[0].location = 0; attrs[0].buffer_slot = 0; attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; attrs[0].offset = 0;
		attrs[1].location = 1; attrs[1].buffer_slot = 0; attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; attrs[1].offset = 8;
		attrs[2].location = 2; attrs[2].buffer_slot = 0; attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; attrs[2].offset = 16;
		SDL_GPUVertexInputState vis = {};
		vis.vertex_buffer_descriptions = &vbdesc;
		vis.num_vertex_buffers         = 1;
		vis.vertex_attributes          = attrs;
		vis.num_vertex_attributes      = 3;
		SDL_GPUColorTargetDescription ct = {};
		ct.format      = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ct.blend_state = blend;
		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader      = vs;
		pi.fragment_shader    = fs;
		pi.vertex_input_state = vis;
		pi.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;
		ui_geom_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!ui_geom_pipeline) {
			SDL_Log("SDL3GPUBackend: ui geometry pipeline failed: %s", SDL_GetError());
			return false;
		}
	}

	// UI composite: ui_scene_tex -> swapchain (premult over + fade).
	if (!ui_scene_composite_pipeline) {
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX, kVertScreen, 0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUiComposite,
		                               /*num_samplers=*/1, /*num_uniform_buffers=*/1);
		if (!vs || !fs) {
			SDL_Log("SDL3GPUBackend: ui scene composite shaders failed: %s", SDL_GetError());
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}
		SDL_GPUColorTargetDescription ct = {};
		ct.format      = SDL_GetGPUSwapchainTextureFormat(device, window);
		ct.blend_state = blend;
		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ct;
		pi.target_info.num_color_targets         = 1;
		ui_scene_composite_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!ui_scene_composite_pipeline) {
			SDL_Log("SDL3GPUBackend: ui scene composite pipeline failed: %s", SDL_GetError());
			return false;
		}
	}
	return true;
}

bool SDL3GPUBackend::EnsureUiLayerResources(int w, int h) {
	if (w <= 0 || h <= 0)
		return false;
	// Like the scene composite but targeting R8G8B8A8 instead of the swapchain format.
	if (!ui_layer_composite_pipeline) {
		SDL_GPUColorTargetBlendState blend = {};
		blend.enable_blend          = true;
		blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		blend.color_blend_op        = SDL_GPU_BLENDOP_ADD;
		blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		blend.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;
		SDL_GPUShader *vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX, kVertScreen, 0);
		SDL_GPUShader *fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, kFragUiComposite,
		                               /*num_samplers=*/1, /*num_uniform_buffers=*/1);
		if (!vs || !fs) {
			if (vs) SDL_ReleaseGPUShader(device, vs);
			if (fs) SDL_ReleaseGPUShader(device, fs);
			return false;
		}
		SDL_GPUColorTargetDescription ctd = {};
		ctd.format      = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ctd.blend_state = blend;
		SDL_GPUGraphicsPipelineCreateInfo pi = {};
		pi.vertex_shader   = vs;
		pi.fragment_shader = fs;
		pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pi.target_info.color_target_descriptions = &ctd;
		pi.target_info.num_color_targets         = 1;
		ui_layer_composite_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
		SDL_ReleaseGPUShader(device, vs);
		SDL_ReleaseGPUShader(device, fs);
		if (!ui_layer_composite_pipeline) {
			SDL_Log("SDL3GPUBackend: ui layer composite pipeline failed: %s", SDL_GetError());
			return false;
		}
	}
	if (ui_layer_w != w || ui_layer_h != h) {
		for (auto &lt : ui_layer_tex)
			if (lt) { SDL_ReleaseGPUTexture(device, lt); lt = nullptr; }
		ui_layer_w = w;
		ui_layer_h = h;
	}
	for (auto &lt : ui_layer_tex) {
		if (lt)
			continue;
		SDL_GPUTextureCreateInfo ti = {};
		ti.type                 = SDL_GPU_TEXTURETYPE_2D;
		ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
		                          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		ti.width                = (Uint32)w;
		ti.height               = (Uint32)h;
		ti.layer_count_or_depth = 1;
		ti.num_levels           = 1;
		lt = SDL_CreateGPUTexture(device, &ti);
		if (!lt)
			return false;
	}
	return true;
}

// 1x1 white so the `color * texel` shader yields the vertex color unchanged for
// solid batches. `copy` must be open and ui_texup_tbuf sized >= 4 bytes.
bool SDL3GPUBackend::EnsureUiWhiteTexture(SDL_GPUCopyPass *copy) {
	if (ui_white_ready)
		return true;
	if (!ui_white_tex) {
		SDL_GPUTextureCreateInfo ti = {};
		ti.type                 = SDL_GPU_TEXTURETYPE_2D;
		ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		ti.width                = 1;
		ti.height               = 1;
		ti.layer_count_or_depth = 1;
		ti.num_levels           = 1;
		ui_white_tex = SDL_CreateGPUTexture(device, &ti);
		if (!ui_white_tex)
			return false;
	}
	if (!ui_texup_tbuf || ui_texup_tbuf_sz < 4)
		return false;
	Uint8 *dst = (Uint8 *)SDL_MapGPUTransferBuffer(device, ui_texup_tbuf, true);
	if (!dst)
		return false;
	dst[0] = dst[1] = dst[2] = dst[3] = 255;
	SDL_UnmapGPUTransferBuffer(device, ui_texup_tbuf);
	SDL_GPUTextureTransferInfo src = {};
	src.transfer_buffer = ui_texup_tbuf;
	src.rows_per_layer  = 1;
	src.pixels_per_row  = 1;
	SDL_GPUTextureRegion region = {};
	region.texture = ui_white_tex;
	region.w = 1;
	region.h = 1;
	region.d = 1;
	SDL_UploadToGPUTexture(copy, &src, &region, false);
	ui_white_ready = true;
	return true;
}

// `copy` must be open and ui_texup_tbuf sized >= w*h*4.
SDL_GPUTexture *SDL3GPUBackend::EnsureUiTexture(SDL_GPUCopyPass *copy, uint64_t key,
                                                const uint8_t *rgba, int w, int h) {
	auto it = ui_tex_cache.find(key);
	if (it != ui_tex_cache.end())
		return it->second;
	if (!rgba || w <= 0 || h <= 0)
		return nullptr;
	SDL_GPUTextureCreateInfo ti = {};
	ti.type                 = SDL_GPU_TEXTURETYPE_2D;
	ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	ti.width                = (Uint32)w;
	ti.height               = (Uint32)h;
	ti.layer_count_or_depth = 1;
	ti.num_levels           = 1;
	SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &ti);
	if (!tex)
		return nullptr;
	const Uint32 need = (Uint32)w * (Uint32)h * 4u;
	if (ui_texup_tbuf && ui_texup_tbuf_sz >= need) {
		Uint8 *dst = (Uint8 *)SDL_MapGPUTransferBuffer(device, ui_texup_tbuf, true);
		if (dst) {
			memcpy(dst, rgba, need);
			SDL_UnmapGPUTransferBuffer(device, ui_texup_tbuf);
			SDL_GPUTextureTransferInfo src = {};
			src.transfer_buffer = ui_texup_tbuf;
			src.rows_per_layer  = (Uint32)h;
			src.pixels_per_row  = (Uint32)w;
			SDL_GPUTextureRegion region = {};
			region.texture = tex;
			region.w = (Uint32)w;
			region.h = (Uint32)h;
			region.d = 1;
			SDL_UploadToGPUTexture(copy, &src, &region, false);
		} else {
			SDL_ReleaseGPUTexture(device, tex);
			return nullptr;
		}
	} else {
		SDL_ReleaseGPUTexture(device, tex);
		return nullptr;
	}
	ui_tex_cache[key] = tex;
	return tex;
}

void SDL3GPUBackend::ReleaseUiTextureCache() {
	for (auto &kv : ui_tex_cache)
		if (kv.second)
			SDL_ReleaseGPUTexture(device, kv.second);
	ui_tex_cache.clear();
}

bool SDL3GPUBackend::CreateLightPipeline() {
	// Additive emissive: src×srcAlpha + dst×1.
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
	// Compute pipeline.
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

	// Graphics pipeline: particle quads additively into scene_tex.
	{
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

void SDL3GPUBackend::UploadUiFrame(const Uint8 *rgba, int w, int h, float global_alpha) {
	if (!rgba || w <= 0 || h <= 0) {
		pending_ui_pixels = nullptr;
		ui_present        = false;
		return;
	}
	float a = global_alpha;
	if (a < 0.0f) a = 0.0f;
	if (a > 1.0f) a = 1.0f;
	ui_global_alpha   = (Uint8)(a * 255.0f + 0.5f);
	pending_ui_pixels = rgba;
	pending_ui_w      = w;
	pending_ui_h      = h;
	ui_dirty          = true;
	ui_present        = true;
}

void SDL3GPUBackend::SubmitUiFrame(const silencer::cppx_ui::GpuUiProgram &program,
                                   float global_alpha) {
	pending_ui_program = &program;
	float a = global_alpha;
	if (a < 0.0f) a = 0.0f;
	if (a > 1.0f) a = 1.0f;
	ui_geom_fade    = (Uint8)(a * 255.0f + 0.5f);
	ui_geom_present = true;
	// Geometry and legacy RGBA-upload paths are mutually exclusive per frame.
	ui_present = false;
}

void SDL3GPUBackend::RequestCapture() {
	capture_pending = true;
	captured_valid  = false;
}

bool SDL3GPUBackend::TakeCapturedFrame(std::vector<Uint8> &rgba, int &w, int &h) {
	if (!captured_valid) return false;
	rgba = captured_rgba;
	w = captured_w;
	h = captured_h;
	captured_valid = false;
	return true;
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
	// Composited in Present(); nothing to flush here.
}

int SDL3GPUBackend::AllocParticleBuffer(Uint32 count) {
	if (!device) return -1;
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
	return -1;
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

// Executes all queued work in a single command buffer.
const char *SDL3GPUBackend::GpuDriverName() const {
	if (!device) return "";
	const char *d = SDL_GetGPUDeviceDriver(device);
	return d ? d : "";
}

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

	// Lazily (re)create the UI overlay texture + transfer buffer.
	if (ui_present && pending_ui_pixels) {
		if (!ui_tex || ui_tex_w != pending_ui_w || ui_tex_h != pending_ui_h) {
			if (ui_tex) SDL_ReleaseGPUTexture(device, ui_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                 = SDL_GPU_TEXTURETYPE_2D;
			ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
			ti.width                = (Uint32)pending_ui_w;
			ti.height               = (Uint32)pending_ui_h;
			ti.layer_count_or_depth = 1;
			ti.num_levels           = 1;
			ui_tex   = SDL_CreateGPUTexture(device, &ti);
			ui_tex_w = pending_ui_w;
			ui_tex_h = pending_ui_h;
		}
		const Uint32 need = (Uint32)pending_ui_w * (Uint32)pending_ui_h * 4u;
		if (ui_tex && (!ui_tbuf || ui_tbuf_sz < need)) {
			if (ui_tbuf) SDL_ReleaseGPUTransferBuffer(device, ui_tbuf);
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size  = need;
			ui_tbuf   = SDL_CreateGPUTransferBuffer(device, &tbi);
			ui_tbuf_sz = need;
		}
	}

	// Lazily (re)create the GPU UI geometry resources for this frame.
	if (ui_geom_present && pending_ui_program) {
		const silencer::cppx_ui::GpuUiProgram &prog = *pending_ui_program;
		// Flush the texture cache when the cppx registries reset (resize/re-bake).
		if (prog.texture_generation != ui_tex_generation) {
			ReleaseUiTextureCache();
			ui_tex_generation = prog.texture_generation;
		}
		const int sw = prog.target_w > 0 ? prog.target_w : 1;
		const int sh = prog.target_h > 0 ? prog.target_h : 1;
		if (!ui_scene_tex || ui_scene_w != sw || ui_scene_h != sh) {
			if (ui_scene_tex) SDL_ReleaseGPUTexture(device, ui_scene_tex);
			SDL_GPUTextureCreateInfo ti = {};
			ti.type                 = SDL_GPU_TEXTURETYPE_2D;
			ti.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			ti.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER |
			                          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			ti.width                = (Uint32)sw;
			ti.height               = (Uint32)sh;
			ti.layer_count_or_depth = 1;
			ti.num_levels           = 1;
			ui_scene_tex = SDL_CreateGPUTexture(device, &ti);
			ui_scene_w = sw;
			ui_scene_h = sh;
		}
		// Only when the program carries group-opacity layers (common path allocates nothing).
		bool has_layers = false;
		for (const auto &cm : prog.commands)
			if (cm.op == silencer::cppx_ui::GpuUiOp::PushLayer) { has_layers = true; break; }
		if (has_layers)
			EnsureUiLayerResources(sw, sh);
		const Uint32 vcount = (Uint32)prog.verts.size();
		if (vcount > 0) {
			if (!ui_vbuf || ui_vbuf_cap < vcount) {
				if (ui_vbuf) SDL_ReleaseGPUBuffer(device, ui_vbuf);
				SDL_GPUBufferCreateInfo bi = {};
				bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
				bi.size  = vcount * (Uint32)sizeof(silencer::cppx_ui::GpuUiVertex);
				ui_vbuf  = SDL_CreateGPUBuffer(device, &bi);
				ui_vbuf_cap = vcount;
			}
			const Uint32 vbytes = vcount * (Uint32)sizeof(silencer::cppx_ui::GpuUiVertex);
			if (!ui_vbuf_tbuf || ui_vbuf_tbuf_sz < vbytes) {
				if (ui_vbuf_tbuf) SDL_ReleaseGPUTransferBuffer(device, ui_vbuf_tbuf);
				SDL_GPUTransferBufferCreateInfo tbi = {};
				tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				tbi.size  = vbytes;
				ui_vbuf_tbuf = SDL_CreateGPUTransferBuffer(device, &tbi);
				ui_vbuf_tbuf_sz = vbytes;
			}
		}
		// Sized BEFORE the copy pass (white 4B + largest pending texture) so it
		// never resizes mid-pass.
		Uint32 maxtex = 4u;
		for (const auto &t : prog.textures) {
			if (ui_tex_cache.find(t.key) != ui_tex_cache.end()) continue;
			const Uint32 b = (Uint32)t.w * (Uint32)t.h * 4u;
			if (b > maxtex) maxtex = b;
		}
		if (!ui_texup_tbuf || ui_texup_tbuf_sz < maxtex) {
			if (ui_texup_tbuf) SDL_ReleaseGPUTransferBuffer(device, ui_texup_tbuf);
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			tbi.size  = maxtex;
			ui_texup_tbuf = SDL_CreateGPUTransferBuffer(device, &tbi);
			ui_texup_tbuf_sz = maxtex;
		}
		// On failure, disable the UI path for this frame.
		if (!ui_geom_pipeline || !ui_scene_composite_pipeline) {
			if (!CreateUiGeometryPipelines())
				ui_geom_present = false;
		}
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

	// ---- 1. Copy pass: upload frame + palette + UI overlay ----
	bool upload_frame = frame_dirty && pending_pixels && frame_tex && frame_tbuf;
	bool upload_ui    = ui_dirty && pending_ui_pixels && ui_tex && ui_tbuf;
	bool upload_ui_geom = ui_geom_present && pending_ui_program &&
	                      !pending_ui_program->verts.empty() && ui_vbuf && ui_vbuf_tbuf;
	if (upload_frame || palette_dirty || upload_ui || upload_ui_geom) {
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

		if (upload_ui) {
			Uint8 *dst = (Uint8 *)SDL_MapGPUTransferBuffer(device, ui_tbuf, false);
			if (dst) {
				const size_t bytes = (size_t)pending_ui_w * (size_t)pending_ui_h * 4u;
				if (ui_global_alpha >= 255) {
					memcpy(dst, pending_ui_pixels, bytes);
				} else {
					// Premultiplied: scaling all four channels by the fraction is the correct dim.
					const Uint32 m = (Uint32)ui_global_alpha;
					for (size_t i = 0; i < bytes; ++i) {
						dst[i] = (Uint8)((pending_ui_pixels[i] * m + 127u) / 255u);
					}
				}
				SDL_UnmapGPUTransferBuffer(device, ui_tbuf);
				SDL_GPUTextureTransferInfo src = {};
				src.transfer_buffer = ui_tbuf;
				src.rows_per_layer  = (Uint32)pending_ui_h;
				src.pixels_per_row  = (Uint32)pending_ui_w;
				SDL_GPUTextureRegion region = {};
				region.texture = ui_tex;
				region.w = (Uint32)pending_ui_w;
				region.h = (Uint32)pending_ui_h;
				region.d = 1;
				SDL_UploadToGPUTexture(copy, &src, &region, false);
			}
			ui_dirty = false;
		}

		if (upload_ui_geom) {
			const silencer::cppx_ui::GpuUiProgram &prog = *pending_ui_program;
			EnsureUiWhiteTexture(copy);
			for (const auto &t : prog.textures)
				EnsureUiTexture(copy, t.key, t.rgba, t.w, t.h);
			const size_t vbytes =
			    prog.verts.size() * sizeof(silencer::cppx_ui::GpuUiVertex);
			void *dst = SDL_MapGPUTransferBuffer(device, ui_vbuf_tbuf, true);
			if (dst) {
				memcpy(dst, prog.verts.data(), vbytes);
				SDL_UnmapGPUTransferBuffer(device, ui_vbuf_tbuf);
				SDL_GPUTransferBufferLocation src = {};
				src.transfer_buffer = ui_vbuf_tbuf;
				src.offset = 0;
				SDL_GPUBufferRegion region = {};
				region.buffer = ui_vbuf;
				region.offset = 0;
				region.size = (Uint32)vbytes;
				SDL_UploadToGPUBuffer(copy, &src, &region, true);
			}
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
				2);
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
		if (has_lights && !light_pipeline) {
			has_lights = CreateLightPipeline();
		}

		SDL_GPUColorTargetInfo ct = {};
		ct.texture  = scene_tex;
		ct.load_op  = SDL_GPU_LOADOP_LOAD; // preserve remap output
		ct.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
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

	// ---- 5b. UI geometry pass: draw the cppx UI into ui_scene_tex (transparent
	// clear) so the fade dims the whole flattened UI once at composite ----
	if (ui_geom_present && pending_ui_program && ui_geom_pipeline && ui_scene_tex &&
	    ui_vbuf && ui_white_ready) {
		const silencer::cppx_ui::GpuUiProgram &prog = *pending_ui_program;
		const SDL_Rect full = {0, 0, ui_scene_w, ui_scene_h};
		// When the program carries no layers, Push/Pop fall back to inline
		// (full-opacity) draws and the common path is unchanged.
		const bool layers_ready =
		    ui_layer_composite_pipeline != nullptr && ui_layer_tex[0] != nullptr &&
		    ui_layer_w == ui_scene_w && ui_layer_h == ui_scene_h;
		SDL_GPUTexture *layer_stack[kMaxUiLayers];
		float layer_opacity[kMaxUiLayers];
		int layer_depth = 0;
		SDL_Rect cur_clip = full; // tracked so a pass switch can re-apply it
		bool clip_active = false;
		SDL_GPUBufferBinding vbind = {ui_vbuf, 0};

		// Re-applies the active clip, which must carry across the pass switches a
		// layer push/pop forces.
		auto begin_geom = [&](SDL_GPUTexture *target,
		                      SDL_GPULoadOp load) -> SDL_GPURenderPass * {
			SDL_GPUColorTargetInfo ct = {};
			ct.texture     = target;
			ct.load_op     = load;
			ct.store_op    = SDL_GPU_STOREOP_STORE;
			ct.clear_color = {0, 0, 0, 0};
			SDL_GPURenderPass *p = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
			if (p) {
				SDL_BindGPUGraphicsPipeline(p, ui_geom_pipeline);
				SDL_BindGPUVertexBuffers(p, 0, &vbind, 1);
				SDL_SetGPUScissor(p, clip_active ? &cur_clip : &full);
			}
			return p;
		};

		SDL_GPURenderPass *pass = begin_geom(ui_scene_tex, SDL_GPU_LOADOP_CLEAR);
		for (const auto &c : prog.commands) {
			if (!pass) break; // a pass (re)begin failed; abandon the frame
			switch (c.op) {
			case silencer::cppx_ui::GpuUiOp::DrawBatch: {
				if (c.vertex_count == 0) break;
				SDL_GPUTexture *tex = ui_white_tex;
				if (c.texture_key != 0) {
					auto it = ui_tex_cache.find(c.texture_key);
					if (it != ui_tex_cache.end() && it->second) tex = it->second;
				}
				SDL_GPUTextureSamplerBinding bind = {tex, nearest_sampler};
				SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
				SDL_DrawGPUPrimitives(pass, c.vertex_count, 1, c.first_vertex, 0);
				break;
			}
			case silencer::cppx_ui::GpuUiOp::SetClip: {
				// Clamp to the target so SDL_GPU scissor validation passes.
				SDL_Rect r = {c.clip_x, c.clip_y, c.clip_w, c.clip_h};
				if (r.x < 0) { r.w += r.x; r.x = 0; }
				if (r.y < 0) { r.h += r.y; r.y = 0; }
				if (r.x + r.w > ui_scene_w) r.w = ui_scene_w - r.x;
				if (r.y + r.h > ui_scene_h) r.h = ui_scene_h - r.y;
				if (r.w < 0) r.w = 0;
				if (r.h < 0) r.h = 0;
				cur_clip = r;
				clip_active = true;
				SDL_SetGPUScissor(pass, &cur_clip);
				break;
			}
			case silencer::cppx_ui::GpuUiOp::ClearClip:
				clip_active = false;
				SDL_SetGPUScissor(pass, &full);
				break;
			case silencer::cppx_ui::GpuUiOp::PushLayer: {
				if (!layers_ready || layer_depth >= kMaxUiLayers)
					break; // fall back to inline (full-opacity) draw
				SDL_EndGPURenderPass(pass);
				SDL_GPUTexture *lt = ui_layer_tex[layer_depth];
				layer_stack[layer_depth] = lt;
				layer_opacity[layer_depth] = c.layer_opacity;
				++layer_depth;
				pass = begin_geom(lt, SDL_GPU_LOADOP_CLEAR);
				break;
			}
			case silencer::cppx_ui::GpuUiOp::PopLayer: {
				if (layer_depth <= 0)
					break;
				SDL_EndGPURenderPass(pass);
				--layer_depth;
				SDL_GPUTexture *lt = layer_stack[layer_depth];
				const float op = layer_opacity[layer_depth];
				SDL_GPUTexture *parent =
				    layer_depth > 0 ? layer_stack[layer_depth - 1] : ui_scene_tex;
				// Composite the flattened layer over its parent at `op` (premultiplied
				// multiply, under the active clip).
				SDL_GPUColorTargetInfo ct = {};
				ct.texture  = parent;
				ct.load_op  = SDL_GPU_LOADOP_LOAD;
				ct.store_op = SDL_GPU_STOREOP_STORE;
				pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
				if (!pass) break;
				SDL_SetGPUScissor(pass, clip_active ? &cur_clip : &full);
				SDL_BindGPUGraphicsPipeline(pass, ui_layer_composite_pipeline);
				SDL_GPUTextureSamplerBinding b = {lt, nearest_sampler};
				SDL_BindGPUFragmentSamplers(pass, 0, &b, 1);
				const float u[4] = {op, 0.f, 0.f, 0.f};
				SDL_PushGPUFragmentUniformData(cmd, 0, u, sizeof(u));
				SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
				// Resume geometry drawing into the parent.
				SDL_BindGPUGraphicsPipeline(pass, ui_geom_pipeline);
				SDL_BindGPUVertexBuffers(pass, 0, &vbind, 1);
				break;
			}
			}
		}
		if (pass) SDL_EndGPURenderPass(pass);
	}

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

	// ---- 8. UI composite pass: ui_tex → swapchain (premult over, LOAD) ----
	if (ui_present && ui_tex && ui_composite_pipeline) {
		SDL_GPUColorTargetInfo ct = {};
		ct.texture  = swapchain;
		ct.load_op  = SDL_GPU_LOADOP_LOAD;
		ct.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
			SDL_BindGPUGraphicsPipeline(pass, ui_composite_pipeline);
			SDL_GPUTextureSamplerBinding bind = {ui_tex, nearest_sampler};
			SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
			SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(pass);
		}
	}

	// ---- 8b. UI geometry composite: ui_scene_tex -> swapchain + fade (LOAD) ----
	if (ui_geom_present && ui_scene_tex && ui_scene_composite_pipeline) {
		SDL_GPUColorTargetInfo ct = {};
		ct.texture  = swapchain;
		ct.load_op  = SDL_GPU_LOADOP_LOAD;
		ct.store_op = SDL_GPU_STOREOP_STORE;
		SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
		if (pass) {
			SDL_BindGPUGraphicsPipeline(pass, ui_scene_composite_pipeline);
			SDL_GPUTextureSamplerBinding bind = {ui_scene_tex, nearest_sampler};
			SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
			// fade.y = 1: SCREEN FADE — dim RGB toward black but keep coverage so the
			// HUD stays opaque (not an opacity fade that lets the world bleed through).
			const float fade[4] = {ui_geom_fade / 255.0f, 1.f, 0.f, 0.f};
			SDL_PushGPUFragmentUniformData(cmd, 0, fade, sizeof(fade));
			SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(pass);
		}
	}

	// ui_geom_present / pending_ui_program intentionally PERSIST across Present()
	// (host refills the program in place; pointer never dangles), so the extra
	// Present() CaptureCompositedFrame issues re-renders the UI instead of a
	// world-only frame.

	// ---- 9. Optional swapchain capture (screenshot) ----
	// Synchronous (fences this submit) so the captured frame is ready for the
	// post-render handler.
	if (capture_pending && swapchain) {
		const Uint32 need = sw_w * sw_h * 4u;
		if (!capture_tbuf || capture_tbuf_sz < need) {
			if (capture_tbuf) SDL_ReleaseGPUTransferBuffer(device, capture_tbuf);
			SDL_GPUTransferBufferCreateInfo tbi = {};
			tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
			tbi.size  = need;
			capture_tbuf = SDL_CreateGPUTransferBuffer(device, &tbi);
			capture_tbuf_sz = need;
		}
		if (capture_tbuf) {
			SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
			SDL_GPUTextureRegion reg = {};
			reg.texture = swapchain;
			reg.w = sw_w;
			reg.h = sw_h;
			reg.d = 1;
			SDL_GPUTextureTransferInfo dst = {};
			dst.transfer_buffer = capture_tbuf;
			dst.rows_per_layer  = sw_h;
			dst.pixels_per_row  = sw_w;
			SDL_DownloadFromGPUTexture(copy, &reg, &dst);
			SDL_EndGPUCopyPass(copy);

			SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
			if (fence) {
				SDL_WaitForGPUFences(device, true, &fence, 1);
				SDL_ReleaseGPUFence(device, fence);
				Uint8 *m = (Uint8 *)SDL_MapGPUTransferBuffer(device, capture_tbuf, false);
				if (m) {
					captured_rgba.resize(need);
					const bool bgra =
						SDL_GetGPUSwapchainTextureFormat(device, window) ==
							SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
						SDL_GetGPUSwapchainTextureFormat(device, window) ==
							SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
					for (Uint32 i = 0; i < need; i += 4) {
						captured_rgba[i + 0] = bgra ? m[i + 2] : m[i + 0];
						captured_rgba[i + 1] = m[i + 1];
						captured_rgba[i + 2] = bgra ? m[i + 0] : m[i + 2];
						captured_rgba[i + 3] = m[i + 3];
					}
					captured_w = (int)sw_w;
					captured_h = (int)sw_h;
					captured_valid = true;
					SDL_UnmapGPUTransferBuffer(device, capture_tbuf);
				}
			}
		}
		capture_pending = false;
		return; // already submitted (with fence) above
	}

	SDL_SubmitGPUCommandBuffer(cmd);
}
