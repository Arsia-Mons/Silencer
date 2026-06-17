#include "yoga_flex_layout.h"

#ifndef SILENCER_HEADLESS

#include <yoga/Yoga.h>

namespace ui {

namespace {

struct NodeContext {
  UiTree *tree = nullptr;
  NodeId id = 0;
};

struct BuildContext {
  std::array<NodeContext, UI_RETAINED_MAX_NODES> node_contexts = {};
  int node_context_count = 0;
  YGConfigRef config = nullptr;
};

MeasureMode map_measure_mode(YGMeasureMode mode) {
  switch (mode) {
  case YGMeasureModeUndefined:
    return MeasureMode::Undefined;
  case YGMeasureModeExactly:
    return MeasureMode::Exactly;
  case YGMeasureModeAtMost:
    return MeasureMode::AtMost;
  }
  return MeasureMode::Undefined;
}

LayoutDirection map_layout_direction(YGDirection direction) {
  switch (direction) {
  case YGDirectionInherit:
    return LayoutDirection::Inherit;
  case YGDirectionLTR:
    return LayoutDirection::Ltr;
  case YGDirectionRTL:
    return LayoutDirection::Rtl;
  }
  return LayoutDirection::Inherit;
}

YGDirection map_layout_direction(LayoutDirection direction) {
  switch (direction) {
  case LayoutDirection::Inherit:
    return YGDirectionInherit;
  case LayoutDirection::Ltr:
    return YGDirectionLTR;
  case LayoutDirection::Rtl:
    return YGDirectionRTL;
  }
  return YGDirectionInherit;
}

YGDirection map_owner_direction(LayoutDirection direction) {
  if (direction == LayoutDirection::Rtl)
    return YGDirectionRTL;
  return YGDirectionLTR;
}

YGSize measure_yoga_node(YGNodeConstRef node, float width,
                         YGMeasureMode width_mode, float height,
                         YGMeasureMode height_mode) {
  const NodeContext *context =
      static_cast<const NodeContext *>(YGNodeGetContext(node));
  if (!context || !context->tree)
    return {0.0f, 0.0f};

  Size measured = {};
  if (!context->tree->measure(context->id,
                              {
                                  .width = width,
                                  .height = height,
                                  .width_mode = map_measure_mode(width_mode),
                                  .height_mode = map_measure_mode(height_mode),
                              },
                              &measured)) {
    return {0.0f, 0.0f};
  }
  return {measured.width, measured.height};
}

float baseline_yoga_node(YGNodeConstRef node, float width, float height) {
  const NodeContext *context =
      static_cast<const NodeContext *>(YGNodeGetContext(node));
  if (!context || !context->tree)
    return height;

  float baseline = height;
  if (!context->tree->baseline(context->id,
                               {
                                   .width = width,
                                   .height = height,
                               },
                               &baseline)) {
    return height;
  }
  return baseline;
}

YGFlexDirection map_direction(FlexDirection direction) {
  switch (direction) {
  case FlexDirection::Row:
    return YGFlexDirectionRow;
  case FlexDirection::RowReverse:
    return YGFlexDirectionRowReverse;
  case FlexDirection::Column:
    return YGFlexDirectionColumn;
  case FlexDirection::ColumnReverse:
    return YGFlexDirectionColumnReverse;
  }
  return YGFlexDirectionColumn;
}

YGAlign map_align(AlignItems align) {
  switch (align) {
  case AlignItems::Auto:
    return YGAlignAuto;
  case AlignItems::Stretch:
    return YGAlignStretch;
  case AlignItems::Start:
    return YGAlignFlexStart;
  case AlignItems::Center:
    return YGAlignCenter;
  case AlignItems::End:
    return YGAlignFlexEnd;
  case AlignItems::Baseline:
    return YGAlignBaseline;
  case AlignItems::SpaceBetween:
    return YGAlignSpaceBetween;
  case AlignItems::SpaceAround:
    return YGAlignSpaceAround;
  case AlignItems::SpaceEvenly:
    return YGAlignSpaceEvenly;
  }
  return YGAlignStretch;
}

YGJustify map_justify(JustifyContent justify) {
  switch (justify) {
  case JustifyContent::Start:
    return YGJustifyFlexStart;
  case JustifyContent::Center:
    return YGJustifyCenter;
  case JustifyContent::End:
    return YGJustifyFlexEnd;
  case JustifyContent::SpaceBetween:
    return YGJustifySpaceBetween;
  case JustifyContent::SpaceAround:
    return YGJustifySpaceAround;
  case JustifyContent::SpaceEvenly:
    return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

YGWrap map_wrap(FlexWrap wrap) {
  switch (wrap) {
  case FlexWrap::NoWrap:
    return YGWrapNoWrap;
  case FlexWrap::Wrap:
    return YGWrapWrap;
  case FlexWrap::WrapReverse:
    return YGWrapWrapReverse;
  }
  return YGWrapNoWrap;
}

YGPositionType map_position(PositionType position) {
  switch (position) {
  case PositionType::Static:
    return YGPositionTypeStatic;
  case PositionType::Relative:
    return YGPositionTypeRelative;
  case PositionType::Absolute:
    return YGPositionTypeAbsolute;
  }
  return YGPositionTypeRelative;
}

YGOverflow map_overflow(Overflow overflow) {
  switch (overflow) {
  case Overflow::Visible:
    return YGOverflowVisible;
  case Overflow::Hidden:
    return YGOverflowHidden;
  case Overflow::Scroll:
    return YGOverflowScroll;
  }
  return YGOverflowVisible;
}

YGDisplay map_display(Display display) {
  switch (display) {
  case Display::Flex:
    return YGDisplayFlex;
  case Display::None:
    return YGDisplayNone;
  case Display::Contents:
    return YGDisplayContents;
  }
  return YGDisplayFlex;
}

YGBoxSizing map_box_sizing(BoxSizing sizing) {
  switch (sizing) {
  case BoxSizing::BorderBox:
    return YGBoxSizingBorderBox;
  case BoxSizing::ContentBox:
    return YGBoxSizingContentBox;
  }
  return YGBoxSizingBorderBox;
}

YGNodeType map_node_type(LayoutNodeType type) {
  switch (type) {
  case LayoutNodeType::Default:
    return YGNodeTypeDefault;
  case LayoutNodeType::Text:
    return YGNodeTypeText;
  }
  return YGNodeTypeDefault;
}

YGErrata map_errata(YogaErrata errata) {
  return static_cast<YGErrata>(static_cast<uint32_t>(errata));
}

void set_dimension(YGNodeRef node, Length length,
                   void (*set_points)(YGNodeRef, float),
                   void (*set_percent)(YGNodeRef, float),
                   void (*set_auto)(YGNodeRef)) {
  switch (length.kind) {
  case LengthKind::Undefined:
    break;
  case LengthKind::Auto:
    set_auto(node);
    break;
  case LengthKind::Points:
    set_points(node, length.value);
    break;
  case LengthKind::Percent:
    set_percent(node, length.value);
    break;
  case LengthKind::Grow:
    set_auto(node);
    YGNodeStyleSetFlexGrow(node, length.value);
    break;
  }
}

void set_optional_length(YGNodeRef node, Length length,
                         void (*set_points)(YGNodeRef, float),
                         void (*set_percent)(YGNodeRef, float)) {
  switch (length.kind) {
  case LengthKind::Undefined:
    break;
  case LengthKind::Auto:
    break;
  case LengthKind::Points:
    set_points(node, length.value);
    break;
  case LengthKind::Percent:
    set_percent(node, length.value);
    break;
  case LengthKind::Grow:
    YGNodeStyleSetFlexGrow(node, length.value);
    break;
  }
}

void set_flex_basis(YGNodeRef node, Length length) {
  switch (length.kind) {
  case LengthKind::Undefined:
    break;
  case LengthKind::Auto:
    YGNodeStyleSetFlexBasisAuto(node);
    break;
  case LengthKind::Points:
    YGNodeStyleSetFlexBasis(node, length.value);
    break;
  case LengthKind::Percent:
    YGNodeStyleSetFlexBasisPercent(node, length.value);
    break;
  case LengthKind::Grow:
    YGNodeStyleSetFlexBasisAuto(node);
    YGNodeStyleSetFlexGrow(node, length.value);
    break;
  }
}

using SetEdgeValueFn = void (*)(YGNodeRef, YGEdge, float);
using SetEdgeAutoFn = void (*)(YGNodeRef, YGEdge);

void set_edge_value(YGNodeRef node, YGEdge edge, StyleValue value,
                    SetEdgeValueFn set_points, SetEdgeValueFn set_percent,
                    SetEdgeAutoFn set_auto) {
  switch (value.kind) {
  case StyleValueKind::Undefined:
    break;
  case StyleValueKind::Points:
    set_points(node, edge, value.value);
    break;
  case StyleValueKind::Percent:
    if (set_percent)
      set_percent(node, edge, value.value);
    break;
  case StyleValueKind::Auto:
    if (set_auto)
      set_auto(node, edge);
    break;
  }
}

void set_edges(YGNodeRef node, const EdgeSizes &edges,
               SetEdgeValueFn set_points, SetEdgeValueFn set_percent,
               SetEdgeAutoFn set_auto) {
  set_edge_value(node, YGEdgeAll, edges.all, set_points, set_percent, set_auto);
  set_edge_value(node, YGEdgeHorizontal, edges.horizontal, set_points,
                 set_percent, set_auto);
  set_edge_value(node, YGEdgeVertical, edges.vertical, set_points, set_percent,
                 set_auto);
  set_edge_value(node, YGEdgeLeft, edges.left, set_points, set_percent,
                 set_auto);
  set_edge_value(node, YGEdgeRight, edges.right, set_points, set_percent,
                 set_auto);
  set_edge_value(node, YGEdgeTop, edges.top, set_points, set_percent, set_auto);
  set_edge_value(node, YGEdgeBottom, edges.bottom, set_points, set_percent,
                 set_auto);
  set_edge_value(node, YGEdgeStart, edges.start, set_points, set_percent,
                 set_auto);
  set_edge_value(node, YGEdgeEnd, edges.end, set_points, set_percent, set_auto);
}

void set_border_edge_value(YGNodeRef node, YGEdge edge, StyleValue value) {
  if (value.kind == StyleValueKind::Points)
    YGNodeStyleSetBorder(node, edge, value.value);
}

void set_border_edges(YGNodeRef node, const EdgeSizes &edges) {
  set_border_edge_value(node, YGEdgeAll, edges.all);
  set_border_edge_value(node, YGEdgeHorizontal, edges.horizontal);
  set_border_edge_value(node, YGEdgeVertical, edges.vertical);
  set_border_edge_value(node, YGEdgeLeft, edges.left);
  set_border_edge_value(node, YGEdgeRight, edges.right);
  set_border_edge_value(node, YGEdgeTop, edges.top);
  set_border_edge_value(node, YGEdgeBottom, edges.bottom);
  set_border_edge_value(node, YGEdgeStart, edges.start);
  set_border_edge_value(node, YGEdgeEnd, edges.end);
}

void set_gap(YGNodeRef node, YGGutter gutter, StyleValue value) {
  switch (value.kind) {
  case StyleValueKind::Undefined:
  case StyleValueKind::Auto:
    break;
  case StyleValueKind::Points:
    YGNodeStyleSetGap(node, gutter, value.value);
    break;
  case StyleValueKind::Percent:
    YGNodeStyleSetGapPercent(node, gutter, value.value);
    break;
  }
}

ComputedEdgeSizes read_layout_edges(YGNodeRef node,
                                    float (*get_edge)(YGNodeConstRef, YGEdge)) {
  return {
      .left = get_edge(node, YGEdgeLeft),
      .right = get_edge(node, YGEdgeRight),
      .top = get_edge(node, YGEdgeTop),
      .bottom = get_edge(node, YGEdgeBottom),
      .start = get_edge(node, YGEdgeStart),
      .end = get_edge(node, YGEdgeEnd),
  };
}

void apply_style(YGNodeRef yoga_node, const NodeSnapshot &snapshot,
                 LayoutViewport viewport) {
  const LayoutStyle &style = snapshot.style;
  YGNodeStyleSetDirection(yoga_node,
                          map_layout_direction(style.layout_direction));
  YGNodeStyleSetBoxSizing(yoga_node, map_box_sizing(style.box_sizing));
  YGNodeStyleSetDisplay(yoga_node, map_display(style.display));
  YGNodeStyleSetPositionType(yoga_node, map_position(style.position));
  YGNodeStyleSetOverflow(yoga_node, map_overflow(style.overflow));
  YGNodeStyleSetFlexDirection(yoga_node, map_direction(style.direction));
  YGNodeStyleSetFlexWrap(yoga_node, map_wrap(style.wrap));
  YGNodeStyleSetAlignItems(yoga_node, map_align(style.align_items));
  YGNodeStyleSetAlignContent(yoga_node, map_align(style.align_content));
  YGNodeStyleSetAlignSelf(yoga_node, map_align(style.align_self));
  YGNodeStyleSetJustifyContent(yoga_node, map_justify(style.justify_content));
  YGNodeSetNodeType(yoga_node, map_node_type(style.node_type));
  YGNodeSetIsReferenceBaseline(yoga_node, style.is_reference_baseline);
  YGNodeSetAlwaysFormsContainingBlock(yoga_node,
                                      style.always_forms_containing_block);
  if (style.flex.is_defined())
    YGNodeStyleSetFlex(yoga_node, style.flex.value);
  set_edges(yoga_node, style.margin, YGNodeStyleSetMargin,
            YGNodeStyleSetMarginPercent, YGNodeStyleSetMarginAuto);
  set_edges(yoga_node, style.padding, YGNodeStyleSetPadding,
            YGNodeStyleSetPaddingPercent, nullptr);
  set_edges(yoga_node, style.position_inset, YGNodeStyleSetPosition,
            YGNodeStyleSetPositionPercent, YGNodeStyleSetPositionAuto);
  if (style.border_width > 0.0f)
    YGNodeStyleSetBorder(yoga_node, YGEdgeAll, style.border_width);
  set_border_edges(yoga_node, style.border_widths);
  set_gap(yoga_node, YGGutterAll, style.gap);
  set_gap(yoga_node, YGGutterRow, style.row_gap);
  set_gap(yoga_node, YGGutterColumn, style.column_gap);

  if (snapshot.id == UI_RETAINED_ROOT_ID) {
    YGNodeStyleSetWidth(yoga_node, viewport.width);
    YGNodeStyleSetHeight(yoga_node, viewport.height);
    return;
  }

  set_dimension(yoga_node, style.width, YGNodeStyleSetWidth,
                YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
  set_dimension(yoga_node, style.height, YGNodeStyleSetHeight,
                YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
  set_optional_length(yoga_node, style.min_width, YGNodeStyleSetMinWidth,
                      YGNodeStyleSetMinWidthPercent);
  set_optional_length(yoga_node, style.min_height, YGNodeStyleSetMinHeight,
                      YGNodeStyleSetMinHeightPercent);
  set_optional_length(yoga_node, style.max_width, YGNodeStyleSetMaxWidth,
                      YGNodeStyleSetMaxWidthPercent);
  set_optional_length(yoga_node, style.max_height, YGNodeStyleSetMaxHeight,
                      YGNodeStyleSetMaxHeightPercent);
  set_flex_basis(yoga_node, style.flex_basis);
  if (style.flex_grow.is_defined())
    YGNodeStyleSetFlexGrow(yoga_node, style.flex_grow.value);
  if (style.flex_shrink.is_defined())
    YGNodeStyleSetFlexShrink(yoga_node, style.flex_shrink.value);
  if (style.aspect_ratio.is_defined())
    YGNodeStyleSetAspectRatio(yoga_node, style.aspect_ratio.value);
}

YGNodeRef build_yoga_tree(UiTree &tree, BuildContext &context, NodeId id,
                          LayoutViewport viewport) {
  NodeSnapshot snapshot = {};
  if (!tree.snapshot(id, &snapshot))
    return nullptr;

  YGNodeRef yoga_node =
      context.config ? YGNodeNewWithConfig(context.config) : YGNodeNew();
  if (!yoga_node)
    return nullptr;

  apply_style(yoga_node, snapshot, viewport);
  if (snapshot.has_measure || snapshot.has_baseline) {
    if (context.node_context_count >= UI_RETAINED_MAX_NODES) {
      YGNodeFree(yoga_node);
      return nullptr;
    }
    NodeContext &node_context =
        context.node_contexts[context.node_context_count++];
    node_context = {
        .tree = &tree,
        .id = id,
    };
    YGNodeSetContext(yoga_node, &node_context);
  }
  if (snapshot.has_measure) {
    YGNodeSetMeasureFunc(yoga_node, measure_yoga_node);
  }
  if (snapshot.has_baseline) {
    YGNodeSetBaselineFunc(yoga_node, baseline_yoga_node);
  }

  for (int i = 0; i < tree.child_count(id); ++i) {
    YGNodeRef child =
        build_yoga_tree(tree, context, tree.child_at(id, i), viewport);
    if (!child) {
      YGNodeFreeRecursive(yoga_node);
      return nullptr;
    }
    YGNodeInsertChild(yoga_node, child, static_cast<uint32_t>(i));
  }

  return yoga_node;
}

bool write_layout(UiTree &tree, NodeId id, YGNodeRef yoga_node, float parent_x,
                  float parent_y) {
  if (!yoga_node)
    return false;

  float left = YGNodeLayoutGetLeft(yoga_node);
  float top = YGNodeLayoutGetTop(yoga_node);
  Rect layout = {
      .x = parent_x + left,
      .y = parent_y + top,
      .width = YGNodeLayoutGetWidth(yoga_node),
      .height = YGNodeLayoutGetHeight(yoga_node),
      .right = YGNodeLayoutGetRight(yoga_node),
      .bottom = YGNodeLayoutGetBottom(yoga_node),
      .direction = map_layout_direction(YGNodeLayoutGetDirection(yoga_node)),
      .had_overflow = YGNodeLayoutGetHadOverflow(yoga_node),
      .margin = read_layout_edges(yoga_node, YGNodeLayoutGetMargin),
      .border = read_layout_edges(yoga_node, YGNodeLayoutGetBorder),
      .padding = read_layout_edges(yoga_node, YGNodeLayoutGetPadding),
  };
  if (!tree.set_layout(id, layout)) {
    return false;
  }

  int children = tree.child_count(id);
  for (int i = 0; i < children; ++i) {
    YGNodeRef yoga_child = YGNodeGetChild(yoga_node, static_cast<uint32_t>(i));
    if (!write_layout(tree, tree.child_at(id, i), yoga_child, layout.x,
                      layout.y))
      return false;
  }
  return true;
}

bool compute_yoga_layout(UiTree &tree, NodeId root_id, LayoutViewport viewport,
                         void *) {
  BuildContext context = {};
  YGNodeRef root = build_yoga_tree(tree, context, root_id, viewport);
  if (!root)
    return false;

  YGNodeCalculateLayout(root, viewport.width, viewport.height, YGDirectionLTR);
  bool ok = write_layout(tree, root_id, root, 0.0f, 0.0f);
  YGNodeFreeRecursive(root);
  return ok;
}

bool compute_yoga_layout_with_config(UiTree &tree, NodeId root_id,
                                     LayoutViewport viewport, void *user) {
  const YogaLayoutConfig *config = static_cast<const YogaLayoutConfig *>(user);
  if (!config)
    return compute_yoga_layout(tree, root_id, viewport, nullptr);

  YGConfigRef yoga_config = YGConfigNew();
  if (!yoga_config)
    return false;

  YGConfigSetUseWebDefaults(yoga_config, config->use_web_defaults);
  if (config->point_scale_factor.is_defined()) {
    YGConfigSetPointScaleFactor(yoga_config, config->point_scale_factor.value);
  }
  if (config->has_errata)
    YGConfigSetErrata(yoga_config, map_errata(config->errata));
  YGConfigSetExperimentalFeatureEnabled(yoga_config,
                                        YGExperimentalFeatureWebFlexBasis,
                                        config->experimental_web_flex_basis);

  BuildContext context = {
      .config = yoga_config,
  };
  YGNodeRef root = build_yoga_tree(tree, context, root_id, viewport);
  if (!root) {
    YGConfigFree(yoga_config);
    return false;
  }

  YGNodeCalculateLayout(root, viewport.width, viewport.height,
                        map_owner_direction(config->owner_direction));
  bool ok = write_layout(tree, root_id, root, 0.0f, 0.0f);
  YGNodeFreeRecursive(root);
  YGConfigFree(yoga_config);
  return ok;
}

} // namespace

FlexLayoutAdapter make_yoga_flex_layout_adapter() {
  return {
      .name = "Yoga",
      .compute = compute_yoga_layout,
      .user = nullptr,
  };
}

FlexLayoutAdapter
make_yoga_flex_layout_adapter(const YogaLayoutConfig *config) {
  return {
      .name = "Yoga",
      .compute = compute_yoga_layout_with_config,
      .user = const_cast<YogaLayoutConfig *>(config),
  };
}

} // namespace ui

#else // SILENCER_HEADLESS

namespace ui {
FlexLayoutAdapter make_yoga_flex_layout_adapter() { return {}; }
FlexLayoutAdapter make_yoga_flex_layout_adapter(const YogaLayoutConfig *) { return {}; }
} // namespace ui

#endif // SILENCER_HEADLESS
