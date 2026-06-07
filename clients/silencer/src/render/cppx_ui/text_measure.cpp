#include "text_measure.h"

#include "font_registry.h"
#include "glyph_fonts.h"
#include "ui/style/text_measure.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <string.h>

// Measure/wrap/align logic adapted from the golden renderer/text_measure_impl;
// the Silencer changes are multi-face selection AND the bitmap glyph-font path
// (when a face has a baked atlas, measure uses its MONOSPACE metrics so measure
// == the executor's glyph paint — same advance, same line box).

namespace silencer::cppx_ui {

namespace {

// The registries the measurer reads. Set once by install_text_measurer; the
// measurer is a free function (the seam is a plain function pointer). The app
// owns these for the whole process, so they never dangle.
FontRegistry *g_fonts = nullptr;
GlyphFonts *g_glyphs = nullptr;

// One query's resolved metrics: bitmap-glyph (monospace) when the face has an
// atlas, else TTF. `adv` is per-character advance (points); `line_h` is the line
// box height (points). For TTF, `adv` is unused (width comes from TTF_GetStringSize).
struct RunMetrics {
  bool glyph = false;
  const GlyphFonts::Face *gf = nullptr; // glyph mode
  float adv = 0.0f;                     // glyph mode: per-char advance (points)
  TTF_Font *font = nullptr;             // TTF mode
  float line_h = 16.0f;
};

RunMetrics metrics_for(const ::ui::TextMetricsQuery &q) {
  RunMetrics m;
  const GlyphFonts::Face *gf = g_glyphs ? g_glyphs->face(q.font_id) : nullptr;
  if (gf && gf->line_height > 0.0f && q.font_size > 0) {
    // font_size is the target cell height (points). Scale native bank metrics by
    // font_size/line_height (same mapping the executor uses) so measure==paint.
    const float gscale = static_cast<float>(q.font_size) / gf->line_height;
    m.glyph = true;
    m.gf = gf;
    m.adv = gf->advance * gscale;
    // The cell height IS font_size; ignore q.line_height so a small legacy
    // line-height token can't clip the (now larger) glyph cell.
    m.line_h = static_cast<float>(q.font_size);
    return m;
  }
  // TTF: face for q.font_id, sized per-query (matches the TTF paint path).
  if (g_fonts) {
    m.font = g_fonts->face(q.font_id);
    if (m.font && q.font_size > 0)
      TTF_SetFontSize(m.font, static_cast<float>(q.font_size));
  }
  if (q.line_height > 0.0f) {
    m.line_h = q.line_height;
  } else if (m.font) {
    int skip = TTF_GetFontLineSkip(m.font);
    m.line_h = skip > 0 ? static_cast<float>(skip) : 16.0f;
  }
  return m;
}

float advance_of(const RunMetrics &m, const char *utf8, uint32_t len) {
  if (len == 0)
    return 0.0f;
  if (m.glyph)
    return static_cast<float>(len) * m.adv; // monospace: spaces advance too
  if (!m.font)
    return 0.0f;
  int w = 0, h = 0;
  if (!TTF_GetStringSize(m.font, utf8, len, &w, &h))
    return 0.0f;
  return static_cast<float>(w);
}

float aligned_x(::ui::TextAlign align, float line_w, float box_w) {
  if (box_w <= 0.0f)
    return 0.0f;
  switch (align) {
  case ::ui::TextAlign::Center:
    return (box_w - line_w) * 0.5f;
  case ::ui::TextAlign::Right:
    return box_w - line_w;
  case ::ui::TextAlign::Left:
  default:
    return 0.0f;
  }
}

bool push_line(::ui::TextMetricsResult &out, uint32_t slice_off,
               uint32_t slice_len, float x, float y, float w, float h) {
  if (out.line_count >= ::ui::UI_MAX_TEXT_LINES)
    return false;
  ::ui::LineRun &run = out.lines[out.line_count++];
  run.slice_offset = slice_off;
  run.slice_len = slice_len;
  run.x = x;
  run.y = y;
  run.w = w;
  run.h = h;
  if (w > out.width)
    out.width = w;
  return true;
}

::ui::TextMetricsResult measure_wrapped(const RunMetrics &m,
                                        const ::ui::TextMetricsQuery &q,
                                        float line_h, float box_w) {
  ::ui::TextMetricsResult out = {};
  const char *s = q.utf8;
  const uint32_t n = q.len;
  uint32_t line_start = 0;
  float y = 0.0f;

  uint32_t i = 0;
  while (line_start < n) {
    uint32_t last_fit_end = line_start;
    uint32_t scan = line_start;
    uint32_t next_line_start = n;
    bool placed = false;
    while (scan <= n) {
      uint32_t word_start = scan;
      while (word_start < n && s[word_start] == ' ')
        ++word_start;
      uint32_t word_end = word_start;
      while (word_end < n && s[word_end] != ' ')
        ++word_end;
      if (word_end == word_start) {
        break;
      }
      float w = advance_of(m, s + line_start, word_end - line_start);
      if (w <= box_w || !placed) {
        last_fit_end = word_end;
        placed = true;
        scan = word_end;
        if (word_end >= n) {
          next_line_start = n;
          break;
        }
      } else {
        next_line_start = last_fit_end;
        while (next_line_start < n && s[next_line_start] == ' ')
          ++next_line_start;
        break;
      }
      if (scan >= n) {
        next_line_start = n;
        break;
      }
    }
    if (!placed) {
      break;
    }
    uint32_t slice_len = last_fit_end - line_start;
    float line_w = advance_of(m, s + line_start, slice_len);
    float x = aligned_x(q.align, line_w, box_w);
    if (!push_line(out, line_start, slice_len, x, y, line_w, line_h)) {
      out.overflowed = true;
      break;
    }
    y += line_h;
    line_start = next_line_start;
    ++i;
    if (i > n)
      break;
  }
  out.height = static_cast<float>(out.line_count) * line_h;
  return out;
}

::ui::TextMetricsResult measure(const ::ui::TextMetricsQuery &q) {
  ::ui::TextMetricsResult out = {};
  RunMetrics m = metrics_for(q);
  const char *utf8 = q.utf8 ? q.utf8 : "";
  uint32_t len = q.len;
  if (!m.glyph && !m.font) {
    float h = q.line_height > 0.0f ? q.line_height : 16.0f;
    push_line(out, 0, len, 0.0f, 0.0f, 0.0f, h);
    out.height = h;
    return out;
  }
  float line_h = m.line_h;
  float box_w = q.wrap_width;

  if (q.wrap == ::ui::TextWrap::Words && box_w > 0.0f && len > 0) {
    return measure_wrapped(m, q, line_h, box_w);
  }

  float w = advance_of(m, utf8, len);
  float x = aligned_x(q.align, w, box_w);
  push_line(out, 0, len, x, 0.0f, w, line_h);
  out.height = line_h;
  return out;
}

} // namespace

void install_text_measurer(FontRegistry *fonts, GlyphFonts *glyphs) {
  g_fonts = fonts;
  g_glyphs = glyphs;
  ::ui::set_text_measurer((fonts || glyphs) ? &measure : nullptr);
}

} // namespace silencer::cppx_ui
