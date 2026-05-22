// Lobby panel-border post-process. Samples the rendered scene texture and a
// border mask, spreading existing border pixels outward without tying the
// effect to any palette index.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[[vk::binding(0, 2)]] Texture2D<float4>  scene  : register(t0, space2);
[[vk::binding(1, 2)]] Texture2D<float4>  border : register(t1, space2);
[[vk::binding(2, 2)]] SamplerState       samp   : register(s0, space2);

float4 frag_lobby_panel_blur(VOut input) : SV_Target {
    uint width;
    uint height;
    scene.GetDimensions(width, height);

    const float2 texel = 1.0 / float2(width, height);
    float4 base = scene.Sample(samp, input.uv);
    float3 accum = float3(0.0, 0.0, 0.0);
    float alpha = 0.0;
    const float radiusLimit = 10.0;

    for (int i = 1; i <= 10; ++i) {
        float radius = float(i) - 0.35;
        float diagonal = radius * 0.70710678;
        float falloff = 1.0 - (float(i) / (radiusLimit + 1.0));
        float weight = falloff * falloff * 0.18;

        float4 tap = border.Sample(samp, input.uv + float2( radius, 0.0) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2(-radius, 0.0) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2(0.0,  radius) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2(0.0, -radius) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2( diagonal,  diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2(-diagonal,  diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2( diagonal, -diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
        tap = border.Sample(samp, input.uv + float2(-diagonal, -diagonal) * texel);
        accum += tap.rgb * tap.a * weight;
        alpha += tap.a * weight;
    }

    if (alpha <= 0.0001) {
        return base;
    }

    float3 blurred = accum / alpha;
    return float4(lerp(base.rgb, blurred, saturate(alpha)), base.a);
}
