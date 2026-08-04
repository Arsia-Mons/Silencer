#include "pipeline_host.h"

#include "perf_trace.h"
#include "draw_executor.h"
#include "sprite_bake.h"
#include "text_measure.h"
#include "ui_draw_geometry.h"
#include "ui_draw_program_builder.h"

#ifndef SILENCER_HEADLESS
#include <SDL3_ttf/SDL_ttf.h>
#endif

#include <cstring>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace silencer::cppx_ui {

PipelineHost::PipelineHost() = default;

PipelineHost::~PipelineHost() {
  ui_.shutdown();
  textures_.shutdown();
  glyph_fonts_.shutdown();
  fonts_.shutdown();
  if (r_)
    SDL_DestroyRenderer(r_);
  if (surf_)
    SDL_DestroySurface(surf_);
}

bool PipelineHost::ensure(int w, int h, const char *font_dir) {
  if (w < 1 || h < 1)
    return false;
  if (surf_ && r_ && pipeline_ && w_ == w && h_ == h)
    return true;

  ui_.shutdown();
  // Chrome + glyph textures are bound to the old renderer; drop them and flag a
  // re-bake (the composition root re-bakes after ensure() returns). TTF glyph
  // textures re-rasterize lazily against the new renderer.
  textures_.shutdown();
  glyph_fonts_.shutdown();
  fonts_.drop_textures();
  chrome_dirty_ = true;
  // Bump the generation so the GPU backend flushes its texture cache (ids reuse).
  ++texture_generation_;
  if (r_) {
    SDL_DestroyRenderer(r_);
    r_ = nullptr;
  }
  if (surf_) {
    SDL_DestroySurface(surf_);
    surf_ = nullptr;
  }

  surf_ = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
  if (!surf_)
    return false;
  r_ = SDL_CreateSoftwareRenderer(surf_);
  if (!r_)
    return false;

  if (!fonts_.loaded()) {
#ifndef SILENCER_HEADLESS
    if (!TTF_WasInit())
      TTF_Init();
#endif
    if (!fonts_.load_faces(font_dir))
      return false;
    install_text_measurer(&fonts_, &glyph_fonts_);
  }
  if (!ui_.initialize(r_, fonts_))
    return false;
  if (!pipeline_)
    pipeline_ = std::make_unique<client::ui::UiPipeline>();

  w_ = w;
  h_ = h;
  packed_.assign((size_t)w * h * 4u, 0);
  prev_valid_ = false; // new surface size: cached frame + prev IR are stale
  const char *dmg_env = getenv("SILENCER_UI_DAMAGE");
  damage_enabled_ = !(dmg_env && dmg_env[0] == '0');
  return true;
}

uint32_t PipelineHost::bake_chrome_sprite(const uint8_t *indices, int w, int h,
                                          const SDL_Color *palette256) {
  if (!r_ || !indices || !palette256 || w < 1 || h < 1)
    return 0;
  bake_scratch_.assign((size_t)w * h * 4u, 0);
  bake_indexed_rgba(indices, w, h, palette256, bake_scratch_.data());
  return textures_.upload_rgba(r_, bake_scratch_.data(), w, h);
}

void PipelineHost::register_legacy(uint32_t texture_id,
                                   const uint8_t *indices, int w, int h,
                                   const SDL_Color *palette256, int legacy_w,
                                   int legacy_h, LegacyFit fit, int cap_l,
                                   int cap_r, int cap_t, int cap_b) {
  textures_.register_legacy(texture_id, indices, w, h, palette256, legacy_w,
                            legacy_h, fit, cap_l, cap_r, cap_t, cap_b);
}

bool PipelineHost::build_glyph_face(int face_id,
                                    const GlyphFonts::GlyphSrc *glyphs,
                                    int count, const SDL_Color *palette256,
                                    float advance, float line_height) {
  if (!r_)
    return false;
  return glyph_fonts_.build_face(r_, face_id, glyphs, count, palette256, advance,
                                 line_height);
}

bool PipelineHost::build_glyph_color_face(int face_id, uint8_t key_r,
                                          uint8_t key_g, uint8_t key_b,
                                          const GlyphFonts::GlyphSrc *glyphs,
                                          int count,
                                          const SDL_Color *palette256,
                                          float advance, float line_height,
                                          uint8_t alpha) {
  if (!r_)
    return false;
  return glyph_fonts_.build_color_face(r_, face_id, key_r, key_g, key_b, glyphs,
                                       count, palette256, advance, line_height,
                                       alpha);
}

