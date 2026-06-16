#include "focus.h"

#include <cmath>
#include <float.h>

namespace ui {

namespace {

bool same_id(NodeId a, NodeId b) { return a != 0 && a == b; }

float center_x(Rect r) { return r.x + r.width * 0.5f; }
float center_y(Rect r) { return r.y + r.height * 0.5f; }

bool contains(Rect r, float x, float y) {
  return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

bool interval_overlaps(float a0, float a1, float b0, float b1) {
  return a0 < b1 && b0 < a1;
}

bool is_candidate_in_direction(Rect from, Rect to, FocusDirection dir) {
  switch (dir) {
  case FocusDirection::Left:
    return center_x(to) < center_x(from);
  case FocusDirection::Right:
    return center_x(to) > center_x(from);
  case FocusDirection::Up:
    return center_y(to) < center_y(from);
  case FocusDirection::Down:
    return center_y(to) > center_y(from);
  }
  return false;
}

float primary_distance(Rect from, Rect to, FocusDirection dir) {
  switch (dir) {
  case FocusDirection::Left:
    return from.x - (to.x + to.width);
  case FocusDirection::Right:
    return to.x - (from.x + from.width);
  case FocusDirection::Up:
    return from.y - (to.y + to.height);
  case FocusDirection::Down:
    return to.y - (from.y + from.height);
  }
  return 0.0f;
}

float perpendicular_miss(Rect from, Rect to, FocusDirection dir) {
  bool horizontal = dir == FocusDirection::Left || dir == FocusDirection::Right;
  if (horizontal) {
    if (interval_overlaps(from.y, from.y + from.height, to.y,
                          to.y + to.height)) {
      return 0.0f;
    }
    return std::fabs(center_y(to) - center_y(from));
  }
  if (interval_overlaps(from.x, from.x + from.width, to.x, to.x + to.width)) {
    return 0.0f;
  }
  return std::fabs(center_x(to) - center_x(from));
}

bool read_nav_dir(const InputFrame &input, FocusDirection *dir) {
  if (input.nav_up) {
    *dir = FocusDirection::Up;
    return true;
  }
  if (input.nav_down) {
    *dir = FocusDirection::Down;
    return true;
  }
  if (input.nav_left) {
    *dir = FocusDirection::Left;
    return true;
  }
  if (input.nav_right) {
    *dir = FocusDirection::Right;
    return true;
  }
  return false;
}

FocusSource navigation_source(const InputFrame &input) {
  if (input.source == FocusSource::Gamepad)
    return FocusSource::Gamepad;
  if (input.source == FocusSource::Touch)
    return FocusSource::Touch;
  if (input.source == FocusSource::Mouse)
    return FocusSource::Mouse;
  return FocusSource::Keyboard;
}

FocusSource pointer_source(const InputFrame &input) {
  return input.source == FocusSource::Touch ? FocusSource::Touch
                                            : FocusSource::Mouse;
}

bool find_active_modal(const UiTree &tree, NodeId id, NodeId *out) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return false;
  if (node.interaction.modal)
    *out = id;
  for (int i = 0; i < tree.child_count(id); ++i) {
    if (!find_active_modal(tree, tree.child_at(id, i), out))
      return false;
  }
  return true;
}

bool subtree_contains(const UiTree &tree, NodeId root, NodeId target) {
  if (same_id(root, target))
    return true;
  for (int i = 0; i < tree.child_count(root); ++i) {
    if (subtree_contains(tree, tree.child_at(root, i), target))
      return true;
  }
  return false;
}

bool collect_focusables(const UiTree &tree, FocusRuntime &runtime, NodeId id,
                        uint32_t *order, NodeId focus_parent,
                        NodeId scroll_container) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return false;

  // A focusable inside a scrolling clip keeps its UNCLIPPED layout rect as its
  // nav rect: directional/sequential nav must be able to REACH a row that is
  // currently scrolled out of (or partially clipped by) the viewport, in true
  // document order (SIL-213). SIL-199's "don't rest on an invisible row" intent
  // is preserved by scroll-into-view (focus_scroll_request below): once focus
  // lands on such a row, the owning container scrolls it into view, so we never
  // permanently rest on an off-viewport node. The full node.layout rect (not the
  // clipped intersection) keeps ordering and direction tests aligned with the
  // row's real document position even while it is off-screen.
  NodeId child_focus_parent = focus_parent;
  if (node.interaction.focusable) {
    child_focus_parent = id;
    if (runtime.focusable_count >= UI_RETAINED_MAX_FOCUSABLES) {
      ++runtime.error_count;
      return false;
    }
    runtime.focusables[runtime.focusable_count++] = {
        .id = id,
        .focus_parent = focus_parent,
        .scroll_container = scroll_container,
        .rect = node.layout,
        .disabled = node.interaction.disabled,
        .initial_focus = node.interaction.initial_focus,
        .order = (*order)++,
    };
  }

