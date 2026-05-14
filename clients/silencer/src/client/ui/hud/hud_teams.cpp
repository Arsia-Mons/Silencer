#include "client/ui/hud/hud_teams.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "render/clay_ui_payloads.h"
#include "resources.h"
#include "surface.h"

#include <vector>

namespace silencer {
namespace client_ui {

int BuildHudTeams(const Resources& resources, Surface* surface,
                  const HudView& view, Uint8 phase) {
	if(view.teams.empty()) return 0;

	struct SpriteSpec { int x; int y; Uint8 bank; Uint16 index; Uint8 rampColor; Uint8 rampPlus; };
	std::vector<SpriteSpec> sprites;
	auto addSprite = [&](int x, int y, Uint8 bank, Uint16 index,
	                     Uint8 rampColor = 0, Uint8 rampPlus = 0) {
		sprites.push_back(SpriteSpec{x, y, bank, index, rampColor, rampPlus});
	};

	if(view.teams.size() == 1) {
		addSprite(SpriteX(resources, 94, 1), SpriteY(resources, 94, 1), 94, 1);
	}else{
		addSprite(SpriteX(resources, 103, 0),
		          SpriteY(resources, 103, 0, -133 + ((int)view.teams.size() - 1) * 20),
		          103, 0);
		addSprite(SpriteX(resources, 103, 1), SpriteY(resources, 103, 1), 103, 1);
	}

	int teamyoffset = 5;
	for(const TeamHudView& team : view.teams) {
		for(int i = 0; i < team.numPeers; i++) {
			const TeamHudView::PeerSlot& slot = team.peerSlots[i];
			if(!slot.present) continue;
			Uint8 index = (slot.state == kPlayerStateDead || slot.state == kPlayerStateDying ? 8 : 4);
			Uint8 rampColor = 0;
			Uint8 rampPlus = 0;
			if(slot.inBase || slot.hasSecret) {
				Uint8 time = 4;
				Uint8 shift = 2;
				rampColor = 210;
				if(slot.hasSecret) {
					time = 8;
					rampColor = 114;
					shift = 0;
				}
				if((phase >> shift) % (time * 2) < time) rampPlus += ((phase >> shift) % time);
				else rampPlus += time - ((phase >> shift) % time);
			}
			addSprite(SpriteX(resources, 103, index + i, 25 + (17 * i)),
			          SpriteY(resources, 103, index + i, teamyoffset),
			          103, index + i, rampColor, rampPlus);
		}

		int playerswithsecret = 0;
		for(int i = 0; i < team.numPeers; i++) {
			if(team.peerSlots[i].hasSecret) playerswithsecret++;
		}
		for(int i = 0; i < 3; i++) {
			Uint8 index = team.secrets > i ? 2 : 3;
			Uint8 color = 0;
			if(index == 3 && playerswithsecret > i - team.secrets && view.tickCount % 12 < 6) index = 2;
			if(team.beamingTerminalId && team.secrets == i && index == 3) {
				color = 224;
				index = 3;
			}
			addSprite(SpriteX(resources, 103, index, -(9 * (3 - i)) + 11),
			          SpriteY(resources, 103, index, teamyoffset),
			          103, index, color);
		}
		teamyoffset += 20;
	}

	CLAY({ .id = CLAY_ID("InGameHudTeamsRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
	       } }) {
		for(unsigned int i = 0; i < sprites.size(); ++i) {
			const SpriteSpec& s = sprites[i];
			int w = SpriteWidth(resources, s.bank, s.index);
			int h = SpriteHeight(resources, s.bank, s.index);
			if(w <= 0 || h <= 0) continue;
			CLAY({ .id = CLAY_IDI("HudTeamSpriteWrap", i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			       },
			       .floating = {
				       .offset = { (float)s.x, (float)s.y },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			}) {
				CLAY({ .id = CLAY_IDI("HudTeamSprite", i),
				       .layout = {
					       .sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
				       },
				       .custom = { .customData = AllocSpriteCustomData({
					       s.bank, s.index, 0, 0, 0, 0, 0, 128, s.rampColor, s.rampPlus,
				       }) },
				}) {}
			}
		}
		int yoffset = 5;
		for(unsigned int i = 0; i < view.teams.size(); ++i) {
			const TeamHudView& team = view.teams[i];
			CLAY({ .id = CLAY_IDI("HudTeamEmblem", i),
			       .layout = {
				       .sizing = { CLAY_SIZING_FIXED((float)team.emblemW), CLAY_SIZING_FIXED((float)team.emblemH) },
			       },
			       .floating = {
				       .offset = { 5, (float)(yoffset + 1) },
				       .attachTo = CLAY_ATTACH_TO_ROOT,
			       },
			       .custom = { .customData = AllocTeamEmblemCustomData({
				       181, team.agency, team.color, 17, true,
			       }) },
			}) {}
			yoffset += 20;
		}
	}
	return (int)view.teams.size();
}

const TeamHudView* FindTeamById(const HudView& view, Uint16 teamId) {
	for(const TeamHudView& team : view.teams){
		if(team.id == teamId) return &team;
	}
	return nullptr;
}

}  // namespace client_ui
}  // namespace silencer
