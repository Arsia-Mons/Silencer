#include "game_tech_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/toggle.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "world.h"
#include "lobby.h"
#include "resources.h"
#include "team.h"
#include "peer.h"
#include "user.h"
#include "buyableitem.h"
#include "config.h"
#include "game.h"

#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleHandle;
using silencer::ui::primitives::ToggleOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords retained ONLY for inspector hit-rect registration.
constexpr int kBtnBackX  = 242;
constexpr int kBtnBackY  = 68;
constexpr int kSlotsX    = 455;
constexpr int kSlotsY    = 100;
constexpr int kColX0     = 410;
constexpr int kColW      = 14;
constexpr int kRowY0     = 125;
constexpr int kRowH      = 13;
constexpr int kLocalCol  = 3;
constexpr int kTechNameY = 350;
constexpr int kDescX     = 405;
constexpr int kDescY0    = 370;
constexpr int kDescLH    = 10;
constexpr int kLocalNameX0    = 425;
constexpr int kLocalNameY0    = 127;
constexpr Uint8 kCheckboxBank = 7;
constexpr Uint8 kCheckboxOn   = 18;
constexpr Uint8 kCheckboxOff  = 19;
constexpr int kCheckboxW      = 13;
constexpr int kCheckboxH      = 13;

// LobbyRightUpperBox interior layout knobs. Box at (238, 64, 160x121).
constexpr uint16_t kUpperBackPadLeft = 4;   // screen 242 - box 238
constexpr uint16_t kUpperBackPadTop  = 4;   // screen 68  - box 64
constexpr uint16_t kUpperPeerColPadLeft = 4;
constexpr uint16_t kUpperPeerColPadTop  = 7;   // gap after back button (68 + 21 = 89; first peer at 112 → 23 gap)
constexpr uint16_t kUpperPeerRowGap     = 5;   // peer name stride is 16; body height 11 → gap 5

// LobbyRightTallBox interior layout knobs. Box at (398, 64, 232x391).
constexpr uint16_t kTallSlotsPadLeft   = 57;  // screen 455 - box 398
constexpr uint16_t kTallSlotsPadTop    = 36;  // screen 100 - box 64
constexpr uint16_t kTallGridPadLeft    = 12;  // screen 410 - box 398
constexpr uint16_t kTallGridPadTop     = 14;  // 125 - (64 + 36 + 11)
constexpr uint16_t kTallGridColGap     = 1;   // col stride 14, checkbox 13 → 1
constexpr uint16_t kTallLabelColGap    = 2;   // 467 - (452 + 13) = 2
constexpr uint16_t kTallLabelRowPadTop = 2;   // legacy local label y = checkbox y + 2
constexpr uint16_t kTallTechNamePadTop = 16;  // gap from grid bottom to centered heading at y=350
constexpr uint16_t kTallDescPadLeft    = 7;   // screen 405 - box 398
constexpr uint16_t kTallDescPadTop     = 5;   // 370 - (350 + 15)
constexpr uint16_t kTallDescRowGap     = 0;   // body height 11, desc stride 10 → tight (clip below)

constexpr int kMaxRows = 32;
struct ClickAdapter {
	GameTechPanelState * state;
	int itemIndex;
};
ClickAdapter g_toggleAdapters[kMaxRows];
ClickAdapter g_descAdapters[kMaxRows];

std::string g_rowLabels[kMaxRows];

char g_localToggleIdBuf [kMaxRows][16];
char g_remoteToggleIdBuf[3 * kMaxRows][16];

void OnRowToggleClicked(void * user) {
	auto * a = static_cast<ClickAdapter *>(user);
	if(a && a->state) a->state->toggleClickedItemIndex = a->itemIndex;
}

void OnDescClicked(void * user) {
	auto * a = static_cast<ClickAdapter *>(user);
	if(a && a->state) a->state->descClickedItemIndex = a->itemIndex;
}

void DescClickProxy(::Clay_ElementId /*id*/,
                    ::Clay_PointerData data,
                    std::intptr_t userPtr) {
	if(data.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * a = reinterpret_cast<ClickAdapter *>(userPtr);
	if(a && a->state) silencer::ui::automation::QueueClick("GTechDescription", &OnDescClicked, a);
}

void OnBackClicked(void * user) {
	auto * s = static_cast<GameTechPanelState *>(user);
	if(s) s->backClicked = true;
}

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

}  // namespace

