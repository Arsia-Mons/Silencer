#include "controls_keybind_list.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/button.h"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace silencer::client_ui::options {

namespace controls_keybind_list_detail {

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

constexpr uint16_t kContentW = 486;
constexpr uint16_t kPresetRowH = 43;
constexpr uint16_t kRowH = 43;
constexpr uint16_t kRowGap = 10;
constexpr uint16_t kActionNameW = 180;
constexpr uint16_t kOperatorW = 45;
constexpr uint16_t kActionGap = 12;
constexpr uint16_t kColumnGap = 12;
constexpr uint16_t kPresetGap = 12;
constexpr uint16_t kSectionGap = 8;
constexpr uint16_t kActionTopGap = 16;
constexpr uint16_t kActionRowH = 33;
constexpr const char * kActionPreset = "options_controls.preset";
constexpr const char * kActionSave = "options_controls.save";
constexpr const char * kActionCancel = "options_controls.cancel";
constexpr const char * kActionPrimaryPrefix = "options_controls.primary.";
constexpr const char * kActionSecondaryPrefix = "options_controls.secondary.";
constexpr const char * kActionOperatorPrefix = "options_controls.operator.";

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String FromStd(const std::string & s) {
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

void TitleText(Clay_String text) {
	CLAY_TEXT(text,
	          CLAY_TEXT_CONFIG({
	              .textColor = { 0.0f, 0.0f, 0.0f, 255.0f },
	              .fontId = 135,
	              .fontSize = 12,
	          }));
}

void RegisterRowsScrollArea(Clay_ElementId clayId,
                            silencer::ui::UiInteractionRegistry& interactions) {
	silencer::ui::UiElementSnapshot element;
	element.id = kKeybindListScrollId;
	element.kind = silencer::ui::UiElementKind::Container;
	element.label = kKeybindListScrollLabel;
	element.value = "scroll";
	element.clayId = clayId;
	element.hasClayId = true;
	interactions.Register(element);
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

int KeybindListVisibleRowsForContentHeight(int contentHeight) {
	const int viewportHeight = contentHeight
	                         - controls_keybind_list_detail::kPresetRowH
	                         - controls_keybind_list_detail::kActionTopGap
	                         - controls_keybind_list_detail::kActionRowH
	                         - controls_keybind_list_detail::kSectionGap * 3;
	if(viewportHeight <= 0) return kKeybindListMinVisibleRows;
	const int rows = (viewportHeight + controls_keybind_list_detail::kRowGap)
	               / (controls_keybind_list_detail::kRowH
	                  + controls_keybind_list_detail::kRowGap);
	return std::max(kKeybindListMinVisibleRows, rows);
}

void BuildKeybindListBody(const KeybindListView & view,
                          silencer::ui::UiInteractionRegistry& interactions) {
	const int rowCount = std::min(view.visibleRowCount, static_cast<int>(view.rows.size()));
	std::vector<std::string> primaryIds(rowCount);
	std::vector<std::string> secondaryIds(rowCount);
	std::vector<std::string> operatorIds(rowCount);
	for(int i = 0; i < rowCount; i++){
		primaryIds[i] = "Primary" + std::to_string(i);
		secondaryIds[i] = "Secondary" + std::to_string(i);
		operatorIds[i] = "Operator" + std::to_string(i);
	}

	CLAY({ .id = CLAY_ID("ControlsTitle"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .floating = {
	           .offset = { 0.0f, view.titleOffsetY },
	           .zIndex = 1,
	           .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_TOP,
	                             .parent = CLAY_ATTACH_POINT_CENTER_TOP },
	           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
	           .attachTo = CLAY_ATTACH_TO_PARENT,
	       } }) {
		controls_keybind_list_detail::TitleText(CLAY_STRING("Configure Controls"));
	}

	CLAY({ .id = CLAY_ID("ControlsContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(controls_keybind_list_detail::kContentW),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = controls_keybind_list_detail::kSectionGap,
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("ControlsPresetRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(controls_keybind_list_detail::kPresetRowH) },
		           .childGap = controls_keybind_list_detail::kPresetGap,
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

		const Clay_ElementId scrollAreaId = CLAY_ID("ControlsRowsViewport");
		CLAY({ .id = scrollAreaId,
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		           .childGap = controls_keybind_list_detail::kRowGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .clip = { .vertical = true } }) {
			controls_keybind_list_detail::RegisterRowsScrollArea(scrollAreaId, interactions);
			for(int i = 0; i < rowCount; i++){
				const KeybindRowView & row = view.rows[i];
				CLAY({ .id = CLAY_IDI("ControlsRow", (uint32_t)i),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(controls_keybind_list_detail::kRowH) },
				           .childGap = controls_keybind_list_detail::kColumnGap,
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

		CLAY({ .id = CLAY_ID("ControlsActionSpacer"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(controls_keybind_list_detail::kActionTopGap) },
		       } }) {}

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
}

}  // namespace silencer::client_ui::options
