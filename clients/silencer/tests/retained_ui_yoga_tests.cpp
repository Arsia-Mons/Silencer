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

static bool snapshot(UiTree &tree, NodeId id, NodeSnapshot *out) {
  CHECK(tree.snapshot(id, out));
  return true;
}

static bool near(float actual, float expected) {
  return actual > expected - 0.01f && actual < expected + 0.01f;
}

static bool yoga_computes_column_gap_and_grow(void) {
  UiTree tree;

  LayoutStyle panel = {};
  panel.width = Length::points(300.0f);
  panel.height = Length::points(200.0f);
  panel.direction = FlexDirection::Column;
  panel.gap = 10.0f;

  LayoutStyle fixed = {};
  fixed.height = Length::points(50.0f);

  LayoutStyle growing = {};
  growing.height = Length::grow(1.0f);

  tree.begin_frame(300.0f, 200.0f);
  NodeId panel_id = tree.begin_keyed_node("Panel", "root", panel);
  NodeId fixed_id = tree.begin_keyed_node("Child", "fixed", fixed);
  CHECK(tree.end_node());
  NodeId growing_id = tree.begin_keyed_node("Child", "growing", growing);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {300.0f, 200.0f}));

  NodeSnapshot panel_snapshot = {};
  NodeSnapshot fixed_snapshot = {};
  NodeSnapshot growing_snapshot = {};
  CHECK(snapshot(tree, panel_id, &panel_snapshot));
  CHECK(snapshot(tree, fixed_id, &fixed_snapshot));
  CHECK(snapshot(tree, growing_id, &growing_snapshot));

  CHECK(panel_snapshot.layout.width == 300.0f);
  CHECK(panel_snapshot.layout.height == 200.0f);
  CHECK(fixed_snapshot.layout.y == 0.0f);
  CHECK(fixed_snapshot.layout.height == 50.0f);
  CHECK(growing_snapshot.layout.y == 60.0f);
  CHECK(growing_snapshot.layout.height == 140.0f);
  return true;
}

static bool yoga_computes_row_percent_and_grow(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.width = Length::points(400.0f);
  row.height = Length::points(100.0f);
  row.direction = FlexDirection::Row;

  LayoutStyle half = {};
  half.width = Length::percent(50.0f);

  LayoutStyle grow = {};
  grow.width = Length::grow(1.0f);

  tree.begin_frame(400.0f, 100.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "root", row);
  NodeId half_id = tree.begin_keyed_node("Child", "half", half);
  CHECK(tree.end_node());
  NodeId grow_id = tree.begin_keyed_node("Child", "grow", grow);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {400.0f, 100.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot half_snapshot = {};
  NodeSnapshot grow_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, half_id, &half_snapshot));
  CHECK(snapshot(tree, grow_id, &grow_snapshot));

  CHECK(row_snapshot.layout.width == 400.0f);
  CHECK(row_snapshot.layout.height == 100.0f);
  CHECK(half_snapshot.layout.x == 0.0f);
  CHECK(half_snapshot.layout.width == 200.0f);
  CHECK(grow_snapshot.layout.x == 200.0f);
  CHECK(grow_snapshot.layout.width == 200.0f);
  return true;
}

static bool yoga_applies_padding_to_child_layout(void) {
  UiTree tree;

  LayoutStyle panel = {};
  panel.width = Length::points(120.0f);
  panel.height = Length::points(80.0f);
  panel.padding = {8.0f, 4.0f, 6.0f, 2.0f};

  LayoutStyle child = {};
  child.width = Length::points(20.0f);
  child.height = Length::points(10.0f);

  tree.begin_frame(120.0f, 80.0f);
  NodeId panel_id = tree.begin_keyed_node("Panel", "padded", panel);
  NodeId child_id = tree.begin_keyed_node("Child", "content", child);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {120.0f, 80.0f}));

  NodeSnapshot panel_snapshot = {};
  NodeSnapshot child_snapshot = {};
  CHECK(snapshot(tree, panel_id, &panel_snapshot));
  CHECK(snapshot(tree, child_id, &child_snapshot));

  CHECK(panel_snapshot.layout.width == 120.0f);
  CHECK(panel_snapshot.layout.height == 80.0f);
  CHECK(child_snapshot.layout.x == 8.0f);
  CHECK(child_snapshot.layout.y == 6.0f);
  CHECK(child_snapshot.layout.width == 20.0f);
  CHECK(child_snapshot.layout.height == 10.0f);
  return true;
}

