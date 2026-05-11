// Particle fragment: palette lookup using the index baked into PVert.pal_u.

struct PVert {
    float4 pos   : SV_Position;
    float  pal_u : TEXCOORD0;
};

Texture2D<float4>  palette : register(t0, space2);
SamplerState       samp    : register(s0, space2);

float4 frag_particle(PVert input) : SV_Target {
    return palette.Sample(samp, float2(input.pal_u, 0.5));
}
