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

#include <map>
#include <string>
#include <vector>

namespace silencer::cppx_ui {

class GlyphFonts {
public:
  // Faces mirror FontRegistry::FaceId (Body/Large/Title/Tiny/Heading). Each maps
  // to a legacy bank; see game_ui_pipeline's glyph bake for the bank wiring.
  static constexpr int kFaceCount = 12;
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

  // Exact-color variant atlas: bakes the glyphs' palettized colors VERBATIM
  // (opaque where index != 0, exactly like origin's DrawText blit — no alpha
  // ramp), registered under the token color the IR will carry. The caller runs
  // the legacy Effect* pipeline on the indexed art FIRST, so the baked pixels
  // are origin's rendered text pixels for that style. The executor selects the
  // variant when a Text command's color matches `key` (premul a=255 == straight)
  // and skips color modulation; unmatched colors use the coverage face.
  // legacy_w/h = the 640x480 design res origin's virtual-canvas scale derives
  // from (carried per variant for the string-variant bake below).
  bool build_color_face(SDL_Renderer *renderer, int face_id, uint8_t key_r,
                        uint8_t key_g, uint8_t key_b, const GlyphSrc *glyphs,
                        int count, const SDL_Color *palette256, float advance,
                        float line_height, int legacy_w, int legacy_h,
                        uint8_t alpha = 255); // <255: premultiplied translucent
                                              // pixels (origin DrawAlphaed text)

  // Per-phase STRING variant (origin text striping parity). origin composites
  // text on its virtual canvas (integer pen, advance per char) and the whole-
  // frame NEAREST magnify (src = int(gx/s)) stripes it per ABSOLUTE position —
  // a single-phase atlas can never match a string whose golden phase differs,
  // and the per-glyph float pen drifts x-phase WITHIN a string. When a Text
  // draw uses an exact-color face at 1:1 virtual scale (gscale == s), this
  // bakes the whole string through origin's chain at its absolute device cell
  // and returns a texture drawn 1:1 (string analog of TextureRegistry::
  // resolve_legacy). Memoized on (face, color, string, X%18, Y%18) —
  // the magnify pattern period divides 18 for every quarter-integer s in play.
  // Textures owned HERE, outside the 64-cap chrome TextureRegistry (the cap
  // note in texture_registry.h stands; string variants are unbounded-ish and
  // get their own capped store). False when the face has no exact-color
  // variant or the draw isn't 1:1 virtual — caller keeps the atlas path.
  struct StringVariant {
    SDL_Texture *texture = nullptr;
    int x = 0, y = 0, w = 0, h = 0; // absolute device cell, drawn 1:1
  };
  bool string_variant(SDL_Renderer *renderer, uint16_t face_id, uint8_t r,
                      uint8_t g, uint8_t b, const char *text, int len,
                      float pen_dev_x, float pen_dev_y, float gscale,
                      int out_w, int out_h, StringVariant *out);

  // The face for an id (Body fallback for out-of-range). null if not loaded.
  const Face *face(uint16_t face_id) const;
  // The exact-color variant for (face, token color); null if none registered.
  const Face *color_face(uint16_t face_id, uint8_t r, uint8_t g,
                         uint8_t b) const;
  bool any_loaded() const;

  void shutdown();

private:
  static constexpr int kVariantCap = 128; // menus ~18 + HUD palette/brightness ramps (~80)
  // Per-screen string count is small (menus ~20); lobby lists stay well under
  // this. At cap the whole store recycles (cheap rebake, no eviction churn).
  static constexpr int kStringVariantCap = 256;
  struct Variant {
    Face face;
    uint16_t face_id = 0;
    uint8_t r = 0, g = 0, b = 0;
    bool used = false;
    // CPU copy of the atlas (straight RGBA, ink opaque) + virtual-canvas dims,
    // kept for the string-variant bake.
    std::vector<uint8_t> pixels;
    int atlas_w = 0;
    int legacy_w = 640, legacy_h = 480;
  };

  Face faces_[kFaceCount];
  Variant variants_[kVariantCap];
  std::map<std::string, StringVariant> string_variants_;
};

} // namespace silencer::cppx_ui