static bool yoga_applies_margins_min_max_and_absolute_positioning(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.width = Length::points(220.0f);
  row.height = Length::points(90.0f);
  row.direction = FlexDirection::Row;
  row.align_items = AlignItems::Start;

  LayoutStyle first = {};
  first.width = Length::points(40.0f);
  first.height = Length::points(20.0f);
  first.margin = {10.0f, 5.0f, 0.0f, 0.0f};

  LayoutStyle clamped = {};
  clamped.width = Length::points(200.0f);
  clamped.height = Length::points(10.0f);
  clamped.max_width = Length::points(90.0f);
  clamped.min_height = Length::points(24.0f);

  LayoutStyle absolute = {};
  absolute.position = PositionType::Absolute;
  absolute.position_inset = {120.0f, 0.0f, 35.0f, 0.0f};
  absolute.width = Length::points(20.0f);
  absolute.height = Length::points(15.0f);

  tree.begin_frame(220.0f, 90.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "root", row);
  NodeId first_id = tree.begin_keyed_node("Child", "first", first);
  CHECK(tree.end_node());
  NodeId clamped_id = tree.begin_keyed_node("Child", "clamped", clamped);
  CHECK(tree.end_node());
  NodeId absolute_id = tree.begin_keyed_node("Child", "absolute", absolute);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {220.0f, 90.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot first_snapshot = {};
  NodeSnapshot clamped_snapshot = {};
  NodeSnapshot absolute_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, first_id, &first_snapshot));
  CHECK(snapshot(tree, clamped_id, &clamped_snapshot));
  CHECK(snapshot(tree, absolute_id, &absolute_snapshot));

  CHECK(row_snapshot.layout.width == 220.0f);
  CHECK(first_snapshot.layout.x == 10.0f);
  CHECK(first_snapshot.layout.width == 40.0f);
  CHECK(clamped_snapshot.layout.x == 55.0f);
  CHECK(clamped_snapshot.layout.width == 90.0f);
  CHECK(clamped_snapshot.layout.height == 24.0f);
  CHECK(absolute_snapshot.layout.x == 120.0f);
  CHECK(absolute_snapshot.layout.y == 35.0f);
  CHECK(absolute_snapshot.layout.width == 20.0f);
  return true;
}

static bool yoga_applies_wrap_and_row_gap(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.width = Length::points(100.0f);
  row.height = Length::points(80.0f);
  row.direction = FlexDirection::Row;
  row.wrap = FlexWrap::Wrap;
  row.align_items = AlignItems::Start;
  row.align_content = AlignItems::Start;
  row.row_gap = 6.0f;

  LayoutStyle child = {};
  child.width = Length::points(60.0f);
  child.height = Length::points(20.0f);

  tree.begin_frame(100.0f, 80.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "wrap", row);
  NodeId first_id = tree.begin_keyed_node("Child", "first", child);
  CHECK(tree.end_node());
  NodeId second_id = tree.begin_keyed_node("Child", "second", child);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {100.0f, 80.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot first_snapshot = {};
  NodeSnapshot second_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, first_id, &first_snapshot));
  CHECK(snapshot(tree, second_id, &second_snapshot));

  CHECK(row_snapshot.layout.width == 100.0f);
  CHECK(first_snapshot.layout.x == 0.0f);
  CHECK(first_snapshot.layout.y == 0.0f);
  CHECK(second_snapshot.layout.x == 0.0f);
  CHECK(second_snapshot.layout.y == 26.0f);
  return true;
}