  // The nearest scroll-clip ancestor for this node's descendants. A node is its
  // children's scroll container when it clips overflow (Scroll/Hidden).
  NodeId child_scroll_container = scroll_container;
  if (node.style.overflow == Overflow::Scroll ||
      node.style.overflow == Overflow::Hidden) {
    child_scroll_container = id;
  }

  for (int i = 0; i < tree.child_count(id); ++i) {
    if (!collect_focusables(tree, runtime, tree.child_at(id, i), order,
                            child_focus_parent, child_scroll_container))
      return false;
  }
  return true;
}

// Walk the focused node's ancestor chain in document order to find its nearest
// scrolling-clip container (overflow Scroll/Hidden) and compute how far that
// container must shift its scroll offset to bring the focused node fully into
// its clip rect. Both rects are in absolute layout space, so the delta is
// offset-independent: the container applies it to whatever local offset it
// holds. Negative delta scrolls toward the top (reveal above), positive scrolls
// down (reveal below); 0 = already in view. SIL-213.
FocusScrollRequest compute_scroll_request(const UiTree &tree, NodeId focused) {
  FocusScrollRequest request = {};
  if (focused == 0)
    return request;

  NodeSnapshot node = {};
  if (!tree.snapshot(focused, &node))
    return request;
  const Rect focus_rect = node.layout;

  NodeId ancestor = node.parent_id;
  while (ancestor != 0) {
    NodeSnapshot anc = {};
    if (!tree.snapshot(ancestor, &anc))
      break;
    if (anc.style.overflow == Overflow::Scroll ||
        anc.style.overflow == Overflow::Hidden) {
      const Rect clip = anc.layout;
      float delta = 0.0f;
      // Reveal a node clipped above the window (scroll up: negative delta), or
      // below it (scroll down: positive delta). If the node is taller than the
      // viewport, prefer aligning its top.
      if (focus_rect.y < clip.y) {
        delta = focus_rect.y - clip.y;
      } else if (focus_rect.y + focus_rect.height > clip.y + clip.height) {
        delta = (focus_rect.y + focus_rect.height) - (clip.y + clip.height);
        // Never scroll so far down that the node's top leaves the top edge.
        const float max_up = focus_rect.y - clip.y;
        if (delta > max_up)
          delta = max_up;
      }
      request.delta_y = delta;
      request.valid = true;
      // Copy the container's control id (truncating to the label cap) so a
      // ScrollView can match against its own props.id without a node lookup.
      const char *cid = anc.control_id ? anc.control_id : "";
      size_t n = 0;
      for (; cid[n] && n + 1 < UI_RETAINED_LABEL_CAP; ++n)
        request.viewport_control_id[n] = cid[n];
      request.viewport_control_id[n] = '\0';
      return request;
    }
    ancestor = anc.parent_id;
  }
  return request;
}

// Recursively record the resolved height of every node carrying a control id.
void collect_measured_sizes(const UiTree &tree, NodeId id,
                            MeasuredSizeRequest *out) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return;
  const char *cid = node.control_id ? node.control_id : "";
  if (cid[0] != '\0' && out->count < UI_RETAINED_MAX_MEASURED) {
    MeasuredSize &slot = out->sizes[out->count++];
    size_t n = 0;
    for (; cid[n] && n + 1 < UI_RETAINED_LABEL_CAP; ++n)
      slot.control_id[n] = cid[n];
    slot.control_id[n] = '\0';
    slot.height = node.layout.height;
  }
  for (int i = 0; i < tree.child_count(id); ++i)
    collect_measured_sizes(tree, tree.child_at(id, i), out);
}

const FocusableLayout *find_layout(const FocusRuntime &runtime, NodeId id) {
  if (id == 0)
    return nullptr;
  for (int i = 0; i < runtime.focusable_count; ++i) {
    if (same_id(runtime.focusables[i].id, id))
      return &runtime.focusables[i];
  }
  return nullptr;
}

bool contains_enabled(const FocusRuntime &runtime, NodeId id) {
  const FocusableLayout *layout = find_layout(runtime, id);
  return layout && !layout->disabled;
}

