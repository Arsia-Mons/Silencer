#pragma once

#include "../input.h"
#include "../style/text_measure.h" // the injected MeasureTextFn seam (text nodes)
#include "../style/visual_style.h" // Color and the shared paint primitives live here now

#include <array>
#include <functional>
#include <stdint.h>

namespace ui {

using NodeId = uint64_t;

// Retained-tree capacity. These are the whole-tree budgets across ALL mounted
// screen layers (an overlay like Options·Controls commits OVER the base menu, so
// both trees share the node budget). The dense keybind table + the lobby cockpit
// grid push well past the original 256; sized generously so commits never
// silently truncate (a too-small cap drops the tail of the tree — late rows +
// the action row — with `errors=N` at commit). Cf. REACT_MAX_FIBERS (react.cpp).
constexpr int UI_RETAINED_MAX_NODES = 512;
constexpr int UI_RETAINED_MAX_DEPTH = 64;
constexpr int UI_RETAINED_MAX_CHILDREN = 128;
constexpr int UI_RETAINED_LABEL_CAP = 48;
constexpr int UI_RETAINED_VALUE_CAP = 640; // sized for the cc lore paragraphs (~560 chars), the longest wrapped Text values

constexpr NodeId UI_RETAINED_ROOT_ID = 0xCBF29CE484222325ull;

enum class LengthKind : uint8_t {
  Undefined,
  Auto,
  Points,
  Percent,
  Grow,
};

struct Length {
  LengthKind kind = LengthKind::Undefined;
  float value = 0.0f;

  constexpr Length() = default;
  constexpr Length(float points) : kind(LengthKind::Points), value(points) {}
  constexpr Length(LengthKind kind, float value = 0.0f)
      : kind(kind), value(value) {}

  static Length undefined() { return {}; }
  static Length auto_size() { return {LengthKind::Auto}; }
  static Length points(float value) { return {LengthKind::Points, value}; }
  static Length percent(float value) { return {LengthKind::Percent, value}; }
  static Length grow(float value = 1.0f) { return {LengthKind::Grow, value}; }
};

enum class StyleValueKind : uint8_t {
  Undefined,
  Points,
  Percent,
  Auto,
};

struct StyleValue {
  StyleValueKind kind = StyleValueKind::Undefined;
  float value = 0.0f;

  constexpr StyleValue() = default;
  constexpr StyleValue(float points)
      : kind(StyleValueKind::Points), value(points) {}
  constexpr StyleValue(StyleValueKind kind, float value = 0.0f)
      : kind(kind), value(value) {}

  static StyleValue undefined() { return {}; }
  static StyleValue points(float value) {
    return {StyleValueKind::Points, value};
  }
  static StyleValue percent(float value) {
    return {StyleValueKind::Percent, value};
  }
  static StyleValue auto_value() { return {StyleValueKind::Auto}; }

  bool is_defined() const { return kind != StyleValueKind::Undefined; }
  operator float() const { return value; }
};

struct StyleFloat {
  bool defined = false;
  float value = 0.0f;

  constexpr StyleFloat() = default;
  constexpr StyleFloat(float value) : defined(true), value(value) {}

  static StyleFloat undefined() { return {}; }
  static StyleFloat points(float value) { return {value}; }

  bool is_defined() const { return defined; }
  operator float() const { return value; }
};

enum class LayoutDirection : uint8_t {
  Inherit,
  Ltr,
  Rtl,
};

enum class FlexDirection : uint8_t {
  Row,
  RowReverse,
  Column,
  ColumnReverse,
};

enum class AlignItems : uint8_t {
  Auto,
  Stretch,
  Start,
  Center,
  End,
  Baseline,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

enum class JustifyContent : uint8_t {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

enum class FlexWrap : uint8_t {
  NoWrap,
  Wrap,
  WrapReverse,
};

enum class PositionType : uint8_t {
  Static,
  Relative,
  Absolute,
};

enum class Overflow : uint8_t {
  Visible,
  Hidden,
  Scroll,
};

enum class Display : uint8_t {
  Flex,
  None,
  Contents,
};

enum class BoxSizing : uint8_t {
  BorderBox,
  ContentBox,
};

enum class LayoutNodeType : uint8_t {
  Default,
  Text,
};

enum class NodeRole : uint8_t {
  Generic,
  Box,
  Text,
  Button,
  Input,
  Checkbox,
  Dialog,
};

enum class SemanticRole : uint8_t {
  Auto,
  Button,
  Checkbox,
  TextBox,
  Tab,
  Dialog,
};

struct EdgeSizes {
  StyleValue left = {};
  StyleValue right = {};
  StyleValue top = {};
  StyleValue bottom = {};
  StyleValue start = {};
  StyleValue end = {};
  StyleValue horizontal = {};
  StyleValue vertical = {};
  StyleValue all = {};

