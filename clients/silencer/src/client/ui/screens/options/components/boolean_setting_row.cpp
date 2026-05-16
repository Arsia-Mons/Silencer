#include "boolean_setting_row.h"

#include "clay_ui_compositor.h"
#include "primitives/button.h"
#include "runtime/UiInteractionRegistry.h"

#include <cstdint>

namespace silencer::client_ui::options {

namespace {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

constexpr uint16_t kRowH = 33;
constexpr uint16_t kButtonIndicatorGap = 24;
constexpr uint16_t kIndicatorGap = 10;
constexpr uint16_t kIndicatorSpriteW = 20;
constexpr uint16_t kIndicatorSpriteH = 33;

void BooleanIndicator(Clay_String id, bool selected)
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

}  // namespace

void BooleanSettingRow(Clay_String id,
                       Clay_String buttonId,
                       Clay_String label,
                       bool selected,
                       const char * actionId,
                       silencer::ui::UiInteractionRegistry& interactions)
{
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(kRowH) },
	           .childGap = kButtonIndicatorGap,
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		Button(buttonId, label,
		       ButtonOpts{ .variant = ButtonVariant::Oval, .size = ButtonSize::Lg },
		       ButtonHandle{ nullptr, actionId, &interactions });
		BooleanIndicator(id, selected);
	}
}

}  // namespace silencer::client_ui::options
