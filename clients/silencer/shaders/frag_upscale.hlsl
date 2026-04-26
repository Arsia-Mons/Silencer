// Upscale: sample scene_tex with the chosen filter (nearest or bilinear)
// and write to the swapchain. Sampler is selected at bind time in Present().

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[[vk::binding(0, 2)]] Texture2D<float4>  scene : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState       samp  : register(s0, space2);

float4 frag_upscale(VOut input) : SV_Target {
    return scene.Sample(samp, input.uv);
}