void GameTechPanelInit(GameTechPanelState & state) {
	state = GameTechPanelState{};
}

void GameTechPanelTick(GameTechPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner) {
	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Peer * localpeer = owner.TechPanelPeer(world, localid);
	Team * team = world.GetPeerTeam(localid);

	int techslotsleft = 0;
	if(localpeer && team){
		User * user = world.lobby.GetUserInfo(localpeer->accountid);
		if(user){
			techslotsleft =
				user->agency[team->agency].techslots - world.TechSlotsUsed(*localpeer);
			state.slotsLeftStr = "Tech slots left: " + std::to_string(techslotsleft);
		}else{
			state.slotsLeftStr.clear();
		}
	}else{
		state.slotsLeftStr.clear();
		if(!localpeer && world.tickcount % 12 == 0){
			owner.TechPanelRequestPeerList(world);
		}
	}

	for(int i = 0; i < 3; i++) state.peerNameStrs[i].clear();
	if(team){
		int peerindex = 0;
		for(int i = 0; i < 4 && peerindex < 3; i++){
			if(team->peers[i] == localid) continue;
			if(i >= team->numpeers){ peerindex++; continue; }
			Peer * peer = owner.TechPanelPeer(world, team->peers[i]);
			User * user = peer ? world.lobby.GetUserInfo(peer->accountid) : nullptr;
			state.peerNameStrs[peerindex] = user ? std::string(user->name) : std::string();
			peerindex++;
		}
	}

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		if(idx >= 0 && idx < static_cast<int>(world.buyableitems.size())){
			BuyableItem * item = world.buyableitems[idx];
			state.techNameStr  = "-";
			state.techNameStr += item->name;
			state.techNameStr += "-";
			char desc[1024];
			std::strncpy(desc, item->description, sizeof(desc));
			desc[sizeof(desc) - 1] = '\0';
			int lineNo = 0;
			char * line = std::strtok(desc, "\n");
			while(line && lineNo < 8){
				state.techDescLines[lineNo++] = line;
				line = std::strtok(nullptr, "\n");
			}
			for(int j = lineNo; j < 8; j++) state.techDescLines[j].clear();
		}
	}

	if(state.toggleClickedItemIndex >= 0){
		const int idx = state.toggleClickedItemIndex;
		state.toggleClickedItemIndex = -1;
		if(localpeer && team && idx >= 0
		   && idx < static_cast<int>(world.buyableitems.size())){
			BuyableItem * item = world.buyableitems[idx];
			const bool interactable = (item->techslots <= techslotsleft)
			                       || ((localpeer->techchoices & item->techchoice) != 0);
			if(interactable){
				const Uint32 newChoices = localpeer->techchoices ^ item->techchoice;
				owner.TechPanelSetTech(world, newChoices);
				Config::GetInstance().defaulttechchoices[team->agency] = newChoices;
				Config::GetInstance().Save();
			}
		}
	}

	if(state.backClicked){
		state.backClicked = false;
		owner.ShowGameJoin(ctx);
		return;
	}
}

