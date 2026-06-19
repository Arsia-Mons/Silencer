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
#include "ui/runtime/focus.h"
#include "ui/runtime/tree.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <string.h>

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
  CHECK(list.dropped_count == 0);

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

// The layout measurement read-back (the channel ScrollView's auto-sizing rides
// on): after Yoga lays out a flex-grown viewport inside a bounded well, the
// viewport's measured height == the well's free space (not its content), and the
// taller content track measures its full natural extent. compute_measured_sizes
// records both, keyed by control id; anything without a control id is skipped.
bool measured_sizes_read_back_resolved_heights() {
  UiTree tree = {};
  tree.reset();

  LayoutStyle root = {};
  root.width = Length::points(200.0f);
  root.height = Length::points(100.0f);
  tree.begin_frame(200.0f, 100.0f);
  tree.begin_keyed_node("Box", "root", root);

  // The bounded well: 60px tall (the height the viewport must grow into).
  LayoutStyle well = {};
  well.width = Length::points(200.0f);
  well.height = Length::points(60.0f);
  tree.begin_keyed_node("Box", "well", well);

  // The flex-grown viewport (ScrollView's clip box): no height, grows to fill
  // the 60px well. Carries control_id "VP".
  LayoutStyle vp = {};
  vp.width = Length::points(200.0f);
  vp.overflow = Overflow::Hidden;
  vp.flex_grow = StyleFloat::points(1.0f);
  vp.flex_basis = Length::points(0.0f);
  vp.min_height = Length::points(0.0f);
  NodeId vp_id = tree.begin_keyed_node("Box", "viewport", vp);
  NodeMetadata vp_meta = {};
  vp_meta.control_id = "VP";
  tree.set_metadata(vp_id, vp_meta);

  // The absolute content track, taller than the viewport. Carries "VPContent".
  LayoutStyle content = {};
  content.position = PositionType::Absolute;
  content.position_inset.left = StyleValue::points(0.0f);
  content.position_inset.right = StyleValue::points(0.0f);
  content.height = Length::points(150.0f);
  NodeId content_id = tree.begin_keyed_node("Box", "content", content);
  NodeMetadata content_meta = {};
  content_meta.control_id = "VPContent";
  tree.set_metadata(content_id, content_meta);
  tree.end_node(); // content

  tree.end_node(); // viewport
  tree.end_node(); // well
  tree.end_node(); // root
  CHECK(tree.end_frame());
  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {200.0f, 100.0f}));

  MeasuredSizeRequest *req = new MeasuredSizeRequest();
  compute_measured_sizes(tree, req);

  auto find = [&](const char *cid, float *out) -> bool {
    for (int i = 0; i < req->count; ++i) {
      if (strcmp(req->sizes[i].control_id, cid) == 0) {
        *out = req->sizes[i].height;
        return true;
      }
    }
    return false;
  };

  float vp_h = 0.0f, content_h = 0.0f;
  CHECK(find("VP", &vp_h));
  CHECK(find("VPContent", &content_h));
  // The viewport grew to the well's bounded height (60), NOT the content's 150.
  CHECK(vp_h == 60.0f);
  // The content track measured its full natural extent.
  CHECK(content_h == 150.0f);
  // So the component derives a real overflow (content > viewport).
  CHECK(content_h > vp_h);

  // Nodes without a control id are not recorded (only VP + VPContent here).
  const int recorded = req->count;
  delete req;
  CHECK(recorded == 2);
  return true;
}

} // namespace

int main() {
  bool ok = clip_brackets_overflow_children() && visible_emits_no_clip() &&
            wheel_dispatches_to_on_wheel() &&
            measured_sizes_read_back_resolved_heights();
  if (!ok) {
    fprintf(stderr, "ui_scroll_tests: FAIL\n");
    return 1;
  }
  printf("ui_scroll_tests: OK (clip brackets overflow children; visible emits "
         "none; wheel -> on_wheel; disabled swallows; measured-size read-back "
         "records flex-grown viewport + content heights)\n");
  return 0;
}
