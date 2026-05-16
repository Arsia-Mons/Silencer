#include "controls_keybind_list.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/button.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace silencer::client_ui::options {

namespace controls_keybind_list_detail {

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

constexpr uint16_t kRowH = 40;
constexpr uint16_t kRowGap = 8;
constexpr uint16_t kActionNameW = 168;
constexpr uint16_t kOperatorW = 45;
constexpr uint16_t kActionGap = 12;
constexpr const char * kActionPreset = "options_controls.preset";
constexpr const char * kActionSave = "options_controls.save";
constexpr const char * kActionCancel = "options_controls.cancel";
constexpr const char * kActionScrollUp = "options_controls.scroll_up";
constexpr const char * kActionScrollDown = "options_controls.scroll_down";
constexpr const char * kActionPrimaryPrefix = "options_controls.primary.";
constexpr const char * kActionSecondaryPrefix = "options_controls.secondary.";
constexpr const char * kActionOperatorPrefix = "options_controls.operator.";

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String FromStd(const std::string & s) {
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

void RowActionButton(Clay_String id,
                     const std::string & text,
                     int row,
                     int slot,
                     bool rebinding,
                     silencer::ui::UiInteractionRegistry& interactions) {
	std::string display = rebinding ? "-" : text;
	std::string actionId = std::string(slot == 0 ? kActionPrimaryPrefix : kActionSecondaryPrefix)
	                     + std::to_string(row);
	Button(id, FromStd(display),
	       ButtonOpts{ .variant = ButtonVariant::Oval, .size = ButtonSize::Sm },
	       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
}

void RowOperatorButton(Clay_String id,
                       const char * text,
                       int row,
                       silencer::ui::UiInteractionRegistry& interactions) {
	std::string actionId = std::string(kActionOperatorPrefix) + std::to_string(row);
	Button(id, FromCStr(text),
	       ButtonOpts{ .variant = ButtonVariant::Ghost,
	                   .size = ButtonSize::Auto,
	                   .minWidth = kOperatorW,
	                   .paddingY = 4 },
	       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
}

}  // namespace controls_keybind_list_detail

void BuildKeybindListBody(const KeybindListView & view,
                          silencer::ui::UiInteractionRegistry& interactions) {
	std::string primaryIds[kKeybindListVisibleRows];
	std::string secondaryIds[kKeybindListVisibleRows];
	std::string operatorIds[kKeybindListVisibleRows];
	for(int i = 0; i < kKeybindListVisibleRows; i++){
		primaryIds[i] = "Primary" + std::to_string(i);
		secondaryIds[i] = "Secondary" + std::to_string(i);
		operatorIds[i] = "Operator" + std::to_string(i);
	}

	controls_keybind_list_detail::BankText(CLAY_STRING("Configure Controls"), controls_keybind_list_detail::BankTextVariant::Title, {});
	CLAY({ .id = CLAY_ID("ControlsPresetRow"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(controls_keybind_list_detail::kRowH) },
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_ID("PresetLabel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(controls_keybind_list_detail::kActionNameW),
		                       CLAY_SIZING_FIT(0) },
		       } }) {
			controls_keybind_list_detail::BankText(CLAY_STRING("Preset:"), controls_keybind_list_detail::BankTextVariant::Heading, {});
		}
		controls_keybind_list_detail::Button(CLAY_STRING("ControlsPresetButton"),
		              controls_keybind_list_detail::FromStd(view.presetText),
		              controls_keybind_list_detail::ButtonOpts{ .variant = controls_keybind_list_detail::ButtonVariant::Oval,
		                                                        .size = controls_keybind_list_detail::ButtonSize::Lg },
		              controls_keybind_list_detail::ButtonHandle{ nullptr, controls_keybind_list_detail::kActionPreset, &interactions });
	}

	CLAY({ .id = CLAY_ID("ControlsRows"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
	           .childGap = controls_keybind_list_detail::kRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		for(int i = 0; i < view.visibleRowCount; i++){
			const KeybindRowView & row = view.rows[i];
			CLAY({ .id = CLAY_IDI("ControlsRow", (uint32_t)i),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(controls_keybind_list_detail::kRowH) },
			           .childGap = 12,
			           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				CLAY({ .id = CLAY_IDI("ControlsAction", (uint32_t)i),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(controls_keybind_list_detail::kActionNameW),
				                       CLAY_SIZING_FIT(0) },
				       } }) {
					controls_keybind_list_detail::BankText(controls_keybind_list_detail::FromStd(row.actionLabel), controls_keybind_list_detail::BankTextVariant::Heading, {});
				}
				controls_keybind_list_detail::RowActionButton(controls_keybind_list_detail::FromStd(primaryIds[i]),
				                row.primaryLabel, i, 0, row.rebindingPrimary, interactions);
				controls_keybind_list_detail::RowOperatorButton(controls_keybind_list_detail::FromStd(operatorIds[i]),
				                  row.operatorLabel.c_str(), i, interactions);
				controls_keybind_list_detail::RowActionButton(controls_keybind_list_detail::FromStd(secondaryIds[i]),
				                row.secondaryLabel, i, 1, row.rebindingSecondary, interactions);
			}
		}
	}

	CLAY({ .id = CLAY_ID("ControlsScrollRow"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
	           .childGap = 20,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		controls_keybind_list_detail::Button(CLAY_STRING("ControlsScrollUpButton"), CLAY_STRING("Up"),
		           controls_keybind_list_detail::ButtonOpts{ .variant = controls_keybind_list_detail::ButtonVariant::Ghost,
		                                                     .size = controls_keybind_list_detail::ButtonSize::Auto,
		                                                     .minWidth = 60,
		                                                     .paddingY = 4 },
		           controls_keybind_list_detail::ButtonHandle{ nullptr, controls_keybind_list_detail::kActionScrollUp, &interactions });
		controls_keybind_list_detail::Button(CLAY_STRING("ControlsScrollDownButton"), CLAY_STRING("Down"),
		           controls_keybind_list_detail::ButtonOpts{ .variant = controls_keybind_list_detail::ButtonVariant::Ghost,
		                                                     .size = controls_keybind_list_detail::ButtonSize::Auto,
		                                                     .minWidth = 80,
		                                                     .paddingY = 4 },
		           controls_keybind_list_detail::ButtonHandle{ nullptr, controls_keybind_list_detail::kActionScrollDown, &interactions });
	}

	CLAY({ .id = CLAY_ID("ControlsActions"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
	           .childGap = controls_keybind_list_detail::kActionGap,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		controls_keybind_list_detail::Button(CLAY_STRING("ControlsSaveButton"), CLAY_STRING("Save"),
		              controls_keybind_list_detail::ButtonOpts{ .variant = controls_keybind_list_detail::ButtonVariant::Oval,
		                                                        .size = controls_keybind_list_detail::ButtonSize::Md },
		              controls_keybind_list_detail::ButtonHandle{ nullptr, controls_keybind_list_detail::kActionSave, &interactions });
		controls_keybind_list_detail::Button(CLAY_STRING("ControlsCancelButton"), CLAY_STRING("Cancel"),
		              controls_keybind_list_detail::ButtonOpts{ .variant = controls_keybind_list_detail::ButtonVariant::Oval,
		                                                        .size = controls_keybind_list_detail::ButtonSize::Md },
		              controls_keybind_list_detail::ButtonHandle{ nullptr, controls_keybind_list_detail::kActionCancel, &interactions });
	}
}

}  // namespace silencer::client_ui::options
