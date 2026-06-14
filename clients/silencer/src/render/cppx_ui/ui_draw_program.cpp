#include "ui_draw_program_builder.h"

#include "ui_draw_geometry.h"
#include "ui_texture_key.h"
#include "ui/runtime/geometry.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace silencer::cppx_ui {

namespace {

// Worst single-command mesh (a full ring band), matching the software
// executor's scratch budget.
constexpr int kVScratch = 8192;
constexpr int kIScratch = 16384;

struct IRect {
  int x = 0, y = 0, w = 0, h = 0;
};

IRect intersect(const IRect &a, const IRect &b) {
  const int x0 = std::max(a.x, b.x);
  const int y0 = std::max(a.y, b.y);
  int x1 = std::min(a.x + a.w, b.x + b.w);
  int y1 = std::min(a.y + a.h, b.y + b.h);
  if (x1 < x0) x1 = x0;
  if (y1 < y0) y1 = y0;
  return {x0, y0, x1 - x0, y1 - y0};
}

// Device-pixel clip rect for a UI-point rect: floor the leading edges, ceil the
// trailing edges (identical to the executor's round_out so clip parity holds).
IRect round_out(const ::ui::DrawRect &r, float scale) {
  const int x0 = static_cast<int>(std::floor(r.x * scale));
  const int y0 = static_cast<int>(std::floor(r.y * scale));
  const int x1 = static_cast<int>(std::ceil((r.x + r.w) * scale));
  const int y1 = static_cast<int>(std::ceil((r.y + r.h) * scale));
  return {x0, y0, x1 - x0, y1 - y0};
}

// Accumulates the flat de-indexed vertex stream and the command list, merging
// consecutive same-texture geometry (with no intervening control op) into one
// DrawBatch. Control ops (clip/layer) flush the open batch first.
class Builder {
public:
  Builder(GpuUiProgram &out, float scale, int target_w, int target_h)
      : out_(out), scale_(scale),
        inv2w_(target_w > 0 ? 2.0f / (float)target_w : 0.f),
        inv2h_(target_h > 0 ? 2.0f / (float)target_h : 0.f) {}

  // Append a tessellated solid/textured mesh, de-indexed, under `key`. Vertex
  // UVs come from the mesh (solid meshes carry uv 0 and sample the 1x1 white).
  void append_mesh(const ::ui::MeshSink &sink, uint64_t key) {
    if (sink.vcount <= 0 || sink.icount <= 0)
      return;
    ensure_batch(key);
    for (int k = 0; k < sink.icount; ++k)
      push_vert(sink.verts[sink.idx[k]]);
  }

  // Append an axis-aligned quad (2 triangles) at device rect `d` with UV
  // (u0,v0)-(u1,v1), premultiplied `color`, under texture `key` (0 = solid).
  void append_quad(const DevRect &d, float u0, float v0, float u1, float v1,
                   ::ui::Color color, uint64_t key) {
    ensure_batch(key);
    const float x0 = d.x, y0 = d.y, x1 = d.x + d.w, y1 = d.y + d.h;
    const GpuUiVertex tl = mk(x0, y0, u0, v0, color);
    const GpuUiVertex tr = mk(x1, y0, u1, v0, color);
    const GpuUiVertex br = mk(x1, y1, u1, v1, color);
    const GpuUiVertex bl = mk(x0, y1, u0, v1, color);
    out_.verts.push_back(tl);
    out_.verts.push_back(tr);
    out_.verts.push_back(br);
    out_.verts.push_back(tl);
    out_.verts.push_back(br);
    out_.verts.push_back(bl);
  }

