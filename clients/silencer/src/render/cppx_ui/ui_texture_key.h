#pragma once

// ui_texture_key.h — stable uint64 keys for the GPU UI texture cache.
//
// The GPU backend caches resident SDL_GPUTextures by an opaque uint64 key
// (sdl3gpubackend ui_tex_cache), flushing the whole cache when the cppx
// registries reset (texture_generation bump). The keys only need to be:
//   - injective across the distinct texture sources within one generation, and
//   - stable frame-to-frame so a texture uploads once and is reused.
// The high byte tags the source so the small per-source ids (TextureRegistry
// 1-based slots, GlyphFonts face ids) can't collide.

#include <cstdint>

namespace silencer::cppx_ui::ui_texture_key {

// Image textures AND lazily-baked legacy sprite variants: both live in
// TextureRegistry slots (1-based texture_id, stable within a generation).
inline uint64_t image(uint32_t texture_id) {
  return (uint64_t)0x01 << 56 | (uint64_t)texture_id;
}

// Glyph coverage-mask atlas (one per FaceId).
inline uint64_t glyph_face(uint16_t face_id) {
  return (uint64_t)0x02 << 56 | (uint64_t)face_id;
}

// Glyph exact-color variant atlas (one per FaceId + token color).
inline uint64_t glyph_color(uint16_t face_id, uint8_t r, uint8_t g, uint8_t b) {
  return (uint64_t)0x03 << 56 | ((uint64_t)face_id << 24) |
         ((uint64_t)r << 16) | ((uint64_t)g << 8) | (uint64_t)b;
}

} // namespace silencer::cppx_ui::ui_texture_key