  constexpr EdgeSizes() = default;
  constexpr EdgeSizes(StyleValue left, StyleValue right, StyleValue top,
                      StyleValue bottom)
      : left(left), right(right), top(top), bottom(bottom) {}

  static EdgeSizes all_edges(StyleValue value) {
    EdgeSizes edges = {};
    edges.all = value;
    return edges;
  }

  static EdgeSizes axes(StyleValue horizontal, StyleValue vertical) {
    EdgeSizes edges = {};
    edges.horizontal = horizontal;
    edges.vertical = vertical;
    return edges;
  }
};

struct ComputedEdgeSizes {
  float left = 0.0f;
  float right = 0.0f;
  float top = 0.0f;
  float bottom = 0.0f;
  float start = 0.0f;
  float end = 0.0f;
};

struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  LayoutDirection direction = LayoutDirection::Inherit;
  bool had_overflow = false;
  ComputedEdgeSizes margin = {};
  ComputedEdgeSizes border = {};
  ComputedEdgeSizes padding = {};
};

struct LayoutStyle {
  // Yoga v3.2.1 style fields, mirrored through repo-owned types so app code
  // stays independent from Yoga headers.
  LayoutDirection layout_direction = LayoutDirection::Inherit;
  BoxSizing box_sizing = BoxSizing::BorderBox;
  Display display = Display::Flex;
  PositionType position = PositionType::Relative;
  Overflow overflow = Overflow::Visible;
  FlexDirection direction = FlexDirection::Column;
  FlexWrap wrap = FlexWrap::NoWrap;
  AlignItems align_items = AlignItems::Stretch;
  AlignItems align_content = AlignItems::Start;
  AlignItems align_self = AlignItems::Auto;
  JustifyContent justify_content = JustifyContent::Start;
  LayoutNodeType node_type = LayoutNodeType::Default;
  bool is_reference_baseline = false;
  bool always_forms_containing_block = false;

  Length width = Length::auto_size();
  Length height = Length::auto_size();
  Length min_width = Length::auto_size();
  Length min_height = Length::auto_size();
  Length max_width = Length::auto_size();
  Length max_height = Length::auto_size();
  Length flex_basis = Length::auto_size();
  StyleFloat flex = {};
  StyleFloat flex_grow = {};
  StyleFloat flex_shrink = {};
  StyleFloat aspect_ratio = {};

  EdgeSizes margin = {};
  EdgeSizes padding = {};
  EdgeSizes position_inset = {};
  EdgeSizes border_widths = {};
  StyleValue gap = {};
  StyleValue row_gap = {};
  StyleValue column_gap = {};

