#include "tree.h"

#include <cstdio> // retained-tree capacity diagnostics (see report_error sites)
#include <string.h>

namespace ui {

namespace {

uint64_t hash_bytes(uint64_t hash, const char *text) {
  const unsigned char *cursor =
      reinterpret_cast<const unsigned char *>(text ? text : "");
  while (*cursor) {
    hash ^= *cursor++;
    hash *= 1099511628211ull;
  }
  return hash;
}

uint64_t mix_u64(uint64_t hash, uint64_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
  hash ^= value >> 32u;
  hash *= 1099511628211ull;
  return hash;
}

} // namespace

UiTree::UiTree() = default;

UiTree::~UiTree() { reset(); }

void UiTree::reset() {
  // destroy_node already resets each used slot to a default Node, and the
  // invariant that only ensure_node dirties a slot (bumping node_capacity_used_)
  // means every slot at/after node_capacity_used_ is already default. So the
  // array is fully cleared by the loop alone. Do NOT bulk-assign `nodes_ = {}`:
  // nodes_ is std::array<Node, 1024> (~8.7 MB), and the braced assignment
  // materializes a value-initialized temporary of that size on the stack,
  // overflowing the 8 MB main-thread stack and crashing in ~UiTree (SIGSEGV at
  // the stack guard page) on shutdown.
  for (int i = 0; i < node_capacity_used_; ++i) {
    destroy_node(nodes_[i]);
  }
  stack_ = {};
  unmounted_ = {};
  node_capacity_used_ = 0;
  stack_count_ = 0;
  unmounted_count_ = 0;
  generation_ = 0;
  error_count_ = 0;
  id_index_.clear();
  free_count_ = 0;
}

void UiTree::begin_frame(float width, float height) {
  ++generation_;
  if (generation_ == 0)
    generation_ = 1;

  stack_count_ = 0;
  unmounted_count_ = 0;

  LayoutStyle root_style = {};
  root_style.width = Length::points(width);
  root_style.height = Length::points(height);

  Node *root = ensure_node(UI_RETAINED_ROOT_ID, 0, "Root", "", root_style);
  if (!root)
    return;

  root->layout = {0.0f, 0.0f, width, height};
  stack_[stack_count_++] = static_cast<int>(root - nodes_.data());
}

bool UiTree::end_frame() {
  if (stack_count_ != 1) {
    report_error();
    stack_count_ = stack_count_ > 0 ? 1 : 0;
  }

  for (int i = 0; i < node_capacity_used_; ++i) {
    Node &node = nodes_[i];
    if (node.id == 0 || node.generation == generation_)
      continue;
    NodeId removed = node.id;
    destroy_node(node);
    if (unmounted_count_ < UI_RETAINED_MAX_NODES) {
      unmounted_[unmounted_count_++] = removed;
    }
  }

  stack_count_ = 0;
  return error_count_ == 0;
}

NodeId UiTree::begin_node(const char *type, const LayoutStyle &style) {
  return begin_keyed_node(type, nullptr, style);
}

NodeId UiTree::begin_keyed_node(const char *type, const char *key,
                                const LayoutStyle &style) {
  if (stack_count_ <= 0 || stack_count_ >= UI_RETAINED_MAX_DEPTH) {
    fprintf(stderr,
            "ui/retained: node nesting too deep (depth=%d max=%d) at %s; raise "
            "UI_RETAINED_MAX_DEPTH in ui/runtime/tree.h\n",
            stack_count_, UI_RETAINED_MAX_DEPTH, type ? type : "?");
    report_error();
    return 0;
  }

  Node &parent = nodes_[stack_[stack_count_ - 1]];
  uint32_t sibling_index = parent.next_child_index++;

  bool keyed = key && key[0] != '\0';
  NodeId id = make_child_id(parent.id, type, key, sibling_index, keyed);
  Node *node = ensure_node(id, parent.id, type, keyed ? key : "", style);
  if (!node)
    return 0;

  if (parent.child_count >= UI_RETAINED_MAX_CHILDREN) {
    fprintf(stderr,
            "ui/retained: too many children on one node (max=%d) adding %s; "
            "raise UI_RETAINED_MAX_CHILDREN in ui/runtime/tree.h\n",
            UI_RETAINED_MAX_CHILDREN, type ? type : "?");
    report_error();
    return 0;
  }
  parent.children[parent.child_count++] = id;

  stack_[stack_count_++] = static_cast<int>(node - nodes_.data());
  return id;
}

bool UiTree::end_node() {
  if (stack_count_ <= 1) {
    report_error();
    return false;
  }
  --stack_count_;
  return true;
}

bool UiTree::set_cleanup(NodeId id, CleanupFn cleanup, void *user) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  node->cleanup = cleanup;
  node->cleanup_user = user;
  return true;
}

