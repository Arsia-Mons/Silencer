#include "interaction_hooks.h"

namespace ui {

ReactContext InteractionContext = {};

namespace {
const InteractionSnapshot *current_snapshot() {
  return static_cast<const InteractionSnapshot *>(use_context(&InteractionContext));
}
} // namespace

bool use_focused() {
  const InteractionSnapshot *s = current_snapshot();
  ReactFiberId me = react_current_fiber_id();
  return s && me != 0 && s->focused_fiber == me;
}

bool use_hovered() {
  const InteractionSnapshot *s = current_snapshot();
  ReactFiberId me = react_current_fiber_id();
  return s && me != 0 && s->hovered_fiber == me;
}

bool use_pressed() {
  const InteractionSnapshot *s = current_snapshot();
  ReactFiberId me = react_current_fiber_id();
  return s && me != 0 && s->pressed_fiber == me;
}

bool use_focus_visible() {
  const InteractionSnapshot *s = current_snapshot();
  ReactFiberId me = react_current_fiber_id();
  return s && me != 0 && s->focused_fiber == me &&
         focus_source_is_visible(s->source);
}

} // namespace ui
