#pragma once

#include "ui/components/common.h"
#include "ui/style/style_patch.h"

#include <cstdint>

namespace silencer {
namespace client_ui {
namespace tokens {

constexpr ::ui::Color kSurfaceMenu = {8, 12, 16, 255};
constexpr ::ui::Color kSurfacePanel = {17, 23, 30, 245};
constexpr ::ui::Color kBorderPanel = {88, 104, 118, 255};
constexpr ::ui::Color kAccent = {92, 164, 224, 255};
constexpr ::ui::Color kAccentHover = {116, 184, 242, 255};
constexpr ::ui::Color kAccentPressed = {70, 138, 202, 255};
constexpr ::ui::Color kTextTitle = {236, 244, 240, 255};
constexpr ::ui::Color kTextMuted = {160, 176, 186, 255};
constexpr ::ui::Color kTextButton = {238, 246, 242, 255};

constexpr std::uint16_t kFontTitleBank = 135;
constexpr std::uint16_t kFontTitleAdvance = 11;
constexpr std::uint16_t kFontBodyBank = 133;
constexpr std::uint16_t kFontBodyAdvance = 7;
constexpr std::uint16_t kFontFooterBank = 133;
constexpr std::uint16_t kFontFooterAdvance = 11;
constexpr float kBorderWidth = 1.0f;

inline ::ui::StylePatch fill_patch(::ui::Color background) {
	return ::ui::patch().background(background);
}

inline ::ui::StylePatch panel_patch(::ui::Color background,
                                    ::ui::Color border,
                                    float border_width = kBorderWidth) {
	return ::ui::patch()
		.background(background)
		.border(::ui::Border{
			{border_width, border_width, border_width, border_width},
			{border, border, border, border},
		});
}

inline ::ui::TextVisual font_text(::ui::Color color,
                                  std::uint16_t font_id,
                                  std::uint16_t font_size) {
	return ::ui::TextVisual{
		.color = color,
		.font_id = font_id,
		.font_size = font_size,
	};
}

inline ::ui::EdgeSizes inset_right_top(float right, float top) {
	::ui::EdgeSizes edges;
	edges.right = ::ui::StyleValue::points(right);
	edges.top = ::ui::StyleValue::points(top);
	return edges;
}

inline ::ui::EdgeSizes inset_left_bottom(float left, float bottom) {
	::ui::EdgeSizes edges;
	edges.left = ::ui::StyleValue::points(left);
	edges.bottom = ::ui::StyleValue::points(bottom);
	return edges;
}

inline ::ui::StylePatch text_patch(::ui::Color color,
                                   std::uint16_t font_id,
                                   std::uint16_t font_size) {
	return ::ui::patch().text(::ui::TextVisual{
		.color = color,
		.font_id = font_id,
		.font_size = font_size,
	});
}

}  // namespace tokens
}  // namespace client_ui
}  // namespace silencer
