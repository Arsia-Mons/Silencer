#include "draw_executor.h"

#include "font_registry.h"
#include "glyph_fonts.h"
#include "sdf_raster.h"
#include "texture_registry.h"
#include "ui_draw_geometry.h"
#include "ui/runtime/geometry.h"

#include <cmath>
#include <math.h>
#include <string.h>

namespace silencer::cppx_ui {

namespace {

// Per-frame scratch for one command's mesh + its SDL conversion. Single static
// instance (UI is single-threaded); sized for the worst single-command mesh
// (a full ring band ~ a few thousand verts).
constexpr int kVScratch = 8192;
constexpr int kIScratch = 16384;
struct Scratch {
  ::ui::Vertex v[kVScratch];
  uint16_t i[kIScratch];
  SDL_Vertex sv[kVScratch];
  int si[kIScratch];
};

void submit(SDL_Renderer *r, const ::ui::MeshSink &sink, Scratch &s,
            float scale) {
  if (sink.vcount <= 0 || sink.icount <= 0)
    return;
  for (int k = 0; k < sink.vcount; ++k) {
    const ::ui::Vertex &v = s.v[k];
    s.sv[k].position = {v.x * scale, v.y * scale}; // points -> device pixels
    s.sv[k].color = {v.color.r / 255.f, v.color.g / 255.f, v.color.b / 255.f,
                     v.color.a / 255.f}; // premultiplied
    s.sv[k].tex_coord = {v.u, v.v};
  }
  for (int k = 0; k < sink.icount; ++k)
    s.si[k] = static_cast<int>(s.i[k]);
  SDL_RenderGeometry(r, nullptr, s.sv, sink.vcount, s.si, sink.icount);
}

SDL_Rect round_out(const ::ui::DrawRect &r, float scale) {
  const int x0 = static_cast<int>(floorf(r.x * scale));
  const int y0 = static_cast<int>(floorf(r.y * scale));
  const int x1 = static_cast<int>(ceilf((r.x + r.w) * scale));
  const int y1 = static_cast<int>(ceilf((r.y + r.h) * scale));
  return {x0, y0, x1 - x0, y1 - y0};
}

// IR text color is PREMULTIPLIED (the color-space contract); TTF rasterizes with
// a STRAIGHT-alpha color and blits under a straight-alpha texture, so we must
// un-premultiply first. Opaque text (a==255) is a no-op; this fixes translucent
// / disabled text from double-darkening.
SDL_Color unpremultiply(::ui::Color c) {
  if (c.a == 0)
    return {0, 0, 0, 0};
  if (c.a == 255)
    return {c.r, c.g, c.b, 255};
  auto un = [&](uint8_t v) -> Uint8 {
    int s = (static_cast<int>(v) * 255 + c.a / 2) / c.a;
    return static_cast<Uint8>(s > 255 ? 255 : s);
  };
  return {un(c.r), un(c.g), un(c.b), c.a};
}

// Render text as origin/main bitmap glyph sprites (monospace, char-33 -> glyph
// in the face atlas), tinted by the token color, nearest-neighbor scaled — the
// chunky look the golden has and TTF cannot reproduce. Returns false if this
// face has no baked glyph atlas (caller falls back to the TTF path).
//
// Canonical glyph cells (SIL-190): every glyph draws into an INTEGER device
// rect whose size is rounded ONCE from the native art (round(gw*gscale) x
// round(atlas_h*gscale)) — same letter, byte-identical pixels anywhere on
// screen. This deliberately diverges from origin, whose whole-frame nearest
// magnify striped glyphs by absolute position (golden supersession documented
// in ORIGIN_GOLDENS.md). The pen still accumulates the DESIGN
// metric (advance * gscale, fractional) and only snaps per glyph, so string
// widths, wraps and centering keep origin's layout exactly (a uniform integer
// advance was tried and re-wrapped prose out of its fixed containers).
bool render_text_glyphs(SDL_Renderer *r, const ::ui::DrawCommandList &list,
                        const ::ui::DrawCommand &c, GlyphFonts *glyphs,
                        float scale) {
  if (!glyphs)
    return false;
  const ::ui::TextData &t = c.payload.text;
  // Exact-color variant first: pre-baked origin text pixels for this token
  // color (opaque palette ramp, no tint). Fall back to the white-coverage
  // atlas modulated by the token color.
  const GlyphFonts::Face *cf =
      t.color.a == 255
          ? glyphs->color_face(t.font_id, t.color.r, t.color.g, t.color.b)
          : nullptr;
  const GlyphFonts::Face *gf = cf ? cf : glyphs->face(t.font_id);
  if (!gf || !gf->atlas || gf->line_height <= 0.f || t.font_size == 0)
    return false;

  // font_size is the target device CELL height in points; the executor scales
  // points -> device by `scale`. glyph scale maps native bank px -> device px.
  // Canonical integer glyph cells + floor-quantized pen — the math is shared
  // with the GPU emitter (ui_draw_geometry::text_layout/glyph_dst) so both paths
  // land glyphs on identical device pixels.
  const TextLayout L = text_layout(gf->advance, gf->line_height, gf->atlas_h,
                                   t.font_size, scale, c.rect);

  // Reveal ramp (origin DrawMessage typewriter "pop"): brighten the trailing
  // glyphs by adding a falling boost to the green channel, resolving the exact-
  // color face PER GLYPH. Glyph metrics are identical across color variants, so
  // L (built from the base face) positions all of them. Mirrors emit_text.
  if (t.reveal_boost > 0 && t.color.a == 255) {
    for (uint16_t i = 0; i < t.text_len; ++i) {
      const unsigned char ch =
          static_cast<unsigned char>(list.text_arena[t.text_off + i]);
      if (ch < GlyphFonts::kFirstChar || ch > GlyphFonts::kLastChar)
        continue;
      const int gi = ch - GlyphFonts::kFirstChar;
      const int dist = (int)t.text_len - (int)i; // 1 = last glyph
      int boost = (int)t.reveal_boost - (dist - 1) * (int)t.reveal_step;
      if (boost < 0)
        boost = 0;
      int g2 = (int)t.color.g + boost;
      if (g2 > 255)
        g2 = 255;
      const GlyphFonts::Face *cfi =
          glyphs->color_face(t.font_id, t.color.r, (uint8_t)g2, t.color.b);
      const GlyphFonts::Face *gfi = cfi ? cfi : glyphs->face(t.font_id);
      if (!gfi || !gfi->atlas)
        continue;
      const int16_t gw = gfi->gw[gi];
      if (gw <= 0)
        continue;
      if (!cfi) {
        SDL_SetTextureColorMod(gfi->atlas, t.color.r, (uint8_t)g2, t.color.b);
        SDL_SetTextureAlphaMod(gfi->atlas, t.color.a);
      }
      SDL_FRect src = {static_cast<float>(gfi->gx[gi]), 0.f,
                       static_cast<float>(gw),
                       static_cast<float>(gfi->atlas_h)};
      const DevRect gd = glyph_dst(L, i, gw);
      SDL_FRect dst = {gd.x, gd.y, gd.w, gd.h};
      SDL_RenderTexture(r, gfi->atlas, &src, &dst);
      if (!cfi) {
        SDL_SetTextureColorMod(gfi->atlas, 255, 255, 255);
        SDL_SetTextureAlphaMod(gfi->atlas, 255);
      }
    }
    return true;
  }

  // Tint: the coverage atlas is a white premultiplied mask; the IR color is
  // premultiplied, so color-mod(rgb) + alpha-mod(a) reproduces the token color
  // exactly (drawn under BLEND_PREMULTIPLIED). Exact-color variants already
  // carry origin's rendered pixels — draw them unmodulated.
  if (!cf) {
    SDL_SetTextureColorMod(gf->atlas, t.color.r, t.color.g, t.color.b);
    SDL_SetTextureAlphaMod(gf->atlas, t.color.a);
  }
  for (uint16_t i = 0; i < t.text_len; ++i) {
    const unsigned char ch =
        static_cast<unsigned char>(list.text_arena[t.text_off + i]);
    if (ch >= GlyphFonts::kFirstChar && ch <= GlyphFonts::kLastChar) {
      const int gi = ch - GlyphFonts::kFirstChar;
      const int16_t gw = gf->gw[gi];
      if (gw > 0) {
        SDL_FRect src = {static_cast<float>(gf->gx[gi]), 0.f,
                         static_cast<float>(gw),
                         static_cast<float>(gf->atlas_h)};
        const DevRect gd = glyph_dst(L, i, gw);
        SDL_FRect dst = {gd.x, gd.y, gd.w, gd.h};
        SDL_RenderTexture(r, gf->atlas, &src, &dst);
      }
    }
    // monospace: every char advances by the fractional design metric (the dst
    // rect above snaps per glyph); art may be wider than the step (overlap).
  }
  if (!cf) {
    SDL_SetTextureColorMod(gf->atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(gf->atlas, 255);
  }
  return true;
}

void render_text(SDL_Renderer *r, const ::ui::DrawCommandList &list,
                 const ::ui::DrawCommand &c, FontRegistry *fonts, float scale,
                 GlyphFonts *glyphs) {
  const ::ui::TextData &t = c.payload.text;
  if (t.text_len == 0 || t.color.a == 0)
    return;
  // Origin bitmap-glyph path (preferred); TTF fallback if the face has no atlas.
  if (render_text_glyphs(r, list, c, glyphs, scale))
    return;
  if (!fonts || !fonts->default_font())
    return;
  char buf[256];
  size_t n = t.text_len < 255 ? t.text_len : 255;
  memcpy(buf, &list.text_arena[t.text_off], n);
  buf[n] = '\0';

  const SDL_Color col = unpremultiply(t.color);
  // Rasterize at DEVICE resolution (font_size * scale) so text is crisp; the
  // destination is positioned in device pixels and sized to the (device-res)
  // texture, so it lands 1:1 with no upscaling blur.
  const int pixel_size =
      t.font_size > 0 ? static_cast<int>(t.font_size * scale + 0.5f) : 0;
  const float dx = c.rect.x * scale, dy = c.rect.y * scale;

  // Fast path: registry-cached texture (string rasterized + uploaded ONCE, then
  // reused every frame instead of rebuilt per frame).
  int tw = 0, th = 0;
  if (SDL_Texture *cached = fonts->cached_text_texture(
          r, t.font_id, buf, n, pixel_size, col, &tw, &th)) {
    SDL_FRect dst = {dx, dy, static_cast<float>(tw), static_cast<float>(th)};
    SDL_RenderTexture(r, cached, nullptr, &dst);
    return;
  }

  // Uncached fallback (empty/over-long/failed): one-off rasterize + free.
  TTF_Font *font = fonts->face(t.font_id);
  if (pixel_size > 0)
    TTF_SetFontSize(font, static_cast<float>(pixel_size));
  SDL_Surface *surface = TTF_RenderText_Blended(font, buf, n, col);
  if (!surface)
    return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
  if (texture) {
    SDL_FRect dst = {dx, dy, static_cast<float>(surface->w),
                     static_cast<float>(surface->h)};
    SDL_RenderTexture(r, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
  }
  SDL_DestroySurface(surface);
}

// Draw a textured Image command (design §9.7). Three paths:
//   - nine-slice (any nine_slice width > 0): 9 sub-rects, corners 1:1, edges
//     stretched along one axis, center stretched both. Radius ignored (cut).
//   - rounded (corner_radius > 0.5, no nine-slice): §9.3 fill tessellation with
//     texture + per-vertex UVs; tint folded into the per-vertex (premultiplied)
//     color.
//   - plain: one SDL_RenderTexture with color/alpha mod from tint, restored to
//     white immediately after so the cached texture is left unmodulated.
// `tint` is premultiplied (transcriber's emit boundary). SDL color/alpha mod
// multiplies the sampled (already-premultiplied) texel — so the premultiplied
// tint multiplies straight through, which is the correct premultiplied tint.
void render_image(SDL_Renderer *r, const ::ui::DrawCommand &c,
                  TextureRegistry *textures, Scratch &s, float feather,
                  float scale) {
  if (!textures)
    return;
  const ::ui::ImageData &img = c.payload.image;
  SDL_Texture *tex = textures->lookup(img.texture_id);
  if (!tex)
    return;

  float tw = 0.f, th = 0.f;
  SDL_GetTextureSize(tex, &tw, &th);
  if (tw <= 0.f || th <= 0.f)
    return;

  const ::ui::Color tint = img.tint;
  const ::ui::SideWidths &ns = img.nine_slice;
  const bool nine = ns.top > 0.f || ns.right > 0.f || ns.bottom > 0.f ||
                    ns.left > 0.f;

  // Source sub-rect (atlasing / partial-fill). w/h==0 => sample the whole
  // texture. Texture-space pixels; nine-slice ignores it (insets are
  // already texture-space).
  const bool has_src = img.src_w > 0.f && img.src_h > 0.f;
  const SDL_FRect src_sub = {img.src_x, img.src_y, img.src_w, img.src_h};

  if (nine) {
    // Legacy chrome nine-slice (origin DispatchButtonNineSlice): swap the
    // draw for a canonical per-size variant baked through origin's virtual
    // nine-slice + NEAREST magnify at phase 0 (U-2/SIL-204 — identical
    // pixels at every position; see resolve_legacy).
    {
      int out_w = 0, out_h = 0;
      TextureRegistry::LegacyVariant v;
      if (SDL_GetCurrentRenderOutputSize(r, &out_w, &out_h) &&
          textures->resolve_legacy(img.texture_id, r, c.rect.x * scale,
                                   c.rect.y * scale, c.rect.w * scale,
                                   c.rect.h * scale, out_w, out_h, &v)) {
        SDL_SetTextureColorMod(v.texture, tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(v.texture, tint.a);
        SDL_FRect d = {(float)v.x, (float)v.y, (float)v.w, (float)v.h};
        SDL_RenderTexture(r, v.texture, nullptr, &d);
        SDL_SetTextureColorMod(v.texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(v.texture, 255);
        return;
      }
    }
    // 9-patch: source insets in texture space, dest insets in dest space.
    // Corners 1:1 (source inset == dest inset); edges/center stretch.
    SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex, tint.a);

    // 3x3 grid (src texture-space, dst device-space) — shared with the GPU
    // emitter so both paths place identical cells.
    ImageCell cells[9];
    const int nc = image_nine_cells(c.rect, scale, tw, th, ns, cells);
    for (int k = 0; k < nc; ++k) {
      SDL_FRect src = {cells[k].src.x, cells[k].src.y, cells[k].src.w,
                       cells[k].src.h};
      SDL_FRect dst = {cells[k].dst.x, cells[k].dst.y, cells[k].dst.w,
                       cells[k].dst.h};
      SDL_RenderTexture(r, tex, &src, &dst);
    }

    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
    return;
  }

  if (img.corner_radius > 0.5f) {
    // Rounded textured rect: §9.3 tessellation, texture + per-vertex UVs, tint
    // folded into per-vertex premultiplied color. The geometry module emits uv
    // 0; we re-derive uv from each vertex's position within the rect.
    ::ui::MeshSink sink{s.v, kVScratch, 0, s.i, kIScratch, 0};
    if (!::ui::tessellate_rect_fill(c.rect, img.corner_radius, tint, sink,
                                    feather))
      return;
    const float rx = c.rect.x, ry = c.rect.y;
    const float rw = c.rect.w > 0.f ? c.rect.w : 1.f;
    const float rh = c.rect.h > 0.f ? c.rect.h : 1.f;
    // UV span: full [0,1] unless a source sub-rect selects an atlas region.
    const float u0 = has_src ? img.src_x / tw : 0.f;
    const float v0 = has_src ? img.src_y / th : 0.f;
    const float u1 = has_src ? (img.src_x + img.src_w) / tw : 1.f;
    const float v1 = has_src ? (img.src_y + img.src_h) / th : 1.f;
    for (int k = 0; k < sink.vcount; ++k) {
      const ::ui::Vertex &v = s.v[k];
      s.sv[k].position = {v.x * scale, v.y * scale}; // points -> device pixels
      s.sv[k].color = {v.color.r / 255.f, v.color.g / 255.f, v.color.b / 255.f,
                       v.color.a / 255.f}; // tint, premultiplied
      s.sv[k].tex_coord = {u0 + ((v.x - rx) / rw) * (u1 - u0),
                           v0 + ((v.y - ry) / rh) * (v1 - v0)}; // uv (sub-rect)
    }
    for (int k = 0; k < sink.icount; ++k)
      s.si[k] = static_cast<int>(s.i[k]);
    SDL_RenderGeometry(r, tex, s.sv, sink.vcount, s.si, sink.icount);
    return;
  }

  // Plain textured rect (optionally sampling an atlas sub-rect). Cover crops
  // the effective source to the dest aspect (centered); Contain letterboxes
  // the dest to the source aspect — both preserve aspect like origin's
  // PackImage cover/contain modes.
  SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(tex, tint.a);
  SDL_FRect src = has_src ? src_sub : SDL_FRect{0.f, 0.f, tw, th};
  SDL_FRect dst = {c.rect.x * scale, c.rect.y * scale, c.rect.w * scale,
                   c.rect.h * scale};
  // Legacy virtual-grid sprite (origin menu chrome): swap the draw for the
  // canonical variant baked through origin's NEAREST magnify at phase 0,
  // drawn 1:1 at the floor-quantized device cell (U-2/SIL-204) — GPU/SW
  // resampling of the native sprite can't reproduce the int(lx/s)
  // duplication bands.
  if (!has_src && !img.flip_h) {
    int out_w = 0, out_h = 0;
    TextureRegistry::LegacyVariant v;
    if (SDL_GetCurrentRenderOutputSize(r, &out_w, &out_h) &&
        textures->resolve_legacy(img.texture_id, r, dst.x, dst.y, dst.w,
                                 dst.h, out_w, out_h, &v)) {
      SDL_SetTextureColorMod(tex, 255, 255, 255);
      SDL_SetTextureAlphaMod(tex, 255);
      SDL_SetTextureColorMod(v.texture, tint.r, tint.g, tint.b);
      SDL_SetTextureAlphaMod(v.texture, tint.a);
      SDL_FRect d = {(float)v.x, (float)v.y, (float)v.w, (float)v.h};
      SDL_RenderTexture(r, v.texture, nullptr, &d);
      SDL_SetTextureColorMod(v.texture, 255, 255, 255);
      SDL_SetTextureAlphaMod(v.texture, 255);
      return;
    }
  }
  // Cover/Contain aspect fit + the native-1:1 snap — shared with the GPU
  // emitter (ui_draw_geometry::image_plain_rects) so both produce identical
  // src/dst (the resolve_legacy fast path above used the un-fitted base dst).
  {
    DevRect fs, fd;
    image_plain_rects(c.rect, scale, tw, th, has_src,
                      ::ui::DrawRect{src_sub.x, src_sub.y, src_sub.w, src_sub.h},
                      img.fit, &fs, &fd);
    src = {fs.x, fs.y, fs.w, fs.h};
    dst = {fd.x, fd.y, fd.w, fd.h};
  }
  if (img.flip_h)
    SDL_RenderTextureRotated(r, tex, &src, &dst, 0.0, nullptr,
                             SDL_FLIP_HORIZONTAL);
  else
    SDL_RenderTexture(r, tex, &src, &dst);
  SDL_SetTextureColorMod(tex, 255, 255, 255);
  SDL_SetTextureAlphaMod(tex, 255);
}

} // namespace

// One offscreen group-opacity layer (design §9.10). The target is a per-frame
// transient sized to the FULL render output (not the node box) so children draw
// at their absolute coordinates with no translation; the whole target is
// composited back over the parent at the layer opacity. `prev_target` is the
// render target that was active when this layer was pushed (restored at pop).
struct LayerSlot {
  SDL_Texture *target = nullptr;
  SDL_Texture *prev_target = nullptr;
  float opacity = 1.f;
};

// Legacy hairline frame snap. origin draws the lobby panel chrome as
// 1-VIRTUAL-px vector strokes on the 853x480 canvas and magnifies the whole
// frame by s=2.25, so each edge line covers the device pixels
// {p : int(p/s) == v} — alternating 2- and 3-px bands whose width/position
// depend on the edge's absolute virtual coordinate. A uniform logical-width
// border can land 1px off and can never produce the 3px phase, so (like
// resolve_legacy for sprites) the EXECUTOR owns the grid: the band geometry is
// computed by the shared legacy_hairline_bands (so the GPU emitter snaps
// identically) and each band is filled here.
bool snap_legacy_hairline_border(SDL_Renderer *r, const ::ui::DrawCommand &c,
                                 float scale) {
  LegacyBand bands[4];
  int n = 0;
  if (!legacy_hairline_bands(c.payload.border, c.rect, scale, bands, &n))
    return false;
  for (int i = 0; i < n; ++i) {
    const ::ui::Color col = bands[i].color;
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    SDL_FRect fr{bands[i].rect.x, bands[i].rect.y, bands[i].rect.w,
                 bands[i].rect.h};
    SDL_RenderFillRect(r, &fr);
  }
  return true;
}

// Legacy solid-fill snap (SIL-207). origin fills chrome plates (the
// create_game scrollbar thumb) as integer rects on the same virtual canvas
// its hairline strokes live on. Our box comes out of flex layout with
// fractional logical edges, so the raw quad can raster half a cell off the
// SNAPPED borders around it. The snapped rect comes from the shared
// legacy_solid_fill_rect (so the GPU emitter snaps identically) and is filled
// here.
bool snap_legacy_solid_fill(SDL_Renderer *r, const ::ui::DrawCommand &c,
                            float scale) {
  DevRect d;
  if (!legacy_solid_fill_rect(c.payload.rect, c.rect, scale, &d))
    return false;
  const ::ui::Color fill = c.payload.rect.fill;
  SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
  SDL_FRect fr{d.x, d.y, d.w, d.h};
  SDL_RenderFillRect(r, &fr);
  return true;
}

void execute_draw_commands(SDL_Renderer *renderer,
                           const ::ui::DrawCommandList &list,
                           FontRegistry *fonts, TextureRegistry *textures,
                           float scale, const RasterConfig &raster,
                           GlyphFonts *glyphs) {
  if (!renderer)
    return;
  if (scale <= 0.f)
    scale = 1.0f;
  static Scratch scratch;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND_PREMULTIPLIED);

  // Per-mode rasterization policy, computed ONCE for the whole list (the mode is
  // the single source of truth; this is the primitive stage reading it).
  //   FringeAa -> feather curved silhouettes by one DEVICE pixel. Geometry is
  //     tessellated in UI points and scaled at submit (* scale), so a feather of
  //     1/scale points is exactly 1px. (Default; reproduces the legacy path.)
  //   Ssaa     -> NO per-primitive feather: emit hard-edged geometry and let the
  //     full-scene supersample (UiSurface) do the anti-aliasing on resolve.
  //   Sdf      -> route rounded shapes to the analytic distance-field rasterizer;
  //     non-rounded shapes still take the hard-quad tessellation path.
  const bool use_sdf = (raster.mode == RenderMode::Sdf);
  const float feather =
      (raster.mode == RenderMode::FringeAa) ? (1.0f / scale) : 0.0f;
  SdfMaskCache *const sdf_cache = raster.sdf_cache;

  SDL_Rect clip_stack[16];
  int clip_depth = 0;

  constexpr int kLayerStackMax = 8;
  LayerSlot layer_stack[kLayerStackMax];
  int layer_depth = 0;

  for (int ci = 0; ci < list.count; ++ci) {
    const ::ui::DrawCommand &c = list.commands[ci];
    ::ui::MeshSink sink{scratch.v, kVScratch, 0, scratch.i, kIScratch, 0};
    switch (c.kind) {
    case ::ui::DrawCommandKind::Rect:
      if (c.payload.rect.fill.a > 0) {
        if (snap_legacy_solid_fill(renderer, c, scale))
          break;
        if (use_sdf && c.payload.rect.corner_radius > 0.5f) {
          sdf_fill_rounded(renderer, sdf_cache, c.rect,
                           c.payload.rect.corner_radius, c.payload.rect.fill,
                           scale);
        } else {
          ::ui::tessellate_rect_fill(c.rect, c.payload.rect.corner_radius,
                                     c.payload.rect.fill, sink, feather);
          submit(renderer, sink, scratch, scale);
        }
      }
      break;
    case ::ui::DrawCommandKind::Gradient: {
      const ::ui::GradientData &gd = c.payload.gradient;
      ::ui::Gradient g{};
      g.angle_deg = gd.angle_deg;
      g.stop_count = gd.stop_count;
      for (int k = 0; k < gd.stop_count && k < ::ui::UI_MAX_GRADIENT_STOPS; ++k)
        g.stops[k] = list.grad_arena[gd.stop_off + k];
      if (use_sdf && gd.corner_radius > 0.5f) {
        sdf_gradient_rounded(renderer, c.rect, gd.corner_radius, g, scale);
      } else {
        ::ui::gradient_fill_colors(c.rect, gd.corner_radius, g, sink, feather);
        submit(renderer, sink, scratch, scale);
      }
      break;
    }
    case ::ui::DrawCommandKind::Border:
      if (snap_legacy_hairline_border(renderer, c, scale))
        break;
      if (use_sdf) {
        sdf_frame_rounded(renderer, sdf_cache, c.rect,
                          c.payload.border.corner_radius,
                          c.payload.border.border, c.payload.border.outline,
                          scale);
      } else {
        ::ui::tessellate_frame(c.rect, c.payload.border.corner_radius,
                               c.payload.border.border, c.payload.border.outline,
                               sink, feather);
        submit(renderer, sink, scratch, scale);
      }
      break;
    case ::ui::DrawCommandKind::Shadow: {
      const ::ui::ShadowData &sd = c.payload.shadow;
      if (sd.color.a > 0) {
        ::ui::Shadow shadow{};
        shadow.color = sd.color;   // premultiplied at emit
        shadow.offset = sd.offset;
        shadow.blur = sd.blur;
        shadow.spread = sd.spread;
        ::ui::tessellate_shadow(c.rect, sd.corner_radius, shadow, sink);
        submit(renderer, sink, scratch, scale);
      }
      break;
    }
    case ::ui::DrawCommandKind::Image:
      render_image(renderer, c, textures, scratch, feather, scale);
      break;
    case ::ui::DrawCommandKind::Text:
      render_text(renderer, list, c, fonts, scale, glyphs);
      break;
    case ::ui::DrawCommandKind::ClipPush: {
      SDL_Rect cr = round_out(c.rect, scale);
      if (clip_depth > 0)
        SDL_GetRectIntersection(&clip_stack[clip_depth - 1], &cr, &cr);
      if (clip_depth < 16)
        clip_stack[clip_depth++] = cr;
      SDL_SetRenderClipRect(renderer, &cr);
      break;
    }
    case ::ui::DrawCommandKind::ClipPop:
      if (clip_depth > 0)
        --clip_depth;
      SDL_SetRenderClipRect(renderer,
                            clip_depth > 0 ? &clip_stack[clip_depth - 1] : nullptr);
      break;
    case ::ui::DrawCommandKind::LayerPush: {
      // Group opacity (design §9.10): redirect this node's subtree into an
      // offscreen transient sized to the full render output, so children draw at
      // their absolute coordinates with no translation. Composited back at pop.
      if (layer_depth >= kLayerStackMax) {
        SDL_assert(false && "layer stack overflow");
        break; // hard no-op in release; brackets stay balanced upstream
      }
      SDL_Texture *prev = SDL_GetRenderTarget(renderer);
      // Output size of the active target (window backbuffer or the parent layer).
      int out_w = 0, out_h = 0;
      if (prev) {
        float tw = 0.f, th = 0.f;
        SDL_GetTextureSize(prev, &tw, &th);
        out_w = static_cast<int>(tw);
        out_h = static_cast<int>(th);
      } else {
        SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
      }
      if (out_w <= 0 || out_h <= 0)
        break;
      SDL_Texture *target =
          SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                            SDL_TEXTUREACCESS_TARGET, out_w, out_h);
      if (!target)
        break;
      SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
      layer_stack[layer_depth++] = {target, prev, c.payload.layer.opacity};
      SDL_SetRenderTarget(renderer, target);
      SDL_SetRenderClipRect(renderer, nullptr); // clip is in target-local space
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
      SDL_RenderClear(renderer); // transparent (0,0,0,0)
      // Re-apply the active clip (target shares the absolute coordinate space).
      if (clip_depth > 0)
        SDL_SetRenderClipRect(renderer, &clip_stack[clip_depth - 1]);
      break;
    }
    case ::ui::DrawCommandKind::LayerPop: {
      if (layer_depth <= 0) {
        SDL_assert(false && "layer stack underflow");
        break;
      }
      LayerSlot slot = layer_stack[--layer_depth];
      SDL_SetRenderTarget(renderer, slot.prev_target);
      // Composite the whole layer back over the parent at the layer opacity.
      // The layer texture is PREMULTIPLIED (cleared transparent, drawn premul),
      // so a correct group fade scales BOTH the premultiplied RGB and the alpha
      // by `opacity` (alpha-mod alone would scale only A, leaving RGB at full
      // intensity and breaking the premultiplied invariant). Hence color-mod +
      // alpha-mod, blended under BLEND_PREMULTIPLIED.
      SDL_SetTextureBlendMode(slot.target, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
      Uint8 amod = static_cast<Uint8>(slot.opacity * 255.0f + 0.5f);
      SDL_SetTextureColorMod(slot.target, amod, amod, amod);
      SDL_SetTextureAlphaMod(slot.target, amod);
      SDL_SetRenderClipRect(renderer,
                            clip_depth > 0 ? &clip_stack[clip_depth - 1]
                                           : nullptr);
      // Both target and parent share the full-output coordinate space, so blit
      // 1:1 over the whole parent (dst = nullptr).
      SDL_RenderTexture(renderer, slot.target, nullptr, nullptr);
      SDL_DestroyTexture(slot.target);
      break;
    }
    default:
      // Custom: reserved enum seat, no renderer path (design §15).
      break;
    }
  }
  // Defensive: composite/destroy any layers left open by a malformed list so we
  // never leak a target or leave the renderer pointed at a freed texture.
  while (layer_depth > 0) {
    LayerSlot slot = layer_stack[--layer_depth];
    SDL_SetRenderTarget(renderer, slot.prev_target);
    SDL_DestroyTexture(slot.target);
  }
  SDL_SetRenderClipRect(renderer, nullptr);
}

} // namespace silencer::cppx_ui
