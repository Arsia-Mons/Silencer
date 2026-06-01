#include "client/ui/hud/InGameHud.h"

#include "client/ui/hud/hud_teams.h"
#include "client/ui/views/HudView.h"
#include "render/renderer.h"

namespace silencer {
namespace client_ui {

// Top-level composition. Reads the per-frame HudView and keeps the remaining
// legacy team strip alive until it can move to the retained frame. Retained
// in-game HUD status/readouts/overlays and system-camera chrome are composed
// by ClientUi after this legacy HUD pass.
void BuildInGameHudUi(Renderer& renderer, const Resources& resources,
                      const HudView& view, Surface* surface) {
	if(!view.mapLoaded) return;
	if(!view.localPlayer.valid) return;

	const PlayerHudView& player = view.viewedPlayer;
	Uint8 phase = renderer.GetHudAnimationPhase();

	if(!player.valid) return;

	BuildHudTeams(view, surface, resources, phase);
}

}  // namespace client_ui
}  // namespace silencer