namespace {

// Pixel-equality of one command across frames, arenas dereferenced: Text/
// Gradient arms hold arena OFFSETS that shift whenever any earlier string or
// stop run changes size, so equality must compare the referenced bytes, never
// the handles — else one changed label false-damages everything packed after
// it. node_id is an identity tag with no pixel effect and is excluded. The
// other arms memcmp their active member only (the union tail beyond it is
// unspecified bytes); a spurious mismatch there just over-paints.
bool command_pixels_equal(const ::ui::DrawCommand &a, const char *a_text,
                          const ::ui::GradientStop *a_grads,
                          const ::ui::DrawCommand &b, const char *b_text,
                          const ::ui::GradientStop *b_grads) {
  if (a.kind != b.kind)
    return false;
  if (memcmp(&a.rect, &b.rect, sizeof(a.rect)) != 0)
    return false;
  switch (a.kind) {
  case ::ui::DrawCommandKind::Text: {
    const ::ui::TextData &ta = a.payload.text;
    const ::ui::TextData &tb = b.payload.text;
    if (ta.text_len != tb.text_len ||
        memcmp(&ta.color, &tb.color, sizeof(ta.color)) != 0 ||
        ta.font_id != tb.font_id || ta.font_size != tb.font_size ||
        ta.line_index != tb.line_index || ta.align != tb.align ||
        ta.reveal_boost != tb.reveal_boost || ta.reveal_step != tb.reveal_step)
      return false;
    return ta.text_len == 0 ||
           memcmp(a_text + ta.text_off, b_text + tb.text_off, ta.text_len) == 0;
  }
  case ::ui::DrawCommandKind::Gradient: {
    const ::ui::GradientData &ga = a.payload.gradient;
    const ::ui::GradientData &gb = b.payload.gradient;
    if (ga.stop_count != gb.stop_count || ga.angle_deg != gb.angle_deg ||
        ga.corner_radius != gb.corner_radius)
      return false;
    return ga.stop_count == 0 ||
           memcmp(a_grads + ga.stop_off, b_grads + gb.stop_off,
                  (size_t)ga.stop_count * sizeof(::ui::GradientStop)) == 0;
  }
  case ::ui::DrawCommandKind::Rect:
    return memcmp(&a.payload.rect, &b.payload.rect,
                  sizeof(::ui::RectData)) == 0;
  case ::ui::DrawCommandKind::Border:
    return memcmp(&a.payload.border, &b.payload.border,
                  sizeof(::ui::BorderData)) == 0;
  case ::ui::DrawCommandKind::Image:
    return memcmp(&a.payload.image, &b.payload.image,
                  sizeof(::ui::ImageData)) == 0;
  case ::ui::DrawCommandKind::Shadow:
    return memcmp(&a.payload.shadow, &b.payload.shadow,
                  sizeof(::ui::ShadowData)) == 0;
  case ::ui::DrawCommandKind::ClipPush:
  case ::ui::DrawCommandKind::ClipPop:
    return memcmp(&a.payload.clip, &b.payload.clip,
                  sizeof(::ui::ClipData)) == 0;
  case ::ui::DrawCommandKind::LayerPush:
  case ::ui::DrawCommandKind::LayerPop:
    return memcmp(&a.payload.layer, &b.payload.layer,
                  sizeof(::ui::LayerData)) == 0;
  default:
    return true; // None/Custom: no payload, no pixels
  }
}

bool is_bracket(::ui::DrawCommandKind k) {
  return k == ::ui::DrawCommandKind::ClipPush ||
         k == ::ui::DrawCommandKind::ClipPop ||
         k == ::ui::DrawCommandKind::LayerPush ||
         k == ::ui::DrawCommandKind::LayerPop;
}

SDL_Rect union_rect(const SDL_Rect &a, const SDL_Rect &b) {
  const int x0 = a.x < b.x ? a.x : b.x;
  const int y0 = a.y < b.y ? a.y : b.y;
  const int x1 = (a.x + a.w) > (b.x + b.w) ? a.x + a.w : b.x + b.w;
  const int y1 = (a.y + a.h) > (b.y + b.h) ? a.y + a.h : b.y + b.h;
  return {x0, y0, x1 - x0, y1 - y0};
}

// Clamp a conservative device-px bound into the surface and fold it into the
// set: union into the first intersecting rect, else a free slot, else the slot
// whose union grows least. (Merge policy is perf-only — the raster pass is
// correct for overlapping rects — so first-fit is fine.)
void add_damage(UiDamage &d, const DevRect &b, int w, int h) {
  int x0 = (int)floorf(b.x), y0 = (int)floorf(b.y);
  int x1 = (int)ceilf(b.x + b.w), y1 = (int)ceilf(b.y + b.h);
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;
  if (x1 > w)
    x1 = w;
  if (y1 > h)
    y1 = h;
  if (x1 <= x0 || y1 <= y0)
    return;
  const SDL_Rect r = {x0, y0, x1 - x0, y1 - y0};
  for (int i = 0; i < d.count; ++i) {
    if (SDL_HasRectIntersection(&d.rects[i], &r)) {
      d.rects[i] = union_rect(d.rects[i], r);
      return;
    }
  }
  if (d.count < UiDamage::kMaxRects) {
    d.rects[d.count++] = r;
    return;
  }
  int best = 0;
  long best_growth = 0x7fffffffL;
  for (int i = 0; i < d.count; ++i) {
    const SDL_Rect u = union_rect(d.rects[i], r);
    const long growth = (long)u.w * u.h - (long)d.rects[i].w * d.rects[i].h;
    if (growth < best_growth) {
      best_growth = growth;
      best = i;
    }
  }
  d.rects[best] = union_rect(d.rects[best], r);
}

} // namespace

