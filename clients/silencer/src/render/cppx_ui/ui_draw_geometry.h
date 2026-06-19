#pragma once

// ui_draw_geometry.h — the parity-critical snap/cell/image rect+UV math, shared
// by the CPU software executor (draw_executor.cpp) and the GPU geometry emitter
// (ui_draw_program.cpp) so both produce byte-identical device rects/UVs (SIL-240).
//
// The origin-parity behaviors (legacy hairline-border grid snap, legacy
// solid-fill snap, canonical integer glyph cells, image nine-slice / plain /
// cover-contain-native-snap) are the most parity-sensitive code in the project.
// Extracting them here means there is ONE implementation, not two that can drift.
//
// SDL-free: pure math over the ::ui IR types. Device-space rects are in physical
// pixels (UI points * scale); texture sub-rects are in texture pixels.

#include "ui/runtime/draw_command.h"

namespace silencer::cppx_ui {

// A rectangle in DEVICE pixels (or, for image src sub-rects, texture pixels).
struct DevRect {
  float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
};

// ---- legacy virtual-grid eligibility (origin's 853x480 virtual canvas) ------

// The two legacy chrome stroke/fill colors (palette idx216 connected frame,
// idx220 inner well) — the eligibility palette for the legacy grid snaps.
bool legacy_stroke(::ui::Color col);

// Quarter-integer virtual scale (device px per legacy virtual px), or 0 if this
// scale has no legacy grid. Shared gate of the legacy snaps.
float legacy_virtual_scale(float scale);

// ---- legacy hairline-border grid snap (draw_executor snap_legacy_hairline) --

struct LegacyBand {
  DevRect rect;       // device px
  ::ui::Color color;  // premultiplied
};

// True if `b` is an eligible legacy hairline border (the tessellated mesh path
// must be suppressed). Writes the up-to-4 snapped band rects to out[] and their
// count to *n (may be 0 even when eligible, if every band is degenerate).
bool legacy_hairline_bands(const ::ui::BorderData &b, ::ui::DrawRect rect,
                           float scale, LegacyBand out[4], int *n);

// ---- legacy solid-fill grid snap (draw_executor snap_legacy_solid_fill) -----

// True if `rd` is an eligible legacy solid fill; writes the snapped device rect
// to *out. False (use the mesh path) when ineligible or the snap is degenerate.
bool legacy_solid_fill_rect(const ::ui::RectData &rd, ::ui::DrawRect rect,
                            float scale, DevRect *out);

// ---- canonical integer glyph cells (draw_executor render_text_glyphs) -------

struct TextLayout {
  bool ok = false;
  float gscale = 0.f; // native bank px -> device px
  float adv = 0.f;    // device-px pen step (fractional design advance)
  int ah = 0;         // device-px glyph cell height (atlas_h * gscale, rounded)
  float penx = 0.f;   // device-px pen origin x (fractional)
  int peny = 0;       // device-px pen origin y (floor-quantized)
};

// Per-string layout from the face metrics + this command's rect. Mirrors
// render_text_glyphs exactly (font_size is the device CELL height in points).
TextLayout text_layout(float advance, float line_height, int atlas_h,
                       float font_size, float scale, ::ui::DrawRect rect);

// Device-px destination rect for the i-th glyph in the string (native width gw).
DevRect glyph_dst(const TextLayout &L, int i, int gw);

// ---- image rects (draw_executor render_image) -------------------------------

struct ImageCell {
  DevRect src; // texture px
  DevRect dst; // device px
};

// Nine-slice 3x3 grid. `tw`/`th` are the texture size; `ns` the texture-space
// insets. Writes up to 9 non-degenerate cells to out[]; returns the count.
int image_nine_cells(::ui::DrawRect rect, float scale, float tw, float th,
                     ::ui::SideWidths ns, ImageCell out[9]);

// Plain image: final source sub-rect (texture px) + destination (device px)
// after Cover/Contain aspect fit and the native-1:1 snap. `src_sub` is honored
// only when has_src.
void image_plain_rects(::ui::DrawRect rect, float scale, float tw, float th,
                       bool has_src, ::ui::DrawRect src_sub, ::ui::ImageFit fit,
                       DevRect *src_out, DevRect *dst_out);

} // namespace silencer::cppx_ui
