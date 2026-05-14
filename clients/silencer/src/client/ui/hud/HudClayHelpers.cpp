#include "client/ui/hud/HudClayHelpers.h"

#include "resources.h"
#include "surface.h"

#include <cstring>

namespace silencer {
namespace client_ui {

Clay_String ClayStringFromStd(const std::string& text) {
	return Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(text.size()),
		.chars = text.c_str(),
	};
}

Clay_String ClayStringFromCString(const char* text) {
	return Clay_String{
		.isStaticallyAllocated = true,
		.length = static_cast<int32_t>(std::strlen(text)),
		.chars = text,
	};
}

Clay_ElementDeclaration HudFloatingElement(const char* id, int x, int y, int w, int h) {
	Clay_String idString{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	return {
		.id = Clay_GetElementId(idString),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
		},
		.floating = {
			.offset = { (float)x, (float)y },
			.attachTo = CLAY_ATTACH_TO_ROOT,
		},
	};
}

int SpriteWidth(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size()) return 0;
	if(index >= res.spritebank[bank].size()) return 0;
	Surface* sprite = res.spritebank[bank][index].get();
	return sprite ? sprite->w : 0;
}

int SpriteHeight(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spritebank.size()) return 0;
	if(index >= res.spritebank[bank].size()) return 0;
	Surface* sprite = res.spritebank[bank][index].get();
	return sprite ? sprite->h : 0;
}

int SpriteOffsetX(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spriteoffsetx.size()) return 0;
	if(index >= res.spriteoffsetx[bank].size()) return 0;
	return res.spriteoffsetx[bank][index];
}

int SpriteOffsetY(const Resources& res, Uint8 bank, Uint16 index) {
	if(bank >= res.spriteoffsety.size()) return 0;
	if(index >= res.spriteoffsety[bank].size()) return 0;
	return res.spriteoffsety[bank][index];
}

int SpriteX(const Resources& res, Uint8 bank, Uint16 index, int logicalX) {
	return logicalX - SpriteOffsetX(res, bank, index);
}

int SpriteY(const Resources& res, Uint8 bank, Uint16 index, int logicalY) {
	return logicalY - SpriteOffsetY(res, bank, index);
}

}  // namespace client_ui
}  // namespace silencer
