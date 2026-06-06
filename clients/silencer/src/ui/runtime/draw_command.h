#pragma once

// The tagged-union DrawCommand IR (styling/render design §8). A trivially-
// copyable POD command stream produced by build_draw_command_list (a pure
// transcriber) and executed linearly by the SDL renderer. Colors are
// PREMULTIPLIED at emit. Variable data (text bytes, gradient stops) lives in
// out-of-line arenas; texture images use integer handles. Raw bitmap commands
// borrow host-owned pixels for immediate same-frame rendering; they still keep
// the command stream trivially copyable.

#include "../style/text_measure.h" // UI_MAX_TEXT_LINES
#include "../style/visual_style.h"
#include "tree.h" // NodeId, UI_RETAINED_MAX_NODES

#include <stdint.h>

#include <type_traits>

namespace ui {

enum class DrawCommandKind : uint8_t {
  None = 0,
  Rect,      // solid (optionally rounded) fill
  Border,    // fused per-side border + signed-offset outline (co-feathered)
  Text,      // one wrapped line (per-line emission)
  Image,     // textured rect: tint + nine-slice + radius
  Bitmap,    // raw indexed-pixel rect, host-owned for this frame
  Gradient,  // linear N-stop fill
  Shadow,    // drop shadow (feathered)
  ClipPush,  // push a clip rect (rect = header.rect)
  ClipPop,
  LayerPush, // begin a group-opacity offscreen layer
  LayerPop,
  Custom,    // reserved enum seat — no payload, no renderer path (design §15)
};

// ---- per-kind POD arms (no owned memory) ----
struct RectData {
  Color fill{};
  float corner_radius = 0.f;
};
struct BorderData { // fused fill + per-side border + outline, co-feathered
  Border border{};
  Outline outline{};
  Color fill{};
  float corner_radius = 0.f;
  bool has_fill = false;
  bool has_outline = false;
};
struct TextData {
  uint32_t text_off = 0; // slice into DrawList::text_arena
  uint16_t text_len = 0;
  Color color{};
  uint16_t font_id = 0;
  uint16_t font_size = 0;
  uint16_t line_index = 0;
  TextAlign align = TextAlign::Left;
};
struct ImageData {
  uint32_t texture_id = 0;
  Color tint{255, 255, 255, 255};
  SideWidths nine_slice{};
  float corner_radius = 0.f;
  ImageFit fit = ImageFit::Stretch;
  uint16_t source_x = 0;
  uint16_t source_y = 0;
  uint16_t source_w = 0;
  uint16_t source_h = 0;
  uint8_t effect_color = 0;
  uint8_t brightness = 128;
  uint8_t ramp_color = 0;
  uint8_t ramp_plus = 0;
  uint8_t team_color = 0;
  uint8_t outline_color = 0;
  bool tile = false;
  bool team_emblem = false;
  bool scaled = false;
};
struct BitmapData {
  const uint8_t *pixels = nullptr;
  uint16_t width = 0;
  uint16_t height = 0;
};
struct GradientData {
  uint16_t stop_off = 0; // slice into DrawList::grad_arena
  uint8_t stop_count = 0;
  float angle_deg = 0.f;
  float corner_radius = 0.f;
};
struct ShadowData {
  Color color{};
  Vec2 offset{};
  float blur = 0.f;
  float spread = 0.f;
  float corner_radius = 0.f;
};
struct ClipData {
  float corner_radius = 0.f; // clip rect itself is the header rect
};
struct LayerData {
  float opacity = 1.f;
};

union DrawPayload {
  RectData rect{}; // first arm carries the default member initializer -> aggregate
  BorderData border;
  TextData text;
  ImageData image;
  BitmapData bitmap;
  GradientData gradient;
  ShadowData shadow;
  ClipData clip;
  LayerData layer;
};

struct DrawCommand {
  DrawCommandKind kind = DrawCommandKind::None;
  NodeId node_id = 0;
  DrawRect rect{};
  DrawPayload payload{};
};

static_assert(std::is_aggregate_v<DrawCommand>,
              "DrawCommand must stay an aggregate so designated-init compiles");
static_assert(std::is_trivially_copyable_v<DrawCommand>,
              "DrawCommand must be trivially copyable (cursor-only reset, memcpy-safe)");
static_assert(std::is_trivially_copyable_v<DrawPayload>);

// ---- capacity, derived from the v1 used set (design §8.7) ----
constexpr int UI_TEXT_LINE_CAP = UI_MAX_TEXT_LINES; // per text node
// Box-like node worst case by emitted KINDS: Shadow + fill/gradient/image + frame
// + ClipPush/Pop + LayerPush/Pop = 7. Text node: up to UI_TEXT_LINE_CAP + clip = lines+2.
// Budget the larger shape per node, not the sum.
constexpr int UI_MAX_CMDS_PER_BOX_NODE = 7;
constexpr int UI_MAX_CMDS_PER_TEXT_NODE = UI_TEXT_LINE_CAP + 2;
constexpr int UI_MAX_CMDS_PER_NODE = UI_MAX_CMDS_PER_BOX_NODE > UI_MAX_CMDS_PER_TEXT_NODE
                                         ? UI_MAX_CMDS_PER_BOX_NODE
                                         : UI_MAX_CMDS_PER_TEXT_NODE;
constexpr int UI_MAX_DRAW_COMMANDS = UI_RETAINED_MAX_NODES * UI_MAX_CMDS_PER_NODE;
constexpr int UI_DRAW_TEXT_ARENA_BYTES = 8 * 1024;
constexpr int UI_DRAW_GRAD_STOPS = 256;

struct DrawCommandList {
  DrawCommand commands[UI_MAX_DRAW_COMMANDS] = {};
  int count = 0;
  char text_arena[UI_DRAW_TEXT_ARENA_BYTES] = {};
  int text_len_used = 0;
  GradientStop grad_arena[UI_DRAW_GRAD_STOPS] = {};
  int grad_count = 0;
  int error_count = 0;

  // Every variable-capacity write is bounds-checked; overflow bumps error_count
  // and fails the frame (never a silent clamp/truncate).
  bool push(const DrawCommand &command);
  bool push_text(const char *bytes, uint16_t len, uint32_t *out_off);
  bool push_stops(const GradientStop *stops, uint8_t n, uint16_t *out_off);
  void reset(); // cursors only; never memset the arrays
};

// Sizes pinned at first build (NodeId is uint64_t => 8-byte aligned header).
static_assert(sizeof(DrawPayload) <= 56, "largest union arm grew; re-check the budget");
static_assert(sizeof(DrawCommand) <= 96, "DrawCommand size budget; update the budget if intentional");
constexpr unsigned UI_DRAWLIST_BUDGET = 512u * 1024u;
static_assert(sizeof(DrawCommandList) < UI_DRAWLIST_BUDGET,
              "DrawCommandList exceeds its byte budget");

} // namespace ui
