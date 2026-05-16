#include "character_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/toggle.h"

#include "config.h"
#include "lobby.h"
#include "resources.h"
#include "team.h"
#include "user.h"
#include "world.h"

#include <cstdio>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextOpts;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleHandle;
using silencer::ui::primitives::ToggleOpts;

namespace silencer::client_ui::lobby {

namespace character_panel_detail {

// Five (id, sprite-index, agency-enum) tuples. The element IDs and sprite
// indices are 1:1 with the legacy CharacterPanel's CHR_TGL_* uids /
// bank-181 indices, so render-time pixel layout is unchanged.
struct AgencyDef {
	const char * id;
	const char * actionId;
	Uint16 spriteIndex;
	Uint8  agency;
};

const AgencyDef kAgencies[5] = {
	{ "CharTglNoxis",     "lobby.character.agency.noxis",     0, Team::NOXIS },
	{ "CharTglLazarus",   "lobby.character.agency.lazarus",   1, Team::LAZARUS },
	{ "CharTglCaliber",   "lobby.character.agency.caliber",   2, Team::CALIBER },
	{ "CharTglStatic",    "lobby.character.agency.static",    3, Team::STATIC },
	{ "CharTglBlackrose", "lobby.character.agency.blackrose", 4, Team::BLACKROSE },
};

// Inspector labels — one per agency, in the same order as kAgencies.
const char * kAgencyLabels[5] = { "Noxis", "Lazarus", "Caliber", "Static", "Blackrose" };

// Per-frame text buffers. The layout pass keeps pointers to these for the
// duration of the layout, so they MUST live past BuildCharacterPanelTree's
// return. Static-lifetime works because the layout consumes them
// synchronously inside the caller's BeginLayout/EndLayout window.
struct StatsBuffers {
	std::string username;
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

// Body text appearance for the four stats lines — matches legacy
// `leveltext` etc.: bank 133, w7, eff=129, brightness=160 (128+32), ramp.
constexpr BankTextOpts kStatsOpts{ /*effectColor*/ 129,
                                   /*brightness*/  160,
                                   /*colorRamp*/   true,
                                   /*drawAlpha*/   false };

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kToggleGap      = 26;
constexpr uint16_t kStatsPadTop    = 10;
constexpr int kStatsChildGap    = 2;

}  // namespace character_panel_detail

void CharacterPanelInit(CharacterPanelState & state) {
	state.selectedAgency = Config::GetInstance().defaultagency;
	state.lastReconciled = -1;  // forces first-frame reconcile pass
}

void CharacterPanelTick(CharacterPanelState & state, World & world) {
	if(static_cast<int>(state.selectedAgency) != state.lastReconciled){
		Config::GetInstance().defaultagency = state.selectedAgency;
		Config::GetInstance().Save();
		state.lastReconciled = state.selectedAgency;
		if(world.IsConnected()){
			world.SetAgency(state.selectedAgency);
		}
	}
}

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	for(const character_panel_detail::AgencyDef & def : character_panel_detail::kAgencies){
		if(action.id == def.actionId){
			state.selectedAgency = def.agency;
			return true;
		}
	}
	return false;
}

void BuildCharacterPanelTree(CharacterPanelState & state,
                             World & world,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	// Refresh stat strings each frame. The legacy panel gated this behind
	// `agencychanged` because Overlay objects retained their text from
	// frame to frame; Clay rebuilds every frame from scratch, so we
	// recompute unconditionally. The cost is five short string
	// concatenations.
	const Uint8 a = state.selectedAgency;
	const char * uname = world.lobby.GetLocalUsername();
	character_panel_detail::g_stats.username = uname ? uname : "";

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

		// Username header — bank 134 / w8 / eff=200. Lands at content y=71.
		CLAY({ .id = CLAY_ID("CharUserWrap") }) {
			BankText(character_panel_detail::FromStd(character_panel_detail::g_stats.username),
			         BankTextVariant::Heading,
			         { .effectColor = 200 });
		}

		// Five agency toggles in a horizontal strip. The toggles have
		// intrinsic sprite sizes; the row uses gap/alignment instead of
		// absolute screen positions.
		CLAY({ .id = CLAY_ID("CharToggleRow"),
		       .layout = {
		           .padding = { 0, 0, 4, 0 },
		           .childGap = character_panel_detail::kToggleGap,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			for(int i = 0; i < 5; ++i){
				const character_panel_detail::AgencyDef & def = character_panel_detail::kAgencies[i];
				const Uint16 spriteW = resources.spritewidth[181][def.spriteIndex];
				const Uint16 spriteH = resources.spriteheight[181][def.spriteIndex];

				Clay_String tglId;
				tglId.isStaticallyAllocated = true;
				tglId.length = static_cast<int32_t>(strlen(def.id));
				tglId.chars  = def.id;

				Toggle(tglId,
				       /*spriteBank=*/  181,
				       /*spriteIndex=*/ def.spriteIndex,
				       /*selected=*/    state.selectedAgency == def.agency,
				       ToggleOpts{ .width  = spriteW > 0 ? spriteW : (Uint16)16,
				                   .height = spriteH > 0 ? spriteH : (Uint16)16,
				                   .effectColor          = 112,
				                   .selectedBrightness   = 128,
				                   .unselectedBrightness = 32 },
				       ToggleHandle{ .hoveredOut = nullptr,
				                     .actionId   = def.actionId,
				                     .interactions = &interactions });

				// Inspector hit rect uses the legacy on-screen coords so
				// label-keyed CLI clicks keep working without depending on
				// the flex-derived bbox being byte-identical. The CLAY
				// inspector dispatch is label-based; the rect is only used
				// when geometric hit-testing is requested.
				const int tx = 20 + i * 42;
				silencer::ui::UiInteractable w;
				w.id = def.actionId;
				w.labelText = character_panel_detail::kAgencyLabels[i];
				w.kind  = silencer::ui::UiInteractableKind::Toggle;
				w.x = tx; w.y = 90;
				w.w = spriteW > 0 ? spriteW : (Uint16)16;
				w.h = spriteH > 0 ? spriteH : (Uint16)16;
				w.selected  = (state.selectedAgency == def.agency);
				interactions.RegisterInteractable(w);
			}
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
					BankText(character_panel_detail::FromStd(*kStatsRows[i].txt),
					         BankTextVariant::BodySm,
					         character_panel_detail::kStatsOpts);
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
