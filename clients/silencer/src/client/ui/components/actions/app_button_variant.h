#pragma once

#include "client/ui/components/actions/app_button.h"
#include "client/ui/components/tokens.h"
#include "ui/style/style_patch.h"

#include <cstdint>

namespace silencer {
namespace client_ui {
namespace app_button_detail {

inline float label_advance(AppButtonSize size) {
	return size == AppButtonSize::MainMenu ? 11.0f : 8.0f;
}

inline float label_height(AppButtonSize size) {
	return size == AppButtonSize::MainMenu ? 19.0f : 15.0f;
}

inline float fit_content_width(const char * label, AppButtonSize size) {
	const char * safe = label ? label : "";
	float width = 0.0f;
	while(*safe++){
		width += label_advance(size);
	}
	return width + 20.0f;
}

inline ::ui::LayoutStyle layout(AppButtonSize size, const char * label = nullptr) {
	switch(size){
	case AppButtonSize::MainMenu:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(196.0f),
			.height = ::ui::Length::points(33.0f),
			.padding = {14.0f, 14.0f, 8.0f, 8.0f},
		};
	case AppButtonSize::FitContent:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(fit_content_width(label, size)),
			.height = ::ui::Length::points(21.0f),
			.padding = {10.0f, 10.0f, 4.0f, 2.0f},
		};
	case AppButtonSize::Chrome:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(156.0f),
			.height = ::ui::Length::points(21.0f),
			.padding = {10.0f, 10.0f, 4.0f, 2.0f},
		};
	case AppButtonSize::Sm:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(128.0f),
			.height = ::ui::Length::points(32.0f),
			.padding = {12.0f, 12.0f, 7.0f, 7.0f},
		};
	case AppButtonSize::Md:
	default:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(156.0f),
			.height = ::ui::Length::points(38.0f),
			.padding = {14.0f, 14.0f, 8.0f, 8.0f},
		};
	}
}

inline std::uint32_t texture_id(std::uint8_t bank, std::uint16_t index) {
	return (static_cast<std::uint32_t>(bank) << 16) |
	       static_cast<std::uint32_t>(index);
}

inline ::ui::BackgroundImage sprite(std::uint8_t bank,
                                    std::uint16_t index,
                                    ::ui::SideWidths nine_slice = {}) {
	return {
		.texture_id = texture_id(bank, index),
		.tint = {255, 255, 255, 255},
		.nine_slice = nine_slice,
	};
}

inline ::ui::Border transparent_border() {
	return ::ui::Border{};
}

inline ::ui::StylePatch transparent_control_patch() {
	return ::ui::patch()
		.background(::ui::Color{0, 0, 0, 0})
		.corner_radius(0.0f)
		.border(transparent_border())
		.outline(::ui::Outline{});
}

inline ::ui::TextVisual sprite_text(AppButtonSize size) {
	if(size == AppButtonSize::MainMenu){
		return ::ui::TextVisual{
			.color = {0, 128, 0, 255},
			.font_id = 135,
			.font_size = 11,
			.align = ::ui::TextAlign::Center,
		};
	}
	return ::ui::TextVisual{
		.color = {0, 128, 0, 255},
		.font_id = 134,
		.font_size = 8,
		.align = ::ui::TextAlign::Center,
	};
}

inline ::ui::StylePatch sprite_patch(std::uint8_t bank,
                                     std::uint16_t index,
                                     AppButtonSize size,
                                     ::ui::SideWidths nine_slice = {}) {
	::ui::StylePatch patch = transparent_control_patch();
	patch.image = ::ui::opt(sprite(bank, index, nine_slice));
	patch.text = ::ui::opt(sprite_text(size));
	return patch;
}

inline ::ui::StyleStatePatch sprite_state_patch(std::uint8_t bank,
                                                std::uint16_t index,
                                                AppButtonSize size,
                                                ::ui::SideWidths nine_slice = {}) {
	::ui::StyleStatePatch patch{};
	patch.base = sprite_patch(bank, index, size, nine_slice);
	patch.hover = transparent_control_patch();
	patch.focus_visible = transparent_control_patch();
	patch.pressed = transparent_control_patch();
	patch.disabled = transparent_control_patch();
	return patch;
}

inline ::ui::StyleStatePatch variant_patch(AppButtonVariant variant,
                                           AppButtonSize size) {
	if(size == AppButtonSize::MainMenu){
		(void)variant;
		return sprite_state_patch(6, 7, size);
	}
	if(variant == AppButtonVariant::Secondary){
		return sprite_state_patch(
			7,
			24,
			size,
			::ui::SideWidths{4.0f, 12.0f, 4.0f, 12.0f});
	}

	auto solid = [](::ui::Color fill, ::ui::Color border) {
		return ::ui::patch()
			.background(fill)
			.gradient(::ui::Gradient{
				.angle_deg = 0.0f,
				.stop_count = 2,
				.stops = {{0.0f, fill}, {1.0f, fill}},
			})
			.border(::ui::Border{
				{tokens::kBorderWidth, tokens::kBorderWidth,
				 tokens::kBorderWidth, tokens::kBorderWidth},
				{border, border, border, border},
			})
			.text(tokens::font_text(tokens::kTextButton,
			                        tokens::kFontBodyBank,
			                        tokens::kFontBodyAdvance));
	};

	switch(variant){
	case AppButtonVariant::Primary: {
		::ui::StyleStatePatch patch{};
		patch.base = solid(tokens::kAccent, tokens::kAccentHover);
		patch.hover = solid(tokens::kAccentHover, tokens::kAccentHover);
		patch.pressed = solid(tokens::kAccentPressed, tokens::kAccent);
		return patch;
	}
	case AppButtonVariant::Ghost: {
		const ::ui::Color transparent = {0, 0, 0, 0};
		::ui::StyleStatePatch patch{};
		patch.base = solid(transparent, transparent);
		patch.hover = solid({255, 255, 255, 18}, transparent);
		patch.pressed = solid({255, 255, 255, 30}, transparent);
		return patch;
	}
	case AppButtonVariant::Secondary:
	default:
		return {};
	}
}

}  // namespace app_button_detail
}  // namespace client_ui
}  // namespace silencer
