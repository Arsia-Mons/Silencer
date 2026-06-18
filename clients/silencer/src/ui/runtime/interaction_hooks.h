#pragma once

// Every-frame interaction reads. The runtime publishes an InteractionSnapshot
// (from the previous frame's focus/hit-test pass) at the tree root via
// InteractionContext; a component reads its own state during render by comparing
// react_current_fiber_id() to the snapshot's per-state fiber ids. The snapshot
// keys interaction by FIBER (the host node committed under a component's fiber is
// mapped back to that fiber when the snapshot is published), so a component knows
// "is THIS me?" without recomputing node-id hashes. One-frame lag is accepted.
// See design §7.

#include "react.h" // ReactContext, ReactFiberId, use_context, react_current_fiber_id
#include "focus.h"       // FocusSource, focus_source_is_visible

namespace ui {

struct InteractionSnapshot {
  ReactFiberId focused_fiber = 0;
  ReactFiberId hovered_fiber = 0;
  ReactFiberId pressed_fiber = 0;
  FocusSource source = FocusSource::None;
};

extern ReactContext InteractionContext; // current = const InteractionSnapshot*

// Scroll-into-view: the runtime publishes the focused node's
// scroll-into-view request (computed by the focus pass) at the tree root via
// FocusScrollContext. A scrollable container reads it during render and, if the
// request targets its own control id, applies the delta to its scroll offset.
extern ReactContext FocusScrollContext; // current = const FocusScrollRequest*

// Layout measurement read-back: the runtime publishes the previous frame's
// post-layout node heights (keyed by control id) at the tree root via
// MeasuredSizeContext. A component reads back its own resolved height during
// render (one-frame lag, by design — matches FocusScrollContext). This is what
// lets a flex-grown ScrollView learn its viewport/content sizes from layout.
extern ReactContext MeasuredSizeContext; // current = const MeasuredSizeRequest*

bool use_focused();       // this component's host is the focused node
bool use_hovered();       // ... is the hovered node
bool use_pressed();       // ... is the pressed (pointer-down) node
bool use_focus_visible(); // focused AND focus arrived via a non-pointer source

// The scroll-into-view delta the focused node's owning container should apply
// this frame, IF that container's control id == `viewport_control_id`. Returns 0
// (no scroll) when there is no request, or it targets a different container.
float use_scroll_into_view(const char *viewport_control_id);

// The resolved (post-layout) height of the node carrying `control_id`, from the
// previous frame. Returns 0 until that node has been measured at least once.
float use_measured_height(const char *control_id);

} // namespace ui
