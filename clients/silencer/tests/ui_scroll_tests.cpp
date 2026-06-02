// SIL-111: the scroll-viewport foundation — the two engine seams the
// ScrollView primitive stands on:
//   1) overflow != Visible emits a ClipPush/ClipPop bracketing the node's
//      children in the draw IR (so a translated content track is scissored to
//      its window);
//   2) the wheel input channel dispatches to a node's on_wheel callback (the
//      runtime routes it to the hovered scrollable).
// The component's offset/clamp math runs on the live react runtime and is
// covered by the gallery screenshot; here we lock the SDL-free plumbing.

#include "ui/runtime/draw_command.h"
#include "ui/runtime/draw_command_builder.h"
#include "ui/runtime/flex_layout.h"
#include "ui/runtime/tree.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <stdio.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

namespace {

// A 200x100 root with one child box of the given overflow, itself wrapping a
// taller child (the would-be scroll content). Returns the viewport node id.
bool build(UiTree *tree, Overflow overflow, NodeId *viewport_out) {
  tree->reset();
  LayoutStyle root = {};
  root.width = Length::points(200.0f);
  root.height = Length::points(100.0f);

  tree->begin_frame(200.0f, 100.0f);
  tree->begin_keyed_node("Box", "root", root);

  LayoutStyle vp = {};
  vp.width = Length::points(200.0f);
  vp.height = Length::points(40.0f);
  vp.overflow = overflow;
  NodeId viewport = tree->begin_keyed_node("Box", "viewport", vp);

  LayoutStyle content = {};
  content.width = Length::points(200.0f);
  content.height = Length::points(400.0f); // taller than the 40px window
  tree->begin_keyed_node("Box", "content", content);
  tree->end_node();

  tree->end_node(); // viewport
  tree->end_node(); // root
  if (!tree->end_frame())
    return false;
  if (viewport_out)
    *viewport_out = viewport;
  return compute_flex_layout(make_yoga_flex_layout_adapter(), *tree,
                             {200.0f, 100.0f});
}

int count_kind(const DrawCommandList &list, DrawCommandKind kind) {
  int n = 0;
  for (int i = 0; i < list.count; ++i)
    if (list.commands[i].kind == kind)
      ++n;
  return n;
}

bool clip_brackets_overflow_children() {
  UiTree tree = {};
  NodeId viewport = 0;
  CHECK(build(&tree, Overflow::Hidden, &viewport));

  static DrawCommandList list;
  CHECK(build_draw_command_list(tree, &list, 0));
  CHECK(list.error_count == 0);

  // Exactly one push + one pop, and the push carries the viewport's window rect.
  CHECK(count_kind(list, DrawCommandKind::ClipPush) == 1);
  CHECK(count_kind(list, DrawCommandKind::ClipPop) == 1);

  int push_i = -1, pop_i = -1;
  for (int i = 0; i < list.count; ++i) {
    if (list.commands[i].kind == DrawCommandKind::ClipPush)
      push_i = i;
    if (list.commands[i].kind == DrawCommandKind::ClipPop)
      pop_i = i;
  }
  CHECK(push_i >= 0 && pop_i > push_i); // pop strictly after push (balanced)
  CHECK(list.commands[push_i].node_id == viewport);
  CHECK(list.commands[push_i].rect.h == 40.0f); // the 40px window, not 400px
  return true;
}

bool visible_emits_no_clip() {
  UiTree tree = {};
  CHECK(build(&tree, Overflow::Visible, nullptr));
  static DrawCommandList list;
  CHECK(build_draw_command_list(tree, &list, 0));
  CHECK(count_kind(list, DrawCommandKind::ClipPush) == 0);
  CHECK(count_kind(list, DrawCommandKind::ClipPop) == 0);
  return true;
}

bool wheel_dispatches_to_on_wheel() {
  UiTree tree = {};
  tree.reset();
  tree.begin_frame(100.0f, 100.0f);
  LayoutStyle s = {};
  s.width = Length::points(100.0f);
  s.height = Length::points(100.0f);
  NodeId id = tree.begin_keyed_node("Box", "scroller", s);

  static float seen_dy;
  static int fired;
  seen_dy = 0.0f;
  fired = 0;
  NodeMetadata meta = {};
  meta.interaction.focusable = true;
  meta.on_wheel = [](const WheelEvent &e) {
    seen_dy = e.dy;
    ++fired;
  };
  tree.set_metadata(id, meta);
  tree.end_node();
  CHECK(tree.end_frame());

  CHECK(tree.invoke_wheel(id, 0.0f, 3.0f));
  CHECK(fired == 1 && seen_dy == 3.0f);

  // A disabled node swallows the wheel (no dispatch).
  NodeMetadata off = meta;
  off.interaction.disabled = true;
  tree.set_metadata(id, off);
  CHECK(!tree.invoke_wheel(id, 0.0f, 5.0f));
  CHECK(fired == 1); // unchanged
  return true;
}

} // namespace

int main() {
  bool ok = clip_brackets_overflow_children() && visible_emits_no_clip() &&
            wheel_dispatches_to_on_wheel();
  if (!ok) {
    fprintf(stderr, "ui_scroll_tests: FAIL\n");
    return 1;
  }
  printf("ui_scroll_tests: OK (clip brackets overflow children; visible emits "
         "none; wheel -> on_wheel; disabled swallows)\n");
  return 0;
}
