#pragma once

#include "../runtime/element.h"
#include "../runtime/interaction_hooks.h" // use_hovered/pressed/focused/focus_visible
#include "../style/resolve.h"             // resolve(), RoleStyle, VisualStyle
#include "../style/theme.h"               // use_theme()

#include <functional>

namespace ui::components {

struct AccessibilityProps {
  ::ui::SemanticRole role = ::ui::SemanticRole::Auto;
  const char *label = nullptr;
  const char *description = nullptr;
};

namespace detail {

struct HostProps {
  ::ui::HostKind kind = ::ui::HostKind::Box;
  const char *key = nullptr;
  const char *id = nullptr;
  int id_offset = 0;
  ::ui::LayoutStyle style = {};
  ::ui::VisualStyle visual = {};
  ::ui::HostTextProps text = {};
  ::ui::NodeInteraction interaction = {};
  ::ui::TextEditMetadata text_edit = {};
  ::ui::AccessibilityProps accessibility = {};
  ::ui::HostCallbacks callbacks = {};
  ::ui::UiChildren children = {};
};

inline ::ui::UiElement Host(const HostProps &props) {
  return ::ui::host(props.kind, {
                                    .key = props.key,
                                    .id = props.id,
                                    .id_offset = props.id_offset,
                                    .style = props.style,
                                    .visual = props.visual,
                                    .text = props.text,
                                    .interaction = props.interaction,
                                    .text_edit = props.text_edit,
                                    .accessibility = props.accessibility,
                                    .callbacks = props.callbacks,
                                    .children = props.children,
                                });
}

template <typename Props> inline const char *component_key(const Props &props) {
  return props.key && props.key[0] != '\0' ? props.key : nullptr;
}

template <typename Props>
inline ::ui::NodeInteraction
interaction_from_props(const Props &props, bool default_focusable = false) {
  return {
      .focusable = default_focusable || props.focusable,
      .disabled = props.disabled,
      .initial_focus = props.autofocus,
  };
}

template <typename Props>
inline ::ui::AccessibilityProps
accessibility_from_props(const Props &props, ::ui::SemanticRole fallback_role) {
  ::ui::SemanticRole role = props.accessibility.role;
  if (role == ::ui::SemanticRole::Auto)
    role = fallback_role;
  return {
      .role = role,
      .label = props.accessibility.label,
      .description = props.accessibility.description,
  };
}

template <typename Props>
inline ::ui::HostCallbacks callbacks_from_props(const Props &props) {
  return {
      .on_focus = props.on_focus,
      .on_blur = props.on_blur,
      .on_activate = props.on_activate,
      .on_key = props.on_key,
      .on_wheel = props.on_wheel,
      .on_text_input = props.on_text_input,
      .on_text_editing = props.on_text_editing,
  };
}

// Reads this component's live interaction state (every-frame read; styling §7).
inline ::ui::InteractionState interaction_state(bool disabled,
                                                bool checked = false) {
  ::ui::InteractionState st{};
  st.hovered = ::ui::use_hovered();
  st.pressed = ::ui::use_pressed();
  st.focused = ::ui::use_focused();
  st.focus_visible = ::ui::use_focus_visible();
  st.checked = checked;
  st.disabled = disabled;
  return st;
}

// Layout-only seed for a control's LayoutStyle: reserves the 1px border box in
// Yoga (style.border_width feeds YGNodeStyleSetBorder) WITHOUT seeding any
// paint. Controls resolve their paint from the theme as .visual; the renderer
// reads node.visual exclusively, while this preserves the control's laid-out
// content box.
inline ::ui::LayoutStyle control_layout_style(::ui::LayoutStyle style = {}) {
  if (style.border_width <= 0.0f)
    style.border_width = 1.0f;
  return style;
}

} // namespace detail

} // namespace ui::components