// Diff the new IR against the retained previous frame and build the damage
// set. Full damage when there is no comparable previous frame (first frame,
// resize, scale change), the command count changed (structural reshape — the
// positional diff would mis-pair everything after the first insertion), any
// bracket command changed (a moved clip/layer boundary exposes pixels no draw
// command's bounds own), the kill switch is set, or the damaged area stops
// being worth multiple passes. count==0 && !full => byte-identical frame.
void PipelineHost::diff_damage(const ::ui::DrawCommandList &list, float scale,
                               UiDamage *out) {
  if (!prev_valid_ || prev_scale_ != scale ||
      list.count != (int)prev_cmds_.size()) {
    out->full = true;
    return;
  }
  bool any = false;
  for (int i = 0; i < list.count; ++i) {
    const ::ui::DrawCommand &prev = prev_cmds_[i];
    const ::ui::DrawCommand &next = list.commands[i];
    if (command_pixels_equal(prev, prev_text_.data(), prev_grads_.data(), next,
                             list.text_arena, list.grad_arena))
      continue;
    any = true;
    if (!damage_enabled_ || is_bracket(prev.kind) || is_bracket(next.kind)) {
      out->full = true;
      return;
    }
    // Both the pixels the command used to cover and the ones it covers now.
    add_damage(*out, command_damage_bounds(prev, scale), w_, h_);
    add_damage(*out, command_damage_bounds(next, scale), w_, h_);
  }
  if (!any)
    return; // unchanged
  long area = 0;
  for (int i = 0; i < out->count; ++i)
    area += (long)out->rects[i].w * out->rects[i].h;
  if (out->count == 0 || area > ((long)w_ * h_ * 6) / 10)
    out->full = true;
}

// SILENCER_UI_PROF=1: per-stage frame-cost split on the CPU path, avg printed
// to stderr every 120 frames. build = tree+layout+focus+IR emit; diff = IR
// damage diff; execute = clear+draw+present; pack = surface copies + retain.
// Off = one cached env test per frame.
namespace {
struct UiProfAcc {
  double build = 0, diff = 0, exec = 0, pack = 0;
  int frames = 0;
};
UiProfAcc g_uiprof;
bool uiprof_on() {
  static int en = -1;
  if (en < 0) {
    const char *e = SDL_getenv("SILENCER_UI_PROF");
    en = (e && e[0] == '1') ? 1 : 0;
  }
  return en == 1;
}
} // namespace