  // Append a tessellated textured mesh (rounded image), deriving each vertex's
  // UV from its position within `rect` (the executor's render_image rounded
  // path). Vertex color carries the premultiplied tint baked by the tessellator.
  void append_image_mesh(const ::ui::MeshSink &sink, uint64_t key,
                         ::ui::DrawRect rect, float u0, float v0, float u1,
                         float v1) {
    if (sink.vcount <= 0 || sink.icount <= 0)
      return;
    ensure_batch(key);
    const float rx = rect.x, ry = rect.y;
    const float rw = rect.w > 0.f ? rect.w : 1.f;
    const float rh = rect.h > 0.f ? rect.h : 1.f;
    for (int k = 0; k < sink.icount; ++k) {
      const ::ui::Vertex &v = sink.verts[sink.idx[k]];
      const float u = u0 + ((v.x - rx) / rw) * (u1 - u0);
      const float vv = v0 + ((v.y - ry) / rh) * (v1 - v0);
      out_.verts.push_back(mk(v.x * scale_, v.y * scale_, u, vv, v.color));
    }
  }

  void emit_set_clip(const IRect &r) {
    flush();
    GpuUiCommand c;
    c.op = GpuUiOp::SetClip;
    c.clip_x = r.x;
    c.clip_y = r.y;
    c.clip_w = r.w;
    c.clip_h = r.h;
    out_.commands.push_back(c);
  }

  void emit_clear_clip() {
    flush();
    GpuUiCommand c;
    c.op = GpuUiOp::ClearClip;
    out_.commands.push_back(c);
  }

  void emit_push_layer(float opacity) {
    flush();
    GpuUiCommand c;
    c.op = GpuUiOp::PushLayer;
    c.layer_opacity = opacity;
    out_.commands.push_back(c);
  }

  void emit_pop_layer() {
    flush();
    GpuUiCommand c;
    c.op = GpuUiOp::PopLayer;
    out_.commands.push_back(c);
  }

  void finish() { flush(); }

private:
  // device px -> clip space (top-left device origin, +y down -> clip +y up).
  GpuUiVertex mk(float dx, float dy, float u, float v, ::ui::Color c) const {
    GpuUiVertex g;
    g.x = dx * inv2w_ - 1.0f;
    g.y = 1.0f - dy * inv2h_;
    g.u = u;
    g.v = v;
    g.r = c.r / 255.f;
    g.g = c.g / 255.f;
    g.b = c.b / 255.f;
    g.a = c.a / 255.f;
    return g;
  }

  void push_vert(const ::ui::Vertex &v) {
    // points -> device pixels (same scale as the legacy submit()) -> clip space.
    out_.verts.push_back(mk(v.x * scale_, v.y * scale_, v.u, v.v, v.color));
  }

  void ensure_batch(uint64_t key) {
    if (batch_open_ && batch_key_ == key)
      return;
    flush();
    batch_open_ = true;
    batch_key_ = key;
    batch_start_ = static_cast<uint32_t>(out_.verts.size());
  }

  void flush() {
    if (!batch_open_)
      return;
    const uint32_t count =
        static_cast<uint32_t>(out_.verts.size()) - batch_start_;
    if (count > 0) {
      GpuUiCommand c;
      c.op = GpuUiOp::DrawBatch;
      c.first_vertex = batch_start_;
      c.vertex_count = count;
      c.texture_key = batch_key_;
      out_.commands.push_back(c);
    }
    batch_open_ = false;
  }

