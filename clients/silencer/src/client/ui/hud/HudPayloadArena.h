#pragma once

#include "render/clay_ui_payloads.h"

namespace silencer {
namespace client_ui {

void HudPayloadBeginFrame();

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload);
silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload);

}  // namespace client_ui
}  // namespace silencer
