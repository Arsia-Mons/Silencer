#include "tech_tree_grid.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/toggle.h"

#include "lobby_screen.h"
#include "world.h"
#include "lobby.h"
#include "team.h"
#include "peer.h"
#include "user.h"
#include "buyableitem.h"

#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleHandle;
using silencer::ui::primitives::ToggleOpts;

namespace silencer::client_ui::lobby {

namespace {

constexpr int kRowH      = 13;
constexpr int kLocalCol  = 3;
constexpr Uint8 kCheckboxBank = 7;
constexpr Uint8 kCheckboxOn   = 18;
constexpr Uint8 kCheckboxOff  = 19;
constexpr int kCheckboxW      = 13;
constexpr int kCheckboxH      = 13;
constexpr const char * kActionTogglePrefix = "lobby.game_tech.toggle.";
constexpr const char * kActionDescriptionPrefix = "lobby.game_tech.description.";

constexpr uint16_t kTallGridPadLeft    = 12;
constexpr uint16_t kTallGridPadTop     = 14;
constexpr uint16_t kTallGridColGap     = 1;
constexpr uint16_t kTallLabelColGap    = 2;
constexpr uint16_t kTallLabelRowPadTop = 2;

constexpr int kMaxRows = 32;
std::string g_rowLabels[kMaxRows];

char g_localToggleIdBuf [kMaxRows][16];
char g_remoteToggleIdBuf[3 * kMaxRows][16];

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

}  // namespace

void BuildTechTreeGrid(World & world,
                       LobbyScreen & owner,
                       silencer::ui::UiInteractionRegistry& interactions) {
	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Team * team = world.GetPeerTeam(localid);

	CLAY({ .id = CLAY_ID("GTechGridWrap"),
	       .layout = {
	           .padding = { kTallGridPadLeft, 0,
	                        kTallGridPadTop,  0 },
	           .childGap = kTallGridColGap,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		if(team){
			// Discover which legacy `col` each team-peer-slot maps to (the
			// legacy iteration assigns peerindex++ for non-local peers; the
			// local peer always lands in column kLocalCol=3). Build a
			// peer-slot ordering keyed by col 0..3.
			struct ColAssign { int peerSlot; bool draw; bool isLocal; };
			ColAssign cols[4] = { {-1,false,false}, {-1,false,false},
			                       {-1,false,false}, {-1,false,false} };
			int peerindex = 0;
			for(int i = 0; i < 4; i++){
				const bool isLocal = (team->peers[i] == localid);
				const bool draw    = (i < team->numpeers);
				const int  col     = isLocal ? kLocalCol : peerindex;
				if(!isLocal) peerindex++;
				if(col >= 0 && col < 4){
					cols[col].peerSlot = i;
					cols[col].draw     = draw;
					cols[col].isLocal  = isLocal;
				}
			}

			int localAdapter = 0;
			int rowLabelSlot = 0;

			for(int col = 0; col < 4; ++col){
				char colIdBuf[24];
				std::snprintf(colIdBuf, sizeof(colIdBuf), "GTechCol%d", col);
				Clay_String colId;
				colId.isStaticallyAllocated = false;
				colId.length = (int32_t)std::strlen(colIdBuf);
				colId.chars  = colIdBuf;

				CLAY({ .id = CLAY_SID(colId),
				       .layout = {
				           .layoutDirection = CLAY_TOP_TO_BOTTOM,
				       } }) {
					if(!cols[col].draw) continue;
					const ColAssign & ca = cols[col];
					Peer * peer = owner.TechPanelPeer(world, team->peers[ca.peerSlot]);

					int techslotsleft = 0;
					if(ca.isLocal && peer){
						User * user = world.lobby.GetUserInfo(peer->accountid);
						if(user){
							techslotsleft = user->agency[team->agency].techslots
							              - world.TechSlotsUsed(*peer);
						}
					}

					for(size_t bIdx = 0; bIdx < world.buyableitems.size(); bIdx++){
						BuyableItem * item = world.buyableitems[bIdx];
						if(!item->techslots) continue;
						if(item->agencyspecific != -1
						   && item->agencyspecific != team->agency){
							continue;
						}

						const bool selected = peer && (peer->techchoices & item->techchoice);
						bool interactable = false;
						if(ca.isLocal){
							interactable = (item->techslots <= techslotsleft)
							            || (peer && (peer->techchoices & item->techchoice));
						}
						const Uint8 brightness = ca.isLocal && interactable ? 128 : 64;
						const Uint8 boxIdx = selected ? kCheckboxOn : kCheckboxOff;

						ToggleOpts tOpts{};
						tOpts.width  = kCheckboxW;
						tOpts.height = kCheckboxH;
						tOpts.effectColor = 0;
						tOpts.selectedBrightness   = brightness;
						tOpts.unselectedBrightness = brightness;

						if(ca.isLocal && localAdapter < kMaxRows){
							std::snprintf(g_localToggleIdBuf[localAdapter],
							              sizeof(g_localToggleIdBuf[localAdapter]),
							              "GTechBoxL%d", static_cast<int>(bIdx));
							Clay_String innerId;
							innerId.isStaticallyAllocated = false;
							innerId.length = (int32_t)std::strlen(g_localToggleIdBuf[localAdapter]);
							innerId.chars  = g_localToggleIdBuf[localAdapter];
							std::string actionId =
								std::string(kActionTogglePrefix) + std::to_string(static_cast<int>(bIdx));
							Toggle(innerId,
							       kCheckboxBank, boxIdx, selected, tOpts,
							       ToggleHandle{ /*hoveredOut*/ nullptr,
							                     /*actionId*/   actionId.c_str(),
							                     /*interactions*/ &interactions });
							localAdapter++;
						}else{
							const int slot = (col * kMaxRows) + static_cast<int>(bIdx);
							if(slot >= 0 && slot < static_cast<int>(sizeof(g_remoteToggleIdBuf)
							                                       / sizeof(g_remoteToggleIdBuf[0]))){
								std::snprintf(g_remoteToggleIdBuf[slot],
								              sizeof(g_remoteToggleIdBuf[slot]),
								              "GTBR%d_%d", col, static_cast<int>(bIdx));
								Clay_String innerId;
								innerId.isStaticallyAllocated = false;
								innerId.length = (int32_t)std::strlen(g_remoteToggleIdBuf[slot]);
								innerId.chars  = g_remoteToggleIdBuf[slot];
								Toggle(innerId,
								       kCheckboxBank, boxIdx, selected, tOpts,
								       ToggleHandle{});
							}
						}
					}
				}
			}

			// Local-peer tech-name label column.
			CLAY({ .id = CLAY_ID("GTechLocalLabels"),
			       .layout = {
			           .padding = { kTallLabelColGap, 0,
			                        kTallLabelRowPadTop, 0 },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				const int localColSlot = 3;
				const ColAssign & lc = cols[localColSlot];
				if(lc.draw){
					Peer * peer = owner.TechPanelPeer(world, team->peers[lc.peerSlot]);
					int techslotsleft = 0;
					if(peer){
						User * user = world.lobby.GetUserInfo(peer->accountid);
						if(user){
							techslotsleft = user->agency[team->agency].techslots
							              - world.TechSlotsUsed(*peer);
						}
					}
					for(size_t bIdx = 0; bIdx < world.buyableitems.size(); bIdx++){
						BuyableItem * item = world.buyableitems[bIdx];
						if(!item->techslots) continue;
						if(item->agencyspecific != -1
						   && item->agencyspecific != team->agency){
							continue;
						}
						bool interactable = (item->techslots <= techslotsleft)
						                 || (peer && (peer->techchoices & item->techchoice));
						const Uint8 brightness = interactable ? 128 : 64;

						if(rowLabelSlot < kMaxRows){
							g_rowLabels[rowLabelSlot] = item->name;
							g_rowLabels[rowLabelSlot] += " (";
							g_rowLabels[rowLabelSlot] += std::to_string(item->techslots);
							g_rowLabels[rowLabelSlot] += ")";

							silencer::ui::UiInteractable desc;
							desc.id = std::string(kActionDescriptionPrefix) + std::to_string(static_cast<int>(bIdx));
							desc.labelText = g_rowLabels[rowLabelSlot];
							desc.kind = silencer::ui::UiInteractableKind::Button;
							desc.clayId = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
							                         static_cast<uint32_t>(bIdx));
							desc.hasClayId = true;
							interactions.RegisterInteractable(desc);

							CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
							                       static_cast<uint32_t>(bIdx)),
							       .layout = {
							           .sizing = { CLAY_SIZING_GROW(0),
							                       CLAY_SIZING_FIXED(kRowH) },
							       } }) {
								BankText(FromStd(g_rowLabels[rowLabelSlot]),
								         BankTextVariant::Body,
								         { .brightness = brightness });
							}
							rowLabelSlot++;
						}
					}
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