static bool yoga_applies_percent_auto_edges_gap_and_layout_readback(void) {
  UiTree tree;

  LayoutStyle panel = {};
  panel.width = Length::points(200.0f);
  panel.height = Length::points(100.0f);
  panel.direction = FlexDirection::Row;
  panel.align_items = AlignItems::Start;
  panel.padding.left = StyleValue::percent(10.0f);
  panel.padding.top = StyleValue::points(5.0f);
  panel.border_widths.left = StyleValue::points(2.0f);
  panel.border_widths.top = StyleValue::points(3.0f);
  panel.gap = StyleValue::percent(10.0f);

  LayoutStyle child = {};
  child.width = Length::points(20.0f);
  child.height = Length::points(10.0f);

  tree.begin_frame(200.0f, 100.0f);
  NodeId panel_id = tree.begin_keyed_node("Panel", "percent-edges", panel);
  NodeId first_id = tree.begin_keyed_node("Child", "first", child);
  CHECK(tree.end_node());
  NodeId second_id = tree.begin_keyed_node("Child", "second", child);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {200.0f, 100.0f}));

  NodeSnapshot panel_snapshot = {};
  NodeSnapshot first_snapshot = {};
  NodeSnapshot second_snapshot = {};
  CHECK(snapshot(tree, panel_id, &panel_snapshot));
  CHECK(snapshot(tree, first_id, &first_snapshot));
  CHECK(snapshot(tree, second_id, &second_snapshot));

  CHECK(near(panel_snapshot.layout.padding.left, 20.0f));
  CHECK(near(panel_snapshot.layout.border.left, 2.0f));
  CHECK(near(panel_snapshot.layout.border.top, 3.0f));
  CHECK(near(first_snapshot.layout.x, 22.0f));
  CHECK(near(first_snapshot.layout.y, 8.0f));
  CHECK(near(second_snapshot.layout.x, 60.0f));
  return true;
}

static bool yoga_applies_auto_margin_and_percent_position(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.width = Length::points(100.0f);
  row.height = Length::points(40.0f);
  row.direction = FlexDirection::Row;
  row.align_items = AlignItems::Start;

  LayoutStyle centered = {};
  centered.width = Length::points(20.0f);
  centered.height = Length::points(10.0f);
  centered.margin.left = StyleValue::auto_value();
  centered.margin.right = StyleValue::auto_value();

  LayoutStyle absolute = {};
  absolute.position = PositionType::Absolute;
  absolute.position_inset.left = StyleValue::percent(50.0f);
  absolute.position_inset.top = StyleValue::percent(25.0f);
  absolute.position_inset.right = StyleValue::auto_value();
  absolute.position_inset.bottom = StyleValue::auto_value();
  absolute.width = Length::points(10.0f);
  absolute.height = Length::points(10.0f);

  tree.begin_frame(100.0f, 40.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "root", row);
  NodeId centered_id = tree.begin_keyed_node("Child", "centered", centered);
  CHECK(tree.end_node());
  NodeId absolute_id = tree.begin_keyed_node("Child", "absolute", absolute);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {100.0f, 40.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot centered_snapshot = {};
  NodeSnapshot absolute_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, centered_id, &centered_snapshot));
  CHECK(snapshot(tree, absolute_id, &absolute_snapshot));

  CHECK(row_snapshot.layout.direction == LayoutDirection::Ltr);
  CHECK(near(centered_snapshot.layout.x, 40.0f));
  CHECK(near(absolute_snapshot.layout.x, 50.0f));
  CHECK(near(absolute_snapshot.layout.y, 10.0f));
  return true;
}

static bool yoga_applies_direction_box_sizing_flex_and_aspect_ratio(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.layout_direction = LayoutDirection::Rtl;
  row.width = Length::points(120.0f);
  row.height = Length::points(40.0f);
  row.direction = FlexDirection::Row;
  row.align_items = AlignItems::Start;

  LayoutStyle fixed = {};
  fixed.width = Length::points(40.0f);
  fixed.height = Length::points(20.0f);

  LayoutStyle flexible = {};
  flexible.flex = 1.0f;
  flexible.height = Length::points(20.0f);

  LayoutStyle aspect = {};
  aspect.width = Length::points(30.0f);
  aspect.aspect_ratio = 2.0f;

  LayoutStyle content_box = {};
  content_box.box_sizing = BoxSizing::ContentBox;
  content_box.width = Length::points(100.0f);
  content_box.height = Length::points(20.0f);
  content_box.padding.left = StyleValue::points(10.0f);
  content_box.padding.right = StyleValue::points(10.0f);

  tree.begin_frame(240.0f, 160.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "rtl", row);
  NodeId fixed_id = tree.begin_keyed_node("Child", "fixed", fixed);
  CHECK(tree.end_node());
  NodeId flexible_id = tree.begin_keyed_node("Child", "flexible", flexible);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  NodeId aspect_id = tree.begin_keyed_node("Child", "aspect", aspect);
  CHECK(tree.end_node());
  NodeId content_box_id =
      tree.begin_keyed_node("Child", "content-box", content_box);
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {240.0f, 160.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot fixed_snapshot = {};
  NodeSnapshot flexible_snapshot = {};
  NodeSnapshot aspect_snapshot = {};
  NodeSnapshot content_box_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, fixed_id, &fixed_snapshot));
  CHECK(snapshot(tree, flexible_id, &flexible_snapshot));
  CHECK(snapshot(tree, aspect_id, &aspect_snapshot));
  CHECK(snapshot(tree, content_box_id, &content_box_snapshot));

  CHECK(row_snapshot.layout.direction == LayoutDirection::Rtl);
  CHECK(near(fixed_snapshot.layout.x, 80.0f));
  CHECK(near(flexible_snapshot.layout.width, 80.0f));
  CHECK(near(aspect_snapshot.layout.height, 15.0f));
  CHECK(near(content_box_snapshot.layout.width, 120.0f));
  return true;
}

