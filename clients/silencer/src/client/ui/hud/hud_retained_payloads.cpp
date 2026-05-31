#include "client/ui/hud/hud_retained_payloads.h"

namespace silencer {
namespace client_ui {

namespace {

constexpr uint32_t kRetainedHudPayloadBit = 0x80000000u;
constexpr uint32_t kRetainedHudTeamEmblemBit = 0x40000000u;
constexpr uint32_t kRetainedHudPayloadIndexMask = 0x0000FFFFu;
constexpr int kRetainedHudSpritePayloadCapacity = 512;
constexpr int kRetainedHudTeamEmblemPayloadCapacity = 64;

RetainedHudSpritePayload g_spritePayloads[kRetainedHudSpritePayloadCapacity];
int g_spritePayloadCount = 0;

RetainedHudTeamEmblemPayload g_teamEmblemPayloads[kRetainedHudTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;

}  // namespace

void HudRetainedPayloadBeginFrame() {
	g_spritePayloadCount = 0;
	g_teamEmblemPayloadCount = 0;
}

uint32_t RetainedHudSpriteTextureId(const RetainedHudSpritePayload& payload) {
	if(g_spritePayloadCount >= kRetainedHudSpritePayloadCapacity) return 0;
	const int index = g_spritePayloadCount++;
	g_spritePayloads[index] = payload;
	return kRetainedHudPayloadBit | static_cast<uint32_t>(index);
}

uint32_t RetainedHudTeamEmblemTextureId(const RetainedHudTeamEmblemPayload& payload) {
	if(g_teamEmblemPayloadCount >= kRetainedHudTeamEmblemPayloadCapacity) return 0;
	const int index = g_teamEmblemPayloadCount++;
	g_teamEmblemPayloads[index] = payload;
	return kRetainedHudPayloadBit | kRetainedHudTeamEmblemBit | static_cast<uint32_t>(index);
}

bool IsRetainedHudPayloadTexture(uint32_t textureId) {
	return (textureId & kRetainedHudPayloadBit) != 0;
}

const RetainedHudSpritePayload * ResolveRetainedHudSpritePayload(uint32_t textureId) {
	if(!IsRetainedHudPayloadTexture(textureId)) return nullptr;
	if((textureId & kRetainedHudTeamEmblemBit) != 0) return nullptr;
	const uint32_t index = textureId & kRetainedHudPayloadIndexMask;
	if(index >= static_cast<uint32_t>(g_spritePayloadCount)) return nullptr;
	return &g_spritePayloads[index];
}

const RetainedHudTeamEmblemPayload * ResolveRetainedHudTeamEmblemPayload(uint32_t textureId) {
	if(!IsRetainedHudPayloadTexture(textureId)) return nullptr;
	if((textureId & kRetainedHudTeamEmblemBit) == 0) return nullptr;
	const uint32_t index = textureId & kRetainedHudPayloadIndexMask;
	if(index >= static_cast<uint32_t>(g_teamEmblemPayloadCount)) return nullptr;
	return &g_teamEmblemPayloads[index];
}

}  // namespace client_ui
}  // namespace silencer
