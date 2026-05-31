#pragma once

#include <SDL3/SDL_stdinc.h>

#include <cstdint>

namespace silencer {
namespace client_ui {

struct RetainedHudSpritePayload {
	Uint8  bank = 0;
	Uint16 index = 0;
	Sint16 srcX = 0;
	Sint16 srcY = 0;
	Sint16 srcW = 0;
	Sint16 srcH = 0;
	Uint8  effectColor = 0;
	Uint8  brightness = 128;
	Uint8  rampColor = 0;
	Uint8  rampPlus = 0;
	Sint16 dstOffsetX = 0;
	Sint16 dstOffsetY = 0;
};

struct RetainedHudTeamEmblemPayload {
	Uint8  bank = 0;
	Uint16 index = 0;
	Uint8  teamColor = 0;
	Uint8  outlineColor = 0;
	bool   scaled = false;
};

void HudRetainedPayloadBeginFrame();

uint32_t RetainedHudSpriteTextureId(const RetainedHudSpritePayload& payload);
uint32_t RetainedHudTeamEmblemTextureId(const RetainedHudTeamEmblemPayload& payload);

bool IsRetainedHudPayloadTexture(uint32_t textureId);
const RetainedHudSpritePayload * ResolveRetainedHudSpritePayload(uint32_t textureId);
const RetainedHudTeamEmblemPayload * ResolveRetainedHudTeamEmblemPayload(uint32_t textureId);

}  // namespace client_ui
}  // namespace silencer
