#include "character_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "clay_inspector.h"
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

namespace {

// Five (id, sprite-index, agency-enum) tuples. The element IDs and sprite
// indices are 1:1 with the legacy CharacterPanel's CHR_TGL_* uids /
// bank-181 indices, so render-time pixel layout is unchanged.
struct AgencyDef {
	const char * id;
	Uint16 spriteIndex;
	Uint8  agency;
};

const AgencyDef kAgencies[5] = {
	{ "CharTglNoxis",     0, Team::NOXIS },
	{ "CharTglLazarus",   1, Team::LAZARUS },
	{ "CharTglCaliber",   2, Team::CALIBER },
	{ "CharTglStatic",    3, Team::STATIC },
	{ "CharTglBlackrose", 4, Team::BLACKROSE },
};

// Inspector labels — one per agency, in the same order as kAgencies.
const char * kAgencyLabels[5] = { "Noxis", "Lazarus", "Caliber", "Static", "Blackrose" };

// Per-frame click adapters: each toggle's onClick gets (state*, agency)
// via a stable per-row record allocated from this fixed-capacity arena.
struct AgencyClickAdapter {
	CharacterPanelState * state;
	Uint8 agency;
};
AgencyClickAdapter g_adapters[5];

void OnAgencyClicked(void * user) {
	auto * a = static_cast<AgencyClickAdapter *>(user);
	if(a && a->state) a->state->selectedAgency = a->agency;
}

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

}  // namespace

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

void BuildCharacterPanelTree(CharacterPanelState & state,
                             World & world,
                             Resources & resources) {
	// Refresh stat strings each frame. The legacy panel gated this behind
	// `agencychanged` because Overlay objects retained their text from
	// frame to frame; Clay rebuilds every frame from scratch, so we
	// recompute unconditionally. The cost is five short string
	// concatenations.
	const Uint8 a = state.selectedAgency;
	const char * uname = world.lobby.GetLocalUsername();
	g_stats.username = uname ? uname : "";

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && !user->retrieving){
		// xptonextlevel is accumulated XP toward current level; threshold
		// is 100*(level+1). Display the remaining amount. Mirrors the
		// legacy formula exactly.
		const int lvl = user->agency[a].level;
		const int remaining = 100 * (lvl + 1) - static_cast<int>(user->agency[a].xptonextlevel);
		g_stats.level  = "LEVEL: "             + std::to_string(lvl);
		g_stats.wins   = "WINS: "              + std::to_string(user->agency[a].wins);
		g_stats.losses = "LOSSES: "            + std::to_string(user->agency[a].losses);
		g_stats.xp     = "XP TO NEXT LEVEL: "  + std::to_string(remaining);
	}else{
		// User info not yet retrieved. Render empty stats — the legacy
		// behavior was to leave Overlay::text empty until the lobby
		// replied, which renders nothing.
		g_stats.level.clear();
		g_stats.wins.clear();
		g_stats.losses.clear();
		g_stats.xp.clear();
	}

	// The parent LobbyCharacterBox is supplied by the lobby shell. This
	// function emits only content, so the layout owns a single scene graph instead
	// of a Box nested inside another Box or root-attached legacy coordinates.
	CLAY({ .id = CLAY_ID("CharacterPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {

		// Username header — bank 134 / w8 / eff=200. Lands at content y=71.
		CLAY({ .id = CLAY_ID("CharUserWrap") }) {
			BankText(FromStd(g_stats.username),
			         BankTextVariant::Heading,
			         { .effectColor = 200 });
		}

		// Five agency toggles in a horizontal strip. The toggles have
		// intrinsic sprite sizes; the row uses gap/alignment instead of
		// absolute screen positions.
		CLAY({ .id = CLAY_ID("CharToggleRow"),
		       .layout = {
		           .padding = { 0, 0, 4, 0 },
		           .childGap = kToggleGap,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			for(int i = 0; i < 5; ++i){
				const AgencyDef & def = kAgencies[i];
				g_adapters[i].state  = &state;
				g_adapters[i].agency = def.agency;

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
				                     .onClick    = &OnAgencyClicked,
				                     .user       = &g_adapters[i] });

				// Inspector hit rect uses the legacy on-screen coords so
				// label-keyed CLI clicks keep working without depending on
				// the flex-derived bbox being byte-identical. The CLAY
				// inspector dispatch is label-based; the rect is only used
				// when geometric hit-testing is requested.
				const int tx = 20 + i * 42;
				silencer::ui::clay_inspector::Widget w;
				w.label = kAgencyLabels[i];
				w.kind  = silencer::ui::clay_inspector::WidgetKind::Toggle;
				w.x = tx; w.y = 90;
				w.w = spriteW > 0 ? spriteW : (Uint16)16;
				w.h = spriteH > 0 ? spriteH : (Uint16)16;
				w.onClick   = &OnAgencyClicked;
				w.clickUser = &g_adapters[i];
				w.selected  = (state.selectedAgency == def.agency);
				silencer::ui::clay_inspector::Register(w);
			}
		}

		// LEVEL / WINS / LOSSES / XP — bank 133 / w7 / eff=129,
		// brightness 160 (128+32), ramp on.
		CLAY({ .id = CLAY_ID("CharStats"),
		       .layout = {
		           .padding = { 0, 0, kStatsPadTop, 0 },
		           .childGap = (uint16_t)kStatsChildGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			const struct { const std::string * txt; const char * id; } kStatsRows[4] = {
				{ &g_stats.level,  "CharStatLvl" },
				{ &g_stats.wins,   "CharStatWin" },
				{ &g_stats.losses, "CharStatLoss" },
				{ &g_stats.xp,     "CharStatXp" },
			};
			for(int i = 0; i < 4; ++i){
				if(kStatsRows[i].txt->empty()) continue;
				Clay_String wrapId;
				wrapId.isStaticallyAllocated = true;
				wrapId.length = static_cast<int32_t>(strlen(kStatsRows[i].id));
				wrapId.chars  = kStatsRows[i].id;
				CLAY({ .id = CLAY_SID(wrapId) }) {
					BankText(FromStd(*kStatsRows[i].txt),
					         BankTextVariant::BodySm,
					         kStatsOpts);
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
