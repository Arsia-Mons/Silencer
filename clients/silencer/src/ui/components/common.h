#pragma once

#include "ui/runtime/element.h"
#include "ui/runtime/interaction_hooks.h"
#include "ui/style/resolve.h"
#include "ui/style/theme.h"

#include <functional>

namespace ui {
namespace components {

struct AccessibilityProps {
	::ui::SemanticRole role = ::ui::SemanticRole::Auto;
	const char * label = nullptr;
	const char * description = nullptr;
};

namespace detail {

struct HostProps {
	::ui::HostKind kind = ::ui::HostKind::Box;
	const char * key = nullptr;
	const char * id = nullptr;
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

inline ::ui::UiElement Host(const HostProps& props) {
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

template <typename Props>
inline ::ui::NodeInteraction
interaction_from_props(const Props& props, bool default_focusable = false) {
	return {
		.focusable = default_focusable || props.focusable,
		.disabled = props.disabled,
		.initial_focus = props.autofocus,
	};
}

template <typename Props>
inline ::ui::AccessibilityProps
accessibility_from_props(const Props& props, ::ui::SemanticRole fallback_role) {
	::ui::SemanticRole role = props.accessibility.role;
	if(role == ::ui::SemanticRole::Auto){
		role = fallback_role;
	}
	return {
		.role = role,
		.label = props.accessibility.label,
		.description = props.accessibility.description,
	};
}

template <typename Props>
inline ::ui::HostCallbacks callbacks_from_props(const Props& props) {
	return {
		.on_focus = props.on_focus,
		.on_blur = props.on_blur,
		.on_activate = props.on_activate,
		.on_key = props.on_key,
		.on_text_input = props.on_text_input,
		.on_text_editing = props.on_text_editing,
	};
}

inline ::ui::InteractionState interaction_state(bool disabled) {
	::ui::InteractionState state{};
	state.hovered = ::ui::use_hovered();
	state.pressed = ::ui::use_pressed();
	state.focused = ::ui::use_focused();
	state.focus_visible = ::ui::use_focus_visible();
	state.disabled = disabled;
	return state;
}

inline bool has_visible_border(const ::ui::VisualStyle& visual) {
	return (visual.border.color.top.a > 0 && visual.border.width.top > 0.0f)
		|| (visual.border.color.right.a > 0 && visual.border.width.right > 0.0f)
		|| (visual.border.color.bottom.a > 0 && visual.border.width.bottom > 0.0f)
		|| (visual.border.color.left.a > 0 && visual.border.width.left > 0.0f);
}

inline ::ui::LayoutStyle control_layout_style(::ui::LayoutStyle style,
                                              const ::ui::VisualStyle& visual) {
	if(style.border_width <= 0.0f && has_visible_border(visual)){
		style.border_width = 1.0f;
	}
	return style;
}

}  // namespace detail
}  // namespace components
}  // namespace ui