  GpuUiProgram &out_;
  float scale_;
  float inv2w_;
  float inv2h_;
  bool batch_open_ = false;
  uint64_t batch_key_ = 0;
  uint32_t batch_start_ = 0;
};

// Add a texture to the frame's manifest once (the backend uploads each key once
// per generation and caches it). Skips keys already added this frame.
void register_tex(GpuUiProgram &out, std::unordered_set<uint64_t> &seen,
                  uint64_t key, const uint8_t *rgba, int w, int h) {
  if (key == 0 || !rgba || w <= 0 || h <= 0)
    return;
  if (!seen.insert(key).second)
    return;
  GpuUiTexture t;
  t.key = key;
  t.rgba = rgba;
  t.w = w;
  t.h = h;
  t.version = 0;
  out.textures.push_back(t);
}

// Lower one Image command to textured quads, mirroring render_image's dispatch
// (legacy variant -> nine-slice 9-patch -> rounded mesh -> plain) and the same
// rect/UV math via ui_draw_geometry.
void emit_image(Builder &b, GpuUiProgram &out,
                std::unordered_set<uint64_t> &seen, const ::ui::DrawCommand &c,
                TextureRegistry *textures, SDL_Renderer *legacy_renderer,
                float scale, int target_w, int target_h, float feather,
                ::ui::Vertex *vscratch, uint16_t *iscratch) {
  if (!textures)
    return;
  const ::ui::ImageData &img = c.payload.image;
  // The executor bails when the base texture is missing (lookup == null) before
  // any path runs; mirror that (legacy variants also carry a base texture).
  const uint8_t *base_rgba = nullptr;
  int base_w = 0, base_h = 0;
  if (!textures->gpu_texture(img.texture_id, &base_rgba, &base_w, &base_h))
    return;
  const float tw = (float)base_w, th = (float)base_h;
  const ::ui::Color tint = img.tint;
  const ::ui::SideWidths &ns = img.nine_slice;
  const bool nine =
      ns.top > 0.f || ns.right > 0.f || ns.bottom > 0.f || ns.left > 0.f;
  const bool has_src = img.src_w > 0.f && img.src_h > 0.f;
  const uint64_t base_key = ui_texture_key::image(img.texture_id);

  // A resolved legacy variant: upload its bytes + draw it 1:1 at its device cell.
  auto emit_variant = [&](const TextureRegistry::LegacyVariant &v) -> bool {
    const uint8_t *vr = nullptr;
    int vw = 0, vh = 0;
    if (!textures->gpu_texture(v.id, &vr, &vw, &vh))
      return false;
    const uint64_t key = ui_texture_key::image(v.id);
    register_tex(out, seen, key, vr, vw, vh);
    DevRect d = {(float)v.x, (float)v.y, (float)v.w, (float)v.h};
    b.append_quad(d, 0.f, 0.f, 1.f, 1.f, tint, key);
    return true;
  };

  if (nine) {
    TextureRegistry::LegacyVariant v;
    if (legacy_renderer &&
        textures->resolve_legacy(img.texture_id, legacy_renderer, c.rect.x * scale,
                                 c.rect.y * scale, c.rect.w * scale,
                                 c.rect.h * scale, target_w, target_h, &v) &&
        emit_variant(v))
      return;
    register_tex(out, seen, base_key, base_rgba, base_w, base_h);
    ImageCell cells[9];
    const int nc = image_nine_cells(c.rect, scale, tw, th, ns, cells);
    const float inv_tw = 1.0f / tw, inv_th = 1.0f / th;
    for (int k = 0; k < nc; ++k) {
      const DevRect s = cells[k].src;
      b.append_quad(cells[k].dst, s.x * inv_tw, s.y * inv_th,
                    (s.x + s.w) * inv_tw, (s.y + s.h) * inv_th, tint, base_key);
    }
    return;
  }

  if (img.corner_radius > 0.5f) {
    ::ui::MeshSink sink{vscratch, kVScratch, 0, iscratch, kIScratch, 0};
    if (!::ui::tessellate_rect_fill(c.rect, img.corner_radius, tint, sink,
                                    feather))
      return;
    register_tex(out, seen, base_key, base_rgba, base_w, base_h);
    const float u0 = has_src ? img.src_x / tw : 0.f;
    const float v0 = has_src ? img.src_y / th : 0.f;
    const float u1 = has_src ? (img.src_x + img.src_w) / tw : 1.f;
    const float v1 = has_src ? (img.src_y + img.src_h) / th : 1.f;
    b.append_image_mesh(sink, base_key, c.rect, u0, v0, u1, v1);
    return;
  }

  // Plain: the legacy virtual-grid variant is tried first (same gate as the
  // executor: no src sub-rect, not flipped), using the un-fitted base device
  // rect; on a miss the plain cover/contain/native rects are emitted.
  if (!has_src && !img.flip_h) {
    const DevRect base_dst = {c.rect.x * scale, c.rect.y * scale,
                              c.rect.w * scale, c.rect.h * scale};
    TextureRegistry::LegacyVariant v;
    if (legacy_renderer &&
        textures->resolve_legacy(img.texture_id, legacy_renderer, base_dst.x,
                                 base_dst.y, base_dst.w, base_dst.h, target_w,
                                 target_h, &v) &&
        emit_variant(v))
      return;
  }
  register_tex(out, seen, base_key, base_rgba, base_w, base_h);
  DevRect src, dst;
  image_plain_rects(c.rect, scale, tw, th, has_src,
                    ::ui::DrawRect{img.src_x, img.src_y, img.src_w, img.src_h},
                    img.fit, &src, &dst);
  const float inv_tw = 1.0f / tw, inv_th = 1.0f / th;
  float u0 = src.x * inv_tw, v0 = src.y * inv_th;
  float u1 = (src.x + src.w) * inv_tw, v1 = (src.y + src.h) * inv_th;
  if (img.flip_h)
    std::swap(u0, u1); // horizontal mirror = swap the U span
  b.append_quad(dst, u0, v0, u1, v1, tint, base_key);
}

// Lower one Text command to per-glyph textured quads, mirroring
// render_text_glyphs (canonical integer cells, floor-quantized pen). The GPU
// path has no TTF fallback — production faces always carry a glyph atlas (the
// chrome bake builds them, with the Body face as a guaranteed fallback).
void emit_text(Builder &b, GpuUiProgram &out, std::unordered_set<uint64_t> &seen,
               const ::ui::DrawCommandList &list, const ::ui::DrawCommand &c,
               GlyphFonts *glyphs, float scale) {
  if (!glyphs)
    return;
  const ::ui::TextData &t = c.payload.text;
  if (t.text_len == 0 || t.color.a == 0)
    return;
  // Exact-color variant first (pre-baked origin pixels), else the white-coverage
  // atlas modulated by the token color.
  const GlyphFonts::Face *cf =
      t.color.a == 255
          ? glyphs->color_face(t.font_id, t.color.r, t.color.g, t.color.b)
          : nullptr;
  const GlyphFonts::Face *gf = cf ? cf : glyphs->face(t.font_id);
  if (!gf || !gf->atlas || gf->line_height <= 0.f || t.font_size == 0)
    return;
  if (gf->atlas_rgba.empty() || gf->atlas_w <= 0 || gf->atlas_h <= 0)
    return;
  const TextLayout L = text_layout(gf->advance, gf->line_height, gf->atlas_h,
                                   t.font_size, scale, c.rect);
  if (!L.ok)
    return;
  register_tex(out, seen, gf->gpu_key, gf->atlas_rgba.data(), gf->atlas_w,
               gf->atlas_h);
  // Coverage atlas: frag = token color * coverage. Color-face pixels are final
  // (drawn unmodulated -> white vertex color).
  const ::ui::Color color =
      cf ? ::ui::Color{255, 255, 255, 255} : t.color; // premultiplied
  const float inv_aw = 1.0f / (float)gf->atlas_w;
  for (uint16_t i = 0; i < t.text_len; ++i) {
    const unsigned char ch =
        static_cast<unsigned char>(list.text_arena[t.text_off + i]);
    if (ch < GlyphFonts::kFirstChar || ch > GlyphFonts::kLastChar)
      continue;
    const int gi = ch - GlyphFonts::kFirstChar;
    const int16_t gw = gf->gw[gi];
    if (gw <= 0)
      continue;
    const DevRect d = glyph_dst(L, i, gw);
    const float u0 = (float)gf->gx[gi] * inv_aw;
    const float u1 = (float)(gf->gx[gi] + gw) * inv_aw;
    // src spans the full atlas height (v 0..1), as in render_text_glyphs.
    b.append_quad(d, u0, 0.f, u1, 1.f, color, gf->gpu_key);
  }
}

} // namespace

