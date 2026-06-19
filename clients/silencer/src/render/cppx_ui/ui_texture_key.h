#pragma once

// Stable uint64 keys for the GPU UI texture cache. The backend caches resident
// textures by this key, flushing on a texture_generation bump. The high byte
// tags the source so the small per-source ids (TextureRegistry slots, GlyphFonts
// face ids) can't collide; keys must be injective within a generation and stable
// frame-to-frame.

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