  // All-edge layout border shorthand: feeds Yoga's all-edge border
  // (YGNodeStyleSetBorder) so the laid-out content box reserves the border.
  // Use border_widths for per-edge Yoga layout borders. This is a LAYOUT field
  // only; paint lives in the resolved VisualStyle (node.visual).
  float border_width = 0.0f;
};

enum class MeasureMode : uint8_t {
  Undefined,
  Exactly,
  AtMost,
};

struct MeasureInput {
  float width = 0.0f;
  float height = 0.0f;
  MeasureMode width_mode = MeasureMode::Undefined;
  MeasureMode height_mode = MeasureMode::Undefined;
};

struct BaselineInput {
  float width = 0.0f;
  float height = 0.0f;
};

struct Size {
  float width = 0.0f;
  float height = 0.0f;
};

struct Point {
  float x = 0.0f;
  float y = 0.0f;
};

struct NodeInteraction {
  bool focusable = false;
  bool disabled = false;
  bool checked = false;
  bool modal = false;
  bool initial_focus = false;
};

struct TextEditMetadata {
  bool show_caret = true; // origin TextInputOpts.showCaret (wall-clock blink)
  int caret = 0;
  int selection_start = 0;
  int selection_end = 0;
  const char *composition = "";
  int composition_start = 0;
  int composition_length = 0;
};

struct FocusEvent {
  NodeId target = 0;
};

struct BlurEvent {
  NodeId target = 0;
};

struct ActivationEvent {
  NodeId target = 0;
};

struct KeyEvent {
  NodeId target = 0;
  ::ui::UiKey key = ::ui::UiKey::Unknown;
  uint16_t modifiers = ::ui::UI_KEY_MOD_NONE;
  bool repeat = false;
};

struct WheelEvent {
  NodeId target = 0;
  float dx = 0.0f;
  float dy = 0.0f; // +y = wheel up
};

struct TextInputEvent {
  NodeId target = 0;
  const char *text = "";
};

struct TextEditingEvent {
  NodeId target = 0;
  const char *text = "";
  int start = 0;
  int length = 0;
};

struct NodeMetadata {
  NodeRole role = NodeRole::Generic;
  SemanticRole semantic_role = SemanticRole::Auto;
  VisualStyle visual = {};   // resolved paint, committed onto the node (dual-path)
  uint64_t fiber_id = 0;     // react fiber that produced this node (interaction keying)
  const char *control_id = "";
  int control_offset = 0;
  const char *accessibility_label = "";
  const char *accessibility_description = "";
  const char *value = "";
  NodeInteraction interaction = {};
  TextEditMetadata text_edit = {};
  std::function<void(const FocusEvent &)> on_focus = {};
  std::function<void(const BlurEvent &)> on_blur = {};
  std::function<void(const ActivationEvent &)> on_activate = {};
  std::function<void(const KeyEvent &)> on_key = {};
  std::function<void(const TextInputEvent &)> on_text_input = {};
  std::function<void(const TextEditingEvent &)> on_text_editing = {};
  std::function<void(const WheelEvent &)> on_wheel = {};
};

using CleanupFn = void (*)(void *user);
using MeasureFn = Size (*)(MeasureInput input, void *user);
using BaselineFn = float (*)(BaselineInput input, void *user);

struct NodeSnapshot {
  NodeId id = 0;
  NodeId parent_id = 0;
  VisualStyle visual = {};
  uint64_t fiber_id = 0;
  const char *type = "";
  const char *key = "";
  const char *control_id = "";
  int control_offset = 0;
  const char *accessibility_label = "";
  const char *accessibility_description = "";
  const char *value = "";
  NodeRole role = NodeRole::Generic;
  SemanticRole semantic_role = SemanticRole::Auto;
  NodeInteraction interaction = {};
  TextEditMetadata text_edit = {};
  LayoutStyle style = {};
  Rect layout = {};
  int child_count = 0;
  bool has_measure = false;
  bool has_baseline = false;
  bool mounted_this_frame = false;
};

class UiTree {
public:
  UiTree();
  ~UiTree();

  UiTree(const UiTree &) = delete;
  UiTree &operator=(const UiTree &) = delete;

  void reset();

  void begin_frame(float width, float height);
  bool end_frame();

  NodeId begin_node(const char *type, const LayoutStyle &style = {});
  NodeId begin_keyed_node(const char *type, const char *key,
                          const LayoutStyle &style = {});
  bool end_node();

  bool set_cleanup(NodeId id, CleanupFn cleanup, void *user);
  bool set_measure(NodeId id, MeasureFn measure, void *user);
  // Install the shared text-measure shim on a text node. The shim builds a
  // TextMetricsQuery from the node's committed value + resolved TextVisual and
  // calls the injected text_measurer() — the SAME measurer the transcriber uses
  // at paint time, so layout==paint by construction (design §10.1). Must be
  // called after set_metadata (reads node->visual.text and node->value).
  bool set_text_measure(NodeId id);
  bool set_baseline(NodeId id, BaselineFn baseline, void *user);
  bool set_metadata(NodeId id, const NodeMetadata &metadata);
  bool measure(NodeId id, MeasureInput input, Size *out) const;
  bool baseline(NodeId id, BaselineInput input, float *out) const;
  bool set_layout(NodeId id, Rect rect);
  bool invoke_focus(NodeId id) const;
  bool invoke_blur(NodeId id) const;
  bool invoke_activate(NodeId id) const;
  bool invoke_key(NodeId id, const ::ui::UiKeyInputEvent &event) const;
  bool invoke_wheel(NodeId id, float dx, float dy) const;
  bool invoke_text_input(NodeId id, const ::ui::UiTextInputEvent &event) const;
  bool invoke_text_editing(NodeId id,
                           const ::ui::UiTextEditingEvent &event) const;

