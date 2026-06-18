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
  bool nav_next = false;
  bool nav_previous = false;

  bool confirm_pressed = false;

  bool pointer_pressed = false;
  bool pointer_down = false;
  bool pointer_released = false;
  bool pointer_valid = false;
  float pointer_x = 0.0f;
  float pointer_y = 0.0f;

  // Scroll-wheel delta this frame (+y = wheel up); routed to the hovered
  // scrollable.
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
  NodeId focus_parent = 0;
  // Nearest ancestor that scroll-clips this node (overflow Scroll/Hidden), or 0
  // if none. Directional nav prefers staying within the same scroll container so
  // it traverses every row in order (including scrolled-off ones) before leaving
  // the container.
  NodeId scroll_container = 0;
  Rect rect = {};
  bool disabled = false;
  bool initial_focus = false;
  uint32_t order = 0;
};

// Scroll-into-view request. After the focus pass resolves, the runtime
// computes — for the focused node's nearest scrolling-clip ancestor — how far
// that container must move its scroll offset to bring the focused node fully
// inside its clip rect. `delta_y` is signed (negative = scroll toward the top to
// reveal a node above the window; positive = scroll down to reveal one below);
// 0 means the focused node is already in view. A scrollable container reacts to
// this each frame (matching its own `control_id`) and applies the delta to its
// local offset. Keyed by control_id (a stable, screen-supplied scroll id) rather
// than node id so the SDL-free ScrollView component — which knows only its own
// `props.id` — can match without a fiber→node self-id lookup.
struct FocusScrollRequest {
  char viewport_control_id[UI_RETAINED_LABEL_CAP] = {};
  float delta_y = 0.0f;
  bool valid = false; // true when the focused node has a scrolling-clip ancestor
};

// Layout measurement read-back. Yoga sizes nodes during layout, but the
// component tree is built BEFORE layout runs, so a component cannot know its own
// resolved size during render. After layout the runtime walks the retained tree
// and records each node's resolved height keyed by its control id; a component
// reads its own measured height back the next build (one-frame lag, by design —
// the same model as FocusScrollRequest). This is what lets ScrollView flex-grow
// and learn its viewport/content sizes instead of taking height props.
constexpr int UI_RETAINED_MAX_MEASURED = UI_RETAINED_MAX_NODES;

struct MeasuredSize {
  char control_id[UI_RETAINED_LABEL_CAP] = {};
  float height = 0.0f;
};

struct MeasuredSizeRequest {
  std::array<MeasuredSize, UI_RETAINED_MAX_MEASURED> sizes = {};
  int count = 0;
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
  bool confirmed_by_pointer = false; // click-confirm vs keyboard confirm
  FocusSource source = FocusSource::None;
  // focus came from auto-default, not real input; the next nav treats it as
  // neutral and enters from the edge instead of advancing past it.
  bool focused_is_auto_default = false;
  FocusScrollRequest scroll_request = {}; // scroll-into-view this frame
  int error_count = 0;
};

void focus_init(FocusRuntime *runtime);
bool focus_update(FocusRuntime *runtime, const UiTree &tree,
                  const InputFrame &input);

// Walk the laid-out retained tree and record each node's resolved height keyed by
// its control id (only nodes with a non-empty control id). Must run AFTER layout.
void compute_measured_sizes(const UiTree &tree, MeasuredSizeRequest *out);

NodeId focus_focused_id(const FocusRuntime &runtime);
NodeId focus_blurred_id(const FocusRuntime &runtime);
NodeId focus_changed_id(const FocusRuntime &runtime);
NodeId focus_confirmed_id(const FocusRuntime &runtime);
bool focus_confirmed_by_pointer(const FocusRuntime &runtime);
NodeId focus_hovered_id(const FocusRuntime &runtime);
NodeId focus_pressed_id(const FocusRuntime &runtime);
FocusSource focus_source(const FocusRuntime &runtime);
const FocusScrollRequest &focus_scroll_request(const FocusRuntime &runtime);
int focus_error_count(const FocusRuntime &runtime);

// Focus is "visible" (gets a focus ring) only when it arrived via a non-pointer
// source. Used to derive focus_visible for the interaction snapshot.
inline bool focus_source_is_visible(FocusSource s) {
  return s == FocusSource::Keyboard || s == FocusSource::Gamepad ||
         s == FocusSource::Programmatic;
}

} // namespace ui
