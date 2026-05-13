#pragma once

#include <cstdint>

namespace silencer {
namespace ui {

struct UiColor {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;
};

namespace colors {
	constexpr UiColor Background{ 0, 0, 0, 255 };
	constexpr UiColor Panel{ 16, 20, 28, 255 };
	constexpr UiColor PanelBorder{ 86, 94, 111, 255 };
	constexpr UiColor Text{ 224, 231, 241, 255 };
	constexpr UiColor Accent{ 159, 201, 255, 255 };
	constexpr UiColor Danger{ 221, 80, 72, 255 };
}

}  // namespace ui
}  // namespace silencer