void build_ui_draw_program(const ::ui::DrawCommandList &list,
                           FontRegistry *fonts, TextureRegistry *textures,
                           GlyphFonts *glyphs, SDL_Renderer *legacy_renderer,
                           float scale, int target_w, int target_h,
                           uint64_t texture_generation, GpuUiProgram &out) {
  (void)fonts; // TTF fallback is not used on the GPU path (glyph atlases only)

  out.clear();
  out.target_w = target_w;
  out.target_h = target_h;
  out.texture_generation = texture_generation;
  if (scale <= 0.f)
    scale = 1.0f;

  // FringeAa: feather curved silhouettes by exactly one device pixel (the
  // production mode; matches the executor's feather = 1/scale).
  const float feather = 1.0f / scale;

  Builder b(out, scale, target_w, target_h);

  static ::ui::Vertex vscratch[kVScratch];
  static uint16_t iscratch[kIScratch];

  std::vector<IRect> clip_stack;
  std::unordered_set<uint64_t> seen_textures;

  for (int ci = 0; ci < list.count; ++ci) {
    const ::ui::DrawCommand &c = list.commands[ci];
    ::ui::MeshSink sink{vscratch, kVScratch, 0, iscratch, kIScratch, 0};
    switch (c.kind) {
    case ::ui::DrawCommandKind::Rect:
      if (c.payload.rect.fill.a > 0) {
        DevRect snapped;
        if (legacy_solid_fill_rect(c.payload.rect, c.rect, scale, &snapped)) {
          b.append_quad(snapped, 0.f, 0.f, 1.f, 1.f, c.payload.rect.fill, 0);
        } else {
          ::ui::tessellate_rect_fill(c.rect, c.payload.rect.corner_radius,
                                     c.payload.rect.fill, sink, feather);
          b.append_mesh(sink, 0);
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
      ::ui::gradient_fill_colors(c.rect, gd.corner_radius, g, sink, feather);
      b.append_mesh(sink, 0);
      break;
    }
    case ::ui::DrawCommandKind::Border: {
      LegacyBand bands[4];
      int nb = 0;
      if (legacy_hairline_bands(c.payload.border, c.rect, scale, bands, &nb)) {
        for (int i = 0; i < nb; ++i)
          b.append_quad(bands[i].rect, 0.f, 0.f, 1.f, 1.f, bands[i].color, 0);
      } else {
        ::ui::tessellate_frame(c.rect, c.payload.border.corner_radius,
                               c.payload.border.border, c.payload.border.outline,
                               sink, feather);
        b.append_mesh(sink, 0);
      }
      break;
    }
    case ::ui::DrawCommandKind::Shadow: {
      const ::ui::ShadowData &sd = c.payload.shadow;
      if (sd.color.a > 0) {
        ::ui::Shadow shadow{};
        shadow.color = sd.color;
        shadow.offset = sd.offset;
        shadow.blur = sd.blur;
        shadow.spread = sd.spread;
        ::ui::tessellate_shadow(c.rect, sd.corner_radius, shadow, sink);
        b.append_mesh(sink, 0);
      }
      break;
    }
    case ::ui::DrawCommandKind::Image:
      emit_image(b, out, seen_textures, c, textures, legacy_renderer, scale,
                 target_w, target_h, feather, vscratch, iscratch);
      break;
    case ::ui::DrawCommandKind::Text:
      emit_text(b, out, seen_textures, list, c, glyphs, scale);
      break;
    case ::ui::DrawCommandKind::ClipPush: {
      IRect cr = round_out(c.rect, scale);
      if (!clip_stack.empty())
        cr = intersect(clip_stack.back(), cr);
      clip_stack.push_back(cr);
      b.emit_set_clip(cr);
      break;
    }
    case ::ui::DrawCommandKind::ClipPop:
      if (!clip_stack.empty())
        clip_stack.pop_back();
      if (!clip_stack.empty())
        b.emit_set_clip(clip_stack.back());
      else
        b.emit_clear_clip();
      break;
    case ::ui::DrawCommandKind::LayerPush:
      // Group opacity (design §9.10): the subtree renders into a transient GPU
      // target the backend composites back at this opacity (premultiplied).
      b.emit_push_layer(c.payload.layer.opacity);
      break;
    case ::ui::DrawCommandKind::LayerPop:
      b.emit_pop_layer();
      break;
    default:
      break;
    }
  }

  b.finish();
}

} // namespace silencer::cppx_ui
