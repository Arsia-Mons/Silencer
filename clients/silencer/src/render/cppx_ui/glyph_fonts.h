#pragma once

// Bitmap glyph fonts — origin/main parity text rendering.
//
// origin renders all UI text as monospace sprite glyphs from the legacy bitmap
// banks (not TTF): fixed `advance` per char, art may overlap. This holder bakes
// one atlas texture per face (all glyphs in a row) and carries the monospace
// metrics, so the draw executor and text measurer read the same numbers
// (measure == paint). Atlases are owned here, separate from TextureRegistry.
//
// build_face bakes a white coverage mask (index 0 transparent, else white *
// luminance/peak) tinted at draw time; build_color_face bakes the palettized
// colors verbatim.

#include <SDL3/SDL.h>

#include <stdint.h>

#include <vector>

namespace silencer::cppx_ui {

class GlyphFonts {
public:
  static constexpr int kFaceCount = 12;
  // Printable ASCII window stored in each atlas: space(32) .. '~'(126).
  static constexpr int kFirstChar = 32;
  static constexpr int kLastChar = 126;
  static constexpr int kGlyphCount = kLastChar - kFirstChar + 1; // 95

  // One source glyph for build_face. A null/empty entry (indices==nullptr ||
  // w<=0) is a blank cell (e.g. space) — no art, advance only.
  struct GlyphSrc {
    const uint8_t *indices = nullptr;
    int w = 0;
    int h = 0;
  };

  struct Face {
    SDL_Texture *atlas = nullptr; // all glyphs packed left-to-right; owned
    int atlas_w = 0;              // atlas width, px (GPU UV denominator)
    int atlas_h = 0;              // atlas (= max native glyph) height, px
    int ascent = 0;               // cap-top..baseline, px (640-space); drives
                                  // caret height. atlas_h - ascent = descender zone
    float advance = 0.f;          // native monospace pen step, px (640-space)
    float line_height = 0.f;      // native bank line height, px (640-space)
    int16_t gx[kGlyphCount] = {}; // glyph x-origin in the atlas
    int16_t gw[kGlyphCount] = {}; // glyph width (0 = blank cell)
    // Resident premultiplied atlas bytes + key so the GPU backend uploads once.
    uint64_t gpu_key = 0;
    std::vector<uint8_t> atlas_rgba;
    bool loaded = false;
  };

  GlyphFonts() = default;
  ~GlyphFonts();
  GlyphFonts(const GlyphFonts &) = delete;
  GlyphFonts &operator=(const GlyphFonts &) = delete;

  // Compose + upload one face's coverage-mask atlas from `count` source glyphs
  // (glyphs[i] is char kFirstChar + i). advance/line_height are native
  // (640-space). Replaces any prior face.
  bool build_face(SDL_Renderer *renderer, int face_id, const GlyphSrc *glyphs,
                  int count, const SDL_Color *palette256, float advance,
                  float line_height);

  // Exact-color variant atlas: bakes palettized colors verbatim, registered
  // under the token color the IR carries. The caller pre-applies the legacy
  // Effect* pipeline to the indexed art, so the baked pixels are origin's
  // rendered text. The executor selects the variant on a color match and skips
  // modulation; unmatched colors use the coverage face.
  bool build_color_face(SDL_Renderer *renderer, int face_id, uint8_t key_r,
                        uint8_t key_g, uint8_t key_b, const GlyphSrc *glyphs,
                        int count, const SDL_Color *palette256, float advance,
                        float line_height,
                        uint8_t alpha = 255); // <255: translucent (DrawAlphaed)

  // The face for an id (Body fallback for out-of-range). null if not loaded.
  const Face *face(uint16_t face_id) const;
  // The exact-color variant for (face, token color); null if none registered.
  const Face *color_face(uint16_t face_id, uint8_t r, uint8_t g,
                         uint8_t b) const;
  bool any_loaded() const;

  void shutdown();

private:
  static constexpr int kVariantCap = 128;
  struct Variant {
    Face face;
    uint16_t face_id = 0;
    uint8_t r = 0, g = 0, b = 0;
    bool used = false;
  };

  Face faces_[kFaceCount];
  Variant variants_[kVariantCap];
};

} // namespace silencer::cppx_ui
