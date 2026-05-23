#include "controls_keybind_list.h"

#include "options_document_runtime.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/button.h"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace silencer::client_ui::options {

namespace controls_keybind_list_detail {

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

constexpr uint16_t kContentW = 486;
constexpr uint16_t kRowH = 43;
constexpr uint16_t kRowGap = 10;
constexpr uint16_t kActionNameW = 180;
constexpr uint16_t kOperatorW = 45;
constexpr uint16_t kColumnGap = 12;

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String FromStd(const std::string & s) {
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
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
	std::string actionId = std::string(slot == 0
	                       ? options_controls::kActionPrimaryPrefix
	                       : options_controls::kActionSecondaryPrefix)
	                     + std::to_string(row);
	Button(id, FromStd(display),
	       ButtonOpts{ .variant = ButtonVariant::Oval, .size = ButtonSize::Sm },
	       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
}

void RowOperatorButton(Clay_String id,
                       const char * text,
                       int row,
                       int minWidth,
                       silencer::ui::UiInteractionRegistry& interactions) {
	std::string actionId = std::string(options_controls::kActionOperatorPrefix)
	                     + std::to_string(row);
	Button(id, FromCStr(text),
	       ButtonOpts{ .variant = ButtonVariant::Ghost,
	                   .size = ButtonSize::Auto,
	                   .minWidth = minWidth,
	                   .paddingY = 4 },
	       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
}

}  // namespace controls_keybind_list_detail

int KeybindRowsVisibleRowsForHeight(int rowsHeight) {
	if(rowsHeight <= 0) return 1;
	// Whole rows that fit at the design row height. The rows GROW to absorb
	// any leftover space (see BuildKeybindRows), so flooring here keeps
	// the last row from clipping while the list still fills the content area.
	const int rows = (rowsHeight + controls_keybind_list_detail::kRowGap)
	               / (controls_keybind_list_detail::kRowH
	                  + controls_keybind_list_detail::kRowGap);
	return std::max(1, rows);
}

void BuildKeybindRows(const KeybindListView & view,
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

	// Scale the hardcoded legacy-pixel horizontal metrics by the screen's
	// horizontal scale so the interior tracks the panel width instead of
	// overflowing it at small window sizes. Vertical metrics are untouched
	// (row height/clip are owned by the #179 vertical fix).
	const float hs = view.hScale;
	auto S = [hs](int v) -> int {
		int r = static_cast<int>(v * hs + 0.5f);
		return r < 1 ? 1 : r;
	};
	const float    contentW    = static_cast<float>(S(controls_keybind_list_detail::kContentW));
	const float    actionNameW = static_cast<float>(S(controls_keybind_list_detail::kActionNameW));
	const int      operatorW   = S(controls_keybind_list_detail::kOperatorW);
	const uint16_t columnGap   = static_cast<uint16_t>(S(controls_keybind_list_detail::kColumnGap));

	const Clay_ElementId scrollAreaId = CLAY_ID("ControlsRowsViewport");
	CLAY({ .id = scrollAreaId,
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(contentW),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = controls_keybind_list_detail::kRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .clip = { .vertical = true } }) {
		controls_keybind_list_detail::RegisterRowsScrollArea(scrollAreaId, interactions);
		for(int i = 0; i < rowCount; i++){
			const KeybindRowView & row = view.rows[i];
			CLAY({ .id = CLAY_IDI("ControlsRow", (uint32_t)i),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(controls_keybind_list_detail::kRowH) },
			           .childGap = columnGap,
			           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				CLAY({ .id = CLAY_IDI("ControlsAction", (uint32_t)i),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(actionNameW),
				                       CLAY_SIZING_FIT(0) },
				       } }) {
					controls_keybind_list_detail::Text(
						controls_keybind_list_detail::FromStd(row.actionLabel),
						{ .size = controls_keybind_list_detail::TextSize::Heading });
				}
				controls_keybind_list_detail::RowActionButton(controls_keybind_list_detail::FromStd(primaryIds[i]),
				                row.primaryLabel, i, 0, row.rebindingPrimary, interactions);
				controls_keybind_list_detail::RowOperatorButton(controls_keybind_list_detail::FromStd(operatorIds[i]),
				                  row.operatorLabel.c_str(), i, operatorW, interactions);
				controls_keybind_list_detail::RowActionButton(controls_keybind_list_detail::FromStd(secondaryIds[i]),
				                row.secondaryLabel, i, 1, row.rebindingSecondary, interactions);
			}
		}
	}
}

}  // namespace silencer::client_ui::options
