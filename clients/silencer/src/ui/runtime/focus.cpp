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
                        uint32_t *order) {
  NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return false;

  if (node.interaction.focusable) {
    if (runtime.focusable_count >= UI_RETAINED_MAX_FOCUSABLES) {
      ++runtime.error_count;
      return false;
    }
    runtime.focusables[runtime.focusable_count++] = {
        .id = id,
        .rect = node.layout,
        .disabled = node.interaction.disabled,
        .initial_focus = node.interaction.initial_focus,
        .order = (*order)++,
    };
  }

  for (int i = 0; i < tree.child_count(id); ++i) {
    if (!collect_focusables(tree, runtime, tree.child_at(id, i), order))
      return false;
  }
  return true;
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
  if (!from || from->disabled)
    return first_enabled(runtime);

  const FocusableLayout *best = nullptr;
  float best_perp = 0.0f;
  float best_primary = 0.0f;
  float best_center = 0.0f;

  for (int i = 0; i < runtime.focusable_count; ++i) {
    const FocusableLayout *candidate = &runtime.focusables[i];
    if (same_id(candidate->id, from_id) || candidate->disabled)
      continue;
    if (!is_candidate_in_direction(from->rect, candidate->rect, dir))
      continue;

    float primary = primary_distance(from->rect, candidate->rect, dir);
    if (primary < 0.0f)
      primary = 0.0f;

    float perp = perpendicular_miss(from->rect, candidate->rect, dir);
    float dx = center_x(candidate->rect) - center_x(from->rect);
    float dy = center_y(candidate->rect) - center_y(from->rect);
    float center = dx * dx + dy * dy;

    bool better = !best || perp < best_perp ||
                  (perp == best_perp && primary < best_primary) ||
                  (perp == best_perp && primary == best_primary &&
                   center < best_center) ||
                  (perp == best_perp && primary == best_primary &&
                   center == best_center && candidate->order < best->order);

    if (better) {
      best = candidate;
      best_perp = perp;
      best_primary = primary;
      best_center = center;
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
  if (!collect_focusables(tree, *runtime, active_scope, &order))
    return false;

  bool restore_parent_focus =
      previous_scope != 0 && !same_id(previous_scope, active_scope) &&
      runtime->previous_focus_before_modal != 0 &&
      contains_enabled(*runtime, runtime->previous_focus_before_modal);
  if (restore_parent_focus) {
    set_focus(*runtime, runtime->previous_focus_before_modal,
              FocusSource::Programmatic);
    runtime->previous_focus_before_modal = 0;
  } else if (!contains_enabled(*runtime, runtime->focused_id)) {
    NodeId next = first_enabled(*runtime);
    set_focus(*runtime, next,
              next != 0 ? FocusSource::Programmatic : FocusSource::None);
  }

  FocusDirection dir = FocusDirection::Down;
  if (read_nav_dir(input, &dir) && !focused_text_input_should_handle_navigation(
                                       tree, *runtime, input, dir)) {
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

  return runtime->error_count == 0;
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

int focus_error_count(const FocusRuntime &runtime) {
  return runtime.error_count;
}

} // namespace ui