void BuildGameTechUpperTree(GameTechPanelState & state,
                            World & world,
                            Resources & resources,
                            LobbyScreen & owner) {
	(void)world;
	(void)resources;
	(void)owner;

	// Back To Teams button.
	CLAY({ .id = CLAY_ID("GTechBackWrap"),
	       .layout = { .padding = { kUpperBackPadLeft, 0,
	                                kUpperBackPadTop,  0 } } }) {
		BankButton(CLAY_STRING("Back To Teams"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnBackClicked,
		                             /*user*/       &state });
	}
	silencer::ui::automation::Widget w;
	w.label = "Back To Teams";
	w.kind  = silencer::ui::automation::WidgetKind::Button;
	w.x = kBtnBackX; w.y = kBtnBackY; w.w = 156; w.h = 21;
	w.onClick = &OnBackClicked; w.clickUser = &state;
	silencer::ui::automation::Register(w);

	// Peer name labels — right-aligned column. ALIGN_X_RIGHT inside a
	// grow-width wrapper aligns each name to the wrapper's right edge.
	CLAY({ .id = CLAY_ID("GTechPeerNames"),
	       .layout = {
	           .padding = { kUpperPeerColPadLeft, 4,
	                        kUpperPeerColPadTop, 0 },
	           .childGap = kUpperPeerRowGap,
	           .childAlignment = { .x = CLAY_ALIGN_X_RIGHT },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		for(int i = 0; i < 3; ++i){
			char idBuf[24];
			std::snprintf(idBuf, sizeof(idBuf), "GTechPeerName%d", i);
			Clay_String wid;
			wid.isStaticallyAllocated = false;
			wid.length = (int32_t)std::strlen(idBuf);
			wid.chars  = idBuf;
			CLAY({ .id = CLAY_SID(wid) }) {
				if(!state.peerNameStrs[i].empty()){
					BankText(FromStd(state.peerNameStrs[i]),
					         BankTextVariant::Body, {});
				}
			}
		}
	}
}

void BuildGameTechTallTree(GameTechPanelState & state,
                           World & world,
                           Resources & resources,
                           LobbyScreen & owner) {
	(void)resources;

	// "Tech slots left: N" — bank 133/w6/eff=129/brightness=144/colorRamp.
	CLAY({ .id = CLAY_ID("GTechSlotsWrap"),
	       .layout = { .padding = { kTallSlotsPadLeft, 0,
	                                kTallSlotsPadTop, 0 } } }) {
		if(!state.slotsLeftStr.empty()){
			BankText(FromStd(state.slotsLeftStr),
			         BankTextVariant::Body,
			         { .effectColor = 129,
			           .brightness  = static_cast<Uint8>(128 + 16),
			           .colorRamp   = true });
		}
	}

	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Team * team = world.GetPeerTeam(localid);

	// Tech grid — 4 column-TOP_TO_BOTTOM children + a label-column for the
	// local peer. Layout strictly inside the tall box via flex padding.
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
							g_toggleAdapters[localAdapter].state = &state;
							g_toggleAdapters[localAdapter].itemIndex = static_cast<int>(bIdx);
							std::snprintf(g_localToggleIdBuf[localAdapter],
							              sizeof(g_localToggleIdBuf[localAdapter]),
							              "GTechBoxL%d", static_cast<int>(bIdx));
							Clay_String innerId;
							innerId.isStaticallyAllocated = false;
							innerId.length = (int32_t)std::strlen(g_localToggleIdBuf[localAdapter]);
							innerId.chars  = g_localToggleIdBuf[localAdapter];
							Toggle(innerId,
							       kCheckboxBank, boxIdx, selected, tOpts,
							       ToggleHandle{ /*hoveredOut*/ nullptr,
							                     /*onClick*/    &OnRowToggleClicked,
							                     /*user*/       &g_toggleAdapters[localAdapter] });
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

							g_descAdapters[rowLabelSlot].state = &state;
							g_descAdapters[rowLabelSlot].itemIndex = static_cast<int>(bIdx);

							CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
							                       static_cast<uint32_t>(bIdx)),
							       .layout = {
							           .sizing = { CLAY_SIZING_GROW(0),
							                       CLAY_SIZING_FIXED(kRowH) },
							       } }) {
								::Clay_OnHover(DescClickProxy,
								               reinterpret_cast<std::intptr_t>(
								                   &g_descAdapters[rowLabelSlot]));
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

	// Centered tech-name heading. ALIGN_X_CENTER in a grow wrapper sized to
	// the legacy 232-wide tall pane.
	CLAY({ .id = CLAY_ID("GTechNameWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(15) },
	           .padding = { 0, 0, kTallTechNamePadTop, 0 },
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	       } }) {
		if(!state.techNameStr.empty()){
			BankText(FromStd(state.techNameStr),
			         BankTextVariant::Heading, {});
		}
	}

	// 8 description lines.
	CLAY({ .id = CLAY_ID("GTechDescWrap"),
	       .layout = {
	           .padding = { kTallDescPadLeft, 0,
	                        kTallDescPadTop,  0 },
	           .childGap = kTallDescRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		for(int i = 0; i < 8; ++i){
			CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechDescLine"),
			                       static_cast<uint32_t>(i)),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(kDescLH) },
			       } }) {
				if(!state.techDescLines[i].empty()){
					BankText(FromStd(state.techDescLines[i]),
					         BankTextVariant::Body,
					         { .effectColor = 129,
					           .brightness  = static_cast<Uint8>(128 + 16),
					           .colorRamp   = true });
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
