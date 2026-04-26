// Fullscreen-triangle vertex shader.
// Generates 3 vertices from SV_VertexID (no VBO).
// Y-flipped UVs to match the MSL pixel convention so all backends sample
// the same framebuffer orientation.

struct VOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VOut vert_screen(uint vid : SV_VertexID) {
    const float2 pos[3] = { float2(-1.0, -1.0), float2( 3.0, -1.0), float2(-1.0,  3.0) };
    const float2 uv[3]  = { float2( 0.0,  1.0), float2( 2.0,  1.0), float2( 0.0, -1.0) };
    VOut o;
    o.pos = float4(pos[vid], 0.0, 1.0);
    o.uv  = uv[vid];
    return o;
}
