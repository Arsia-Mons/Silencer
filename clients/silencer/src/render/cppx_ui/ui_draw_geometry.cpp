#include "ui_draw_geometry.h"

#include <algorithm>
#include <cmath>

namespace silencer::cppx_ui {

bool legacy_stroke(::ui::Color col) {
  return col.a == 255 && ((col.r == 8 && col.g == 84 && col.b == 0) ||
                          (col.r == 24 && col.g == 124 && col.b == 20));
}

float legacy_virtual_scale(float scale) {
  const float sv = scale * 1.5f;
  const float q = sv * 4.f;
  if (std::fabs(q - std::floor(q + 0.5f)) > 0.001f || sv < 1.f)
    return 0.f;
  return sv;
}

bool legacy_hairline_bands(const ::ui::BorderData &b, ::ui::DrawRect rect,
                           float scale, LegacyBand out[4], int *n) {
  *n = 0;
  if (b.corner_radius > 0.01f)
    return false;
  if (b.outline.width > 0.f && b.outline.color.a > 0)
    return false;
  if (b.has_fill && b.fill.a > 0)
    return false; // fused fill takes the mesh path
  const float sv = legacy_virtual_scale(scale);
  if (sv <= 0.f)
    return false;
  const float kMaxHairline = 2.5f; // logical; anything wider isn't a hairline
  const bool top = b.border.width.top > 0.f && b.border.color.top.a > 0;
  const bool right = b.border.width.right > 0.f && b.border.color.right.a > 0;
  const bool bottom = b.border.width.bottom > 0.f && b.border.color.bottom.a > 0;
  const bool left = b.border.width.left > 0.f && b.border.color.left.a > 0;
  if (!(top || right || bottom || left))
    return false;
  if ((top && (b.border.width.top > kMaxHairline ||
               !legacy_stroke(b.border.color.top))) ||
      (right && (b.border.width.right > kMaxHairline ||
                 !legacy_stroke(b.border.color.right))) ||
      (bottom && (b.border.width.bottom > kMaxHairline ||
                  !legacy_stroke(b.border.color.bottom))) ||
      (left && (b.border.width.left > kMaxHairline ||
                !legacy_stroke(b.border.color.left))))
    return false;

  const float dx0 = rect.x * scale, dy0 = rect.y * scale;
  const float dx1 = (rect.x + rect.w) * scale;
  const float dy1 = (rect.y + rect.h) * scale;
  auto cell_lo = [&](float dev) { return (long)std::lround(dev / sv); };
  auto cell_hi = [&](float dev) { return (long)std::lround(dev / sv) - 1; };
  auto span_of = [&](long v, float *lo, float *hi) {
    *lo = std::ceil((float)v * sv);
    *hi = std::ceil((float)(v + 1) * sv);
  };
  float lx0, lx1, rx0, rx1, ty0, ty1, by0, by1;
  span_of(cell_lo(dx0), &lx0, &lx1);
  span_of(cell_hi(dx1), &rx0, &rx1);
  span_of(cell_lo(dy0), &ty0, &ty1);
  span_of(cell_hi(dy1), &by0, &by1);

  int count = 0;
  auto add = [&](float x0, float y0, float x1, float y1, ::ui::Color col) {
    if (x1 <= x0 || y1 <= y0)
      return;
    out[count].rect = {x0, y0, x1 - x0, y1 - y0};
    out[count].color = col;
    ++count;
  };
  // Bands span the full outer extent (corners overlap; same opaque ink). Order
  // matches the executor's fill order (top, bottom, left, right).
  if (top)
    add(lx0, ty0, rx1, ty1, b.border.color.top);
  if (bottom)
    add(lx0, by0, rx1, by1, b.border.color.bottom);
  if (left)
    add(lx0, ty0, lx1, by1, b.border.color.left);
  if (right)
    add(rx0, ty0, rx1, by1, b.border.color.right);
  *n = count;
  return true;
}

bool legacy_solid_fill_rect(const ::ui::RectData &rd, ::ui::DrawRect rect,
                            float scale, DevRect *out) {
  if (rd.corner_radius > 0.01f)
    return false;
  if (!legacy_stroke(rd.fill))
    return false;
  const float sv = legacy_virtual_scale(scale);
  if (sv <= 0.f)
    return false;
  // Edge -> nearest virtual line, line -> first device px of its cell
  // (cell(v) = [ceil(v*sv), ceil((v+1)*sv)), as in the border snap).
  auto snap = [&](float logical) {
    return std::ceil((float)std::lround(logical * scale / sv) * sv);
  };
  const float x0 = snap(rect.x), y0 = snap(rect.y);
  const float x1 = snap(rect.x + rect.w), y1 = snap(rect.y + rect.h);
  if (x1 <= x0 || y1 <= y0)
    return false;
  *out = {x0, y0, x1 - x0, y1 - y0};
  return true;
}

TextLayout text_layout(float advance, float line_height, int atlas_h,
                       float font_size, float scale, ::ui::DrawRect rect) {
  TextLayout L;
  if (line_height <= 0.f || font_size == 0.f)
    return L; // ok=false
  // font_size is the target device CELL height in points; scale maps points ->
  // device. glyph scale maps native bank px -> device px.
  L.gscale = (font_size * scale) / line_height;
  L.adv = advance * L.gscale;
  L.ah = static_cast<int>(static_cast<float>(atlas_h) * L.gscale + 0.5f);
  // floor (not round) quantization: matches the legacy SW renderer's dst-rect
  // floor, so 1:1-scale text (in-game 640x480) lands on its exact pixels.
  L.penx = rect.x * scale;
  L.peny = static_cast<int>(std::floor(rect.y * scale));
  L.ok = true;
  return L;
}

DevRect glyph_dst(const TextLayout &L, int i, int gw) {
  return {std::floor(L.penx + L.adv * static_cast<float>(i)),
          static_cast<float>(L.peny),
          static_cast<float>(static_cast<int>(gw * L.gscale + 0.5f)),
          static_cast<float>(L.ah)};
}

int image_nine_cells(::ui::DrawRect rect, float scale, float tw, float th,
                     ::ui::SideWidths ns, ImageCell out[9]) {
  // Source insets in texture space, dest insets in dest space. Corners 1:1
  // (source inset == dest inset); edges/center stretch.
  const float sl = ns.left, sr = ns.right, st = ns.top, sb = ns.bottom;
  const float dx0 = rect.x, dy0 = rect.y;
  const float dx1 = rect.x + rect.w, dy1 = rect.y + rect.h;
  // Source edges are texture-space (unscaled); dest edges are points -> device.
  const float sx[4] = {0.f, sl, tw - sr, tw};
  const float sy[4] = {0.f, st, th - sb, th};
  const float dx[4] = {dx0 * scale, (dx0 + sl) * scale, (dx1 - sr) * scale,
                       dx1 * scale};
  const float dy[4] = {dy0 * scale, (dy0 + st) * scale, (dy1 - sb) * scale,
                       dy1 * scale};
  int n = 0;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      DevRect src = {sx[col], sy[row], sx[col + 1] - sx[col],
                     sy[row + 1] - sy[row]};
      DevRect dst = {dx[col], dy[row], dx[col + 1] - dx[col],
                     dy[row + 1] - dy[row]};
      if (src.w <= 0.f || src.h <= 0.f || dst.w <= 0.f || dst.h <= 0.f)
        continue;
      out[n].src = src;
      out[n].dst = dst;
      ++n;
    }
  }
  return n;
}

