#include "client/ui/hud/hud_buy_tech_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "surface.h"
#include "ui/primitives/box.h"
#include "ui/primitives/text.h"
#include "ui/runtime/UiInteractionRegistry.h"

#include <string>

namespace silencer {
namespace client_ui {

void BuildBuyTechOverlay(const BuyTechOverlayView& view,
                         Surface* surface,
                         silencer::ui::UiInteractionRegistry& interactions) {
	if(view.rows.empty()) return;

	using namespace silencer::ui::primitives;

	CLAY({ .id = CLAY_ID("InGameBuyTechRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 0, 0, 120, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .floating = {
		       .attachTo = CLAY_ATTACH_TO_ROOT,
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
				silencer::ui::UiInteractable widget;
				widget.id = "ingame.buytech.row." + std::to_string(row.index);
				widget.labelText = row.name;
				widget.kind = silencer::ui::UiInteractableKind::ListRow;
				widget.index = row.index;
				widget.selected = row.selected;
				widget.clayId = rowClayId;
				widget.hasClayId = true;
				interactions.RegisterInteractable(widget);
				if(row.selected){
					interactions.FocusInteractableById(widget.id);
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
						Text(ClayStringFromStd(row.name),
						     { .size = TextSize::Heading,
						       .effect = TextEffect::LegacyPalette(0, row.brightness) });
					}
					CLAY({ .id = CLAY_IDI("InGameBuyTechPrice", i),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIT(0) },
						       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
					       } }) {
						Text(ClayStringFromStd(row.price),
						     { .size = TextSize::Heading,
						       .effect = TextEffect::LegacyPalette(0, row.brightness) });
					}
				}
			}
			CLAY({ .id = CLAY_ID("InGameBuyTechFooter"),
			       .layout = {
				       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			       } }) {
				Text(ClayStringFromStd(view.footer), { .size = TextSize::Heading });
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
