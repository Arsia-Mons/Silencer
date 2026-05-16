#include "client/ui/hud/HudPayloadArena.h"

namespace silencer {
namespace client_ui {

namespace hudpayloadarena_detail {
constexpr int kSpritePayloadCapacity = 512;
silencer::clay_bridge::SpritePayload  g_spritePayloads[kSpritePayloadCapacity];
silencer::clay_bridge::ClayCustomData g_spriteCustomData[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

constexpr int kTeamEmblemPayloadCapacity = 64;
silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
silencer::clay_bridge::ClayCustomData    g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;
}  // namespace hudpayloadarena_detail

void HudPayloadBeginFrame() {
	hudpayloadarena_detail::g_spritePayloadCount = 0;
	hudpayloadarena_detail::g_teamEmblemPayloadCount = 0;
}

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload) {
	if(hudpayloadarena_detail::g_spritePayloadCount >= hudpayloadarena_detail::kSpritePayloadCapacity) return nullptr;
	hudpayloadarena_detail::g_spritePayloads[hudpayloadarena_detail::g_spritePayloadCount] = payload;
	hudpayloadarena_detail::g_spriteCustomData[hudpayloadarena_detail::g_spritePayloadCount] = {
		silencer::clay_bridge::CustomKind::Sprite,
		&hudpayloadarena_detail::g_spritePayloads[hudpayloadarena_detail::g_spritePayloadCount],
	};
	return &hudpayloadarena_detail::g_spriteCustomData[hudpayloadarena_detail::g_spritePayloadCount++];
}

silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload) {
	if(hudpayloadarena_detail::g_teamEmblemPayloadCount >= hudpayloadarena_detail::kTeamEmblemPayloadCapacity) return nullptr;
	hudpayloadarena_detail::g_teamEmblemPayloads[hudpayloadarena_detail::g_teamEmblemPayloadCount] = payload;
	hudpayloadarena_detail::g_teamEmblemCustomData[hudpayloadarena_detail::g_teamEmblemPayloadCount] = {
		silencer::clay_bridge::CustomKind::TeamEmblem,
		&hudpayloadarena_detail::g_teamEmblemPayloads[hudpayloadarena_detail::g_teamEmblemPayloadCount],
	};
	return &hudpayloadarena_detail::g_teamEmblemCustomData[hudpayloadarena_detail::g_teamEmblemPayloadCount++];
}

}  // namespace client_ui
}  // namespace silencer
