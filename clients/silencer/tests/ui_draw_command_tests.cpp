// P3: the new tagged-union DrawCommand IR — aggregate/POD invariants + bounds.
#include "ui/runtime/draw_command.h"

#include <stdio.h>
#include <type_traits>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

static_assert(std::is_aggregate_v<DrawCommand>);
static_assert(std::is_trivially_copyable_v<DrawCommand>);
static_assert(std::is_trivially_copyable_v<DrawPayload>);

static bool test_designated_init() {
  // Designated init at a call site must compile (aggregate union).
  DrawCommand c{.kind = DrawCommandKind::Rect,
                .node_id = 7,
                .rect = {1, 2, 3, 4},
                .payload = {.rect = {.fill = {10, 20, 30, 255}, .corner_radius = 6.f}}};
  CHECK(c.kind == DrawCommandKind::Rect);
  CHECK(c.node_id == 7);
  CHECK(c.payload.rect.fill == (Color{10, 20, 30, 255}));
  CHECK(c.payload.rect.corner_radius == 6.f);
  return true;
}

static bool test_push_overflow() {
  static DrawCommandList list; // large; keep off the stack
  list.reset();
  // Content push() stops at the reserve boundary (defined overflow: the command
  // is dropped, the frame is not failed) so closing brackets always have room.
  const int content_cap = UI_MAX_DRAW_COMMANDS - UI_DRAW_COMMANDS_RESERVE;
  for (int i = 0; i < content_cap; ++i)
    CHECK(list.push({.kind = DrawCommandKind::Rect}));
  CHECK(list.count == content_cap);
  CHECK(list.dropped_count == 0);
  CHECK(!list.push({.kind = DrawCommandKind::Rect})); // content saturated -> dropped
  CHECK(list.dropped_count == 1);
  // push_close() may use the reserved tail so every open clip/layer still
  // balances even on a saturated frame (INV4).
  for (int i = 0; i < UI_DRAW_COMMANDS_RESERVE; ++i)
    CHECK(list.push_close({.kind = DrawCommandKind::ClipPop}));
  CHECK(list.count == UI_MAX_DRAW_COMMANDS);
  CHECK(!list.push_close({.kind = DrawCommandKind::ClipPop})); // hard cap
  CHECK(list.dropped_count == 2);
  return true;
}

static bool test_text_arena() {
  static DrawCommandList list;
  list.reset();
  uint32_t off = 99;
  const char *a = "hello";
  CHECK(list.push_text(a, 5, &off));
  CHECK(off == 0 && list.text_len_used == 5);
  uint32_t off2 = 0;
  CHECK(list.push_text("hi", 2, &off2));
  CHECK(off2 == 5);
  // a full text arena drops the bytes (defined overflow), never fails the frame:
  list.reset();
  CHECK(list.text_len_used == 0 && list.dropped_count == 0);
  return true;
}

static bool test_stops_and_reset() {
  static DrawCommandList list;
  list.reset();
  GradientStop s[2] = {{0.f, {0, 0, 0, 255}}, {1.f, {255, 255, 255, 255}}};
  uint16_t off = 7;
  CHECK(list.push_stops(s, 2, &off));
  CHECK(off == 0 && list.grad_count == 2);
  list.reset();
  CHECK(list.count == 0 && list.grad_count == 0 && list.text_len_used == 0 &&
        list.dropped_count == 0);
  return true;
}

int main() {
  bool ok = test_designated_init() && test_push_overflow() && test_text_arena() &&
            test_stops_and_reset();
  if (!ok) {
    fprintf(stderr, "ui_draw_command_tests: FAIL\n");
    return 1;
  }
  printf("ui_draw_command_tests: OK (sizeof DrawCommand=%zu, DrawPayload=%zu, "
         "DrawCommandList=%zu, max_cmds=%d)\n",
         sizeof(DrawCommand), sizeof(DrawPayload), sizeof(DrawCommandList),
         UI_MAX_DRAW_COMMANDS);
  return 0;
}
