#include "draw_command_builder.h"

#include "../style/text_measure.h"

#include <string.h>

namespace ui {

namespace {

// Role/intrinsic default paint for nodes whose resolved VisualStyle leaves a
// slot unset (bare text color; an input's selection/caret).
constexpr Color kTransparent = {0, 0, 0, 0};
constexpr Color kButtonFill = {6, 16, 8, 255};       // near-black green glass
constexpr Color kButtonDisabledFill = {6, 14, 8, 255};
constexpr Color kButtonBorder = {46, 125, 69, 255};  // #2E7D45 green-dim
constexpr Color kButtonDisabledBorder = {30, 74, 44, 255};
constexpr Color kFocusBorder = {92, 208, 92, 255};   // #5CD05C (matches theme focus_ring)
constexpr float kFocusBorderWidth = 2.0f;
constexpr float kFocusBorderOffset = 2.0f;
constexpr Color kCheckedFill = {34, 192, 76, 255};   // #22C04C toggle-on green
constexpr Color kInputFill = {18, 22, 28, 255};
constexpr Color kSelectionFill = {72, 116, 164, 180};
constexpr Color kCaretFill = {232, 240, 248, 255};
constexpr float kCaretWidth = 1.0f;
constexpr Color kTextFill = {92, 208, 92, 255};      // #5CD05C green (bare-text fallback)
constexpr Color kTextDisabledFill = {46, 90, 55, 255};

bool has_color(Color color) { return color.a > 0; }

// Straight-alpha -> premultiplied (the IR's storage convention).
Color premul(Color c) {
  return {
      static_cast<uint8_t>(static_cast<int>(c.r) * c.a / 255),
      static_cast<uint8_t>(static_cast<int>(c.g) * c.a / 255),
      static_cast<uint8_t>(static_cast<int>(c.b) * c.a / 255),
      c.a,
  };
}

DrawRect to_draw_rect(const Rect &r) {
  return {r.x, r.y, r.width, r.height};
}

Color control_fill(const NodeSnapshot &node) {
  if (node.interaction.disabled)
    return kButtonDisabledFill;
  if (node.role == NodeRole::Checkbox && node.interaction.checked)
    return kCheckedFill;
  if (node.role == NodeRole::Input)
    return kInputFill;
  return kButtonFill;
}

int text_length(const char *value) {
  return static_cast<int>(strlen(value ? value : ""));
}

int clamp_int(int value, int low, int high) {
  if (value < low)
    return low;
  if (value > high)
    return high;
  return value;
}

// Emit one Text command for a byte slice at an explicit, already-positioned
// per-line draw rect (the measurer baked in alignment + line y).
bool push_text_line(DrawCommandList &list, NodeId node_id, const DrawRect &rect,
                    const char *bytes, uint32_t byte_len, Color straight_color,
                    uint16_t font_id, float font_size, uint16_t line_index,
                    TextAlign align, uint8_t reveal_boost = 0,
                    uint8_t reveal_step = 0) {
  uint16_t n = byte_len > 0xFFFFu ? static_cast<uint16_t>(0xFFFFu)
                                  : static_cast<uint16_t>(byte_len);
  uint32_t off = 0;
  if (!list.push_text(bytes ? bytes : "", n, &off))
    return false;

  DrawCommand command = {};
  command.kind = DrawCommandKind::Text;
  command.node_id = node_id;
  command.rect = rect;
  command.payload.text = {
      .text_off = off,
      .text_len = n,
      .color = premul(straight_color),
      .font_id = font_id,
      .font_size = font_size,
      .line_index = line_index,
      .align = align,
      .reveal_boost = reveal_boost,
      .reveal_step = reveal_step,
  };
  return list.push(command);
}

// Single-line convenience used by inputs and the no-measurer fallback.
bool push_text_command(DrawCommandList &list, NodeId node_id, const Rect &rect,
                       const char *value, Color straight_color,
                       float font_size, TextAlign align) {
  const char *safe = value ? value : "";
  uint32_t len = static_cast<uint32_t>(strlen(safe));
  return push_text_line(list, node_id, to_draw_rect(rect), safe, len,
                        straight_color, 0, font_size, 0, align);
}

// The content box of a node: its laid-out rect minus its own border + padding.
Rect content_rect(const Rect &layout) {
  Rect r = layout;
  r.x = layout.x + layout.border.left + layout.padding.left;
  r.y = layout.y + layout.border.top + layout.padding.top;
  r.width = layout.width - layout.border.left - layout.border.right -
            layout.padding.left - layout.padding.right;
  r.height = layout.height - layout.border.top - layout.border.bottom -
             layout.padding.top - layout.padding.bottom;
  if (r.width < 0.0f)
    r.width = 0.0f;
  if (r.height < 0.0f)
    r.height = 0.0f;
  return r;
}

// Measured pen advance for a byte sub-slice [0,len) of value, via the injected
// measurer (design §10.5). No measurer (hermetic tests) => fixed 8px/byte so
// caret math stays deterministic.
float measured_advance(const char *value, uint32_t len, uint16_t font_id,
                       float font_size, float line_height) {
  if (len == 0)
    return 0.0f;
  MeasureTextFn measurer = text_measurer();
  if (!measurer)
    return static_cast<float>(len) * 8.0f;
  TextMetricsQuery query = {};
  query.utf8 = value;
  query.len = len;
  query.font_id = font_id;
  query.font_size = font_size;
  query.align = TextAlign::Left;
  query.wrap = TextWrap::None;
  query.line_height = line_height;
  query.wrap_width = 0.0f;
  TextMetricsResult result = measurer(query);
  return result.width;
}

// Cap-top..baseline of the resolved face, in points, via the injected measurer.
// 0 when unavailable (caller falls back to the line box). Drives the caret
// height so it spans the glyph ink, not the descender-inclusive cell.
float measured_ascent(uint16_t font_id, float font_size, float line_height) {
  MeasureTextFn measurer = text_measurer();
  if (!measurer)
    return 0.0f;
  TextMetricsQuery query = {};
  query.utf8 = "";
  query.len = 0;
  query.font_id = font_id;
  query.font_size = font_size;
  query.align = TextAlign::Left;
  query.wrap = TextWrap::None;
  query.line_height = line_height;
  query.wrap_width = 0.0f;
  return measurer(query).ascent;
}

bool push_rect_command(DrawCommandList &list, NodeId node_id, const Rect &rect,
                       Color straight_fill, float corner_radius) {
  DrawCommand command = {};
  command.kind = DrawCommandKind::Rect;
  command.node_id = node_id;
  command.rect = to_draw_rect(rect);
  command.payload.rect = {
      .fill = premul(straight_fill),
      .corner_radius = corner_radius,
  };
  return list.push(command);
}

bool push_gradient_command(DrawCommandList &list, NodeId node_id,
                           const Rect &rect, const Gradient &gradient,
                           float corner_radius) {
  GradientStop premul_stops[UI_MAX_GRADIENT_STOPS] = {};
  uint8_t n = gradient.stop_count;
  if (n > UI_MAX_GRADIENT_STOPS)
    n = UI_MAX_GRADIENT_STOPS;
  for (uint8_t i = 0; i < n; ++i) {
    premul_stops[i].t = gradient.stops[i].t;
    premul_stops[i].color = premul(gradient.stops[i].color);
  }
  uint16_t off = 0;
  if (!list.push_stops(premul_stops, n, &off))
    return false;

  DrawCommand command = {};
  command.kind = DrawCommandKind::Gradient;
  command.node_id = node_id;
  command.rect = to_draw_rect(rect);
  command.payload.gradient = {
      .stop_off = off,
      .stop_count = n,
      .angle_deg = gradient.angle_deg,
      .corner_radius = corner_radius,
  };
  return list.push(command);
}

// Shadow command, emitted before the fill so it sits behind the node
// (design §9.8 order: Shadow -> fill/Gradient/Image -> children -> Border).
bool append_shadow(DrawCommandList &list, const NodeSnapshot &node) {
  const Shadow &sh = node.visual.shadow;
  if (sh.color.a == 0)
    return true;

  DrawCommand command = {};
  command.kind = DrawCommandKind::Shadow;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.shadow = {
      .color = premul(sh.color),
      .offset = sh.offset,
      .blur = sh.blur,
      .spread = sh.spread,
      .corner_radius = node.visual.corner_radius,
  };
  return list.push(command);
}

// Image command (textured rect), emitted after the fill (design §9.8). The
// executor cuts corner_radius on nine-slice per §9.7.
bool append_image(DrawCommandList &list, const NodeSnapshot &node) {
  const BackgroundImage &bi = node.visual.image;
  if (bi.texture_id == 0)
    return true;

  DrawCommand command = {};
  command.kind = DrawCommandKind::Image;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.image = {
      .texture_id = bi.texture_id,
      .tint = premul(bi.tint),
      .nine_slice = bi.nine_slice,
      .corner_radius = node.visual.corner_radius,
      .src_x = bi.src_x,
      .src_y = bi.src_y,
      .src_w = bi.src_w,
      .src_h = bi.src_h,
      .flip_h = bi.flip_h,
      .fit = bi.fit,
  };
  return list.push(command);
}

// Rect command carrying only the resolved fill; the stroke is a separate Border
// command emitted by append_frame.
bool append_rect(DrawCommandList &list, const NodeSnapshot &node,
                 bool focused) {
  const VisualStyle &v = node.visual;
  bool visual_box = has_color(v.background) || v.gradient.stop_count > 0 ||
                    (v.border.color.top.a > 0 && v.border.width.top > 0.0f) ||
                    (v.outline.color.a > 0 && v.outline.width > 0.0f);
  bool control_box = node.role == NodeRole::Button ||
                     node.role == NodeRole::Checkbox ||
                     node.role == NodeRole::Input;
  if (!visual_box && !control_box && !focused) {
    return true;
  }

  // A resolved gradient IS the fill — emit it in place of the solid Rect.
  if (v.gradient.stop_count > 0) {
    return push_gradient_command(list, node.id, node.layout, v.gradient,
                                 v.corner_radius);
  }

  // Sprite-backed (image) controls carry their own baked interior, so skip the
  // intrinsic vector control fill — else a control_fill rect slabs an opaque pill
  // behind a transparent-interior sprite. Mirrors append_frame's texture_id guard.
  Color fill = has_color(v.background)
                   ? v.background
                   : (control_box && v.image.texture_id == 0 && !v.chromeless
                          ? control_fill(node)
                          : kTransparent);

  return push_rect_command(list, node.id, node.layout, fill, v.corner_radius);
}

// The per-side border + signed-offset outline (focus ring): the resolved border,
// else a control-role default; the resolved outline, else the focus-ring fallback.
bool append_frame(DrawCommandList &list, const NodeSnapshot &node,
                  bool focused) {
  const VisualStyle &v = node.visual;
  bool control_box = node.role == NodeRole::Button ||
                     node.role == NodeRole::Checkbox ||
                     node.role == NodeRole::Input;

  // ANY painted side qualifies — side-masked frames (e.g. the lobby
  // stepped-pane seams) paint right/bottom only.
  Border border = {};
  bool has_border = false;
  const bool any_border_side =
      (v.border.color.top.a > 0 && v.border.width.top > 0.0f) ||
      (v.border.color.right.a > 0 && v.border.width.right > 0.0f) ||
      (v.border.color.bottom.a > 0 && v.border.width.bottom > 0.0f) ||
      (v.border.color.left.a > 0 && v.border.width.left > 0.0f);
  if (any_border_side) {
    border = v.border;
    has_border = true;
  } else if (control_box && v.image.texture_id == 0 && !v.chromeless) {
    // Sprite-backed or chromeless controls carry their own baked edge, so skip
    // the intrinsic vector control border.
    Color border_color =
        node.interaction.disabled ? kButtonDisabledBorder : kButtonBorder;
    border.width = {1.0f, 1.0f, 1.0f, 1.0f};
    border.color = {border_color, border_color, border_color, border_color};
    has_border = true;
  }

  Outline outline = {};
  bool has_outline = false;
  if (v.outline.color.a > 0 && v.outline.width > 0.0f) {
    outline = v.outline;
    has_outline = true;
  } else if (focused && !node.interaction.disabled &&
             v.image.texture_id == 0 && !v.chromeless) {
    // Sprite-backed or chromeless controls own their focus look; the vector
    // fallback focus ring applies only to vector controls.
    outline = {kFocusBorderWidth, kFocusBorder, kFocusBorderOffset};
    has_outline = true;
  }

  if (!has_border && !has_outline)
    return true;

  if (has_border) {
    border.color.top = premul(border.color.top);
    border.color.right = premul(border.color.right);
    border.color.bottom = premul(border.color.bottom);
    border.color.left = premul(border.color.left);
  }
  if (has_outline)
    outline.color = premul(outline.color);

  DrawCommand command = {};
  command.kind = DrawCommandKind::Border;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.border = {
      .border = has_border ? border : Border{},
      .outline = has_outline ? outline : Outline{},
      .fill = {},
      .corner_radius = v.corner_radius,
      .has_fill = false,
      .has_outline = has_outline,
  };
  return list.push(command);
}

// TEXT (role==Text): measure-driven, multi-line. Emits one Text command per
// measured LineRun. Measure == layout (same measurer, design §10.4). Overflow
// beyond UI_MAX_TEXT_LINES is a failed frame, never a silent drop.
bool append_text(DrawCommandList &list, const NodeSnapshot &node,
                 bool inherited_disabled) {
  if (node.role != NodeRole::Text)
    return true;

  const VisualStyle &v = node.visual;
  Color color = has_color(v.text.color)
                    ? v.text.color
                    : ((node.interaction.disabled || inherited_disabled)
                           ? kTextDisabledFill
                           : kTextFill);
  float font_size = v.text.font_size > 0.f ? v.text.font_size : 15.f;
  uint16_t font_id = v.text.font_id;
  TextAlign align = v.text.align;
  const char *value = node.value ? node.value : "";
  uint32_t value_len = static_cast<uint32_t>(strlen(value));

  Rect content = content_rect(node.layout);

  MeasureTextFn measurer = text_measurer();
  if (!measurer) {
    // Hermetic fallback (no measurer installed): one line at the content rect.
    return push_text_command(list, node.id, content, value, color, font_size,
                             align);
  }

  TextMetricsQuery query = {};
  query.utf8 = value;
  query.len = value_len;
  query.font_id = font_id;
  query.font_size = font_size;
  query.align = align;
  query.wrap = v.text.wrap;
  query.line_height = v.text.line_height;
  query.wrap_width = content.width;
  TextMetricsResult metrics = measurer(query);

  for (uint8_t i = 0; i < metrics.line_count; ++i) {
    const LineRun &line = metrics.lines[i];
    if (line.slice_offset + line.slice_len > value_len)
      return false; // measurer returned an out-of-range slice — fail the frame
    DrawRect rect = {content.x + line.x, content.y + line.y, line.w, line.h};
    // The reveal ramp lives at the text's trailing edge: last line only.
    const bool last_line = (i + 1 == metrics.line_count);
    if (!push_text_line(list, node.id, rect, value + line.slice_offset,
                        line.slice_len, color, font_id, font_size, i, align,
                        last_line ? v.text.reveal_boost : 0,
                        last_line ? v.text.reveal_step : 0))
      return false;
  }

  if (metrics.overflowed) {
    ++list.error_count; // text needed > UI_MAX_TEXT_LINES — never a silent drop
    return false;
  }
  return true;
}

// INPUT contents (role==Input): selection rect, value text, caret rect. Caret
// and selection x come from real measured advances of the value's byte prefixes
// (design §10.5), so the cursor lands exactly under the laid-out glyph.
bool append_input_contents(DrawCommandList &list, const NodeSnapshot &node,
                           bool focused, bool inherited_disabled,
                           InputScrollStore *input_scroll) {
  if (node.role != NodeRole::Input)
    return true;

  // The text pen sits at the input's content box (border + padding), so the
  // screen owns the inset — the transcriber adds none of its own.
  constexpr float kTextHeight = 16.0f;
  const float inset_l = node.layout.border.left + node.layout.padding.left;
  const float inset_r = node.layout.border.right + node.layout.padding.right;
  float text_y = node.layout.y + (node.layout.height - kTextHeight) * 0.5f;
  Rect text_rect = {};
  text_rect.x = node.layout.x + inset_l;
  text_rect.y = text_y;
  text_rect.width = node.layout.width - inset_l - inset_r;
  text_rect.height = kTextHeight;

  const char *value = node.value ? node.value : "";
  const char *display_value =
      node.display_value && node.display_value[0] ? node.display_value : value;
  int length = text_length(value);
  int display_length = text_length(display_value);
  float font_size = node.visual.text.font_size > 0.f ? node.visual.text.font_size
                                                      : 15.f;
  // Inputs are single-line; reuse the node's resolved text line height if any.
  float line_height = node.visual.text.line_height;
  uint16_t font_id = node.visual.text.font_id;

  int selection_start = clamp_int(node.text_edit.selection_start, 0, length);
  int selection_end = clamp_int(node.text_edit.selection_end, 0, length);
  if (selection_end < selection_start) {
    int tmp = selection_start;
    selection_start = selection_end;
    selection_end = tmp;
  }

  auto advance_to = [&](int idx) -> float {
    int display_idx = clamp_int(idx, 0, display_length);
    return measured_advance(display_value, static_cast<uint32_t>(display_idx), font_id,
                            font_size, line_height);
  };

  // Single-line horizontal scroll: window the value so the caret stays visible.
  // The window leaves a caret-width sliver at the right edge so the end-of-text
  // caret isn't clipped. An unfocused field shows from its start.
  float caret_w =
      node.visual.caret.width > 0.f ? node.visual.caret.width : kCaretWidth;
  float caret_px = advance_to(clamp_int(node.text_edit.caret, 0, length));
  float full_w = advance_to(display_length);
  float visible_w = text_rect.width - caret_w;
  if (visible_w < 0.f)
    visible_w = 0.f;
  float max_scroll = full_w - visible_w;
  if (max_scroll < 0.f)
    max_scroll = 0.f;
  float scroll_x = 0.f;
  if (focused && input_scroll) {
    // Stateful minimal scroll: shift the window only when the caret falls off an
    // edge, so the window doesn't chase the caret until it hits an edge.
    auto it = input_scroll->find(node.id);
    float prev = it != input_scroll->end() ? it->second : 0.f;
    if (caret_px < prev)
      prev = caret_px;
    else if (caret_px > prev + visible_w)
      prev = caret_px - visible_w;
    if (prev > max_scroll)
      prev = max_scroll;
    if (prev < 0.f)
      prev = 0.f;
    (*input_scroll)[node.id] = prev;
    scroll_x = prev;
  } else if (focused) {
    // Stateless right-pin fallback (no store, e.g. tests/goldens).
    scroll_x = caret_px - visible_w;
    if (scroll_x < 0.f)
      scroll_x = 0.f;
  } else if (input_scroll) {
    input_scroll->erase(node.id); // drop stale offset; unfocused shows the head
  }

  // Clip value/selection/caret to the content window so scrolled text doesn't
  // bleed past the field edges. The border was already painted, so this only
  // scissors the glyphs.
  Rect clip_rect = {};
  clip_rect.x = text_rect.x;
  clip_rect.y = node.layout.y;
  clip_rect.width = text_rect.width;
  clip_rect.height = node.layout.height;
  {
    DrawCommand clip = {};
    clip.kind = DrawCommandKind::ClipPush;
    clip.node_id = node.id;
    clip.rect = to_draw_rect(clip_rect);
    if (!list.push(clip))
      return false;
  }

  if (focused && selection_end > selection_start) {
    float start_x = advance_to(selection_start);
    float end_x = advance_to(selection_end);
    Rect sel = {};
    sel.x = text_rect.x + start_x - scroll_x;
    sel.y = text_rect.y;
    sel.width = end_x - start_x;
    sel.height = text_rect.height;
    if (!push_rect_command(list, node.id, sel, kSelectionFill, 0.0f))
      return false;
  }

  Color text_color =
      has_color(node.visual.text.color)
          ? node.visual.text.color
          : ((node.interaction.disabled || inherited_disabled) ? kTextDisabledFill
                                                               : kTextFill);
  Rect value_rect = text_rect;
  value_rect.x -= scroll_x;
  if (!push_text_command(list, node.id, value_rect, display_value, text_color,
                         font_size, TextAlign::Left))
    return false;

  if (focused && node.text_edit.show_caret) {
    int caret = clamp_int(node.text_edit.caret, 0, length);
    float caret_x = advance_to(caret);
    // Caret height tracks the glyph ink (measured ascent, cap-top..baseline,
    // top-aligned with the text), not the descender-inclusive cell — else it
    // hangs ~1-2px below the visible baseline. Falls back to min(boxH*4/5, line
    // box) when ascent is unavailable.
    const Caret &ck = node.visual.caret;
    float ascent = measured_ascent(font_id, font_size, line_height);
    Rect caret_rect = {};
    caret_rect.x = text_rect.x + caret_x - scroll_x;
    caret_rect.width = ck.width > 0.f ? ck.width : kCaretWidth;
    if (ascent > 0.f) {
      caret_rect.height = ascent;
      caret_rect.y = text_y;
    } else {
      float caret_h = node.layout.height * 0.8f;
      float line_box = line_height > 0.f ? line_height : kTextHeight;
      if (line_box < caret_h)
        caret_h = line_box;
      caret_rect.height = caret_h;
      caret_rect.y =
          node.layout.y + (node.layout.height - caret_rect.height) * 0.5f;
    }
    Color caret_color = ck.color.a > 0 ? ck.color : kCaretFill;
    if (!push_rect_command(list, node.id, caret_rect, caret_color, 0.0f))
      return false;
  }

  {
    DrawCommand clip = {};
    clip.kind = DrawCommandKind::ClipPop;
    clip.node_id = node.id;
    if (!list.push(clip))
      return false;
  }
  return true;
}

// LayerPush bracketing the node's subtree (design §9.10): the executor renders
// it to a transient target and composites back at the layer opacity, so
// overlapping translucent children flatten once (no double-darkening).
bool push_layer(DrawCommandList &list, const NodeSnapshot &node) {
  DrawCommand command = {};
  command.kind = DrawCommandKind::LayerPush;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.layer = {.opacity = node.visual.opacity};
  return list.push(command);
}

bool pop_layer(DrawCommandList &list, const NodeSnapshot &node) {
  DrawCommand command = {};
  command.kind = DrawCommandKind::LayerPop;
  command.node_id = node.id;
  return list.push(command);
}

// CLIP: overflow != Visible brackets the children in ClipPush/ClipPop, scissoring
// content outside the node's border box (design §9.7; the executor intersects
// nested clips). Required by the scroll-viewport primitive.
bool push_clip(DrawCommandList &list, const NodeSnapshot &node) {
  DrawCommand command = {};
  command.kind = DrawCommandKind::ClipPush;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  return list.push(command);
}

bool pop_clip(DrawCommandList &list, const NodeSnapshot &node) {
  DrawCommand command = {};
  command.kind = DrawCommandKind::ClipPop;
  command.node_id = node.id;
  return list.push(command);
}

bool append_node(const UiTree &tree, DrawCommandList &list, NodeId id,
                 bool inherited_disabled, NodeId focused_id,
                 InputScrollStore *input_scroll) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return false;

