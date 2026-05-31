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

bool use_focused();       // this component's host is the focused node
bool use_hovered();       // ... is the hovered node
bool use_pressed();       // ... is the pressed (pointer-down) node
bool use_focus_visible(); // focused AND focus arrived via a non-pointer source

} // namespace ui
