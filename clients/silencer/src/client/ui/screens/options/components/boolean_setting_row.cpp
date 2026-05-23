#include "boolean_setting_row.h"

#include "clay_ui_compositor.h"

#include <cstdint>

namespace silencer::client_ui::options {

namespace {

constexpr uint16_t kIndicatorGap = 10;
constexpr uint16_t kIndicatorSpriteW = 20;
constexpr uint16_t kIndicatorSpriteH = 33;

}  // namespace

void BooleanSettingIndicator(Clay_String id, bool selected)
{
	CLAY({ .id = CLAY_SIDI(id, 2),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(kIndicatorSpriteH) },
	           .childGap = kIndicatorGap,
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_SIDI(id, 3),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kIndicatorSpriteW), CLAY_SIZING_FIXED(kIndicatorSpriteH) },
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImage(
		                     6, selected ? 12 : 13) } }) {}
		CLAY({ .id = CLAY_SIDI(id, 4),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kIndicatorSpriteW), CLAY_SIZING_FIXED(kIndicatorSpriteH) },
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImage(
		                     6, selected ? 15 : 14) } }) {}
	}
}

}  // namespace silencer::client_ui::options