const uint8_t *PipelineHost::render(const client::ui::UiPipelineFrame &frame,
                                    int *out_w, int *out_h, bool *out_unchanged,
                                    UiDamage *out_damage) {
  if (out_unchanged)
    *out_unchanged = false;
  if (out_damage)
    *out_damage = {};
  if (!surf_ || !r_ || !pipeline_)
    return nullptr;

  const bool prof = uiprof_on();
  const uint64_t p0 = prof ? SDL_GetTicksNS() : 0;
  uint64_t cb_in = 0, p_diff = 0, p_exec = 0, p_end = 0;

  // Responsive content scale: surface height / logical-layout height (the ratio
  // the composition root set in frame.layout). May be < 1 on small windows —
  // keep it in lockstep with the composition root's cppxScale, never re-clamp.
  float device_scale = (frame.layout.height > 0.0f)
                           ? static_cast<float>(h_) / frame.layout.height
                           : 1.0f;
  // Tree/layout/focus/IR build always run (they advance animation/interaction
  // state); only the raster is conditional, decided after the IR is built.
  bool unchanged = false;
  pipeline_->render_client_ui_frame(frame, [&] {
    if (prof)
      cb_in = SDL_GetTicksNS();
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();

    UiDamage dmg;
    diff_damage(list, device_scale, &dmg);
    if (prof)
      p_diff = SDL_GetTicksNS();
    if (!dmg.full && dmg.count == 0) {
      unchanged = true;
      p_exec = p_end = p_diff;
      return;
    }

    PERF_SCOPE("ui.raster");
    const int pitch = surf_->pitch;
    const uint8_t *src = (const uint8_t *)surf_->pixels;
    if (dmg.full) {
      // Transparent clear: the UI composites over the already-rendered world.
      const float scale =
          ui_.begin_frame(::ui::Color{0, 0, 0, 0}, device_scale, 1);
      execute_draw_commands(r_, list, &fonts_, &textures_, scale, {},
                            &glyph_fonts_);
      ui_.resolve_frame();
      ui_.present();
      if (prof)
        p_exec = SDL_GetTicksNS();
      // Copy to a tightly-packed buffer (the surface pitch may be padded).
      for (int y = 0; y < h_; ++y)
        memcpy(&packed_[(size_t)y * w_ * 4u], src + (size_t)y * pitch,
               (size_t)w_ * 4u);
    } else {
      // Region repaint: overwrite-clear each damage rect, then re-execute the
      // full list clipped to it — painter order inside the rect reproduces the
      // full-repaint pixels exactly. Clear-then-execute stays sequential per
      // rect so an overlapping later rect rebuilds the overlap from its own
      // clear instead of double-blending the earlier pass's pixels.
      for (int i = 0; i < dmg.count; ++i) {
        const SDL_Rect &dr = dmg.rects[i];
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r_, 0, 0, 0, 0);
        const SDL_FRect fr = {(float)dr.x, (float)dr.y, (float)dr.w,
                              (float)dr.h};
        SDL_RenderFillRect(r_, &fr);
        RasterConfig rc;
        rc.damage = &dr;
        execute_draw_commands(r_, list, &fonts_, &textures_, device_scale, rc,
                              &glyph_fonts_);
      }
      ui_.resolve_frame();
      ui_.present();
      if (prof)
        p_exec = SDL_GetTicksNS();
      for (int i = 0; i < dmg.count; ++i) {
        const SDL_Rect &dr = dmg.rects[i];
        for (int y = dr.y; y < dr.y + dr.h; ++y)
          memcpy(&packed_[((size_t)y * w_ + dr.x) * 4u],
                 src + (size_t)y * pitch + (size_t)dr.x * 4u,
                 (size_t)dr.w * 4u);
      }
    }

    prev_cmds_.assign(list.commands, list.commands + list.count);
    prev_text_.assign(list.text_arena, list.text_arena + list.text_len_used);
    prev_grads_.assign(list.grad_arena, list.grad_arena + list.grad_count);
    prev_scale_ = device_scale;
    prev_valid_ = true;

    if (out_damage) {
      out_damage->full = dmg.full;
      if (dmg.full) {
        out_damage->count = 1;
        out_damage->rects[0] = {0, 0, w_, h_};
      } else {
        out_damage->count = dmg.count;
        for (int i = 0; i < dmg.count; ++i)
          out_damage->rects[i] = dmg.rects[i];
      }
    }
    if (prof)
      p_end = SDL_GetTicksNS();
  });

  if (prof && cb_in) {
    const uint64_t p1 = SDL_GetTicksNS();
    g_uiprof.build += ((p1 - p0) - (double)(p_end - cb_in)) / 1e6;
    g_uiprof.diff += (p_diff - cb_in) / 1e6;
    g_uiprof.exec += (p_exec - p_diff) / 1e6;
    g_uiprof.pack += (p_end - p_exec) / 1e6;
    if (++g_uiprof.frames == 120) {
      fprintf(stderr,
              "[ui-prof] avg ms over 120 frames: build+layout+ir=%.2f "
              "diff=%.2f execute=%.2f pack=%.2f\n",
              g_uiprof.build / 120, g_uiprof.diff / 120, g_uiprof.exec / 120,
              g_uiprof.pack / 120);
      g_uiprof = {};
    }
  }

  if (out_unchanged)
    *out_unchanged = unchanged;
  if (out_w)
    *out_w = w_;
  if (out_h)
    *out_h = h_;
  return packed_.data();
}

const GpuUiProgram *
PipelineHost::build_gpu_frame(const client::ui::UiPipelineFrame &frame) {
  if (!surf_ || !r_ || !pipeline_)
    return nullptr;

  // Same content scale render() derives.
  const float device_scale = (frame.layout.height > 0.0f)
                                 ? static_cast<float>(h_) / frame.layout.height
                                 : 1.0f;

  pipeline_->render_client_ui_frame(frame, [&] {
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();
    PERF_SCOPE("ui.gpu_build");
    build_ui_draw_program(list, &fonts_, &textures_, &glyph_fonts_, r_,
                          device_scale, w_, h_, texture_generation_,
                          gpu_program_);
  });
  return &gpu_program_;
}

} // namespace silencer::cppx_ui