bool is_focus_descendant(const FocusRuntime &runtime,
                         const FocusableLayout &candidate,
                         NodeId ancestor_id) {
  NodeId parent = candidate.focus_parent;
  while (parent != 0) {
    if (same_id(parent, ancestor_id))
      return true;
    const FocusableLayout *layout = find_layout(runtime, parent);
    if (!layout)
      return false;
    parent = layout->focus_parent;
  }
  return false;
}

bool has_directional_focus_descendant(const FocusRuntime &runtime,
                                      const FocusableLayout &candidate,
                                      const FocusableLayout &from,
                                      FocusDirection dir) {
  for (int i = 0; i < runtime.focusable_count; ++i) {
    const FocusableLayout &other = runtime.focusables[i];
    if (other.disabled || same_id(other.id, candidate.id))
      continue;
    if (!is_focus_descendant(runtime, other, candidate.id))
      continue;
    if (is_candidate_in_direction(from.rect, other.rect, dir))
      return true;
  }
  return false;
}

bool set_focus(FocusRuntime &runtime, NodeId id, FocusSource source) {
  if (same_id(runtime.focused_id, id))
    return false;
  runtime.blurred_id = runtime.focused_id;
  runtime.focused_id = id;
  runtime.focus_changed_id = id;
  runtime.source = source;
  return true;
}

bool focused_text_input_should_handle_navigation(const UiTree &tree,
                                                 const FocusRuntime &runtime,
                                                 const InputFrame &input,
                                                 FocusDirection dir) {
  if (dir != FocusDirection::Left && dir != FocusDirection::Right)
    return false;

  NodeSnapshot focused = {};
  if (!tree.snapshot(runtime.focused_id, &focused))
    return false;
  if (focused.role != NodeRole::Input &&
      focused.semantic_role != SemanticRole::TextBox)
    return false;

  for (int i = 0; i < input.key_event_count; ++i) {
    const ::ui::UiKeyInputEvent &event = input.key_events[i];
    if (event.key == ::ui::UiKey::Left || event.key == ::ui::UiKey::Right ||
        event.key == ::ui::UiKey::Home || event.key == ::ui::UiKey::End ||
        event.key == ::ui::UiKey::Backspace ||
        event.key == ::ui::UiKey::DeleteForward) {
      return true;
    }
  }
  return input.text_event_count > 0 || input.editing_event_count > 0;
}

NodeId first_enabled(const FocusRuntime &runtime) {
  for (int i = 0; i < runtime.focusable_count; ++i) {
    if (runtime.focusables[i].initial_focus && !runtime.focusables[i].disabled)
      return runtime.focusables[i].id;
  }
  for (int i = 0; i < runtime.focusable_count; ++i) {
    if (!runtime.focusables[i].disabled)
      return runtime.focusables[i].id;
  }
  return 0;
}

// First/last enabled node in pure document (array) order. Unlike first_enabled
// these have no initial_focus preference: edge entry from a neutral state must
// be the true topmost/bottommost item, matching resolve_sequential's Tab /
// Shift+Tab.
NodeId first_enabled_in_order(const FocusRuntime &runtime) {
  for (int i = 0; i < runtime.focusable_count; ++i) {
    if (!runtime.focusables[i].disabled)
      return runtime.focusables[i].id;
  }
  return 0;
}

NodeId last_enabled(const FocusRuntime &runtime) {
  for (int i = runtime.focusable_count - 1; i >= 0; --i) {
    if (!runtime.focusables[i].disabled)
      return runtime.focusables[i].id;
  }
  return 0;
}

NodeId resolve_sequential(const FocusRuntime &runtime, NodeId from_id,
                          bool reverse) {
  if (runtime.focusable_count <= 0)
    return 0;

  int start = reverse ? runtime.focusable_count : -1;
  for (int i = 0; i < runtime.focusable_count; ++i) {
    if (same_id(runtime.focusables[i].id, from_id)) {
      start = i;
      break;
    }
  }

  for (int offset = 1; offset <= runtime.focusable_count; ++offset) {
    int idx = reverse ? start - offset : start + offset;
    idx %= runtime.focusable_count;
    if (idx < 0)
      idx += runtime.focusable_count;
    const FocusableLayout &candidate = runtime.focusables[idx];
    if (!candidate.disabled)
      return candidate.id;
  }
  return 0;
}

NodeId hovered_enabled(const FocusRuntime &runtime, const InputFrame &input) {
  if (!input.pointer_valid)
    return 0;
  for (int i = runtime.focusable_count - 1; i >= 0; --i) {
    const FocusableLayout &layout = runtime.focusables[i];
    if (!layout.disabled &&
        contains(layout.rect, input.pointer_x, input.pointer_y)) {
      return layout.id;
    }
  }
  return 0;
}

