#include "controls_keybind_list.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace silencer::client_ui::options {

namespace controls_keybind_list_detail {

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kPanelW = 540;
constexpr uint16_t kPanelPadX = 20;
constexpr uint16_t kPanelPadY = 24;
constexpr uint16_t kRowH = 40;
constexpr uint16_t kRowGap = 8;
constexpr uint16_t kActionNameW = 168;
constexpr uint16_t kKeyButtonW = 112;
constexpr uint16_t kKeyButtonH = 33;
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

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

std::string InteractionLabel(Clay_String id, Clay_String text) {
	std::string idText = ToStd(id);
	if(idText == "PresetButton") return "Preset";
	if(idText == "ScrollUp") return "Scroll Up";
	if(idText == "ScrollDown") return "Scroll Down";
	return ToStd(text);
}

void RegisterButton(const char * label,
                    const std::string & actionId,
                    int x, int y, int w, int h,
                    silencer::ui::UiInteractionRegistry& interactions) {
	silencer::ui::UiInteractable widget;
	widget.id = actionId;
	widget.labelText = label;
	widget.kind = silencer::ui::UiInteractableKind::Button;
	widget.x = x; widget.y = y; widget.w = w; widget.h = h;
	interactions.RegisterInteractable(widget);
}

void ButtonElement(Clay_String id,
                   Clay_String text,
                   uint16_t width,
                   uint16_t height,
                   uint16_t imageIndex,
                   BankTextVariant textVariant,
                   const char * actionId,
                   silencer::ui::UiInteractionRegistry& interactions) {
	if(actionId && *actionId){
		silencer::ui::UiInteractable widget;
		widget.id = actionId;
		widget.labelText = InteractionLabel(id, text);
		widget.kind = silencer::ui::UiInteractableKind::Button;
		widget.clayId = CLAY_SID(id);
		widget.hasClayId = true;
		interactions.RegisterInteractable(widget);
	}
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
	                       CLAY_SIZING_FIXED(static_cast<float>(height)) },
	           .padding = { 0, 0,
	                        static_cast<uint16_t>(height > 25 ? 8 : 4), 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = silencer::clay_bridge::PackImage(6, imageIndex) } }) {
		bool hovered = ::Clay_Hovered();
		BankText(text, textVariant,
		         { .brightness = hovered ? static_cast<Uint8>(136)
		                                  : static_cast<Uint8>(128) });
	}
}

void TextButton(Clay_String id,
                Clay_String text,
                uint16_t width,
                uint16_t height,
                const char * actionId,
                silencer::ui::UiInteractionRegistry& interactions) {
	if(actionId && *actionId){
		silencer::ui::UiInteractable widget;
		widget.id = actionId;
		widget.labelText = InteractionLabel(id, text);
		widget.kind = silencer::ui::UiInteractableKind::Button;
		widget.clayId = CLAY_SID(id);
		widget.hasClayId = true;
		interactions.RegisterInteractable(widget);
	}
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
	                       CLAY_SIZING_FIXED(static_cast<float>(height)) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		bool hovered = ::Clay_Hovered();
		BankText(text, BankTextVariant::Heading,
		         { .brightness = hovered ? static_cast<Uint8>(136)
		                                  : static_cast<Uint8>(128) });
	}
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
	ButtonElement(id, FromStd(display), kKeyButtonW, kKeyButtonH, 28,
	              BankTextVariant::Title, actionId.c_str(), interactions);
}

void RowOperatorButton(Clay_String id,
                       const char * text,
                       int row,
                       silencer::ui::UiInteractionRegistry& interactions) {
	std::string actionId = std::string(kActionOperatorPrefix) + std::to_string(row);
	TextButton(id, FromCStr(text), kOperatorW, kKeyButtonH,
	           actionId.c_str(), interactions);
}

}  // namespace controls_keybind_list_detail

