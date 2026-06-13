#include "interaction_hooks.h"

#include <string.h>

namespace ui {

ReactContext InteractionContext = {};
ReactContext FocusScrollContext = {};

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

float use_scroll_into_view(const char *viewport_control_id) {
  const FocusScrollRequest *r =
      static_cast<const FocusScrollRequest *>(use_context(&FocusScrollContext));
  if (!r || !r->valid || r->delta_y == 0.0f || !viewport_control_id)
    return 0.0f;
  if (strcmp(r->viewport_control_id, viewport_control_id) != 0)
    return 0.0f;
  return r->delta_y;
}

} // namespace ui
