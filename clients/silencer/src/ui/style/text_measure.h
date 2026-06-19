#pragma once

// The SDL-free text-measurement seam. ONE MeasureTextFn is installed once at
// startup and used as BOTH the Yoga MeasureFn and the draw-time per-line emitter,
// so measure == draw by construction. ui/ never touches SDL_ttf. See design §10.

#include "visual_style.h" // TextAlign, TextWrap, LineRun

#include <stdint.h>

#include <type_traits>

namespace ui {

constexpr int UI_MAX_TEXT_LINES = 32; // sized for the lore paragraph (~23 wrapped lines); content beyond this truncates to what fits (graceful overflow, INV2)

struct TextMetricsQuery {
  const char *utf8 = nullptr; // borrowed; points into the shared string arena
  uint32_t len = 0;           // byte length
  uint16_t font_id = 0;
  float font_size = 0.f;
  TextAlign align = TextAlign::Left;
  TextWrap wrap = TextWrap::None;
  float line_height = 0.f; // 0 => face natural line skip
  float wrap_width = 0.f;  // <=0 or wrap==None => single line / no wrap
};

struct TextMetricsResult {
  float width = 0.f;   // max line w (the node's content width)
  float height = 0.f;  // sum of line h
  float ascent = 0.f;  // cap-top..baseline, in points; 0 => unknown. Drives the
                       // input caret height (glyph ink, not the full cell).
  uint8_t line_count = 0; // <= UI_MAX_TEXT_LINES
  LineRun lines[UI_MAX_TEXT_LINES] = {};
  bool overflowed = false; // wrapped output needed > UI_MAX_TEXT_LINES lines
};

static_assert(std::is_trivially_copyable_v<TextMetricsResult>);

using MeasureTextFn = TextMetricsResult (*)(const TextMetricsQuery &);

void set_text_measurer(MeasureTextFn fn); // install once at startup
MeasureTextFn text_measurer();            // null until installed

} // namespace ui