  // hidden => skip paint for the node and its subtree (design §9.10 / §8.5).
  if (node.visual.hidden)
    return true;

  bool disabled = inherited_disabled || node.interaction.disabled;
  bool focused = focused_id != 0 && focused_id == node.id;

  // Group opacity < 1 brackets the node's paint + subtree in a LayerPush/Pop so
  // the executor composites the group as a unit (design §9.10).
  const bool layer = node.visual.opacity < 1.0f;
  if (layer && !push_layer(list, node))
    return false;

  // Paint order per design §9.8: Shadow -> fill/Gradient -> Image -> [children]
  // -> Border+Outline.
  if (!append_shadow(list, node) ||
      !append_rect(list, node, focused) ||
      !append_image(list, node) ||
      !append_frame(list, node, focused) ||
      !append_text(list, node, inherited_disabled) ||
      !append_input_contents(list, node, focused, inherited_disabled,
                             input_scroll))
    return false;

  const bool clip = node.style.overflow != Overflow::Visible;
  if (clip && !push_clip(list, node))
    return false;
  for (int i = 0; i < tree.child_count(id); ++i) {
    if (!append_node(tree, list, tree.child_at(id, i), disabled, focused_id,
                     input_scroll))
      return false;
  }
  if (clip && !pop_clip(list, node))
    return false;

  if (layer && !pop_layer(list, node))
    return false;
  return true;
}

} // namespace

bool build_draw_command_list(const UiTree &tree, DrawCommandList *out,
                             NodeId focused_id, InputScrollStore *input_scroll) {
  if (!out || !tree.contains(tree.root_id()))
    return false;
  out->reset();
  return append_node(tree, *out, tree.root_id(), false, focused_id,
                     input_scroll) &&
         out->error_count == 0;
}

} // namespace ui