bool UiTree::set_measure(NodeId id, MeasureFn measure, void *user) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  node->measure = measure;
  node->measure_user = user;
  return true;
}

namespace {

// Resolved effective text paint for a node, mirroring the transcriber's source-
// selection so layout and paint agree. The IR's font-size fallback is 15.
float effective_font_size(const UiTree::TextMeasureView &view) {
  if (view.font_size > 0)
    return view.font_size;
  return 15;
}

// The shared text-measure shim (design §10.1). user is the node's stable
// TextMeasureView. Builds a query and routes through the injected measurer; if
// no measurer is installed (hermetic layout tests), falls back to a fixed
// per-byte advance so layout stays deterministic.
Size text_measure_shim(MeasureInput input, void *user) {
  const auto *view = static_cast<const UiTree::TextMeasureView *>(user);
  if (!view)
    return {0.0f, 16.0f};
  const char *utf8 = view->utf8 ? view->utf8 : "";
  uint32_t len = static_cast<uint32_t>(strlen(utf8));
  float font_size = effective_font_size(*view);

  MeasureTextFn measurer = text_measurer();
  // Wrap inside any constrained width: AtMost (fit-content parents) AND
  // Exactly (definite width, e.g. percent) — a definite-width wrapped Text
  // must wrap at that width or measure returns one overflowing line while
  // the paint path wraps, and the box height is a single line.
  float wrap_width = (input.width_mode == MeasureMode::AtMost ||
                      input.width_mode == MeasureMode::Exactly)
                         ? input.width
                         : 0.0f;
  if (!measurer) {
    // Deterministic fallback for tests without an installed measurer.
    float width = static_cast<float>(len) * 8.0f;
    if (input.width_mode == MeasureMode::AtMost && width > input.width)
      width = input.width;
    return {width, 16.0f};
  }

  TextMetricsQuery query = {};
  query.utf8 = utf8;
  query.len = len;
  query.font_id = view->font_id;
  query.font_size = font_size;
  query.align = view->align;
  query.wrap = view->wrap;
  query.line_height = view->line_height;
  query.wrap_width = wrap_width; // box width; wrap only happens when wrap==Words
  TextMetricsResult result = measurer(query);
  float height = result.height > 0.0f ? result.height : 16.0f;
  return {result.width, height};
}

} // namespace

bool UiTree::set_text_measure(NodeId id) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  // Snapshot the resolved text paint into a stable per-node view the shim reads.
  node->text_measure_view = {
      .utf8 = node->value,
      .font_id = node->visual.text.font_id,
      .font_size = node->visual.text.font_size,
      .align = node->visual.text.align,
      .wrap = node->visual.text.wrap,
      .line_height = node->visual.text.line_height,
  };
  node->measure = text_measure_shim;
  node->measure_user = &node->text_measure_view;
  return true;
}

bool UiTree::set_baseline(NodeId id, BaselineFn baseline, void *user) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  node->baseline = baseline;
  node->baseline_user = user;
  return true;
}

bool UiTree::set_metadata(NodeId id, const NodeMetadata &metadata) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  node->role = metadata.role;
  node->semantic_role = metadata.semantic_role;
  node->visual = metadata.visual;
  node->fiber_id = metadata.fiber_id;
  node->interaction = metadata.interaction;
  node->text_edit = metadata.text_edit;
  copy_value(node->composition, metadata.text_edit.composition);
  node->text_edit.composition = node->composition;
  node->on_focus = metadata.on_focus;
  node->on_blur = metadata.on_blur;
  node->on_hover = metadata.on_hover;
  node->on_activate = metadata.on_activate;
  node->on_key = metadata.on_key;
  node->on_wheel = metadata.on_wheel;
  node->on_text_input = metadata.on_text_input;
  node->on_text_editing = metadata.on_text_editing;
  node->control_offset = metadata.control_offset;
  copy_label(node->control_id, metadata.control_id);
  copy_value(node->accessibility_label, metadata.accessibility_label);
  copy_value(node->accessibility_description,
             metadata.accessibility_description);
  copy_value(node->value, metadata.value);
  copy_value(node->display_value, metadata.display_value);
  return true;
}

