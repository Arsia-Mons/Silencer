#include "ui/runtime/flex_layout.h"
#include "ui/runtime/tree.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

static bool snapshot(UiTree &tree, NodeId id, NodeSnapshot *out) {
  CHECK(tree.snapshot(id, out));
  return true;
}

static bool positional_children_keep_identity_by_position(void) {
  UiTree tree;

  tree.begin_frame(640.0f, 480.0f);
  NodeId first = tree.begin_node("Item");
  CHECK(first != 0);
  CHECK(tree.end_node());
  NodeId second = tree.begin_node("Item");
  CHECK(second != 0);
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  tree.begin_frame(640.0f, 480.0f);
  NodeId first_again = tree.begin_node("Item");
  CHECK(tree.end_node());
  NodeId second_again = tree.begin_node("Item");
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(first_again == first);
  CHECK(second_again == second);
  CHECK(first != second);
  CHECK(tree.unmounted_count() == 0);
  return true;
}

static bool keyed_children_keep_identity_across_reorder(void) {
  UiTree tree;

  tree.begin_frame(640.0f, 480.0f);
  NodeId alpha = tree.begin_keyed_node("Row", "alpha");
  CHECK(tree.end_node());
  NodeId beta = tree.begin_keyed_node("Row", "beta");
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  tree.begin_frame(640.0f, 480.0f);
  NodeId beta_again = tree.begin_keyed_node("Row", "beta");
  CHECK(tree.end_node());
  NodeId alpha_again = tree.begin_keyed_node("Row", "alpha");
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(alpha_again == alpha);
  CHECK(beta_again == beta);
  CHECK(alpha != beta);
  CHECK(tree.unmounted_count() == 0);
  return true;
}

static int g_cleanup_count = 0;

static void count_cleanup(void *user) {
  int *count = static_cast<int *>(user);
  if (count)
    *count += 1;
  g_cleanup_count += 1;
}

static bool unmounted_nodes_run_cleanup_once(void) {
  UiTree tree;
  g_cleanup_count = 0;
  int local_cleanup_count = 0;

  tree.begin_frame(640.0f, 480.0f);
  NodeId kept = tree.begin_keyed_node("Panel", "kept");
  CHECK(tree.end_node());
  NodeId removed = tree.begin_keyed_node("Panel", "removed");
  CHECK(tree.set_cleanup(removed, count_cleanup, &local_cleanup_count));
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  tree.begin_frame(640.0f, 480.0f);
  NodeId kept_again = tree.begin_keyed_node("Panel", "kept");
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(kept_again == kept);
  CHECK(!tree.contains(removed));
  CHECK(tree.unmounted_count() == 1);
  CHECK(tree.unmounted_at(0) == removed);
  CHECK(local_cleanup_count == 1);
  CHECK(g_cleanup_count == 1);

  tree.begin_frame(640.0f, 480.0f);
  kept_again = tree.begin_keyed_node("Panel", "kept");
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(kept_again == kept);
  CHECK(local_cleanup_count == 1);
  CHECK(g_cleanup_count == 1);
  return true;
}

static bool style_layout_and_child_count_are_retained(void) {
  UiTree tree;
  LayoutStyle panel_style = {};
  panel_style.width = Length::points(320.0f);
  panel_style.height = Length::grow();
  panel_style.direction = FlexDirection::Row;
  panel_style.gap = 12.0f;
  panel_style.padding = {4.0f, 5.0f, 6.0f, 7.0f};

  tree.begin_frame(800.0f, 600.0f);
  NodeId panel = tree.begin_keyed_node("Panel", "loadout", panel_style);
  NodeId label = tree.begin_node("Text");
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.set_layout(panel, {10.0f, 20.0f, 300.0f, 200.0f}));
  CHECK(tree.end_frame());

  NodeSnapshot panel_snapshot = {};
  CHECK(snapshot(tree, panel, &panel_snapshot));
  CHECK(panel_snapshot.id == panel);
  CHECK(panel_snapshot.parent_id == tree.root_id());
  CHECK(strcmp(panel_snapshot.type, "Panel") == 0);
  CHECK(strcmp(panel_snapshot.key, "loadout") == 0);
  CHECK(panel_snapshot.child_count == 1);
  CHECK(panel_snapshot.style.width.kind == LengthKind::Points);
  CHECK(panel_snapshot.style.width.value == 320.0f);
  CHECK(panel_snapshot.style.height.kind == LengthKind::Grow);
  CHECK(panel_snapshot.style.direction == FlexDirection::Row);
  CHECK(panel_snapshot.style.gap == 12.0f);
  CHECK(panel_snapshot.style.padding.left == 4.0f);
  CHECK(panel_snapshot.layout.x == 10.0f);
  CHECK(panel_snapshot.layout.height == 200.0f);

  NodeSnapshot label_snapshot = {};
  CHECK(snapshot(tree, label, &label_snapshot));
  CHECK(label_snapshot.parent_id == panel);
  return true;
}

struct AdapterProbe {
  int call_count = 0;
  NodeId observed_root = 0;
  LayoutViewport observed_viewport = {};
};

static bool adapter_probe(UiTree &tree, NodeId root_id, LayoutViewport viewport,
                          void *user) {
  AdapterProbe *probe = static_cast<AdapterProbe *>(user);
  if (!probe)
    return false;
  probe->call_count += 1;
  probe->observed_root = root_id;
  probe->observed_viewport = viewport;
  return tree.set_layout(root_id,
                         {0.0f, 0.0f, viewport.width, viewport.height});
}

static bool flex_layout_adapter_delegates_to_selected_engine(void) {
  UiTree tree;
  tree.begin_frame(1024.0f, 768.0f);
  CHECK(tree.end_frame());

  AdapterProbe probe = {};
  FlexLayoutAdapter adapter = {
      .name = "probe",
      .compute = adapter_probe,
      .user = &probe,
  };

  CHECK(compute_flex_layout(adapter, tree, {1280.0f, 720.0f}));
  CHECK(probe.call_count == 1);
  CHECK(probe.observed_root == tree.root_id());
  CHECK(probe.observed_viewport.width == 1280.0f);
  CHECK(probe.observed_viewport.height == 720.0f);

  NodeSnapshot root = {};
  CHECK(snapshot(tree, tree.root_id(), &root));
  CHECK(root.layout.width == 1280.0f);
  CHECK(root.layout.height == 720.0f);
  return true;
}

int main(void) {
  if (!positional_children_keep_identity_by_position())
    return 1;
  if (!keyed_children_keep_identity_across_reorder())
    return 1;
  if (!unmounted_nodes_run_cleanup_once())
    return 1;
  if (!style_layout_and_child_count_are_retained())
    return 1;
  if (!flex_layout_adapter_delegates_to_selected_engine())
    return 1;
  return 0;
}
