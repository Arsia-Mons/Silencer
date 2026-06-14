#include "ui_draw_program_builder.h"

#include "ui/runtime/geometry.h"

#include <algorithm>
#include <cmath>

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

  // Append a tessellated solid/textured mesh, de-indexed, under `key`.
  void append_mesh(const ::ui::MeshSink &sink, uint64_t key) {
    if (sink.vcount <= 0 || sink.icount <= 0)
      return;
    ensure_batch(key);
    for (int k = 0; k < sink.icount; ++k)
      push_vert(sink.verts[sink.idx[k]]);
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

} // namespace

void build_ui_draw_program(const ::ui::DrawCommandList &list,
                           FontRegistry *fonts, TextureRegistry *textures,
                           GlyphFonts *glyphs, SDL_Renderer *legacy_renderer,
                           float scale, int target_w, int target_h,
                           uint64_t texture_generation, GpuUiProgram &out) {
  // Stage 1 scope: solid mesh (rect/border/gradient/shadow) + clip scissor.
  // Text, images, the legacy snaps (stage 2) and layers (stage 3) land next.
  (void)fonts;
  (void)textures;
  (void)glyphs;
  (void)legacy_renderer;

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

  for (int ci = 0; ci < list.count; ++ci) {
    const ::ui::DrawCommand &c = list.commands[ci];
    ::ui::MeshSink sink{vscratch, kVScratch, 0, iscratch, kIScratch, 0};
    switch (c.kind) {
    case ::ui::DrawCommandKind::Rect:
      if (c.payload.rect.fill.a > 0) {
        ::ui::tessellate_rect_fill(c.rect, c.payload.rect.corner_radius,
                                   c.payload.rect.fill, sink, feather);
        b.append_mesh(sink, 0);
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
    case ::ui::DrawCommandKind::Border:
      ::ui::tessellate_frame(c.rect, c.payload.border.corner_radius,
                             c.payload.border.border, c.payload.border.outline,
                             sink, feather);
      b.append_mesh(sink, 0);
      break;
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
    case ::ui::DrawCommandKind::Image:
    case ::ui::DrawCommandKind::Text:
      // Stage 2: textured quads (glyph atlas / sprite) + legacy snaps.
      break;
    case ::ui::DrawCommandKind::LayerPush:
    case ::ui::DrawCommandKind::LayerPop:
      // Stage 3: group-opacity transient render targets. Until then the
      // subtree draws inline (full opacity), which is correct for opacity==1.
      break;
    default:
      break;
    }
  }

  b.finish();
}

} // namespace silencer::cppx_ui