NodeId resolve_spatial(const FocusRuntime &runtime, NodeId from_id,
                       FocusDirection dir) {
  const FocusableLayout *from = find_layout(runtime, from_id);
  if (!from || from->disabled) {
    // Neutral entry: enter from the edge matching the direction. Down/Right
    // come in at the top/left (first in order); Up/Left at the bottom/right
    // (last in order).
    if (dir == FocusDirection::Up || dir == FocusDirection::Left)
      return last_enabled(runtime);
    return first_enabled_in_order(runtime);
  }

  const FocusableLayout *best = nullptr;
  float best_perp = 0.0f;
  float best_primary = 0.0f;
  float best_center = 0.0f;
  bool best_in_container = false;

  for (int i = 0; i < runtime.focusable_count; ++i) {
    const FocusableLayout *candidate = &runtime.focusables[i];
    if (same_id(candidate->id, from_id) || candidate->disabled)
      continue;
    if (!is_candidate_in_direction(from->rect, candidate->rect, dir))
      continue;
    if (has_directional_focus_descendant(runtime, *candidate, *from, dir))
      continue;

    float primary = primary_distance(from->rect, candidate->rect, dir);
    if (primary < 0.0f)
      primary = 0.0f;

    float perp = perpendicular_miss(from->rect, candidate->rect, dir);
    float dx = center_x(candidate->rect) - center_x(from->rect);
    float dy = center_y(candidate->rect) - center_y(from->rect);
    float center = dx * dx + dy * dy;

    // SIL-213: when `from` lives in a scrolling container, prefer candidates in
    // that SAME container so directional nav walks every row in order (even ones
    // scrolled off-screen, whose nav rect now sits outside the viewport) before
    // it leaves the list. Without this, a row at the viewport edge would jump to
    // a closer node OUTSIDE the list (e.g. the preset row just above the window),
    // skipping the off-screen rows the user is trying to reach.
    bool in_container = from->scroll_container != 0 &&
                        same_id(candidate->scroll_container,
                                from->scroll_container);

    bool better;
    if (!best) {
      better = true;
    } else if (in_container != best_in_container) {
      better = in_container; // same-container candidates always win
    } else {
      better = perp < best_perp ||
               (perp == best_perp && primary < best_primary) ||
               (perp == best_perp && primary == best_primary &&
                center < best_center) ||
               (perp == best_perp && primary == best_primary &&
                center == best_center && candidate->order < best->order);
    }

    if (better) {
      best = candidate;
      best_perp = perp;
      best_primary = primary;
      best_center = center;
      best_in_container = in_container;
    }
  }

  return best ? best->id : from_id;
}

} // namespace

void focus_init(FocusRuntime *runtime) {
  if (!runtime)
    return;
  *runtime = {};
}

