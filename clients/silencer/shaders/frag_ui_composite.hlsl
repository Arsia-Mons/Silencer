// cppx UI composite fragment shader (SIL-240).
//
// Samples the fully-rendered, premultiplied UI layer (ui_scene_tex) and dims it
// by the global fade fraction before the premultiplied-over composite onto the
// swapchain. Because the layer is premultiplied, the correct fade scales ALL
// FOUR channels by the same fraction -- this reproduces the legacy CPU
// "flatten the whole UI, then dim, then composite over the world" path exactly
// (the per-primitive alternative is not equivalent when translucent UI overlaps
// during a fade). fade.x == 1 (at rest) is a passthrough.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[[vk::binding(0, 2)]] Texture2D<float4> ui   : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState      samp : register(s0, space2);
[[vk::binding(0, 3)]] cbuffer Fade : register(b0, space3) {
    float4 fade; // .x = global fade fraction [0,1]
};

float4 frag_ui_composite(VOut input) : SV_Target {
    return ui.Sample(samp, input.uv) * fade.x;
}
