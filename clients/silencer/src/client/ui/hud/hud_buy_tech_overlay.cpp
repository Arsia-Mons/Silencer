#include "client/ui/hud/hud_buy_tech_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "resources.h"
#include "surface.h"
#include "ui/primitives/text.h"
#include "ui/runtime/UiInteractionRegistry.h"

#include <cstring>
#include <string>

namespace silencer {
namespace client_ui {

namespace {

Clay_ElementDeclaration IndexedFloatingElement(const char* id, uint32_t index,
                                              int x, int y, int w, int h) {
	Clay_String idString{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	return {
		.id = Clay_GetElementIdWithIndex(idString, index),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
		},
		.floating = {
			.offset = { (float)x, (float)y },
			.attachTo = CLAY_ATTACH_TO_ROOT,
		},
	};
}

void BuildSpriteAt(const char* id, uint32_t index,
                   int x, int y, int w, int h,
                   Uint8 bank, Uint16 spriteIndex,
                   Uint8 brightness = 128) {
	CLAY(IndexedFloatingElement(id, index, x, y, w, h)) {
		CLAY({ .id = CLAY_IDI("InGameBuyTechSprite", index),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
		       },
		       .custom = { .customData = AllocSpriteCustomData({
			       bank, spriteIndex, 0, 0, 0, 0, 0, brightness, 0, 0,
		       }) },
		}) {}
	}
}

}  // namespace

void BuildBuyTechOverlay(const BuyTechOverlayView& view,
                         const Resources& resources,
                         Surface* surface,
                         silencer::ui::UiInteractionRegistry& interactions) {
	if(view.rows.empty()) return;

	using namespace silencer::ui::primitives;

	const int backgroundX = SpriteX(resources, 102, 0);
	const int backgroundY = SpriteY(resources, 102, 0);
	const int backgroundW = SpriteWidth(resources, 102, 0);
	const int backgroundH = SpriteHeight(resources, 102, 0);
	if(backgroundW <= 0 || backgroundH <= 0) return;

	const int highlightX = SpriteX(resources, 102, 1);
	const int highlightBaseY = SpriteY(resources, 102, 1);
	const int highlightW = SpriteWidth(resources, 102, 1);
	const int highlightH = SpriteHeight(resources, 102, 1);
	const Uint16 rowLineHeight = TextLineHeight(TextSize::Heading);
	const Uint16 rowAdvance = TextAdvance(TextSize::Heading);

	CLAY({ .id = CLAY_ID("InGameBuyTechRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       },
	       .floating = {
		       .attachTo = CLAY_ATTACH_TO_ROOT,
	       } }) {
		BuildSpriteAt("InGameBuyTechBackground", 0,
		              backgroundX, backgroundY, backgroundW, backgroundH,
		              102, 0);

		for(unsigned int i = 0; i < view.rows.size(); ++i) {
			const BuyTechRowView& row = view.rows[i];
			const int yoffset = static_cast<int>(i) * 25;
			const int rowX = highlightX;
			const int rowY = highlightBaseY + yoffset;
			silencer::ui::UiInteractable widget;
			widget.id = "ingame.buytech.row." + std::to_string(row.index);
			widget.labelText = row.name;
			widget.kind = silencer::ui::UiInteractableKind::ListRow;
			widget.index = row.index;
			widget.selected = row.selected;
			widget.clayId = CLAY_IDI("InGameBuyTechRow", row.index);
			widget.hasClayId = true;
			widget.requestInitialFocus = row.selected;
			interactions.RegisterInteractable(widget);

			CLAY(IndexedFloatingElement("InGameBuyTechRow", row.index,
			                            rowX, rowY, highlightW, highlightH)) {
				if(row.selected){
					CLAY({ .id = CLAY_IDI("InGameBuyTechHighlight", row.index),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED((float)highlightW), CLAY_SIZING_FIXED((float)highlightH) },
					       },
					       .custom = { .customData = AllocSpriteCustomData({
						       102, 1, 0, 0, 0, 0, 0, 128, 0, 0,
					       }) },
					}) {}
				}
			}

			const int iconX = SpriteX(resources, row.spriteBank, row.spriteIndex, 169);
			const int iconY = SpriteY(resources, row.spriteBank, row.spriteIndex, 139 + yoffset);
			const int iconW = SpriteWidth(resources, row.spriteBank, row.spriteIndex);
			const int iconH = SpriteHeight(resources, row.spriteBank, row.spriteIndex);
			if(iconW > 0 && iconH > 0){
				BuildSpriteAt("InGameBuyTechIcon", row.index,
				              iconX, iconY, iconW, iconH,
				              row.spriteBank, row.spriteIndex, row.brightness);
			}

			CLAY(IndexedFloatingElement("InGameBuyTechName", row.index,
			                            222, 145 + yoffset,
			                            180, rowLineHeight)) {
				Text(ClayStringFromStd(row.name),
				     { .size = TextSize::Heading,
				       .effect = TextEffect::LegacyPalette(0, row.brightness) });
			}

			const int priceWidth = static_cast<int>(row.price.size()) * rowAdvance;
			CLAY(IndexedFloatingElement("InGameBuyTechPrice", row.index,
			                            440 - (priceWidth / 2), 145 + yoffset,
			                            priceWidth, rowLineHeight)) {
				Text(ClayStringFromStd(row.price),
				     { .size = TextSize::Heading,
				       .effect = TextEffect::LegacyPalette(0, row.brightness) });
			}
		}

		const int footerWidth = static_cast<int>(view.footer.size()) * rowAdvance;
		CLAY({ .id = CLAY_ID("InGameBuyTechFooter"),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)footerWidth), CLAY_SIZING_FIXED((float)rowLineHeight) },
		       },
		       .floating = {
			       .offset = { (float)(320 - (footerWidth / 2)), 275.0f },
			       .attachTo = CLAY_ATTACH_TO_ROOT,
		       } }) {
			Text(ClayStringFromStd(view.footer), { .size = TextSize::Heading });
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
