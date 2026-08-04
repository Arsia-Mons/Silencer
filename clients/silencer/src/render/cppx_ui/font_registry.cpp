#include "font_registry.h"

#ifndef SILENCER_HEADLESS
#include <SDL3_ttf/SDL_textengine.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

namespace silencer::cppx_ui {

#ifdef SILENCER_HEADLESS

FontRegistry::~FontRegistry() = default;
bool FontRegistry::load_faces(const char *) { return false; }
void FontRegistry::shutdown() {}
TTF_Font *FontRegistry::sized_face(uint16_t, int) { return nullptr; }
bool FontRegistry::draw_text(SDL_Renderer *, uint16_t, int, const char *,
                             size_t, SDL_Color, float, float) {
  return false;
}
void FontRegistry::drop_textures() {}
// sized()/glyph() are unreferenced in headless builds; no definitions needed.

#else // !SILENCER_HEADLESS

namespace {
// Indexed by FaceId.
const char *kFaceFile[FontRegistry::FaceCount] = {
    "silencer-ui.otf",       // Body
    "silencer-ui-large.otf", // Large
    "silencer-title.otf",    // Title
    "silencer-tiny.otf",     // Tiny
    "silencer-135.otf",      // Heading (bank 135)
};
} // namespace

FontRegistry::~FontRegistry() { shutdown(); }

bool FontRegistry::load_faces(const char *font_dir) {
  bool ok = true;
  for (int i = 0; i < FaceCount; ++i) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/%s", font_dir, kFaceFile[i]);
    // Nominal base; real sizes are per-(face,size) copies via sized_face.
    faces_[i] = TTF_OpenFont(path, 16.0f);
    if (!faces_[i]) {
      SDL_Log("FontRegistry: TTF_OpenFont(%s) failed: %s", path, SDL_GetError());
      ok = false;
      continue;
    }
    // These faces are generated pixel fonts (each pixel a 1x1-em square
    // outline). FreeType's hinter grid-fits those squares and collapses or
    // doubles rows at any size that isn't the native em, which reads as
    // horizontal banding. Pure geometric scaling is always right for them.
    TTF_SetFontHinting(faces_[i], TTF_HINTING_NONE);
  }
  return ok;
}

void FontRegistry::shutdown() {
  drop_textures(); // free textures before the renderer is destroyed
  for (auto &sf : sized_) {
    if (sf->font)
      TTF_CloseFont(sf->font);
  }
  sized_.clear();
  for (int i = 0; i < FaceCount; ++i) {
    if (faces_[i]) {
      TTF_CloseFont(faces_[i]);
      faces_[i] = nullptr;
    }
  }
}

void FontRegistry::drop_textures() {
  for (auto &sf : sized_) {
    for (auto &kv : sf->glyphs) {
      if (kv.second.tex)
        SDL_DestroyTexture(kv.second.tex);
    }
    sf->glyphs.clear();
  }
}

FontRegistry::SizedFont *FontRegistry::sized(uint16_t font_id, int pixel_size) {
  if (pixel_size <= 0)
    return nullptr;
  const uint16_t idx = font_id < FaceCount ? font_id : Body;
  const uint16_t face_id = faces_[idx] ? idx : (uint16_t)Body;
  if (!faces_[face_id])
    return nullptr;
  for (auto &sf : sized_) {
    if (sf->face_id == face_id && sf->pixel_size == pixel_size)
      return sf.get();
  }
  TTF_Font *font = TTF_CopyFont(faces_[face_id]);
  if (!font)
    return nullptr;
  TTF_SetFontSize(font, static_cast<float>(pixel_size));
  TTF_SetFontHinting(font, TTF_HINTING_NONE);
  sized_.push_back(std::unique_ptr<SizedFont>(new SizedFont{}));
  SizedFont *sf = sized_.back().get();
  sf->font = font;
  sf->face_id = face_id;
  sf->pixel_size = pixel_size;
  return sf;
}

TTF_Font *FontRegistry::sized_face(uint16_t font_id, int pixel_size) {
  SizedFont *sf = sized(font_id, pixel_size);
  return sf ? sf->font : nullptr;
}

FontRegistry::Glyph &FontRegistry::glyph(SDL_Renderer *renderer, SizedFont &sf,
                                         uint32_t glyph_index) {
  Glyph &g = sf.glyphs[glyph_index];
  if (!g.loaded) {
    ++rasterizations_;
    // White coverage; draw sites tint via color/alpha mod. A null image
    // (blank cell) stays a no-op glyph.
    SDL_Surface *surface =
        TTF_GetGlyphImageForIndex(sf.font, glyph_index, nullptr);
    if (surface) {
      g.tex = SDL_CreateTextureFromSurface(renderer, surface);
      SDL_DestroySurface(surface);
    }
    g.loaded = true;
  }
  return g;
}

bool FontRegistry::draw_text(SDL_Renderer *renderer, uint16_t font_id,
                             int pixel_size, const char *text, size_t len,
                             SDL_Color color, float x, float y) {
  SizedFont *sf = sized(font_id, pixel_size);
  if (!sf || !renderer || !text || len == 0)
    return false;
  // SDL_ttf's own layout engine positions the run (fractional 26.6 pen,
  // kerning — the same layout TTF_GetStringSize measures, so measure ==
  // paint); a transient TTF_Text exposes the positioned glyph ops and the
  // pixels come from our rasterize-once glyph cache.
  TTF_Text *tt = TTF_CreateText(nullptr, sf->font, text, len);
  if (!tt)
    return false;
  TTF_UpdateText(tt);
  const TTF_TextData *td = tt->internal;
  for (int i = 0; td && i < td->num_ops; ++i) {
    const TTF_DrawOperation &op = td->ops[i];
    if (op.cmd != TTF_DRAW_COMMAND_COPY)
      continue;
    Glyph &g = glyph(renderer, *sf, op.copy.glyph_index);
    if (!g.tex)
      continue;
    SDL_SetTextureColorMod(g.tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(g.tex, color.a);
    const SDL_FRect src = {(float)op.copy.src.x, (float)op.copy.src.y,
                           (float)op.copy.src.w, (float)op.copy.src.h};
    const SDL_FRect dst = {x + (float)op.copy.dst.x, y + (float)op.copy.dst.y,
                           (float)op.copy.dst.w, (float)op.copy.dst.h};
    SDL_RenderTexture(renderer, g.tex, &src, &dst);
  }
  TTF_DestroyText(tt);
  return true;
}

#endif // !SILENCER_HEADLESS

} // namespace silencer::cppx_ui
