// Palette remap: R8_UNORM indexed frame + RGBA palette → RGBA8 scene_tex.
// UV math: R8 stores byte n as n/255. Palette texel n sits at (n+0.5)/256.
//
// One SamplerState per Texture2D, both at the matching register number
// (t0/s0, t1/s1). SDL3 binds texture+sampler pairs via SDL_BindGPUFragment-
// Samplers; in SPIRV each pair satisfies one VK_DESCRIPTOR_TYPE_COMBINED_
// IMAGE_SAMPLER descriptor at fragment set 2. The same SDL_GPUSampler may
// be bound to multiple slots at runtime.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

Texture2D<float4>  frame        : register(t0, space2);
SamplerState       samp_frame   : register(s0, space2);
Texture2D<float4>  palette      : register(t1, space2);
SamplerState       samp_palette : register(s1, space2);

float4 frag_remap(VOut input) : SV_Target {
    float idx = frame.Sample(samp_frame, input.uv).r;
    float u   = idx * (255.0 / 256.0) + 0.5 / 256.0;
    return palette.Sample(samp_palette, float2(u, 0.5));
}
