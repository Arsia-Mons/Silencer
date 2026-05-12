#include "clay_character_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
#include "primitives/form_border.h"
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
using silencer::ui::primitives::FormBorder;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleHandle;
using silencer::ui::primitives::ToggleOpts;

namespace silencer::ui::lobby_clay {

namespace {

// Five (id, sprite-index, agency-enum) tuples. The Clay element IDs and
// sprite indices are 1:1 with the legacy CharacterPanel's CHR_TGL_* uids /
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

// Per-frame text buffers. The Clay layout keeps pointers to these for the
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

// LobbyCharacterBox geometry — matches the C2-documented "Top-left status
// panel" rectangle in the baked BG (docs/plans/lobby-chrome-rectangles.md).
// The Box floats @ ROOT at the legacy sprite coord; its children lay out
// via flex (TOP_TO_BOTTOM with per-row padding) so the username / toggles
// / stats positions emerge from layout rather than from absolute @ROOT
// offsets.
constexpr int   kBoxX             = 10;
constexpr int   kBoxY             = 64;
constexpr int   kBoxW             = 218;
constexpr int   kBoxH             = 121;
constexpr Uint8 kBoxStrokeColor   = 216;
// Inside-box layout knobs — derived from the legacy on-screen widget coords:
//   username @ (20, 71), toggle row @ y=90, stat rows @ y=130/143/156/169.
// With box border=1 + padding={left=6, top=6} the content top-left lands
// at (17, 71). The username wrapper adds left=3 → text at (20, 71). The
// toggle row wrapper adds left=3 + top=4 → first toggle at (20, 86+4=90).
// Stats wrapper adds top=24 → first stat at (17, 106+24=130). Bank-133
// height=11 + childGap=2 yields stat rows at 130 / 143 / 156 / 169.
constexpr int kBoxPadLeft       = 6;
constexpr int kBoxPadTop        = 6;
constexpr int kUserPadLeft      = 3;
constexpr int kToggleRowPadLeft = 3;
constexpr int kToggleRowPadTop  = 4;
constexpr int kToggleGap        = 26;  // 42 stride − 16 sprite = 26
constexpr int kStatsPadTop      = 24;
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

	// LobbyCharacterBox — Clay-drawn chrome around the character widgets.
	// Children lay out via TOP_TO_BOTTOM flex (username, toggle row, stats
	// column) inside the box; positions emerge from the per-row padding
	// constants above, not from floating @ ROOT.
	CLAY({ .id = CLAY_ID("LobbyCharacterBox"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kBoxW),
	                       CLAY_SIZING_FIXED(kBoxH) },
	           .padding = { /*left=*/(uint16_t)kBoxPadLeft, /*right=*/0,
	                        /*top=*/(uint16_t)kBoxPadTop,  /*bottom=*/0 },
	           .childGap = 0,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .border = FormBorder(kBoxStrokeColor),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)kBoxX, (float)kBoxY } } }) {

		// Username header — bank 134 / w8 / eff=200. Lands at content y=71.
		CLAY({ .id = CLAY_ID("CharUserWrap"),
		       .layout = { .padding = { (uint16_t)kUserPadLeft, 0, 0, 0 } } }) {
			BankText(FromStd(g_stats.username),
			         BankTextVariant::Heading,
			         { .effectColor = 200 });
		}

		// Five agency toggles in a horizontal strip. With LEFT_TO_RIGHT
		// + childGap=26 and 16-px-wide toggle sprites the on-screen
		// stride is 42 px, matching the legacy `xmargin` exactly.
		CLAY({ .id = CLAY_ID("CharToggleRow"),
		       .layout = {
		           .padding = { (uint16_t)kToggleRowPadLeft, 0,
		                        (uint16_t)kToggleRowPadTop,  0 },
		           .childGap = (uint16_t)kToggleGap,
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
		// brightness 160 (128+32), ramp on. TOP_TO_BOTTOM flex stacks
		// the four rows at y=130 / 143 / 156 / 169 from the wrapper's
		// 24-px top padding + bank-133 height (11) + childGap (2).
		CLAY({ .id = CLAY_ID("CharStats"),
		       .layout = {
		           .padding = { 0, 0, (uint16_t)kStatsPadTop, 0 },
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

}  // namespace silencer::ui::lobby_clay
