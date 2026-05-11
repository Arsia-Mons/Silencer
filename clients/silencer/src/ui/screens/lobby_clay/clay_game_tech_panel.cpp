#include "clay_game_tech_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/toggle.h"

#include "lobby_clay_screen.h"
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

namespace silencer::ui::lobby_clay {

namespace {

// Legacy GameTechPanel coordinates — copied verbatim from
// game_tech_panel.cpp so on-screen geometry matches one-for-one.
constexpr int kBtnBackX  = 242;
constexpr int kBtnBackY  = 68;
constexpr int kSlotsX    = 455;
constexpr int kSlotsY    = 100;
constexpr int kColX0     = 410;     // checkbox column 0 x
constexpr int kColW      = 14;      // checkbox column stride
constexpr int kRowY0     = 125;     // first checkbox row y
constexpr int kRowH      = 13;      // checkbox row stride
constexpr int kLocalCol  = 3;       // local peer always lands in column 3
constexpr int kTechNameY = 350;     // centered Heading at y=350
constexpr int kDescX     = 405;
constexpr int kDescY0    = 370;
constexpr int kDescLH    = 10;
constexpr int kPeerNameAnchorX = 375;  // right-aligned at x=375
constexpr int kPeerNameY0     = 112;
constexpr int kPeerNameStride = 16;
constexpr int kLocalNameX0    = 425;   // local-row tech-name x offset (425 + col*14)
constexpr int kLocalNameY0    = 127;   // local-row tech-name y offset (127 + ipos*13)
constexpr Uint8 kCheckboxBank = 7;
constexpr Uint8 kCheckboxOn   = 18;
constexpr Uint8 kCheckboxOff  = 19;
constexpr int kCheckboxW      = 13;
constexpr int kCheckboxH      = 13;

// Per-row click adapter storage. Local-column checkboxes + tech-name labels
// each get their own. 32 covers the Uint8-uid protocol limit (110..199 per
// column) for any agency's tech list.
constexpr int kMaxRows = 32;
struct ClickAdapter {
	GameTechPanelState * state;
	int itemIndex;
};
ClickAdapter g_toggleAdapters[kMaxRows];
ClickAdapter g_descAdapters[kMaxRows];

// Pointer-stable per-row label slab. Filled inside BuildGameTechPanelTree
// from buyableitem->name + techslots; Clay holds the c_str() pointers for
// the duration of the layout pass.
std::string g_rowLabels[kMaxRows];

// Per-row Clay_String ID buffers for the inner Toggle elements. The Toggle
// primitive takes a Clay_String (not a Clay_ElementId) and computes
// CLAY_SID(id) internally — so we need pointer-stable distinct strings per
// row. Local-column boxes get one slot per row; remote-column boxes are
// keyed by (col, bIdx) but we only need 3 cols × 32 rows = 96 slots.
char g_localToggleIdBuf [kMaxRows][16];
char g_remoteToggleIdBuf[3 * kMaxRows][16];

void OnRowToggleClicked(void * user) {
	auto * a = static_cast<ClickAdapter *>(user);
	if(a && a->state) a->state->toggleClickedItemIndex = a->itemIndex;
}

void DescClickProxy(::Clay_ElementId /*id*/,
                    ::Clay_PointerData data,
                    std::intptr_t userPtr) {
	if(data.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * a = reinterpret_cast<ClickAdapter *>(userPtr);
	if(a && a->state) a->state->descClickedItemIndex = a->itemIndex;
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
                       LobbyClayScreen & owner) {
	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Peer * localpeer = owner.TechPanelPeer(world, localid);
	Team * team = world.GetPeerTeam(localid);

	// "Tech slots left: N" — mirrors legacy uid-70 overlay refresh.
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

	// Refresh non-local peer name strings — used by BuildTree for the three
	// remote-peer column header texts.
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

	// Description-overlay click → rewrite tech-name + 8 description lines.
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

	// Checkbox click in the local column → toggle the tech bit + persist.
	// Legacy gate: only fires when the row's effectbrightness was 128 in
	// Build's "interactable" branch (item.techslots <= slotsleft OR already
	// chosen). Re-check here against current state.
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

	// Back To Teams → ShowGameJoin (mirrors legacy GTECH_BTN_BACK handler).
	if(state.backClicked){
		state.backClicked = false;
		owner.ShowGameJoin(ctx);
		return;
	}
}

void BuildGameTechPanelTree(GameTechPanelState & state,
                            World & world,
                            Resources & resources,
                            LobbyClayScreen & owner) {
	using silencer::clay_bridge::PackImage;

	// Right border chrome sprite (bank 7 idx 8) — same anchor-compensation
	// pattern as the other right-pane panels.
	const Uint16 borderW = resources.spritewidth[7][8];
	const Uint16 borderH = resources.spriteheight[7][8];
	const int borderX = 0 - resources.spriteoffsetx[7][8];
	const int borderY = 0 - resources.spriteoffsety[7][8];
	CLAY({ .id = CLAY_ID("GTechRightBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(borderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(borderH)) },
	       },
	       .image    = { .imageData = PackImage(7, 8) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(borderX),
	                                   static_cast<float>(borderY) } } }) {}

	// Back To Teams button (top of pane).
	const int backOffX = kBtnBackX - resources.spriteoffsetx[7][24];
	const int backOffY = kBtnBackY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GTechBackWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)backOffX, (float)backOffY } } }) {
		BankButton(CLAY_STRING("Back To Teams"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnBackClicked,
		                             /*user*/       &state });
	}

	// Tech slots left text (bank 133/w6/eff=129/brightness=144/colorRamp).
	if(!state.slotsLeftStr.empty()){
		CLAY({ .id = CLAY_ID("GTechSlots"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)kSlotsX, (float)kSlotsY } } }) {
			BankText(FromStd(state.slotsLeftStr),
			         BankTextVariant::Body,
			         { .effectColor = 129,
			           .brightness  = static_cast<Uint8>(128 + 16),
			           .colorRamp   = true });
		}
	}

	const Uint8 localid = owner.TechPanelLocalPeerId(world);
	Team * team = world.GetPeerTeam(localid);

	// No team yet (e.g., test driver swapped panels without joining a game)
	// — emit only the chrome + Back button. Mirrors the legacy state where
	// GameTechPanel::Tick early-returns at `if(!team) return;` leaving all
	// overlays/checkboxes at their initial empty/draw=false state.
	if(team){
		// Walk team peers + buyableitems and emit the 4-column grid.
		int peerindex = 0;  // Counts non-local peers only (columns 0..2).
		int localAdapter = 0;
		int rowLabelSlot = 0;

		for(int i = 0; i < 4; i++){
			const bool isLocal = (team->peers[i] == localid);
			const bool draw    = (i < team->numpeers);
			const int  col     = isLocal ? kLocalCol : peerindex;
			if(!isLocal) peerindex++;

			if(!draw) continue;

			Peer * peer = owner.TechPanelPeer(world, team->peers[i]);

			// Non-local peer: header name + column separator sprite.
			if(!isLocal && col < 3){
				const std::string & nm = state.peerNameStrs[col];
				if(!nm.empty()){
					// Right-align at x = kPeerNameAnchorX using bank 133/w6.
					const int textPxW = static_cast<int>(nm.size()) * 6;
					const int nameX = kPeerNameAnchorX - textPxW;
					const int nameY = kPeerNameY0 + ((2 - col) * kPeerNameStride);
					CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechPeerName"),
					                       static_cast<uint32_t>(col)),
					       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
					                     .offset   = { (float)nameX,
					                                   (float)nameY } } }) {
						BankText(FromStd(nm), BankTextVariant::Body, {});
					}
				}
				// Separator-line sprite (bank 7 idx 20+(2-col)) at (0,0)
				// anchor — apply spriteoffset compensation so the visible
				// pixels land at the same place the legacy renderer paints.
				const Uint8 sepIdx = static_cast<Uint8>(20 + (2 - col));
				const Uint16 sw = resources.spritewidth[7][sepIdx];
				const Uint16 sh = resources.spriteheight[7][sepIdx];
				const int sx = 0 - resources.spriteoffsetx[7][sepIdx];
				const int sy = 0 - resources.spriteoffsety[7][sepIdx];
				CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechSep"),
				                       static_cast<uint32_t>(col)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(sw)),
				                       CLAY_SIZING_FIXED(static_cast<float>(sh)) },
				       },
				       .image    = { .imageData = PackImage(7, sepIdx) },
				       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
				                     .offset   = { (float)sx, (float)sy } } }) {}
			}

			// Per-row checkbox column + (local-only) tech-name labels.
			int techslotsleft = 0;
			if(isLocal && peer){
				User * user = world.lobby.GetUserInfo(peer->accountid);
				if(user){
					techslotsleft = user->agency[team->agency].techslots
					              - world.TechSlotsUsed(*peer);
				}
			}

			int ipos = 0;
			for(size_t bIdx = 0; bIdx < world.buyableitems.size(); bIdx++){
				BuyableItem * item = world.buyableitems[bIdx];
				if(!item->techslots) continue;
				if(item->agencyspecific != -1 && item->agencyspecific != team->agency){
					continue;
				}

				const bool selected = peer && (peer->techchoices & item->techchoice);

				// Legacy "interactable" gate (variable named `usable` in
				// legacy code but inverted: interactable iff techslots fit
				// remaining slots OR already chosen). Non-local columns
				// always render dim (brightness 64).
				bool interactable = false;
				if(isLocal){
					interactable = (item->techslots <= techslotsleft)
					            || (peer && (peer->techchoices & item->techchoice));
				}
				const Uint8 brightness = isLocal && interactable ? 128 : 64;
				const Uint8 boxIdx = selected ? kCheckboxOn : kCheckboxOff;

				const int boxX = kColX0 + (col * kColW)
				               - resources.spriteoffsetx[kCheckboxBank][boxIdx];
				const int boxY = kRowY0 + (ipos * kRowH)
				               - resources.spriteoffsety[kCheckboxBank][boxIdx];

				ToggleOpts tOpts{};
				tOpts.width  = kCheckboxW;
				tOpts.height = kCheckboxH;
				tOpts.effectColor = 0;
				tOpts.selectedBrightness   = brightness;
				tOpts.unselectedBrightness = brightness;

				if(isLocal && localAdapter < kMaxRows){
					g_toggleAdapters[localAdapter].state = &state;
					g_toggleAdapters[localAdapter].itemIndex = static_cast<int>(bIdx);
					std::snprintf(g_localToggleIdBuf[localAdapter],
					              sizeof(g_localToggleIdBuf[localAdapter]),
					              "GTechBoxL%d", static_cast<int>(bIdx));
					Clay_String innerId;
					innerId.isStaticallyAllocated = false;
					innerId.length = static_cast<int32_t>(
						std::strlen(g_localToggleIdBuf[localAdapter]));
					innerId.chars  = g_localToggleIdBuf[localAdapter];
					CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechBoxLocal"),
					                       static_cast<uint32_t>(bIdx)),
					       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
					                     .offset   = { (float)boxX,
					                                   (float)boxY } } }) {
						Toggle(innerId,
						       kCheckboxBank, boxIdx, selected, tOpts,
						       ToggleHandle{ /*hoveredOut*/ nullptr,
						                     /*onClick*/    &OnRowToggleClicked,
						                     /*user*/       &g_toggleAdapters[localAdapter] });
					}
					localAdapter++;
				}else{
					// Remote-peer column — read-only display, no onClick.
					const int slot = (col * kMaxRows) + static_cast<int>(bIdx);
					if(slot >= 0 && slot < static_cast<int>(sizeof(g_remoteToggleIdBuf)
					                                       / sizeof(g_remoteToggleIdBuf[0]))){
						std::snprintf(g_remoteToggleIdBuf[slot],
						              sizeof(g_remoteToggleIdBuf[slot]),
						              "GTBR%d_%d", col, static_cast<int>(bIdx));
						Clay_String innerId;
						innerId.isStaticallyAllocated = false;
						innerId.length = static_cast<int32_t>(
							std::strlen(g_remoteToggleIdBuf[slot]));
						innerId.chars  = g_remoteToggleIdBuf[slot];
						CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechBoxRemote"),
						                       static_cast<uint32_t>(slot)),
						       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
						                     .offset   = { (float)boxX,
						                                   (float)boxY } } }) {
							Toggle(innerId,
							       kCheckboxBank, boxIdx, selected, tOpts,
							       ToggleHandle{});
						}
					}
				}

				// Local-column row tech-name overlay — clickable to swap
				// the description.
				if(isLocal){
					if(rowLabelSlot < kMaxRows){
						g_rowLabels[rowLabelSlot] = item->name;
						g_rowLabels[rowLabelSlot] += " (";
						g_rowLabels[rowLabelSlot] += std::to_string(item->techslots);
						g_rowLabels[rowLabelSlot] += ")";

						g_descAdapters[rowLabelSlot].state = &state;
						g_descAdapters[rowLabelSlot].itemIndex = static_cast<int>(bIdx);

						const int lblX = kLocalNameX0 + (col * kColW);
						const int lblY = kLocalNameY0 + (ipos * kRowH);
						CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
						                       static_cast<uint32_t>(bIdx)),
						       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
						                     .offset   = { (float)lblX,
						                                   (float)lblY } } }) {
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
				ipos++;
			}
		}
	}

	// Tech-name centered Heading (uid 60). Empty string suppresses emission
	// — matches legacy where an empty Overlay paints nothing.
	if(!state.techNameStr.empty()){
		const int len = static_cast<int>(state.techNameStr.size());
		const int x = 401 + (116 - (len * 8) / 2);
		CLAY({ .id = CLAY_ID("GTechName"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)x, (float)kTechNameY } } }) {
			BankText(FromStd(state.techNameStr),
			         BankTextVariant::Heading, {});
		}
	}

	// 8 description lines (uid 61..68). Skip empty lines to match the
	// legacy DrawText "no chars → no paint" behavior.
	for(int i = 0; i < 8; i++){
		if(state.techDescLines[i].empty()) continue;
		const int y = kDescY0 + (i * kDescLH);
		CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechDesc"),
		                       static_cast<uint32_t>(i)),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)kDescX, (float)y } } }) {
			BankText(FromStd(state.techDescLines[i]),
			         BankTextVariant::Body,
			         { .effectColor = 129,
			           .brightness  = static_cast<Uint8>(128 + 16),
			           .colorRamp   = true });
		}
	}
}

}  // namespace silencer::ui::lobby_clay
