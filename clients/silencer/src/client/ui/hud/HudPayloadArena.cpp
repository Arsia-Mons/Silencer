#include "client/ui/hud/HudPayloadArena.h"

#include <algorithm>
#include <cstring>

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

constexpr int kStringArenaCapacity = 65536;
char g_stringArena[kStringArenaCapacity];
int g_stringArenaOffset = 0;

Clay_String EmptyString() {
	static const char kEmpty[] = "";
	return Clay_String{ .isStaticallyAllocated = true, .length = 0, .chars = kEmpty };
}
}  // namespace hudpayloadarena_detail

void HudPayloadBeginFrame() {
	hudpayloadarena_detail::g_spritePayloadCount = 0;
	hudpayloadarena_detail::g_teamEmblemPayloadCount = 0;
	hudpayloadarena_detail::g_stringArenaOffset = 0;
}

Clay_String AllocHudString(const char* text, int length) {
	if(!text || length <= 0) return hudpayloadarena_detail::EmptyString();
	if(length + 1 > hudpayloadarena_detail::kStringArenaCapacity -
	                  hudpayloadarena_detail::g_stringArenaOffset) {
		return hudpayloadarena_detail::EmptyString();
	}
	char* dst =
		&hudpayloadarena_detail::g_stringArena[hudpayloadarena_detail::g_stringArenaOffset];
	std::memcpy(dst, text, static_cast<size_t>(length));
	dst[length] = '\0';
	hudpayloadarena_detail::g_stringArenaOffset += length + 1;
	return Clay_String{
		.isStaticallyAllocated = false,
		.length = length,
		.chars = dst,
	};
}

Clay_String AllocHudString(const std::string& text) {
	return AllocHudString(text.c_str(), std::max(0, static_cast<int>(text.size())));
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
