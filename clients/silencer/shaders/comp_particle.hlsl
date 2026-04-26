// Compute kernel: advance particle positions by dt, decay life.
// Dispatch with threadcount_x=64; caller rounds count up to 64.

struct Particle {
    float x;        float y;        float vx;       float vy;
    float life;     float max_life; uint  color_idx; uint flags;
};

[[vk::binding(0, 1)]] RWStructuredBuffer<Particle> parts : register(u0, space1);
[[vk::binding(0, 2)]] cbuffer Dt : register(b0, space2) {
    float dt;
    float _pad0;
    float _pad1;
    float _pad2;
};

[numthreads(64, 1, 1)]
void update_particles(uint3 dtid : SV_DispatchThreadID) {
    uint id = dtid.x;
    Particle p = parts[id];
    if (p.life <= 0.0) return;
    p.x    += p.vx * dt;
    p.y    += p.vy * dt;
    p.life -= dt;
    if (p.life < 0.0) p.life = 0.0;
    parts[id] = p;
}
