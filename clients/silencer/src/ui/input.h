#pragma once

#include <stdint.h>
#include <string.h>

namespace ui {

constexpr int UI_INPUT_MAX_KEY_EVENTS = 16;
constexpr int UI_INPUT_MAX_TEXT_EVENTS = 8;
constexpr int UI_INPUT_TEXT_CAP = 64;

enum class UiFocusSource {
  None,
  Keyboard,
  Gamepad,
  Mouse,
  Touch,
  Programmatic,
};

enum class UiKey {
  Unknown,
  Backspace,
  DeleteForward,
  Left,
  Right,
  Home,
  End,
  Enter,
  Tab,
  A,
  C,
  V,
  X,
  Up,
  Down,
  PageUp,
  PageDown,
};

enum UiKeyModifier : uint16_t {
  UI_KEY_MOD_NONE = 0,
  UI_KEY_MOD_SHIFT = 1u << 0u,
  UI_KEY_MOD_CTRL = 1u << 1u,
  UI_KEY_MOD_ALT = 1u << 2u,
  UI_KEY_MOD_SUPER = 1u << 3u,
};

struct UiKeyInputEvent {
  UiKey key = UiKey::Unknown;
  uint16_t modifiers = UI_KEY_MOD_NONE;
  bool repeat = false;
};

struct UiTextInputEvent {
  char text[UI_INPUT_TEXT_CAP] = {};
};

struct UiTextEditingEvent {
  char text[UI_INPUT_TEXT_CAP] = {};
  int start = 0;
  int length = 0;
};

struct UiInputFrame {
  bool nav_up = false;
  bool nav_down = false;
  bool nav_left = false;
  bool nav_right = false;
  bool nav_next = false;
  bool nav_previous = false;

  bool confirm_pressed = false;
  bool confirm_down = false;
  bool confirm_released = false;

  bool cancel_pressed = false;
  bool cancel_down = false;
  bool cancel_released = false;

  bool pointer_pressed = false;
  bool pointer_down = false;
  bool pointer_released = false;
  // Press-edge detail: OS click streak (SDL button.clicks; 2 = double-click)
  // and the modifier state at press time (shift+click extends a selection).
  // 0 clicks (synthetic/injected presses) reads as a single click.
  int pointer_clicks = 0;
  uint16_t pointer_mods = UI_KEY_MOD_NONE;

  // Scroll-wheel delta this frame (+y = wheel up); routed to the hovered
  // scrollable.
  float wheel_x = 0.0f;
  float wheel_y = 0.0f;

  UiKeyInputEvent key_events[UI_INPUT_MAX_KEY_EVENTS] = {};
  int key_event_count = 0;

  UiTextInputEvent text_events[UI_INPUT_MAX_TEXT_EVENTS] = {};
  int text_event_count = 0;

  UiTextEditingEvent editing_events[UI_INPUT_MAX_TEXT_EVENTS] = {};
  int editing_event_count = 0;

  UiFocusSource source = UiFocusSource::Keyboard;
};

inline bool ui_input_push_key(UiInputFrame &frame, UiKey key,
                              uint16_t modifiers = UI_KEY_MOD_NONE,
                              bool repeat = false) {
  if (frame.key_event_count >= UI_INPUT_MAX_KEY_EVENTS)
    return false;
  frame.key_events[frame.key_event_count++] = {
      .key = key,
      .modifiers = modifiers,
      .repeat = repeat,
  };
  return true;
}

inline bool ui_input_push_text(UiInputFrame &frame, const char *text) {
  if (frame.text_event_count >= UI_INPUT_MAX_TEXT_EVENTS)
    return false;
  UiTextInputEvent &event = frame.text_events[frame.text_event_count++];
  const char *safe_text = text ? text : "";
  strncpy(event.text, safe_text, UI_INPUT_TEXT_CAP - 1);
  event.text[UI_INPUT_TEXT_CAP - 1] = '\0';
  return true;
}

inline bool ui_input_push_editing(UiInputFrame &frame, const char *text,
                                  int start, int length) {
  if (frame.editing_event_count >= UI_INPUT_MAX_TEXT_EVENTS)
    return false;
  UiTextEditingEvent &event = frame.editing_events[frame.editing_event_count++];
  const char *safe_text = text ? text : "";
  strncpy(event.text, safe_text, UI_INPUT_TEXT_CAP - 1);
  event.text[UI_INPUT_TEXT_CAP - 1] = '\0';
  event.start = start;
  event.length = length;
  return true;
}

} // namespace ui
