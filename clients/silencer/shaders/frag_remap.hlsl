// Palette remap: R8_UNORM indexed frame + RGBA palette → RGBA8 scene_tex.
// UV math: R8 stores byte n as n/255. Palette texel n sits at (n+0.5)/256.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[[vk::binding(0, 2)]] Texture2D<float4>  frame   : register(t0, space2);
[[vk::binding(1, 2)]] Texture2D<float4>  palette : register(t1, space2);
[[vk::binding(2, 2)]] SamplerState       samp    : register(s0, space2);

float4 frag_remap(VOut input) : SV_Target {
    float idx = frame.Sample(samp, input.uv).r;
    float u   = idx * (255.0 / 256.0) + 0.5 / 256.0;
    return palette.Sample(samp, float2(u, 0.5));
}