void RegisterKeybindListWidgets(int surfaceW,
                                silencer::ui::UiInteractionRegistry& interactions) {
	const int panelX = (surfaceW - controls_keybind_list_detail::kPanelW) / 2;
	const int panelY = 34;
	const int x = panelX + controls_keybind_list_detail::kPanelPadX;
	int y = panelY + controls_keybind_list_detail::kPanelPadY + 34;
	controls_keybind_list_detail::RegisterButton("Preset", controls_keybind_list_detail::kActionPreset, x + controls_keybind_list_detail::kActionNameW, y, 220, 33, interactions);
	y += controls_keybind_list_detail::kRowH + controls_keybind_list_detail::kRowGap;
	for(int i = 0; i < kKeybindListVisibleRows; i++){
		const std::string row = std::to_string(i);
		controls_keybind_list_detail::RegisterButton("Primary binding", std::string(controls_keybind_list_detail::kActionPrimaryPrefix) + row,
		               x + controls_keybind_list_detail::kActionNameW, y + 4, controls_keybind_list_detail::kKeyButtonW, controls_keybind_list_detail::kKeyButtonH, interactions);
		controls_keybind_list_detail::RegisterButton("Binding operator", std::string(controls_keybind_list_detail::kActionOperatorPrefix) + row,
		               x + controls_keybind_list_detail::kActionNameW + controls_keybind_list_detail::kKeyButtonW + 12,
		               y + 4, controls_keybind_list_detail::kOperatorW, controls_keybind_list_detail::kKeyButtonH, interactions);
		controls_keybind_list_detail::RegisterButton("Secondary binding",
		               std::string(controls_keybind_list_detail::kActionSecondaryPrefix) + row,
		               x + controls_keybind_list_detail::kActionNameW + controls_keybind_list_detail::kKeyButtonW + 12 + controls_keybind_list_detail::kOperatorW + 12,
		               y + 4, controls_keybind_list_detail::kKeyButtonW, controls_keybind_list_detail::kKeyButtonH, interactions);
		y += controls_keybind_list_detail::kRowH + controls_keybind_list_detail::kRowGap;
	}
	controls_keybind_list_detail::RegisterButton("Scroll Up", controls_keybind_list_detail::kActionScrollUp,
	               panelX + controls_keybind_list_detail::kPanelW - 42, panelY + 92, 24, 24, interactions);
	controls_keybind_list_detail::RegisterButton("Scroll Down", controls_keybind_list_detail::kActionScrollDown,
	               panelX + controls_keybind_list_detail::kPanelW - 42, panelY + 280, 24, 24, interactions);
	controls_keybind_list_detail::RegisterButton("Save", controls_keybind_list_detail::kActionSave, panelX + 102, panelY + 338, 156, 21, interactions);
	controls_keybind_list_detail::RegisterButton("Cancel", controls_keybind_list_detail::kActionCancel, panelX + 282, panelY + 338, 156, 21, interactions);
}

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
		controls_keybind_list_detail::ButtonElement(CLAY_STRING("PresetButton"), controls_keybind_list_detail::FromStd(view.presetText), 220, 33,
		              23, controls_keybind_list_detail::BankTextVariant::Title, controls_keybind_list_detail::kActionPreset, interactions);
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
		controls_keybind_list_detail::TextButton(CLAY_STRING("ScrollUp"), CLAY_STRING("Up"), 60, 24,
		           controls_keybind_list_detail::kActionScrollUp, interactions);
		controls_keybind_list_detail::TextButton(CLAY_STRING("ScrollDown"), CLAY_STRING("Down"), 80, 24,
		           controls_keybind_list_detail::kActionScrollDown, interactions);
	}

	CLAY({ .id = CLAY_ID("ControlsActions"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
	           .childGap = controls_keybind_list_detail::kActionGap,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		controls_keybind_list_detail::ButtonElement(CLAY_STRING("Save"), CLAY_STRING("Save"), 156, 21, 24,
		              controls_keybind_list_detail::BankTextVariant::Heading, controls_keybind_list_detail::kActionSave, interactions);
		controls_keybind_list_detail::ButtonElement(CLAY_STRING("Cancel"), CLAY_STRING("Cancel"), 156, 21, 24,
		              controls_keybind_list_detail::BankTextVariant::Heading, controls_keybind_list_detail::kActionCancel, interactions);
	}
}

}  // namespace silencer::client_ui::options
