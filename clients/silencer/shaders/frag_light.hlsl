// Additive emissive light disc rendered into scene_tex.
// Pixel-space distance avoids aspect-ratio distortion.
// Uniform buffer 0 layout (48 bytes = 3 × float4):
//   float4[0]: cx, cy, radius, intensity
//   float4[1]: r,  g,  b,     game_w
//   float4[2]: game_h, (pad x3)

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

struct LightParams {
    float cx;       float cy;       float radius;   float intensity;
    float r;        float g;        float b;        float gw;
    float gh;       float _p0;      float _p1;      float _p2;
};

[[vk::binding(0, 3)]] cbuffer LightParamsCB : register(b0, space3) {
    LightParams p;
};

float4 frag_light(VOut input) : SV_Target {
    float2 px = input.uv * float2(p.gw, p.gh);
    float  d  = length(px - float2(p.cx, p.cy));
    float  f  = 1.0 - smoothstep(0.0, p.radius, d);
    f = f * f; // quadratic falloff
    float3 contrib = float3(p.r, p.g, p.b) * f * p.intensity;
    return float4(contrib, f * p.intensity);
}
