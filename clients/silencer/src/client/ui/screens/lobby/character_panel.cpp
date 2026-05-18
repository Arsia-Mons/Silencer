#include "character_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include "config.h"
#include "lobby.h"
#include "resources.h"
#include "team.h"
#include "user.h"
#include "world.h"

#include <cstdio>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

namespace silencer::client_ui::lobby {

namespace character_panel_detail {

struct AgencyDef {
	Uint8  agency;
	const char * label;
};

const AgencyDef kAgencies[5] = {
	{ Team::NOXIS,     "Noxis" },
	{ Team::LAZARUS,   "Lazarus" },
	{ Team::CALIBER,   "Caliber" },
	{ Team::STATIC,    "Static" },
	{ Team::BLACKROSE, "Blackrose" },
};

constexpr const char * kActionNewCharacter = "lobby.character.new";

// Per-frame text buffers. The layout pass keeps pointers to these for the
// duration of the layout, so they MUST live past BuildCharacterPanelTree's
// return. Static-lifetime works because the layout consumes them
// synchronously inside the caller's BeginLayout/EndLayout window.
struct StatsBuffers {
	std::string account;
	std::string agent;
	std::string agency;
	std::string level;
	std::string wins;
	std::string losses;
	std::string xp;
};
StatsBuffers g_stats;

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars = s.c_str();
	return cs;
}

const TextOpts kStatsOpts{
	.size = TextSize::BodySm,
	.effect = TextEffect::LegacyPalette(129, 160, true),
};

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kStatsPadTop    = 10;
constexpr int kStatsChildGap    = 2;

const char * AgencyLabel(Uint8 agency) {
	for(const AgencyDef& def : kAgencies){
		if(def.agency == agency) return def.label;
	}
	return "Unknown";
}

}  // namespace character_panel_detail

void CharacterPanelInit(CharacterPanelState & state) {
	state.selectedAgency = Config::GetInstance().defaultagency;
	state.lastReconciled = -1;  // forces first-frame reconcile pass
	state.newCharacterRequested = false;
}

void CharacterPanelTick(CharacterPanelState & state, World & world) {
	state.selectedAgency = world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	if(static_cast<int>(state.selectedAgency) != state.lastReconciled){
		state.lastReconciled = state.selectedAgency;
		if(world.IsConnected()){
			world.SetAgency(state.selectedAgency);
		}
	}
}

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == character_panel_detail::kActionNewCharacter){
		state.newCharacterRequested = true;
		return true;
	}
	return false;
}

