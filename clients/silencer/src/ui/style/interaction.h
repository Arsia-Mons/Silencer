#pragma once

// InteractionState: the per-frame interaction flags a component feeds to
// resolve(). hovered/pressed/focused/focus_visible come from the runtime
// interaction hooks (every-frame read); checked/active/disabled are
// component-supplied (no framework hook). See design §5.1, §7.

namespace ui {

struct InteractionState {
  bool hovered = false;
  bool pressed = false;
  bool focused = false;
  bool focus_visible = false;
  bool checked = false;
  bool active = false; // component-supplied; precedence between checked & disabled
  bool disabled = false;
};

} // namespace ui
