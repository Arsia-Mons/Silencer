// cppx UI geometry fragment shader (SIL-240).
//
// Premultiplied vertex color modulated by one texture. The single multiply
// reproduces every cppx draw the legacy executor performed:
//   - solid fills/borders/gradients/shadows: bind a 1x1 white texture, so
//     result = vertex_color (the tessellated, gouraud-interpolated premultiplied
//     mesh colour, fringe band included);
//   - sprite images: vertex_color carries the premultiplied tint, the texel is
//     premultiplied -> tint * texel (matches SDL color/alpha-mod on a premult
//     texture);
//   - glyph coverage atlas (white premultiplied mask): vertex_color carries the
//     premultiplied token colour -> colour * coverage;
//   - exact-colour glyph faces: vertex_color is white -> texel passes through.
// Drawn under premultiplied-over blending (SDL_GPU_BLENDFACTOR_ONE /
// ONE_MINUS_SRC_ALPHA), the GPU analogue of SDL_BLENDMODE_BLEND_PREMULTIPLIED.

struct VOut {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : TEXCOORD1;
};

[[vk::binding(0, 2)]] Texture2D<float4> tex  : register(t0, space2);
[[vk::binding(1, 2)]] SamplerState      samp : register(s0, space2);

float4 frag_ui(VOut input) : SV_Target {
    return input.color * tex.Sample(samp, input.uv);
}