void image_plain_rects(::ui::DrawRect rect, float scale, float tw, float th,
                       bool has_src, ::ui::DrawRect src_sub, ::ui::ImageFit fit,
                       DevRect *src_out, DevRect *dst_out) {
  DevRect src = has_src ? DevRect{src_sub.x, src_sub.y, src_sub.w, src_sub.h}
                        : DevRect{0.f, 0.f, tw, th};
  DevRect dst = {rect.x * scale, rect.y * scale, rect.w * scale,
                 rect.h * scale};
  // Cover crops the effective source to the dest aspect (centered); Contain
  // letterboxes the dest to the source aspect — origin PackImage cover/contain.
  if (fit == ::ui::ImageFit::Cover && rect.w > 0.f && rect.h > 0.f) {
    const float box_aspect = rect.w / rect.h;
    if (src.w / src.h > box_aspect) {
      const float crop_w = src.h * box_aspect;
      src.x += (src.w - crop_w) * 0.5f;
      src.w = crop_w;
    } else {
      const float crop_h = src.w / box_aspect;
      src.y += (src.h - crop_h) * 0.5f;
      src.h = crop_h;
    }
  } else if (fit == ::ui::ImageFit::Contain && src.w > 0.f && src.h > 0.f) {
    const float fit_scale = std::min(dst.w / src.w, dst.h / src.h);
    const float fit_w = src.w * fit_scale, fit_h = src.h * fit_scale;
    dst.x += (dst.w - fit_w) * 0.5f;
    dst.y += (dst.h - fit_h) * 0.5f;
    dst.w = fit_w;
    dst.h = fit_h;
  }
  // Native 1:1 sprite draw (in-game HUD at 640x480): authored coords land
  // within 1/3 device px of the integer cell — snap so the fractional rect
  // can't bleed an extra row/column (stipple art is parity-sensitive).
  if (std::fabs(dst.w - src.w) < 0.5f && std::fabs(dst.h - src.h) < 0.5f) {
    dst.x = std::floor(dst.x + 0.5f);
    dst.y = std::floor(dst.y + 0.5f);
    dst.w = src.w;
    dst.h = src.h;
  }
  *src_out = src;
  *dst_out = dst;
}

} // namespace silencer::cppx_ui
