#include "client/ui/hud/HudPayloadArena.h"

namespace silencer {
namespace client_ui {

namespace {
constexpr int kSpritePayloadCapacity = 512;
silencer::clay_bridge::SpritePayload  g_spritePayloads[kSpritePayloadCapacity];
silencer::clay_bridge::ClayCustomData g_spriteCustomData[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

constexpr int kTeamEmblemPayloadCapacity = 64;
silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
silencer::clay_bridge::ClayCustomData    g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;
}  // namespace

void HudPayloadBeginFrame() {
	g_spritePayloadCount = 0;
	g_teamEmblemPayloadCount = 0;
}

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload) {
	if(g_spritePayloadCount >= kSpritePayloadCapacity) return nullptr;
	g_spritePayloads[g_spritePayloadCount] = payload;
	g_spriteCustomData[g_spritePayloadCount] = {
		silencer::clay_bridge::CustomKind::Sprite,
		&g_spritePayloads[g_spritePayloadCount],
	};
	return &g_spriteCustomData[g_spritePayloadCount++];
}

silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload) {
	if(g_teamEmblemPayloadCount >= kTeamEmblemPayloadCapacity) return nullptr;
	g_teamEmblemPayloads[g_teamEmblemPayloadCount] = payload;
	g_teamEmblemCustomData[g_teamEmblemPayloadCount] = {
		silencer::clay_bridge::CustomKind::TeamEmblem,
		&g_teamEmblemPayloads[g_teamEmblemPayloadCount],
	};
	return &g_teamEmblemCustomData[g_teamEmblemPayloadCount++];
}

}  // namespace client_ui
}  // namespace silencer
