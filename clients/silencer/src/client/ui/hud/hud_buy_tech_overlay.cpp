#include "client/ui/hud/hud_buy_tech_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "surface.h"
#include "ui/primitives/bank_text.h"
#include "ui/primitives/box.h"
#include "ui/runtime/UiAutomationRegistry.h"

#include <string>

namespace silencer {
namespace client_ui {

void BuildBuyTechOverlay(const BuyTechOverlayView& view, Surface* surface) {
	if(view.rows.empty()) return;

	using namespace silencer::ui::primitives;

	CLAY({ .id = CLAY_ID("InGameBuyTechRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 0, 0, 120, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		CLAY(Box(BoxVariants::Chrome, {
		       .id = CLAY_ID("InGameBuyTechPanel"),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED(345), CLAY_SIZING_FIT(0) },
			       .padding = { 10, 10, 10, 10 },
			       .childGap = 8,
			       .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 200 },
		})) {
			for(unsigned int i = 0; i < view.rows.size(); ++i) {
				const BuyTechRowView& row = view.rows[i];
				const Clay_ElementId rowClayId = CLAY_IDI("InGameBuyTechRow", row.index);
				silencer::ui::automation::Widget widget;
				widget.id = "ingame.buytech.row." + std::to_string(row.index);
				widget.labelText = row.name;
				widget.kind = silencer::ui::automation::WidgetKind::ListRow;
				widget.index = row.index;
				widget.selected = row.selected;
				widget.clayId = rowClayId;
				widget.hasClayId = true;
				silencer::ui::automation::Register(widget);
				if(row.selected){
					silencer::ui::automation::FocusWidgetById(widget.id);
				}
				CLAY({ .id = CLAY_IDI("InGameBuyTechRow", row.index),
				       .layout = {
					       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(25) },
					       .padding = { 6, 8, 2, 2 },
					       .childGap = 10,
					       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
				       },
				       .backgroundColor = row.selected
					       ? Clay_Color{ 38, 0, 0, 255 }
					       : Clay_Color{ 0, 0, 0, 0 },
				}) {
					CLAY({ .id = CLAY_IDI("InGameBuyTechIcon", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(33), CLAY_SIZING_FIXED(21) },
					       },
					       .custom = { .customData = AllocSpriteCustomData({
						       row.spriteBank, row.spriteIndex,
						       0, 0, 0, 0, 0, row.brightness, 0, 0,
					       }) },
					}) {}
					CLAY({ .id = CLAY_IDI("InGameBuyTechName", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
					       } }) {
						BankText(ClayStringFromStd(row.name), BankTextVariant::Heading,
						         { .brightness = row.brightness });
					}
					CLAY({ .id = CLAY_IDI("InGameBuyTechPrice", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIT(0) },
						       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
					       } }) {
						BankText(ClayStringFromStd(row.price), BankTextVariant::Heading,
						         { .brightness = row.brightness });
					}
				}
			}
			CLAY({ .id = CLAY_ID("InGameBuyTechFooter"),
			       .layout = {
				       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			       } }) {
				BankText(ClayStringFromStd(view.footer), BankTextVariant::Heading);
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