  bool snapshot(NodeId id, NodeSnapshot *out) const;
  bool contains(NodeId id) const;

  // Stable per-node view the text-measure shim reads. utf8 points into the
  // node's own committed value buffer; the resolved font params mirror the
  // transcriber's source-selection so layout==paint.
  struct TextMeasureView {
    const char *utf8 = "";
    uint16_t font_id = 0;
    float font_size = 0.f; // from visual.text.font_size (0 => fallback)
    TextAlign align = TextAlign::Left;
    TextWrap wrap = TextWrap::None;
    float line_height = 0.0f;
  };

  NodeId root_id() const { return UI_RETAINED_ROOT_ID; }
  NodeId current_parent_id() const;
  int node_count() const;
  int child_count(NodeId id) const;
  NodeId child_at(NodeId id, int index) const;
  int unmounted_count() const { return unmounted_count_; }
  NodeId unmounted_at(int index) const;
  int error_count() const { return error_count_; }

private:
  struct Node {
    NodeId id = 0;
    NodeId parent_id = 0;
    VisualStyle visual = {};
    uint64_t fiber_id = 0;
    uint32_t generation = 0;
    uint32_t next_child_index = 0;
    int child_count = 0;
    bool mounted_this_frame = false;
    char type[UI_RETAINED_LABEL_CAP] = {};
    char key[UI_RETAINED_LABEL_CAP] = {};
    char control_id[UI_RETAINED_LABEL_CAP] = {};
    int control_offset = 0;
    char accessibility_label[UI_RETAINED_VALUE_CAP] = {};
    char accessibility_description[UI_RETAINED_VALUE_CAP] = {};
    char value[UI_RETAINED_VALUE_CAP] = {};
    char composition[UI_RETAINED_VALUE_CAP] = {};
    std::array<NodeId, UI_RETAINED_MAX_CHILDREN> children = {};
    NodeRole role = NodeRole::Generic;
    SemanticRole semantic_role = SemanticRole::Auto;
    NodeInteraction interaction = {};
    TextEditMetadata text_edit = {};
    std::function<void(const FocusEvent &)> on_focus = {};
    std::function<void(const BlurEvent &)> on_blur = {};
    std::function<void(const ActivationEvent &)> on_activate = {};
    std::function<void(const KeyEvent &)> on_key = {};
    std::function<void(const TextInputEvent &)> on_text_input = {};
    std::function<void(const TextEditingEvent &)> on_text_editing = {};
    std::function<void(const WheelEvent &)> on_wheel = {};
    LayoutStyle style = {};
    Rect layout = {};
    CleanupFn cleanup = nullptr;
    void *cleanup_user = nullptr;
    MeasureFn measure = nullptr;
    void *measure_user = nullptr;
    TextMeasureView text_measure_view = {};
    BaselineFn baseline = nullptr;
    void *baseline_user = nullptr;
  };

  Node *find_mutable(NodeId id);
  const Node *find(NodeId id) const;
  Node *ensure_node(NodeId id, NodeId parent_id, const char *type,
                    const char *key, const LayoutStyle &style);
  void destroy_node(Node &node);
  NodeId make_child_id(NodeId parent_id, const char *type, const char *key,
                       uint32_t sibling_index, bool keyed) const;
  void copy_label(char (&dest)[UI_RETAINED_LABEL_CAP], const char *source);
  void copy_value(char (&dest)[UI_RETAINED_VALUE_CAP], const char *source);
  void report_error();

  std::array<Node, UI_RETAINED_MAX_NODES> nodes_ = {};
  std::array<int, UI_RETAINED_MAX_DEPTH> stack_ = {};
  std::array<NodeId, UI_RETAINED_MAX_NODES> unmounted_ = {};
  int node_capacity_used_ = 0;
  int stack_count_ = 0;
  int unmounted_count_ = 0;
  uint32_t generation_ = 0;
  int error_count_ = 0;
};

} // namespace ui
