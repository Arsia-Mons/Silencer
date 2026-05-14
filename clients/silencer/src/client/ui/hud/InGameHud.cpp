#include "client/ui/hud/InGameHud.h"

#include "client/ui/hud/hud_buy_tech_overlay.h"
#include "client/ui/hud/hud_chat_overlay.h"
#include "client/ui/hud/hud_readouts.h"
#include "client/ui/hud/hud_secret_overlays.h"
#include "client/ui/hud/hud_status_sprites.h"
#include "client/ui/hud/hud_system_camera.h"
#include "client/ui/hud/hud_teams.h"
#include "client/ui/views/HudView.h"
#include "render/renderer.h"

namespace silencer {
namespace client_ui {

// Top-level composition. Reads the per-frame HudView, picks a viewed player,
// and orders the sub-builders. Each sub-builder owns a single concern (status
// sprites, readouts, team strip, secret hack overlay, trace time, buy/tech
// overlay, chat overlay, system-camera frame) and lives in its own TU.
void BuildInGameHudUi(Renderer& renderer, const Resources& resources,
                      const HudView& view, Surface* surface) {
	if(!view.mapLoaded) return;
	if(!view.localPlayer.valid) return;

	const PlayerHudView& player = view.viewedPlayer;
	Uint8 phase = renderer.GetHudAnimationPhase();

	if(view.systemCamera[0].active){
		BuildHudSystemCameraFrame(resources, surface, 95, 2, 92, 381);
	}
	if(view.systemCamera[1].active){
		BuildHudSystemCameraFrame(resources, surface, 95, 11, 92, 318);
	}

	if(!player.valid) return;

	Uint8 currentammo = BuildHudStatusSprites(renderer, resources, surface, player, phase);
	BuildHudReadouts(player, currentammo, surface);

	int teamCount = BuildHudTeams(resources, surface, view, phase);
	const TeamHudView* team = FindTeamById(view, player.teamId);

	if(team && team->baseDoorId){
		int yoffset = 60;
		if(teamCount >= 3) yoffset += (teamCount * 20) - 65;
		BuildHudSecretSprites(resources, surface, view, *team, yoffset, phase);
		if(!team->beamingTerminalId){
			BuildHudSecretProgress(surface, player, yoffset, team->secretProgress, phase);
		}
	}

	Uint8 tracetime = 0;
	if(team && team->beamingTerminalId && team->beamingTerminalTraceTime > 0){
		tracetime = team->beamingTerminalTraceTime;
	}
	if(player.tracetime > 0) tracetime = player.tracetime;
	if(tracetime > 0) BuildHudTraceTime(tracetime, surface);

	if(view.buyTech.visible){
		BuildBuyTechOverlay(surface, view.buyTech);
	}

	if(view.showChatTicks || player.chatActive){
		BuildChatOverlay(view, surface);
	}
}

}  // namespace client_ui
}  // namespace silencer
