#pragma once

// The tagged-union DrawCommand IR (styling/render design §8). A trivially-
// copyable POD command stream produced by build_draw_command_list (a pure
// transcriber) and executed linearly by the SDL renderer. Colors are
// PREMULTIPLIED at emit. Variable data (text bytes, gradient stops) lives in
// out-of-line arenas; arms hold integer handles, never pointers, so DrawCommand
// stays trivially copyable and DrawList stays relocatable.

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
  float font_size = 0.f;
  uint16_t line_index = 0;
  TextAlign align = TextAlign::Left;
  // Per-glyph trailing-edge brightness ramp (see TextVisual): the last glyph of
  // this run gets +reveal_boost on the green channel, falling off by reveal_step
  // per glyph toward the start. 0 = no ramp (the common single-batch path).
  uint8_t reveal_boost = 0;
  uint8_t reveal_step = 0;
};
struct ImageData {
  uint32_t texture_id = 0;
  Color tint{255, 255, 255, 255};
  SideWidths nine_slice{};
  float corner_radius = 0.f;
  // Source sub-rect in texture pixels (atlasing / partial-fill). w==0 || h==0
  // => sample the whole texture. Honored by the plain + rounded paths.
  float src_x = 0.f, src_y = 0.f, src_w = 0.f, src_h = 0.f;
  bool flip_h = false; // mirror horizontally
  ImageFit fit = ImageFit::Stretch; // plain path only
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

// A node holds at most this many clip/layer brackets open across its child
// recursion: one LayerPush (group opacity) + one ClipPush (overflow clip). The
// input field's own clip opens and closes within the node's own paint, before
// the child loop, so it is never simultaneously open with these. Bump this if
// you add a new per-node bracket kind — the reserve below depends on it.
constexpr int UI_MAX_OPEN_BRACKETS_PER_NODE = 2;

// Closing-bracket commands (ClipPop/LayerPop) draw from a reserved tail of the
// command buffer so a content-saturated frame can still emit a matching pop for
// every open clip/layer (INV4: clip/layer balance holds unconditionally). The
// reserve covers the worst case — every level of the retained tree's max-depth
// recursion holding all of its brackets open at once — so a dropped pop, and the
// unbalanced LayerPush composite smudge it would cause, is structurally
// impossible.
constexpr int UI_DRAW_COMMANDS_RESERVE =
    UI_MAX_OPEN_BRACKETS_PER_NODE * UI_RETAINED_MAX_DEPTH;

struct DrawCommandList {
  DrawCommand commands[UI_MAX_DRAW_COMMANDS] = {};
  int count = 0;
  char text_arena[UI_DRAW_TEXT_ARENA_BYTES] = {};
  int text_len_used = 0;
  GradientStop grad_arena[UI_DRAW_GRAD_STOPS] = {};
  int grad_count = 0;
  int dropped_count = 0; // degradation telemetry; never gates the frame

  // Fixed-capacity, defined-overflow writes (INV5): a full arena drops the one
  // command and bumps dropped_count, never fails the frame. push() leaves the
  // reserve for closing brackets; push_close() (ClipPop/LayerPop) may use it so
  // balance always holds. Returns whether the command landed — callers gate the
  // matching pop on the push's return, never abort the walk.
  bool push(const DrawCommand &command);
  bool push_close(const DrawCommand &command);
  bool push_text(const char *bytes, uint16_t len, uint32_t *out_off);
  bool push_stops(const GradientStop *stops, uint8_t n, uint16_t *out_off);
  void reset(); // cursors only; never memset the arrays
};

// Sizes pinned at first build (NodeId is uint64_t => 8-byte aligned header).
static_assert(sizeof(DrawPayload) <= 56, "largest union arm grew; re-check the budget");
static_assert(sizeof(DrawCommand) <= 96, "DrawCommand size budget; update the budget if intentional");
// Scales with UI_RETAINED_MAX_NODES (UI_MAX_DRAW_COMMANDS = nodes * cmds/node).
// Raised alongside the 256->512 node-cap bump so the per-frame draw list still
// fits with headroom (512 nodes * 7 cmds * <=96B ~= 344KB).
// UI_MAX_TEXT_LINES 8->32 (the cc wrapped lore paragraph) raised the per-text-
// node command ceiling to 34, so the worst-case list grew ~3x.
// 512->1024 node bump (in-game center-message glyph stacks): 1024 * 34 * 96B.
constexpr unsigned UI_DRAWLIST_BUDGET = 4096u * 1024u;
static_assert(sizeof(DrawCommandList) < UI_DRAWLIST_BUDGET,
              "DrawCommandList exceeds its byte budget");

} // namespace ui
