#include "client/ui/hud/hud_player_list_overlay.h"

#include "clay/clay.h"
#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "primitives/text.h"
#include "surface.h"

#include <cstdio>
#include <string>

namespace silencer {
namespace client_ui {

void BuildPlayerListOverlay(const HudView& view, Surface* surface) {
	if(view.teams.empty()) return;
	bool anyPeers = false;
	for(const TeamHudView& team : view.teams){
		if(!team.playerListPeers.empty()){ anyPeers = true; break; }
	}
	if(!anyPeers) return;

	using namespace silencer::ui::primitives;

	CLAY({ .id = CLAY_ID("PlayerListRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 50, 50, 50, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		CLAY({ .id = CLAY_ID("PlayerListPanel"),
		       .layout = {
			       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED((float)(10 + (view.teams.size() * 58))) },
			       .padding = { 10, 10, 10, 0 },
			       .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 128 },
		}) {
			for(unsigned int teamIndex = 0; teamIndex < view.teams.size(); ++teamIndex) {
				const TeamHudView& team = view.teams[teamIndex];
				CLAY({ .id = CLAY_IDI("PlayerListTeamRow", teamIndex),
				       .layout = {
					       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(58) },
					       .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_IDI("PlayerListEmblemSlot", teamIndex),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(40), CLAY_SIZING_GROW(0) },
						       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					       } }) {
						CLAY({ .id = CLAY_IDI("PlayerListTeamEmblem", teamIndex),
						       .layout = { .sizing = { CLAY_SIZING_FIXED((float)team.emblemW), CLAY_SIZING_FIXED((float)team.emblemH) } },
						       .custom = { .customData = AllocTeamEmblemCustomData({181, team.agency, team.color, 17, true}) },
						}) {}
					}
					int yoffset = ((4 - (int)team.playerListPeers.size()) * 12) / 2;
					if(yoffset < 0) yoffset = 0;
					CLAY({ .id = CLAY_IDI("PlayerListPeerColumn", teamIndex),
					       .layout = {
						       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
						       .padding = { 0, 0, (uint16_t)yoffset, 0 },
						       .layoutDirection = CLAY_TOP_TO_BOTTOM,
					       } }) {
						for(unsigned int peerIndex = 0; peerIndex < team.playerListPeers.size(); ++peerIndex) {
							const TeamPeerView& peer = team.playerListPeers[peerIndex];
							char stats[100];
							std::snprintf(stats, sizeof(stats), "L:%d    E:%d  S:%d  J:%d  H:%d  C:%d",
							              peer.agencyLevel, peer.agencyEndurance, peer.agencyShield,
							              peer.agencyJetpack, peer.agencyHacking, peer.agencyContacts);
							std::string statsString = stats;
							CLAY({ .id = CLAY_IDI("PlayerListPeerRow", (teamIndex * 8) + peerIndex),
							       .layout = {
								       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(12) },
								       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
								       .layoutDirection = CLAY_LEFT_TO_RIGHT,
							       } }) {
								CLAY({ .id = CLAY_IDI("PlayerListPeerName", (teamIndex * 8) + peerIndex),
							       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) } } }) {
									Text(ClayStringFromStd(peer.displayName),
									     { .size = TextSize::Body });
								}
								CLAY({ .id = CLAY_IDI("PlayerListPeerStats", (teamIndex * 8) + peerIndex),
								       .layout = { .sizing = { CLAY_SIZING_FIXED((float)((statsString.size() + 1) * 6)), CLAY_SIZING_FIT(0) } } }) {
									Text(ClayStringFromStd(statsString),
									     { .size = TextSize::Body });
								}
							}
						}
					}
				}
			}
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
