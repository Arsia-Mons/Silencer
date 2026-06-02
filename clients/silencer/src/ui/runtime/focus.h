#pragma once

#include "../input.h"
#include "tree.h"

#include <array>

namespace ui {

constexpr int UI_RETAINED_MAX_FOCUSABLES = UI_RETAINED_MAX_NODES;

enum class FocusDirection {
  Up,
  Down,
  Left,
  Right,
};

enum class FocusSource {
  None,
  Keyboard,
  Gamepad,
  Mouse,
  Touch,
  Programmatic,
};

struct InputFrame {
  bool nav_up = false;
  bool nav_down = false;
  bool nav_left = false;
  bool nav_right = false;

  bool confirm_pressed = false;

  bool pointer_pressed = false;
  bool pointer_down = false;
  bool pointer_released = false;
  bool pointer_valid = false;
  float pointer_x = 0.0f;
  float pointer_y = 0.0f;

  // Scroll-wheel delta this frame (+y = wheel up); routed to the hovered
  // scrollable. SIL-111.
  float wheel_x = 0.0f;
  float wheel_y = 0.0f;

  ::ui::UiKeyInputEvent key_events[::ui::UI_INPUT_MAX_KEY_EVENTS] = {};
  int key_event_count = 0;

  ::ui::UiTextInputEvent text_events[::ui::UI_INPUT_MAX_TEXT_EVENTS] = {};
  int text_event_count = 0;

  ::ui::UiTextEditingEvent editing_events[::ui::UI_INPUT_MAX_TEXT_EVENTS] = {};
  int editing_event_count = 0;

  FocusSource source = FocusSource::Keyboard;
};

struct FocusableLayout {
  NodeId id = 0;
  Rect rect = {};
  bool disabled = false;
  bool initial_focus = false;
  uint32_t order = 0;
};

struct FocusRuntime {
  std::array<FocusableLayout, UI_RETAINED_MAX_FOCUSABLES> focusables = {};
  int focusable_count = 0;

  NodeId active_scope_id = 0;
  NodeId previous_focus_before_modal = 0;
  NodeId focused_id = 0;
  NodeId blurred_id = 0;
  NodeId focus_changed_id = 0;
  NodeId hovered_id = 0; // top-most enabled focusable under the pointer this frame
  NodeId pointer_press_origin = 0;
  NodeId confirmed_id = 0;
  FocusSource source = FocusSource::None;
  int error_count = 0;
};

void focus_init(FocusRuntime *runtime);
bool focus_update(FocusRuntime *runtime, const UiTree &tree,
                  const InputFrame &input);

NodeId focus_focused_id(const FocusRuntime &runtime);
NodeId focus_blurred_id(const FocusRuntime &runtime);
NodeId focus_changed_id(const FocusRuntime &runtime);
NodeId focus_confirmed_id(const FocusRuntime &runtime);
NodeId focus_hovered_id(const FocusRuntime &runtime);
NodeId focus_pressed_id(const FocusRuntime &runtime);
FocusSource focus_source(const FocusRuntime &runtime);
int focus_error_count(const FocusRuntime &runtime);

// Focus is "visible" (gets a focus ring) only when it arrived via a non-pointer
// source. Used to derive focus_visible for the interaction snapshot.
inline bool focus_source_is_visible(FocusSource s) {
  return s == FocusSource::Keyboard || s == FocusSource::Gamepad ||
         s == FocusSource::Programmatic;
}

} // namespace ui
