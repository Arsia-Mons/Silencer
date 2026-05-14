#include "client/ui/hud/hud_system_camera.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "render/clay_ui_payloads.h"
#include "resources.h"
#include "surface.h"

namespace silencer {
namespace client_ui {

void BuildHudSystemCameraFrame(const Resources& resources, Surface* surface,
                               Uint8 bank, Uint16 index, Uint8 offsetBank,
                               int logicalY) {
	if(bank >= resources.spritebank.size()) return;
	if(index >= resources.spritebank[bank].size()) return;
	Surface* sprite = resources.spritebank[bank][index].get();
	if(!sprite) return;
	int x = -SpriteOffsetX(resources, bank, index);
	int y = -SpriteOffsetY(resources, offsetBank, index) + logicalY;

	CLAY({ .id = CLAY_IDI("HudSystemCameraFrameRoot", index),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		CLAY({ .id = CLAY_IDI("HudSystemCameraFrame", index),
		       .layout = {
			       .sizing = { CLAY_SIZING_FIXED((float)sprite->w), CLAY_SIZING_FIXED((float)sprite->h) },
		       },
		       .floating = {
			       .offset = { (float)x, (float)y },
			       .attachTo = CLAY_ATTACH_TO_ROOT,
		       },
		       .custom = { .customData = AllocSpriteCustomData({
			       bank, index, 0, 0, 0, 0, 0, 128, 0, 0,
		       }) },
		}) {}
	}
}

}  // namespace client_ui
}  // namespace silencer
