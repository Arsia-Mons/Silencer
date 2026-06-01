#include "draw_command_builder.h"

#include "../style/text_measure.h"

#include <string.h>

namespace ui {

namespace {

// Default paint for nodes whose resolved VisualStyle leaves a slot unset (a
// bare text node with no .visual.text.color; an input's intrinsic
// selection/caret). These are role/intrinsic defaults — NOT a node.style paint
// fallback (that legacy path is gone; the transcriber reads node.visual only).
// Premultiplication happens at emit (premul()), never here.
constexpr Color kTransparent = {0, 0, 0, 0};
constexpr Color kFocusBorder = {96, 165, 250, 255}; // accent (matches theme focus_ring)
constexpr float kFocusBorderWidth = 2.0f;
constexpr float kFocusBorderOffset = 2.0f;
constexpr Color kSelectionFill = {72, 116, 164, 180};
constexpr Color kCaretFill = {232, 240, 248, 255};
constexpr Color kTextFill = {226, 234, 242, 255};
constexpr Color kTextDisabledFill = {126, 134, 148, 255};

bool has_color(Color color) { return color.a > 0; }

// Straight-alpha -> premultiplied (the IR's storage convention). The only
// transparent color is (0,0,0,0); a==0 always maps to fully transparent.
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

// Emit one Text command for a byte slice at an explicit draw rect. The rect is
// the already-positioned per-line box (the measurer baked alignment + line y);
// the executor blits at rect.x/rect.y verbatim.
bool push_text_line(DrawCommandList &list, NodeId node_id, const DrawRect &rect,
                    const char *bytes, uint32_t byte_len, Color straight_color,
                    uint16_t font_id, uint16_t font_size, uint16_t line_index,
                    TextAlign align) {
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
  };
  return list.push(command);
}

// Single-line convenience used by inputs and the no-measurer fallback.
bool push_text_command(DrawCommandList &list, NodeId node_id, const Rect &rect,
                       const char *value, Color straight_color,
                       uint16_t font_id, uint16_t font_size,
                       TextAlign align) {
  const char *safe = value ? value : "";
  uint32_t len = static_cast<uint32_t>(strlen(safe));
  return push_text_line(list, node_id, to_draw_rect(rect), safe, len,
                        straight_color, font_id, font_size, 0, align);
}

// The content box of a node: its laid-out rect minus its own border + padding.
// Text layout (wrap width, alignment, line origin) happens inside this box.
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
// caret math stays deterministic. wrap=None, no box (single run advance).
float measured_advance(const char *value, uint32_t len, uint16_t font_id,
                       uint16_t font_size, float line_height) {
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

float measured_line_height(uint16_t font_id, uint16_t font_size,
                           float line_height) {
  if (line_height > 0.0f)
    return line_height;
  MeasureTextFn measurer = text_measurer();
  if (!measurer)
    return 16.0f;
  TextMetricsQuery query = {};
  query.utf8 = "M";
  query.len = 1;
  query.font_id = font_id;
  query.font_size = font_size;
  query.align = TextAlign::Left;
  query.wrap = TextWrap::None;
  query.line_height = line_height;
  query.wrap_width = 0.0f;
  TextMetricsResult result = measurer(query);
  if (result.line_count > 0 && result.lines[0].h > 0.0f)
    return result.lines[0].h;
  return result.height > 0.0f ? result.height : 16.0f;
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

// GRADIENT FILL: emit a Gradient command (the gradient IS the fill, replacing
// the solid Rect). Authored stops are STRAIGHT alpha; premultiply at emit, the
// IR's storage convention. Stops live in the list's grad_arena via push_stops.
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

// SHADOW: emit a Shadow command BEFORE the fill so it sits behind the node
// (design §9.8 ordering: Shadow -> fill/Gradient/Image -> children -> Border).
// Only the component-resolved VisualStyle carries a shadow (no legacy source);
// emit when shadow.color.a>0. The shadow color is authored straight; premultiply
// at emit, the IR's storage convention.
bool append_shadow(DrawCommandList &list, const NodeSnapshot &node) {
  const Shadow &sh = node.visual.shadow;
  if (sh.color.a == 0)
    return true; // no shadow => emit nothing (resolved-output presence cue).

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

// IMAGE: emit an Image command (textured rect) AFTER the fill (design §9.8:
// fill/Gradient/Image). Only the component-resolved VisualStyle carries an
// image; emit when image.texture_id!=0. The tint is authored straight;
// premultiply at emit. nine_slice + corner_radius pass through verbatim (the
// executor cuts radius on nine-slice per §9.7).
bool append_image(DrawCommandList &list, const NodeSnapshot &node) {
  const BackgroundImage &bi = node.visual.image;
  if (bi.texture_id == 0)
    return true; // no image => emit nothing (resolved-output presence cue).

  DrawCommand command = {};
  command.kind = DrawCommandKind::Image;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.image = {
      .texture_id = bi.texture_id,
      .tint = premul(bi.tint),
      .nine_slice = bi.nine_slice,
      .corner_radius = node.visual.corner_radius,
      .source_x = bi.source_x,
      .source_y = bi.source_y,
      .source_w = bi.source_w,
      .source_h = bi.source_h,
      .effect_color = bi.effect_color,
      .brightness = bi.brightness,
      .ramp_color = bi.ramp_color,
      .ramp_plus = bi.ramp_plus,
      .team_color = bi.team_color,
      .outline_color = bi.outline_color,
      .tile = bi.tile,
      .team_emblem = bi.team_emblem,
      .scaled = bi.scaled,
  };
  return list.push(command);
}

bool append_bitmap(DrawCommandList &list, const NodeSnapshot &node) {
  const RawBitmapImage &bitmap = node.visual.bitmap;
  if (!bitmap.pixels || bitmap.width == 0 || bitmap.height == 0)
    return true;

  DrawCommand command = {};
  command.kind = DrawCommandKind::Bitmap;
  command.node_id = node.id;
  command.rect = to_draw_rect(node.layout);
  command.payload.bitmap = {
      .pixels = bitmap.pixels,
      .width = bitmap.width,
      .height = bitmap.height,
  };
  return list.push(command);
}

// FILL: emit a Rect command carrying only the resolved fill (the legacy
// append_rect fused fill + border + focus into one command; the new IR splits
// the stroke into a separate Border command, emitted by append_frame).
bool append_rect(DrawCommandList &list, const NodeSnapshot &node,
                 bool focused) {
  const VisualStyle &v = node.visual;
  bool image_box = v.image.texture_id != 0;
  bool visual_box = has_color(v.background) || v.gradient.stop_count > 0 ||
                    image_box ||
                    (v.border.color.top.a > 0 && v.border.width.top > 0.0f) ||
                    (v.outline.color.a > 0 && v.outline.width > 0.0f);
  if (!visual_box && !focused) {
    return true;
  }

  // A resolved gradient (stop_count>0) IS the fill — emit it in place of the
  // solid Rect. Gradients only ever come from the resolved VisualStyle.
  if (v.gradient.stop_count > 0) {
    return push_gradient_command(list, node.id, node.layout, v.gradient,
                                 v.corner_radius);
  }
  if (image_box && !has_color(v.background)) {
    return true;
  }

  Color fill = has_color(v.background) ? v.background : kTransparent;

  return push_rect_command(list, node.id, node.layout, fill, v.corner_radius);
}

// FRAME: the fused per-side border + signed-offset outline (focus ring). Mirror
// append_rect's source-selection: component-resolved border preferred, else the
// legacy single-color style/control border mapped to all four sides; outline is
// the component-resolved focus ring, else the legacy focus injection.
bool append_frame(DrawCommandList &list, const NodeSnapshot &node,
                  bool focused) {
  const VisualStyle &v = node.visual;

  // Border: emit only the resolved per-side VisualStyle border. Component/theme
  // resolution owns defaults; the draw-list builder must not invent a fallback
  // fill or stroke for transparent sprite/list controls.
  Border border = {};
  bool has_border = false;
  if (v.border.color.top.a > 0 && v.border.width.top > 0.0f) {
    border = v.border;
    has_border = true;
  }

  // Outline / focus ring: prefer the component-resolved outline, else the
  // legacy any-source injection (focused && !disabled).
  Outline outline = {};
  bool has_outline = false;
  if (v.outline.color.a > 0 && v.outline.width > 0.0f) {
    outline = v.outline;
    has_outline = true;
  } else if (focused && !node.interaction.disabled) {
    outline = {kFocusBorderWidth, kFocusBorder, kFocusBorderOffset};
    has_outline = true;
  }

  if (!has_border && !has_outline)
    return true;

  // Premultiply every color into the IR.
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

// TEXT (role==Text): measure-driven, multi-line. Asks the injected measurer for
// per-line layout (alignment + wrap baked in) and emits one Text command per
// LineRun, line_index propagated. Measure == layout because both call the same
// measurer (design §10.4). Overflow beyond UI_MAX_TEXT_LINES is a failed frame.
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
  uint16_t font_size =
      v.text.font_size > 0 ? v.text.font_size : static_cast<uint16_t>(15);
  uint16_t font_id = v.text.font_id;
  TextAlign align = v.text.align;
  const char *value = node.value ? node.value : "";
  uint32_t value_len = static_cast<uint32_t>(strlen(value));

  Rect content = content_rect(node.layout);

  MeasureTextFn measurer = text_measurer();
  if (!measurer) {
    // Hermetic fallback (no measurer installed): one line at the content rect.
    return push_text_command(list, node.id, content, value, color, font_id,
                             font_size, align);
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
    if (!push_text_line(list, node.id, rect, value + line.slice_offset,
                        line.slice_len, color, font_id, font_size, i, align))
      return false;
  }

  if (metrics.overflowed) {
    ++list.error_count; // text needed > UI_MAX_TEXT_LINES — never a silent drop
    return false;
  }
  return true;
}

// INPUT contents (role==Input): selection rect, value text, caret rect. Caret
// and selection x come from REAL measured advances of the value's byte prefixes
// (design §10.5) — never the old kCharWidth=8 hack — so the cursor lands exactly
// under the glyph the same measurer laid out.
bool append_input_contents(DrawCommandList &list, const NodeSnapshot &node,
                           bool focused, bool inherited_disabled) {
  if (node.role != NodeRole::Input)
    return true;

  constexpr float kInsetX = 8.0f;
  uint16_t font_size = node.visual.text.font_size > 0 ? node.visual.text.font_size
                                                      : static_cast<uint16_t>(15);
  // Inputs are single-line; reuse the node's resolved text line height if any.
  float line_height = node.visual.text.line_height;
  uint16_t font_id = node.visual.text.font_id;
  float text_height = measured_line_height(font_id, font_size, line_height);
  float text_y = node.layout.y + (node.layout.height - text_height) * 0.5f;
  Rect text_rect = {};
  text_rect.x = node.layout.x + kInsetX;
  text_rect.y = text_y;
  text_rect.width = node.layout.width - kInsetX * 2.0f;
  text_rect.height = text_height;

  const char *value = node.value ? node.value : "";
  int length = text_length(value);

  int selection_start = clamp_int(node.text_edit.selection_start, 0, length);
  int selection_end = clamp_int(node.text_edit.selection_end, 0, length);
  if (selection_end < selection_start) {
    int tmp = selection_start;
    selection_start = selection_end;
    selection_end = tmp;
  }

  // Measured pen x for the prefix [0, idx) of the value.
  auto advance_to = [&](int idx) -> float {
    return measured_advance(value, static_cast<uint32_t>(idx), font_id,
                            font_size, line_height);
  };

  if (focused && selection_end > selection_start) {
    float start_x = advance_to(selection_start);
    float end_x = advance_to(selection_end);
    Rect sel = {};
    sel.x = text_rect.x + start_x;
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
  if (!push_text_command(list, node.id, text_rect, value, text_color, font_id,
                         font_size, TextAlign::Left))
    return false;

  if (focused) {
    int caret = clamp_int(node.text_edit.caret, 0, length);
    float caret_x = advance_to(caret);
    Rect caret_rect = {};
    caret_rect.x = text_rect.x + caret_x;
    caret_rect.y = text_rect.y - 1.0f;
    caret_rect.width = 1.0f;
    caret_rect.height = text_rect.height + 2.0f;
    if (!push_rect_command(list, node.id, caret_rect, kCaretFill, 0.0f))
      return false;
  }
  return true;
}

// Emit a LayerPush bracketing the node's subtree (design §9.10). The header rect
// is the node's border-box; the executor sizes a transient render-to-target to
// it and composites the whole subtree back at the layer opacity, so overlapping
// translucent children flatten once (no double-darkening).
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

bool append_node(const UiTree &tree, DrawCommandList &list, NodeId id,
                 bool inherited_disabled, NodeId focused_id) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return false;

  // hidden => skip paint entirely (layout already happened upstream; design
  // §9.10 / §8.5). The node and its subtree contribute nothing to the IR.
  if (node.visual.hidden)
    return true;

  bool disabled = inherited_disabled || node.interaction.disabled;
  bool focused = focused_id != 0 && focused_id == node.id;

  // Group opacity: opacity < 1 brackets the node's own paint + subtree in a
  // LayerPush/LayerPop so the executor composites the group as a unit (design
  // §9.10). opacity >= 1 emits no layer (the common path). Brackets are
  // contiguous and balanced by construction (push before paint, pop after).
  const bool layer = node.visual.opacity < 1.0f;
  if (layer && !push_layer(list, node))
    return false;

  // Paint order per design §9.8: Shadow -> fill/Gradient -> Image -> [children]
  // -> Border+Outline. (Children are appended after this node's own paint.)
  if (!append_shadow(list, node) ||
      !append_rect(list, node, focused) ||
      !append_image(list, node) ||
      !append_bitmap(list, node) ||
      !append_frame(list, node, focused) ||
      !append_text(list, node, inherited_disabled) ||
      !append_input_contents(list, node, focused, inherited_disabled))
    return false;

  for (int i = 0; i < tree.child_count(id); ++i) {
    if (!append_node(tree, list, tree.child_at(id, i), disabled, focused_id))
      return false;
  }

  if (layer && !pop_layer(list, node))
    return false;
  return true;
}

} // namespace

bool build_draw_command_list(const UiTree &tree, DrawCommandList *out,
                             NodeId focused_id) {
  if (!out || !tree.contains(tree.root_id()))
    return false;
  out->reset();
  return append_node(tree, *out, tree.root_id(), false, focused_id) &&
         out->error_count == 0;
}

} // namespace ui