bool focus_update(FocusRuntime *runtime, const UiTree &tree,
                  const InputFrame &input) {
  if (!runtime || !tree.contains(tree.root_id()))
    return false;

  runtime->focusable_count = 0;
  runtime->confirmed_id = 0;
  runtime->confirmed_by_pointer = false;
  runtime->focus_changed_id = 0;
  runtime->blurred_id = 0;

  NodeId active_scope = tree.root_id();
  if (!find_active_modal(tree, tree.root_id(), &active_scope)) {
    ++runtime->error_count;
    return false;
  }
  NodeId previous_scope = runtime->active_scope_id;
  if (!same_id(previous_scope, active_scope) && runtime->focused_id != 0 &&
      tree.contains(runtime->focused_id) &&
      !subtree_contains(tree, active_scope, runtime->focused_id)) {
    runtime->previous_focus_before_modal = runtime->focused_id;
  }
  runtime->active_scope_id = active_scope;

  uint32_t order = 0;
  if (!collect_focusables(tree, *runtime, active_scope, &order, 0, 0))
    return false;

  bool restore_parent_focus =
      previous_scope != 0 && !same_id(previous_scope, active_scope) &&
      runtime->previous_focus_before_modal != 0 &&
      contains_enabled(*runtime, runtime->previous_focus_before_modal);
  // When a nav input is present this frame, leave focus neutral so the nav
  // pass below enters from the direction's matching edge (SIL-211). Without
  // nav (e.g. the focused element vanished), auto-focus the first enabled node.
  FocusDirection nav_probe = FocusDirection::Down;
  bool nav_present =
      input.nav_next || input.nav_previous || read_nav_dir(input, &nav_probe);
  if (restore_parent_focus) {
    set_focus(*runtime, runtime->previous_focus_before_modal,
              FocusSource::Programmatic);
    runtime->previous_focus_before_modal = 0;
  } else if (!nav_present && !contains_enabled(*runtime, runtime->focused_id)) {
    NodeId next = first_enabled(*runtime);
    set_focus(*runtime, next,
              next != 0 ? FocusSource::Programmatic : FocusSource::None);
  }

  FocusDirection dir = FocusDirection::Down;
  if (input.nav_next || input.nav_previous) {
    NodeId next =
        resolve_sequential(*runtime, runtime->focused_id, input.nav_previous);
    if (next != 0 && !same_id(next, runtime->focused_id)) {
      set_focus(*runtime, next, navigation_source(input));
    }
  } else if (read_nav_dir(input, &dir) &&
             !focused_text_input_should_handle_navigation(tree, *runtime, input,
                                                          dir)) {
    NodeId next = resolve_spatial(*runtime, runtime->focused_id, dir);
    if (next != 0 && !same_id(next, runtime->focused_id)) {
      set_focus(*runtime, next, navigation_source(input));
    }
  }

  if (input.pointer_pressed) {
    NodeId hovered = hovered_enabled(*runtime, input);
    runtime->pointer_press_origin = hovered;
    if (hovered != 0) {
      set_focus(*runtime, hovered, pointer_source(input));
    }
  }

  if (input.pointer_released) {
    NodeId hovered = hovered_enabled(*runtime, input);
    if (same_id(runtime->pointer_press_origin, hovered)) {
      runtime->confirmed_id = hovered;
      runtime->confirmed_by_pointer = true;
    }
    runtime->pointer_press_origin = 0;
  } else if (!input.pointer_down) {
    runtime->pointer_press_origin = 0;
  }

  if (input.confirm_pressed &&
      contains_enabled(*runtime, runtime->focused_id)) {
    runtime->confirmed_id = runtime->focused_id;
    runtime->confirmed_by_pointer = false;
  }

  runtime->hovered_id = hovered_enabled(*runtime, input);

  // SIL-213: publish how far the focused node's nearest scrolling container must
  // move to bring it into view. The owning ScrollView reads this next build and
  // applies the delta to its local offset (one-frame lag, by design). Gate it on
  // focus having MOVED this frame via a keyboard/gamepad/programmatic source: a
  // continuously-published request would fight wheel/pointer scrolling (the user
  // wheels a focused row off-screen and scroll-into-view yanks it back). Firing
  // only on the nav edge means each Down/Up triggers exactly one follow-scroll,
  // and free wheel/drag scrolling is untouched.
  runtime->scroll_request = {};
  if (runtime->focus_changed_id != 0 &&
      focus_source_is_visible(runtime->source)) {
    runtime->scroll_request =
        compute_scroll_request(tree, runtime->focused_id);
  }

  return runtime->error_count == 0;
}

void compute_measured_sizes(const UiTree &tree, MeasuredSizeRequest *out) {
  if (!out)
    return;
  // Reset by zeroing the count only — the array is large (one slot per node), so
  // value-initializing a `{}` temporary would materialize it on the stack and
  // blow the frame. Stale tail entries past `count` are never read.
  out->count = 0;
  if (!tree.contains(tree.root_id()))
    return;
  collect_measured_sizes(tree, tree.root_id(), out);
}

NodeId focus_focused_id(const FocusRuntime &runtime) {
  return runtime.focused_id;
}

NodeId focus_blurred_id(const FocusRuntime &runtime) {
  return runtime.blurred_id;
}

NodeId focus_changed_id(const FocusRuntime &runtime) {
  return runtime.focus_changed_id;
}

NodeId focus_confirmed_id(const FocusRuntime &runtime) {
  return runtime.confirmed_id;
}

bool focus_confirmed_by_pointer(const FocusRuntime &runtime) {
  return runtime.confirmed_by_pointer;
}

NodeId focus_hovered_id(const FocusRuntime &runtime) {
  return runtime.hovered_id;
}

NodeId focus_pressed_id(const FocusRuntime &runtime) {
  return runtime.pointer_press_origin;
}

FocusSource focus_source(const FocusRuntime &runtime) { return runtime.source; }

const FocusScrollRequest &focus_scroll_request(const FocusRuntime &runtime) {
  return runtime.scroll_request;
}

int focus_error_count(const FocusRuntime &runtime) {
  return runtime.error_count;
}

} // namespace ui
