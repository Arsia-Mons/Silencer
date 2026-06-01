#pragma once

#include "client/ui/components/actions/app_button.h"
#include "client/ui/components/tokens.h"
#include "ui/style/style_patch.h"

#include <cstdint>

namespace silencer {
namespace client_ui {
namespace app_button_detail {

inline ::ui::LayoutStyle layout(AppButtonSize size) {
	switch(size){
	case AppButtonSize::MainMenu:
		return {
			.align_items = ::ui::AlignItems::Center,
			.justify_content = ::ui::JustifyContent::Center,
			.width = ::ui::Length::points(196.0f),
			.height = ::ui::Length::points(33.0f),
			.padding = {14.0f, 14.0f, 8.0f, 8.0f},
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

inline ::ui::StylePatch sprite_patch(std::uint8_t bank,
                                     std::uint16_t index,
                                     ::ui::SideWidths nine_slice = {}) {
	return ::ui::patch()
		.image(sprite(bank, index, nine_slice))
		.text(tokens::font_text(tokens::kTextButton,
		                        tokens::kFontBodyBank,
		                        tokens::kFontBodyAdvance));
}

inline ::ui::StyleStatePatch variant_patch(AppButtonVariant variant,
                                           AppButtonSize size) {
	if(size == AppButtonSize::MainMenu){
		(void)variant;
		return sprite_patch(6, 7);
	}
	if(variant == AppButtonVariant::Secondary){
		return sprite_patch(
			7,
			24,
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
