#include "clay_character_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
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

void EmitFloating(int x, int y, Clay_String id, void (*body)(void *), void * user) {
	(void)id;
	(void)body;
	(void)user;
	// Helper not used — kept here as a comment of intent. Actual emission
	// happens inline below to keep the CLAY macros + closures readable.
	(void)x; (void)y;
}

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

	// Username header — bank 134 / w8 / eff=200 at (20, 71). Heading
	// variant matches the (bank 134, w8) tuple.
	CLAY({ .id = CLAY_ID("CharUserWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { 20, 71 } } }) {
		BankText(FromStd(g_stats.username),
		         BankTextVariant::Heading,
		         { .effectColor = 200 });
	}

	// Five agency toggles in a horizontal strip, base x = 20, dy = 0,
	// xmargin = 42, baseline y = 90. Each toggle's offset compensates
	// for the sprite anchor (spriteoffsetx/y[181][i]) so the rendered
	// pixels land at the legacy Overlay/Sprite path's coordinates.
	const int xmargin = 42;
	const int baseY   = 90;
	for(int i = 0; i < 5; ++i){
		const AgencyDef & def = kAgencies[i];
		const int tx = 20 + i * xmargin;
		const int ox = tx     - resources.spriteoffsetx[181][def.spriteIndex];
		const int oy = baseY  - resources.spriteoffsety[181][def.spriteIndex];

		g_adapters[i].state  = &state;
		g_adapters[i].agency = def.agency;

		const Uint16 spriteW = resources.spritewidth[181][def.spriteIndex];
		const Uint16 spriteH = resources.spriteheight[181][def.spriteIndex];

		Clay_String wrapId;
		wrapId.isStaticallyAllocated = true;
		wrapId.length = static_cast<int32_t>(strlen(def.id));
		wrapId.chars  = def.id;
		// Suffix the inner toggle id so the wrap and the toggle's own
		// CLAY block have distinct hashes. (CLAY_STRING_ARG would copy
		// — we want pointer-stable static literals here.)
		static const char kInnerSfx[5][8] = { "Tgl0", "Tgl1", "Tgl2", "Tgl3", "Tgl4" };
		Clay_String innerId;
		innerId.isStaticallyAllocated = true;
		innerId.length = 4;
		innerId.chars  = kInnerSfx[i];

		CLAY({ .id = CLAY_SID(wrapId),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)ox, (float)oy } } }) {
			Toggle(innerId,
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
		}
	}

	// LEVEL / WINS / LOSSES / XP — bank 133 / w7 / eff=129, brightness
	// 160 (128+32), ramp on. Positions (17, 130) / (17, 143) / (17, 156)
	// / (17, 169) match the legacy Overlay coordinates one-for-one.
	const struct { int y; const std::string * txt; const char * id; } kStatsRows[4] = {
		{ 130, &g_stats.level,  "CharStatLvl" },
		{ 143, &g_stats.wins,   "CharStatWin" },
		{ 156, &g_stats.losses, "CharStatLoss" },
		{ 169, &g_stats.xp,     "CharStatXp" },
	};
	for(int i = 0; i < 4; ++i){
		if(kStatsRows[i].txt->empty()) continue;
		Clay_String wrapId;
		wrapId.isStaticallyAllocated = true;
		wrapId.length = static_cast<int32_t>(strlen(kStatsRows[i].id));
		wrapId.chars  = kStatsRows[i].id;
		CLAY({ .id = CLAY_SID(wrapId),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { 17, (float)kStatsRows[i].y } } }) {
			BankText(FromStd(*kStatsRows[i].txt),
			         BankTextVariant::BodySm,
			         kStatsOpts);
		}
	}
}

}  // namespace silencer::ui::lobby_clay
