#include "client/ui/hud/InGameHud.h"

#include "client/ui/hud/hud_status_sprites.h"
#include "client/ui/hud/hud_teams.h"
#include "client/ui/views/HudView.h"
#include "render/renderer.h"

namespace silencer {
namespace client_ui {

// Top-level composition. Reads the per-frame HudView, picks a viewed player,
// and orders the sub-builders. Each sub-builder owns a single concern (status
// sprites and team strip) and lives in its own TU. Retained in-game HUD
// readouts/overlays and system-camera chrome are composed by ClientUi after
// this legacy HUD pass.
void BuildInGameHudUi(Renderer& renderer, const Resources& resources,
                      const HudView& view, Surface* surface) {
	if(!view.mapLoaded) return;
	if(!view.localPlayer.valid) return;

	const PlayerHudView& player = view.viewedPlayer;
	Uint8 phase = renderer.GetHudAnimationPhase();

	if(!player.valid) return;

	BuildHudStatusSprites(player, surface, resources, renderer, phase);

	BuildHudTeams(view, surface, resources, phase);
}

}  // namespace client_ui
}  // namespace silencer