bool UiTree::measure(NodeId id, MeasureInput input, Size *out) const {
  if (!out)
    return false;
  const Node *node = find(id);
  if (!node || !node->measure)
    return false;
  *out = node->measure(input, node->measure_user);
  return true;
}

bool UiTree::baseline(NodeId id, BaselineInput input, float *out) const {
  if (!out)
    return false;
  const Node *node = find(id);
  if (!node || !node->baseline)
    return false;
  *out = node->baseline(input, node->baseline_user);
  return true;
}

bool UiTree::set_layout(NodeId id, Rect rect) {
  Node *node = find_mutable(id);
  if (!node)
    return false;
  node->layout = rect;
  return true;
}

bool UiTree::invoke_focus(NodeId id) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_focus)
    return false;
  node->on_focus({.target = id});
  return true;
}

bool UiTree::invoke_blur(NodeId id) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_blur)
    return false;
  node->on_blur({.target = id});
  return true;
}

bool UiTree::invoke_hover(NodeId id, bool entered) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_hover)
    return false;
  node->on_hover({.target = id, .entered = entered});
  return true;
}

bool UiTree::invoke_activate(NodeId id) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_activate)
    return false;
  node->on_activate({.target = id});
  return true;
}

bool UiTree::invoke_key(NodeId id, const ::ui::UiKeyInputEvent &event) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_key)
    return false;
  node->on_key({
      .target = id,
      .key = event.key,
      .modifiers = event.modifiers,
      .repeat = event.repeat,
  });
  return true;
}

bool UiTree::invoke_wheel(NodeId id, float dx, float dy) const {
  const Node *node = find(id);
  if (!node || node->interaction.disabled || !node->on_wheel)
    return false;
  node->on_wheel({.target = id, .dx = dx, .dy = dy});
  return true;
}

bool UiTree::invoke_text_input(NodeId id,
                               const ::ui::UiTextInputEvent &event) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_text_input)
    return false;
  node->on_text_input({
      .target = id,
      .text = event.text,
  });
  return true;
}

bool UiTree::invoke_text_editing(NodeId id,
                                 const ::ui::UiTextEditingEvent &event) const {
  const Node *node = find(id);
  if (!node || !node->interaction.focusable || node->interaction.disabled ||
      !node->on_text_editing)
    return false;
  node->on_text_editing({
      .target = id,
      .text = event.text,
      .start = event.start,
      .length = event.length,
  });
  return true;
}

bool UiTree::snapshot(NodeId id, NodeSnapshot *out) const {
  if (!out)
    return false;
  const Node *node = find(id);
  if (!node)
    return false;
  *out = {
      .id = node->id,
      .parent_id = node->parent_id,
      .visual = node->visual,
      .fiber_id = node->fiber_id,
      .type = node->type,
      .key = node->key,
      .control_id = node->control_id,
      .control_offset = node->control_offset,
      .accessibility_label = node->accessibility_label,
      .accessibility_description = node->accessibility_description,
      .value = node->value,
      .display_value = node->display_value,
      .role = node->role,
      .semantic_role = node->semantic_role,
      .interaction = node->interaction,
      .text_edit = node->text_edit,
      .style = node->style,
      .layout = node->layout,
      .child_count = node->child_count,
      .has_measure = node->measure != nullptr,
      .has_baseline = node->baseline != nullptr,
      .mounted_this_frame = node->mounted_this_frame,
  };
  return true;
}

bool UiTree::contains(NodeId id) const { return find(id) != nullptr; }

NodeId UiTree::current_parent_id() const {
  if (stack_count_ <= 0)
    return 0;
  return nodes_[stack_[stack_count_ - 1]].id;
}

int UiTree::node_count() const {
  int count = 0;
  for (int i = 0; i < node_capacity_used_; ++i) {
    if (nodes_[i].id != 0)
      ++count;
  }
  return count;
}

int UiTree::child_count(NodeId id) const {
  const Node *node = find(id);
  return node ? node->child_count : 0;
}

NodeId UiTree::child_at(NodeId id, int index) const {
  const Node *node = find(id);
  if (!node || index < 0 || index >= node->child_count)
    return 0;
  return node->children[index];
}

