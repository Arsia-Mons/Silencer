// cppx UI geometry vertex shader (SIL-240).
//
// Standard vertex-buffer path: per-vertex {position, uv, color} fed via a
// vertex-input layout (SDL_GPUVertexInputState). Positions arrive already in
// CLIP SPACE (the points->device->clip transform is baked CPU-side in the
// emitter), so the shader is a pass-through — no uniform, no storage buffer.
// SDL_GPU HLSL vertex inputs use TEXCOORDn semantics where n == attribute
// location. Colors are PREMULTIPLIED [0,1].

struct VIn {
    float2 pos   : TEXCOORD0; // clip space
    float2 uv    : TEXCOORD1; // [0,1]
    float4 color : TEXCOORD2; // premultiplied
};

struct VOut {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : TEXCOORD1;
};

VOut vert_ui(VIn input) {
    VOut o;
    o.pos   = float4(input.pos, 0.0, 1.0);
    o.uv    = input.uv;
    o.color = input.color;
    return o;
}
