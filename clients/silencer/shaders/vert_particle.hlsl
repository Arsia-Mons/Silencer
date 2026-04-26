// Particle vertex shader: derives quad corners from a GPU storage buffer.
// 6 vertices per particle (2 triangles). Dead particles (life<=0) produce
// a degenerate quad outside clip space and are silently discarded.
// Uniform buffer 0: float4(game_w, game_h, 0, 0).

struct Particle {
    float x;        float y;        float vx;       float vy;
    float life;     float max_life; uint  color_idx; uint flags;
};

struct PVert {
    float4 pos   : SV_Position;
    float  pal_u : TEXCOORD0;
};

[[vk::binding(0, 0)]] StructuredBuffer<Particle> parts : register(t0, space0);
[[vk::binding(0, 1)]] cbuffer FrameInfo : register(b0, space1) {
    float4 fi; // xy = (game_w, game_h)
};

PVert vert_particle(uint vid : SV_VertexID) {
    const float2 off[6] = {
        float2(-1.5, -1.5), float2( 1.5, -1.5), float2(-1.5,  1.5),
        float2(-1.5,  1.5), float2( 1.5, -1.5), float2( 1.5,  1.5)
    };
    uint pid = vid / 6;
    uint cor = vid % 6;
    Particle p = parts[pid];
    float2 ndc = (float2(p.x, p.y) + off[cor]) / fi.xy * 2.0 - 1.0;
    ndc.y = -ndc.y;
    PVert o;
    o.pos   = (p.life > 0.0) ? float4(ndc, 0.0, 1.0) : float4(2.0, 2.0, 0.0, 0.0);
    o.pal_u = float(p.color_idx) * (255.0 / 256.0) + 0.5 / 256.0;
    return o;
}
