#pragma once

// Bitmap glyph fonts — origin/main parity text rendering.
//
// origin/main renders ALL UI text as sprite glyphs from the legacy bitmap font
// banks 132..136 (renderer.cpp::DrawText), NOT TTF. Each glyph is a sprite; the
// layout is MONOSPACE (a fixed `advance` per character, glyph art may overlap),
// glyph index = char - 33 (banks 133..136) or char - 34 (bank 132). The golden
// captures are those glyphs rendered at the legacy 640x480 then upscaled ~1.5x
// nearest-neighbor — the chunky look TTF can never reproduce.
//
// This holder bakes one ATLAS TEXTURE per face (all glyphs packed in a row) and
// carries the monospace metrics, so the draw executor and the text measurer both
// read the SAME numbers (measure == paint). Atlas textures are owned here,
// independent of the 64-entry chrome TextureRegistry — five textures total.
//
// Glyphs are baked as a WHITE COVERAGE mask: index 0 -> transparent; every other
// index -> white premultiplied by its palette color's luminance, normalized per
// face so the brightest glyph pixel = full coverage. This preserves the original
// art's anti-aliased ramp (dim edges) as an alpha falloff. The draw executor
// tints each glyph by the Text command's token color, so the design's color
// family drives text color (origin varied it via EffectColor/brightness) AND the
// muted golden greens reproduce exactly when the token carries them.

#include <SDL3/SDL.h>

#include <stdint.h>

namespace silencer::cppx_ui {

class GlyphFonts {
public:
  // Faces mirror FontRegistry::FaceId (Body/Large/Title/Tiny/Heading). Each maps
  // to a legacy bank; see game_ui_pipeline's glyph bake for the bank wiring.
  static constexpr int kFaceCount = 5;
  // Printable ASCII window stored in each atlas: space(32) .. '~'(126).
  static constexpr int kFirstChar = 32;
  static constexpr int kLastChar = 126;
  static constexpr int kGlyphCount = kLastChar - kFirstChar + 1; // 95

  // One source glyph for build_face: indexed pixels + dims. A null/empty entry
  // (indices==nullptr || w<=0) is a blank cell (e.g. space) — no art, advance
  // only.
  struct GlyphSrc {
    const uint8_t *indices = nullptr;
    int w = 0;
    int h = 0;
  };

  struct Face {
    SDL_Texture *atlas = nullptr; // all glyphs packed left-to-right; owned
    int atlas_h = 0;              // atlas (= max native glyph) height, px
    float advance = 0.f;          // native monospace pen step, px (640-space)
    float line_height = 0.f;      // native bank line height, px (640-space)
    int16_t gx[kGlyphCount] = {}; // glyph x-origin in the atlas
    int16_t gw[kGlyphCount] = {}; // glyph width (0 = blank cell)
    bool loaded = false;
  };

  GlyphFonts() = default;
  ~GlyphFonts();
  GlyphFonts(const GlyphFonts &) = delete;
  GlyphFonts &operator=(const GlyphFonts &) = delete;

  // Compose + upload one face's atlas from `count` source glyphs (count <=
  // kGlyphCount; glyphs[i] is the char kFirstChar + i). `palette256` resolves
  // each glyph index to a color whose luminance becomes the coverage (alpha),
  // normalized per face. advance / line_height are the native (640-space)
  // metrics. Replaces any prior face.
  bool build_face(SDL_Renderer *renderer, int face_id, const GlyphSrc *glyphs,
                  int count, const SDL_Color *palette256, float advance,
                  float line_height);

  // The face for an id (Body fallback for out-of-range). null if not loaded.
  const Face *face(uint16_t face_id) const;
  bool any_loaded() const;

  void shutdown();

private:
  Face faces_[kFaceCount];
};

} // namespace silencer::cppx_ui