void BuildCharacterPanelTree(CharacterPanelState & state,
                             World & world,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;
	// Refresh stat strings each frame. The legacy panel gated this behind
	// `agencychanged` because Overlay objects retained their text from
	// frame to frame; Clay rebuilds every frame from scratch, so we
	// recompute unconditionally. The cost is five short string
	// concatenations.
	const Uint8 a = state.selectedAgency;
	const char * account = world.lobby.GetLocalUsername();
	character_panel_detail::g_stats.account = account ? account : "";
	if(const Lobby::Character * ch = world.lobby.GetSelectedCharacter()){
		character_panel_detail::g_stats.agent = "AGENT: ";
		character_panel_detail::g_stats.agent += ch->name;
	}else{
		character_panel_detail::g_stats.agent = "AGENT: none selected";
	}
	character_panel_detail::g_stats.agency = "AGENCY: ";
	character_panel_detail::g_stats.agency += character_panel_detail::AgencyLabel(a);

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && !user->retrieving){
		// xptonextlevel is accumulated XP toward current level; threshold
		// is 100*(level+1). Display the remaining amount. Mirrors the
		// legacy formula exactly.
		const int lvl = user->agency[a].level;
		const int remaining = 100 * (lvl + 1) - static_cast<int>(user->agency[a].xptonextlevel);
		character_panel_detail::g_stats.level  = "LEVEL: "             + std::to_string(lvl);
		character_panel_detail::g_stats.wins   = "WINS: "              + std::to_string(user->agency[a].wins);
		character_panel_detail::g_stats.losses = "LOSSES: "            + std::to_string(user->agency[a].losses);
		character_panel_detail::g_stats.xp     = "XP TO NEXT LEVEL: "  + std::to_string(remaining);
	}else{
		// User info not yet retrieved. Render empty stats — the legacy
		// behavior was to leave Overlay::text empty until the lobby
		// replied, which renders nothing.
		character_panel_detail::g_stats.level.clear();
		character_panel_detail::g_stats.wins.clear();
		character_panel_detail::g_stats.losses.clear();
		character_panel_detail::g_stats.xp.clear();
	}

	// The parent LobbyCharacterBox is supplied by the lobby shell. This
	// function emits only content, so the layout owns a single scene graph instead
	// of a Box nested inside another Box or root-attached legacy coordinates.
	CLAY({ .id = CLAY_ID("CharacterPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { character_panel_detail::kPanelPad, character_panel_detail::kPanelPad, character_panel_detail::kPanelPad, character_panel_detail::kPanelPad },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {

		// Account header. Match the legacy character panel treatment:
		// font bank 134 / width 8 with palette color 200.
		CLAY({ .id = CLAY_ID("CharUserWrap") }) {
			Text(character_panel_detail::FromStd(character_panel_detail::g_stats.account),
			     { .size = TextSize::Heading,
			       .effect = TextEffect::LegacyPalette(200) });
		}

		CLAY({ .id = CLAY_ID("CharIdentity"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
		           .padding = { 0, 0, 6, 0 },
		           .childGap = 2,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			Text(character_panel_detail::FromStd(character_panel_detail::g_stats.agent),
			     { .size = TextSize::BodySm,
			       .effect = TextEffect::LegacyPalette(129, 176, true) });
			Text(character_panel_detail::FromStd(character_panel_detail::g_stats.agency),
			     { .size = TextSize::BodySm,
			       .effect = TextEffect::LegacyPalette(129, 160, true) });
		}

		CLAY({ .id = CLAY_ID("CharNewButtonWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(25) },
		           .padding = { 0, 0, 0, 4 },
		           .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
		       } }) {
			Button(CLAY_STRING("CharNewCharacterButton"),
			       CLAY_STRING("New Character"),
			       ButtonOpts{ .variant = ButtonVariant::Chrome,
			                   .size = ButtonSize::Auto,
			                   .minWidth = 122 },
			       ButtonHandle{ nullptr, character_panel_detail::kActionNewCharacter, &interactions });
			silencer::ui::UiInteractable w;
			w.id = character_panel_detail::kActionNewCharacter;
			w.labelText = "New Character";
			w.kind = silencer::ui::UiInteractableKind::Button;
			interactions.RegisterInteractable(w);
		}

		// LEVEL / WINS / LOSSES / XP — bank 133 / w7 / eff=129,
		// brightness 160 (128+32), ramp on.
		CLAY({ .id = CLAY_ID("CharStats"),
		       .layout = {
		           .padding = { 0, 0, character_panel_detail::kStatsPadTop, 0 },
		           .childGap = (uint16_t)character_panel_detail::kStatsChildGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			const struct { const std::string * txt; const char * id; } kStatsRows[4] = {
				{ &character_panel_detail::g_stats.level,  "CharStatLvl" },
				{ &character_panel_detail::g_stats.wins,   "CharStatWin" },
				{ &character_panel_detail::g_stats.losses, "CharStatLoss" },
				{ &character_panel_detail::g_stats.xp,     "CharStatXp" },
			};
			for(int i = 0; i < 4; ++i){
				if(kStatsRows[i].txt->empty()) continue;
				Clay_String wrapId;
				wrapId.isStaticallyAllocated = true;
				wrapId.length = static_cast<int32_t>(strlen(kStatsRows[i].id));
				wrapId.chars  = kStatsRows[i].id;
				CLAY({ .id = CLAY_SID(wrapId) }) {
					Text(character_panel_detail::FromStd(*kStatsRows[i].txt),
					         character_panel_detail::kStatsOpts);
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
