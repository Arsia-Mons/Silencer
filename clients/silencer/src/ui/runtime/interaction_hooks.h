#pragma once

// Every-frame interaction reads. The runtime publishes the previous frame's
// focus/hit-test results as an InteractionSnapshot at the tree root; a component
// reads its own state by comparing react_current_fiber_id() to the snapshot's
// per-state fiber ids (keyed by fiber, one-frame lag). See design §7.

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

// The focused node's scroll-into-view request, published at the tree root; a
// scrollable reads it and, if it targets its own control id, applies the delta.
extern ReactContext FocusScrollContext; // current = const FocusScrollRequest*

// The previous frame's post-layout node heights (keyed by control id); a
// component reads back its own resolved height during render (one-frame lag).
extern ReactContext MeasuredSizeContext; // current = const MeasuredSizeRequest*

bool use_focused();       // this component's host is the focused node
bool use_hovered();
bool use_pressed();
bool use_focus_visible(); // focused AND focus arrived via a non-pointer source

// Scroll-into-view delta for this container, or 0 if the request targets a
// different control id (or there is none).
float use_scroll_into_view(const char *viewport_control_id);

// Resolved height of the node carrying `control_id` from the previous frame;
// 0 until measured at least once.
float use_measured_height(const char *control_id);

} // namespace ui
