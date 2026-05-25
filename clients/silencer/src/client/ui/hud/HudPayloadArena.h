#pragma once

#include "clay/clay.h"
#include "render/clay_ui_payloads.h"

#include <string>

namespace silencer {
namespace client_ui {

void HudPayloadBeginFrame();

Clay_String AllocHudString(const char* text, int length);
Clay_String AllocHudString(const std::string& text);

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload);
silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload);
silencer::clay_bridge::ClayCustomData* AllocMessageBackgroundCustomData();

}  // namespace client_ui
}  // namespace silencer
