#pragma once

#include "ui/components/common.h"
#include "ui/components/text.h"
#include "ui/runtime/tree.h"
#include "ui/style/style_patch.h"

#include <cstdint>
#include <functional>

namespace silencer::client_ui::style {

inline std::uint32_t SpriteTextureId(std::uint8_t bank, std::uint16_t index) {
	return (static_cast<std::uint32_t>(bank) << 16u) |
	       static_cast<std::uint32_t>(index);
}

inline ::ui::Border SolidBorder(::ui::Color color, float width = 1.0f) {
	return ::ui::Border{
		.width = {width, width, width, width},
		.color = {color, color, color, color},
	};
}

inline ::ui::StyleStatePatch SpriteBackground(std::uint8_t bank = 6,
                                               std::uint16_t index = 0) {
	return ::ui::patch().image(::ui::BackgroundImage{
		.texture_id = SpriteTextureId(bank, index),
		.tint = {255, 255, 255, 255},
	});
}

inline ::ui::StyleStatePatch LegacyOvalButtonStyle(std::uint16_t spriteIndex = 7) {
	::ui::StyleStatePatch style{};
	style.base = SpriteBackground(6, spriteIndex).base;
	style.hover = SpriteBackground(6, static_cast<std::uint16_t>(spriteIndex + 4)).base;
	style.focus_visible = SpriteBackground(6, static_cast<std::uint16_t>(spriteIndex + 4)).base;
	style.pressed = SpriteBackground(6, static_cast<std::uint16_t>(spriteIndex + 4)).base;
	style.disabled = SpriteBackground(6, spriteIndex).base;
	return style;
}

inline ::ui::StyleStatePatch LegacyTextButtonStyle() {
	::ui::StyleStatePatch style{};
	style.base = ::ui::patch().background(::ui::Color{0, 0, 0, 0});
	style.hover = ::ui::patch().background(::ui::Color{0, 0, 0, 0});
	style.focus_visible = ::ui::patch().background(::ui::Color{0, 0, 0, 0});
	style.pressed = ::ui::patch().background(::ui::Color{0, 0, 0, 0});
	style.disabled = ::ui::patch().background(::ui::Color{0, 0, 0, 0});
	return style;
}

inline ::ui::StyleStatePatch LegacyButtonStyle() {
	return LegacyOvalButtonStyle();
}

inline ::ui::StyleStatePatch LegacyButtonTextStyle(std::uint16_t fontSize = 10) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = {0, 207, 38, 255},
		.font_id = 135,
		.font_size = fontSize,
		.align = ::ui::TextAlign::Center,
	});
}

inline ::ui::LayoutStyle ButtonLabelLayout(float height = 19.0f) {
	return ::ui::LayoutStyle{
		.width = ::ui::Length::percent(100.0f),
		.height = ::ui::Length::points(height),
	};
}

inline ::ui::StyleStatePatch LegacyTitleTextStyle(std::uint16_t fontSize = 14) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = {0, 207, 38, 255},
		.font_id = 133,
		.font_size = fontSize,
		.align = ::ui::TextAlign::Center,
	});
}

inline ::ui::StyleStatePatch LegacyBodyTextStyle(std::uint16_t fontSize = 8) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = {0, 207, 38, 255},
		.font_id = 133,
		.font_size = fontSize,
	});
}

inline ::ui::StyleStatePatch LegacyHeadingTextStyle(std::uint16_t fontSize = 8) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = {0, 207, 38, 255},
		.font_id = 134,
		.font_size = fontSize,
	});
}

inline ::ui::StyleStatePatch LegacyScreenTitleTextStyle(std::uint16_t fontSize = 12) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = {0, 207, 38, 255},
		.font_id = 135,
		.font_size = fontSize,
		.align = ::ui::TextAlign::Center,
	});
}

struct LegacyButtonProps {
	const char * key = nullptr;
	const char * id = nullptr;
	int id_offset = 0;
	bool disabled = false;
	bool focusable = false;
	bool autofocus = false;
	::ui::components::AccessibilityProps accessibility = {};
	std::function<void(const ::ui::FocusEvent &)> on_focus = {};
	std::function<void(const ::ui::BlurEvent &)> on_blur = {};
	std::function<void(const ::ui::KeyEvent &)> on_key = {};
	std::function<void(const ::ui::TextInputEvent &)> on_text_input = {};
	std::function<void(const ::ui::TextEditingEvent &)> on_text_editing = {};
	const char * label = nullptr;
	std::function<void(const ::ui::ActivationEvent &)> on_activate = {};
	::ui::LayoutStyle layout = {
		.direction = ::ui::FlexDirection::Column,
		.align_items = ::ui::AlignItems::Center,
		.justify_content = ::ui::JustifyContent::Start,
		.width = ::ui::Length::points(196.0f),
		.height = ::ui::Length::points(33.0f),
		.padding = {24.0f, 24.0f, 8.0f, 7.0f},
	};
	::ui::StyleStatePatch style = LegacyOvalButtonStyle();
	::ui::StyleStatePatch text_style = LegacyButtonTextStyle(11);
	::ui::LayoutStyle text_layout = ButtonLabelLayout();
};

inline ::ui::UiElement LegacyButton(const LegacyButtonProps& props) {
	::ui::NodeInteraction interaction =
		::ui::components::detail::interaction_from_props(props, true);
	const ::ui::VisualStyle visual = ::ui::resolve(
		::ui::use_theme().box,
		props.style,
		::ui::components::detail::interaction_state(props.disabled));
	return ::ui::components::detail::Host(::ui::components::detail::HostProps{
		.kind = ::ui::HostKind::Box,
		.key = props.key,
		.id = props.id,
		.id_offset = props.id_offset,
		.style = props.layout,
		.visual = visual,
		.text = ::ui::HostTextProps{.value = props.label},
		.interaction = interaction,
		.accessibility = ::ui::components::detail::accessibility_from_props(
			props,
			::ui::SemanticRole::Button),
		.callbacks = ::ui::components::detail::callbacks_from_props(props),
		.children = ::ui::children({
			::ui::components::Text(::ui::components::TextProps{
				.key = "label",
				.value = props.label,
				.layout = props.text_layout,
				.style = props.text_style,
			}),
		}),
	});
}

}  // namespace silencer::client_ui::style