static bool yoga_applies_flex_shrink_and_reports_overflow(void) {
  UiTree tree;

  LayoutStyle shrink_row = {};
  shrink_row.width = Length::points(100.0f);
  shrink_row.height = Length::points(30.0f);
  shrink_row.direction = FlexDirection::Row;
  shrink_row.align_items = AlignItems::Start;

  LayoutStyle shrinking = {};
  shrinking.width = Length::points(80.0f);
  shrinking.height = Length::points(10.0f);
  shrinking.flex_shrink = 1.0f;

  LayoutStyle overflow_row = {};
  overflow_row.width = Length::points(50.0f);
  overflow_row.height = Length::points(30.0f);
  overflow_row.direction = FlexDirection::Row;
  overflow_row.align_items = AlignItems::Start;

  LayoutStyle wide = {};
  wide.width = Length::points(80.0f);
  wide.height = Length::points(10.0f);
  wide.flex_shrink = 0.0f;

  tree.begin_frame(160.0f, 120.0f);
  CHECK(tree.begin_keyed_node("Row", "shrink", shrink_row) != 0);
  NodeId first_id = tree.begin_keyed_node("Child", "first", shrinking);
  CHECK(tree.end_node());
  NodeId second_id = tree.begin_keyed_node("Child", "second", shrinking);
  CHECK(tree.end_node());
  CHECK(tree.end_node());

  NodeId overflow_row_id =
      tree.begin_keyed_node("Row", "overflow", overflow_row);
  NodeId wide_id = tree.begin_keyed_node("Child", "wide", wide);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {160.0f, 120.0f}));

  NodeSnapshot first_snapshot = {};
  NodeSnapshot second_snapshot = {};
  NodeSnapshot overflow_row_snapshot = {};
  NodeSnapshot wide_snapshot = {};
  CHECK(snapshot(tree, first_id, &first_snapshot));
  CHECK(snapshot(tree, second_id, &second_snapshot));
  CHECK(snapshot(tree, overflow_row_id, &overflow_row_snapshot));
  CHECK(snapshot(tree, wide_id, &wide_snapshot));

  CHECK(near(first_snapshot.layout.width, 50.0f));
  CHECK(near(second_snapshot.layout.width, 50.0f));
  CHECK(overflow_row_snapshot.layout.had_overflow);
  CHECK(near(wide_snapshot.layout.width, 80.0f));
  return true;
}

static float return_baseline(BaselineInput input, void *user) {
  (void)input;
  return user ? *static_cast<float *>(user) : 0.0f;
}

static bool yoga_uses_retained_baseline_function_and_node_flags(void) {
  UiTree tree;
  float low_baseline = 10.0f;
  float high_baseline = 20.0f;

  LayoutStyle row = {};
  row.width = Length::points(100.0f);
  row.height = Length::points(40.0f);
  row.direction = FlexDirection::Row;
  row.align_items = AlignItems::Baseline;

  LayoutStyle low = {};
  low.width = Length::points(20.0f);
  low.height = Length::points(30.0f);
  low.node_type = LayoutNodeType::Text;

  LayoutStyle high = {};
  high.width = Length::points(20.0f);
  high.height = Length::points(20.0f);
  high.is_reference_baseline = true;
  high.always_forms_containing_block = true;

  tree.begin_frame(100.0f, 40.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "baseline", row);
  NodeId low_id = tree.begin_keyed_node("Child", "low", low);
  CHECK(tree.set_baseline(low_id, return_baseline, &low_baseline));
  CHECK(tree.end_node());
  NodeId high_id = tree.begin_keyed_node("Child", "high", high);
  CHECK(tree.set_baseline(high_id, return_baseline, &high_baseline));
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {100.0f, 40.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot low_snapshot = {};
  NodeSnapshot high_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, low_id, &low_snapshot));
  CHECK(snapshot(tree, high_id, &high_snapshot));

  CHECK(low_snapshot.has_baseline);
  CHECK(high_snapshot.has_baseline);
  CHECK(low_snapshot.style.node_type == LayoutNodeType::Text);
  CHECK(high_snapshot.style.is_reference_baseline);
  CHECK(high_snapshot.style.always_forms_containing_block);
  CHECK(near(row_snapshot.layout.height, 40.0f));
  CHECK(near(low_snapshot.layout.y, 10.0f));
  CHECK(near(high_snapshot.layout.y, 0.0f));
  return true;
}

