#include "text_measure.h"

#include "font_registry.h"
#include "glyph_fonts.h"
#include "ui/style/text_measure.h"

#ifndef SILENCER_HEADLESS
#include <SDL3_ttf/SDL_ttf.h>
#endif

#include <string.h>

// When a face has a baked glyph atlas, measure uses its monospace metrics so
// measure == the executor's glyph paint; otherwise TTF.

namespace silencer::cppx_ui {

namespace {

// The registries the measurer reads. Set once by install_text_measurer; the app
// owns them for the process lifetime, so they never dangle.
FontRegistry *g_fonts = nullptr;
GlyphFonts *g_glyphs = nullptr;

// One query's resolved metrics: bitmap-glyph (monospace) when the face has an
// atlas, else TTF. For TTF, `adv` is unused (width via TTF_GetStringSize on
// the fixed-size face — the same layout engine FontRegistry::draw_text blits
// from, so measure == paint).
struct RunMetrics {
  bool glyph = false;
  const GlyphFonts::Face *gf = nullptr; // glyph mode
  float adv = 0.0f;                     // glyph mode: per-char advance (points)
  TTF_Font *font = nullptr;             // TTF mode: (face, size) instance
  float line_h = 16.0f;
  float ascent = 0.0f; // cap-top..baseline, points
};

RunMetrics metrics_for(const ::ui::TextMetricsQuery &q) {
  RunMetrics m;
  const GlyphFonts::Face *gf = g_glyphs ? g_glyphs->face(q.font_id) : nullptr;
  if (gf && gf->line_height > 0.0f && q.font_size > 0) {
    // Scale native bank metrics by font_size/line_height (the executor's mapping)
    // so measure == paint.
    const float gscale = static_cast<float>(q.font_size) / gf->line_height;
    m.glyph = true;
    m.gf = gf;
    m.adv = gf->advance * gscale;
    // Cell height IS font_size; ignore q.line_height so a small line-height
    // token can't clip the larger glyph cell.
    m.line_h = static_cast<float>(q.font_size);
    if (gf->ascent > 0)
      m.ascent = static_cast<float>(gf->ascent) * gscale;
    return m;
  }
  // TTF: the (face, size) font instance — fixed-size, never resized (resizing
  // flushes SDL_ttf's glyph cache), matching the paint path.
#ifndef SILENCER_HEADLESS
  if (g_fonts && q.font_size > 0)
    m.font = g_fonts->sized_face(q.font_id, q.font_size);
  if (q.line_height > 0.0f) {
    m.line_h = q.line_height;
  } else if (m.font) {
    int skip = TTF_GetFontLineSkip(m.font);
    m.line_h = skip > 0 ? static_cast<float>(skip) : 16.0f;
  }
  if (m.font) {
    int asc = TTF_GetFontAscent(m.font);
    if (asc > 0)
      m.ascent = static_cast<float>(asc);
  }
#else
  if (q.line_height > 0.0f)
    m.line_h = q.line_height;
#endif
  return m;
}

float advance_of(const RunMetrics &m, const char *utf8, uint32_t len) {
  if (len == 0)
    return 0.0f;
  if (m.glyph)
    return static_cast<float>(len) * m.adv; // monospace; spaces advance too
  if (!m.font)
    return 0.0f;
#ifndef SILENCER_HEADLESS
  int w = 0, h = 0;
  if (!TTF_GetStringSize(m.font, utf8, len, &w, &h))
    return 0.0f;
  return static_cast<float>(w);
#else
  return 0.0f;
#endif
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

bool is_utf8_cont(char c) {
  return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// Largest UTF-8-boundary prefix [from, from+k) of s whose advance fits box_w,
// to hard-break an unspaced token. Always returns >= one whole char so wrapping
// makes progress.
uint32_t char_fit(const RunMetrics &m, const char *s, uint32_t from,
                  uint32_t end, float box_w) {
  uint32_t k = 1;
  while (from + k < end && is_utf8_cont(s[from + k]))
    ++k;
  while (from + k < end) {
    uint32_t next = k + 1;
    while (from + next < end && is_utf8_cont(s[from + next]))
      ++next;
    if (advance_of(m, s + from, next) > box_w)
      break;
    k = next;
  }
  return k;
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

// Word-wrap a single paragraph (no embedded '\n') of s[start, end) at box_w.
// Returns false (and sets out.overflowed) on the line cap. An empty paragraph
// emits one empty line so a bare '\n' keeps its vertical slot.
bool wrap_paragraph(::ui::TextMetricsResult &out, const RunMetrics &m,
                    const ::ui::TextMetricsQuery &q, float line_h, float box_w,
                    const char *s, uint32_t start, uint32_t end, float &y) {
  uint32_t line_start = start;
  if (line_start >= end) {
    if (!push_line(out, line_start, 0, aligned_x(q.align, 0.0f, box_w), y, 0.0f,
                   line_h)) {
      out.overflowed = true;
      return false;
    }
    y += line_h;
    return true;
  }
  uint32_t guard = 0;
  while (line_start < end) {
    uint32_t last_fit_end = line_start;
    uint32_t scan = line_start;
    uint32_t next_line_start = end;
    bool placed = false;
    while (scan <= end) {
      uint32_t word_start = scan;
      while (word_start < end && s[word_start] == ' ')
        ++word_start;
      uint32_t word_end = word_start;
      while (word_end < end && s[word_end] != ' ')
        ++word_end;
      if (word_end == word_start) {
        break;
      }
      float w = advance_of(m, s + line_start, word_end - line_start);
      if (w <= box_w) {
        last_fit_end = word_end;
        placed = true;
        scan = word_end;
        if (word_end >= end) {
          next_line_start = end;
          break;
        }
      } else {
        // The line-so-far plus this word overflows. Char-break the word when it
        // can't fit the column alone or nothing's been placed yet (char_fit
        // measures from line_start, so placed words stay); else wrap the whole
        // word to the next line (greedy).
        float word_w = advance_of(m, s + word_start, word_end - word_start);
        if (word_w > box_w || !placed) {
          uint32_t fit = char_fit(m, s, line_start, end, box_w);
          if (line_start + fit > last_fit_end)
            last_fit_end = line_start + fit;
          placed = true;
        }
        next_line_start = last_fit_end;
        while (next_line_start < end && s[next_line_start] == ' ')
          ++next_line_start;
        break;
      }
      if (scan >= end) {
        next_line_start = end;
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
      return false;
    }
    y += line_h;
    line_start = next_line_start;
    if (++guard > end - start)
      break;
  }
  return true;
}

::ui::TextMetricsResult measure_wrapped(const RunMetrics &m,
                                        const ::ui::TextMetricsQuery &q,
                                        float line_h, float box_w) {
  // '\n' is a hard break; soft-wrap each paragraph by width.
  ::ui::TextMetricsResult out = {};
  const char *s = q.utf8;
  const uint32_t n = q.len;
  float y = 0.0f;
  uint32_t para_start = 0;
  while (para_start <= n) {
    uint32_t para_end = para_start;
    while (para_end < n && s[para_end] != '\n')
      ++para_end;
    if (!wrap_paragraph(out, m, q, line_h, box_w, s, para_start, para_end, y))
      break;
    if (para_end >= n)
      break;
    para_start = para_end + 1;
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
    ::ui::TextMetricsResult wrapped = measure_wrapped(m, q, line_h, box_w);
    wrapped.ascent = m.ascent;
    return wrapped;
  }

  float w = advance_of(m, utf8, len);
  float x = aligned_x(q.align, w, box_w);
  push_line(out, 0, len, x, 0.0f, w, line_h);
  out.height = line_h;
  out.ascent = m.ascent;
  return out;
}

} // namespace

void install_text_measurer(FontRegistry *fonts, GlyphFonts *glyphs) {
  g_fonts = fonts;
  g_glyphs = glyphs;
  ::ui::set_text_measurer((fonts || glyphs) ? &measure : nullptr);
}

} // namespace silencer::cppx_ui