NodeId UiTree::unmounted_at(int index) const {
  if (index < 0 || index >= unmounted_count_)
    return 0;
  return unmounted_[index];
}

UiTree::Node *UiTree::find_mutable(NodeId id) {
  auto it = id_index_.find(id);
  return it == id_index_.end() ? nullptr : &nodes_[it->second];
}

const UiTree::Node *UiTree::find(NodeId id) const {
  auto it = id_index_.find(id);
  return it == id_index_.end() ? nullptr : &nodes_[it->second];
}

UiTree::Node *UiTree::ensure_node(NodeId id, NodeId parent_id, const char *type,
                                  const char *key, const LayoutStyle &style) {
  if (id == 0) {
    report_error();
    return nullptr;
  }

  Node *existing = find_mutable(id);
  if (existing) {
    existing->parent_id = parent_id;
    existing->generation = generation_;
    existing->next_child_index = 0;
    existing->child_count = 0;
    existing->mounted_this_frame = false;
    existing->style = style;
    existing->role = NodeRole::Generic;
    existing->semantic_role = SemanticRole::Auto;
    existing->interaction = {};
    existing->text_edit = {};
    existing->on_focus = {};
    existing->on_blur = {};
    existing->on_hover = {};
    existing->on_activate = {};
    existing->on_key = {};
    existing->on_wheel = {};
    existing->on_text_input = {};
    existing->on_text_editing = {};
    existing->baseline = nullptr;
    existing->baseline_user = nullptr;
    existing->control_offset = 0;
    existing->control_id[0] = '\0';
    existing->accessibility_label[0] = '\0';
    existing->accessibility_description[0] = '\0';
    existing->value[0] = '\0';
    existing->composition[0] = '\0';
    copy_label(existing->type, type);
    copy_label(existing->key, key);
    return existing;
  }

  int slot;
  if (free_count_ > 0) {
    slot = free_slots_[--free_count_];
  } else if (node_capacity_used_ < UI_RETAINED_MAX_NODES) {
    slot = node_capacity_used_++;
  } else {
    fprintf(stderr,
            "ui/retained: node pool full (max=%d) adding %s; raise "
            "UI_RETAINED_MAX_NODES in ui/runtime/tree.h\n",
            UI_RETAINED_MAX_NODES, type ? type : "?");
    report_error();
    return nullptr;
  }

  Node &node = nodes_[slot];
  node = {};
  node.id = id;
  node.parent_id = parent_id;
  node.generation = generation_;
  node.style = style;
  node.mounted_this_frame = true;
  copy_label(node.type, type);
  copy_label(node.key, key);
  id_index_[id] = slot;
  return &node;
}

void UiTree::destroy_node(Node &node) {
  if (node.id == 0)
    return;
  if (node.cleanup)
    node.cleanup(node.cleanup_user);
  id_index_.erase(node.id);
  if (free_count_ < UI_RETAINED_MAX_NODES)
    free_slots_[free_count_++] = static_cast<int>(&node - nodes_.data());
  node = {};
}

NodeId UiTree::make_child_id(NodeId parent_id, const char *type,
                             const char *key, uint32_t sibling_index,
                             bool keyed) const {
  uint64_t hash = 1469598103934665603ull;
  hash = mix_u64(hash, parent_id);
  hash = hash_bytes(hash, type);
  hash = mix_u64(hash, keyed ? 0x9E3779B97F4A7C15ull : 0x85EBCA6B27D4EB2Full);
  if (keyed) {
    hash = hash_bytes(hash, key);
  } else {
    hash = mix_u64(hash, sibling_index);
  }
  return hash == 0 ? 1ull : hash;
}

void UiTree::copy_label(char (&dest)[UI_RETAINED_LABEL_CAP],
                        const char *source) {
  const char *safe_source = source ? source : "";
  strncpy(dest, safe_source, UI_RETAINED_LABEL_CAP - 1);
  dest[UI_RETAINED_LABEL_CAP - 1] = '\0';
}

void UiTree::copy_value(char (&dest)[UI_RETAINED_VALUE_CAP],
                        const char *source) {
  const char *safe_source = source ? source : "";
  strncpy(dest, safe_source, UI_RETAINED_VALUE_CAP - 1);
  dest[UI_RETAINED_VALUE_CAP - 1] = '\0';
}

void UiTree::report_error() { ++error_count_; }

} // namespace ui