static bool yoga_adapter_accepts_layout_config(void) {
  UiTree tree;

  LayoutStyle row = {};
  row.width = Length::points(100.0f);
  row.height = Length::points(40.0f);
  row.direction = FlexDirection::Row;
  row.align_items = AlignItems::Start;

  LayoutStyle child = {};
  child.width = Length::points(20.0f);
  child.height = Length::points(10.0f);

  tree.begin_frame(100.0f, 40.0f);
  NodeId row_id = tree.begin_keyed_node("Row", "configured", row);
  NodeId child_id = tree.begin_keyed_node("Child", "child", child);
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  YogaLayoutConfig config = {};
  config.owner_direction = LayoutDirection::Rtl;
  config.point_scale_factor = 0.0f;
  config.has_errata = true;
  config.errata = YogaErrata::None;
  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(&config), tree,
                            {100.0f, 40.0f}));

  NodeSnapshot row_snapshot = {};
  NodeSnapshot child_snapshot = {};
  CHECK(snapshot(tree, row_id, &row_snapshot));
  CHECK(snapshot(tree, child_id, &child_snapshot));

  CHECK(row_snapshot.layout.direction == LayoutDirection::Rtl);
  CHECK(near(child_snapshot.layout.x, 80.0f));
  return true;
}

struct MeasureProbe {
  int count = 0;
  MeasureInput input = {};
};

static Size measure_text_node(MeasureInput input, void *user) {
  MeasureProbe *probe = static_cast<MeasureProbe *>(user);
  if (probe) {
    probe->count += 1;
    probe->input = input;
  }
  return {72.0f, 18.0f};
}

static bool yoga_uses_retained_measure_function(void) {
  UiTree tree;
  MeasureProbe probe = {};

  LayoutStyle panel = {};
  panel.width = Length::points(160.0f);
  panel.height = Length::points(80.0f);
  panel.align_items = AlignItems::Start;

  tree.begin_frame(160.0f, 80.0f);
  NodeId panel_id = tree.begin_keyed_node("Panel", "text-container", panel);
  NodeId text_id = tree.begin_keyed_node("Text", "label");
  CHECK(tree.set_measure(text_id, measure_text_node, &probe));
  CHECK(tree.end_node());
  CHECK(tree.end_node());
  CHECK(tree.end_frame());

  CHECK(compute_flex_layout(make_yoga_flex_layout_adapter(), tree,
                            {160.0f, 80.0f}));

  NodeSnapshot panel_snapshot = {};
  NodeSnapshot text_snapshot = {};
  CHECK(snapshot(tree, panel_id, &panel_snapshot));
  CHECK(snapshot(tree, text_id, &text_snapshot));

  CHECK(probe.count > 0);
  CHECK(panel_snapshot.layout.width == 160.0f);
  CHECK(text_snapshot.layout.width == 72.0f);
  CHECK(text_snapshot.layout.height == 18.0f);
  return true;
}

int main(void) {
  if (!yoga_computes_column_gap_and_grow())
    return 1;
  if (!yoga_computes_row_percent_and_grow())
    return 1;
  if (!yoga_applies_padding_to_child_layout())
    return 1;
  if (!yoga_applies_margins_min_max_and_absolute_positioning())
    return 1;
  if (!yoga_applies_wrap_and_row_gap())
    return 1;
  if (!yoga_applies_percent_auto_edges_gap_and_layout_readback())
    return 1;
  if (!yoga_applies_auto_margin_and_percent_position())
    return 1;
  if (!yoga_applies_direction_box_sizing_flex_and_aspect_ratio())
    return 1;
  if (!yoga_applies_flex_shrink_and_reports_overflow())
    return 1;
  if (!yoga_uses_retained_baseline_function_and_node_flags())
    return 1;
  if (!yoga_adapter_accepts_layout_config())
    return 1;
  if (!yoga_uses_retained_measure_function())
    return 1;
  return 0;
}
