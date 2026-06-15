// cppx UI composite fragment shader (SIL-240).
//
// Samples the fully-rendered, premultiplied UI layer and composites it. Two
// callers, selected by fade.y:
//
//   fade.y == 0  -- GROUP OPACITY (LayerPush/LayerPop). Scaling ALL FOUR
//                   premultiplied channels by fade.x is a true opacity: the
//                   flattened subtree gets more translucent and the parent
//                   shows through. This is what group opacity means.
//
//   fade.y == 1  -- SCREEN FADE (the ui_scene_tex -> swapchain transition fade).
//                   Scale only RGB toward black and KEEP the coverage (alpha).
//                   The legacy path drew world + HUD into one indexed buffer and
//                   palette-faded the whole thing, so the HUD stayed opaque and
//                   dimmed to black. Scaling alpha too (an opacity fade) makes
//                   the HUD translucent and lets the faded world bleed through
//                   mid-fade -- the bug this flag fixes. Keeping alpha makes the
//                   whole screen (world via its palette fade, UI via this) fade
//                   to black uniformly.
//
// fade.x == 1 (at rest) is a passthrough either way.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[[vk::binding(0, 2)]] Texture2D<float4> ui   : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState      samp : register(s0, space2);
[[vk::binding(0, 3)]] cbuffer Fade : register(b0, space3) {
    float4 fade; // .x = global fade fraction [0,1]; .y = 1 to preserve coverage
};

float4 frag_ui_composite(VOut input) : SV_Target {
    float4 c = ui.Sample(samp, input.uv);
    float a = lerp(c.a * fade.x, c.a, fade.y);
    return float4(c.rgb * fade.x, a);
}
