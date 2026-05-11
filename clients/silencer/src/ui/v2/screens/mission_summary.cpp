#include "mission_summary.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "game.h"
#include "game_state.h"
#include "lobby.h"
#include "screen_context.h"
#include "stats.h"
#include "user.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <cstdio>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

namespace {

// Mirrors MissionSummaryScreen::AddSummaryLine. Pads `name` with spaces so
// the result is exactly maxchars (180 / fontwidth=6 = 30) chars, with the
// value text right-justified at the end.
std::string SummaryLine(const char * name, unsigned int value, bool percentage = false)
{
	char valuetext[16];
	snprintf(valuetext, sizeof(valuetext), "%u%s", value, percentage ? "%" : " ");
	const int maxchars = 30;
	int used = (int)std::char_traits<char>::length(name) + (int)std::char_traits<char>::length(valuetext);
	std::string s = name;
	for(int i = 0; i < maxchars - used; i++) s += " ";
	s += valuetext;
	return s;
}

unsigned int Accuracy(unsigned int fires, unsigned int hits)
{
	if(!fires) return 0;
	return (unsigned int)((float(hits) / fires) * 100);
}

}  // namespace

Node BuildMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers,
                         const MissionSummaryState * state)
{
	(void)ctx;
	// Mirrors MissionSummaryScreen::Build. At preview-gate state (post-Build,
	// after Refresh(), pre-Tick) the rendered pixels are:
	//   - Two background sprites (bank=6 idx=0, bank=7 idx=5)
	//   - "Mission Summary" title (bank=135, width=12) at (102, 44).
	//   - "+ N XP" XP text (bank=136, width=15) centered on x=467, y=45.
	//   - Textbox lines. The legacy textbox auto-scrolls in AddLine: after
	//     all 60 AddLine calls, `scrolled = text.size()-27` captured before
	//     the final push_back = 59-27 = 32. The visible window therefore
	//     starts at deque[32] ("  Shaped:") and renders 28 lines through
	//     deque[59] ("  Player kills:") at y = 92 + i*11.
	//   - 6 "Current X Level:" left labels at (390, 97+i*46) bank=133 width=6
	//   - 6 level values at x = 556 - text.length()*6, same y.
	//   - "*NEW UPGRADE AVAILABLE*" banner at (467 - 23*6/2, 77) bank=133
	//     width=6 when state->show_banner.
	//   - Upgrade B196x33 buttons at (62, -180 + i*46) when state->show_upgrade[i].
	//   - "Done" B196x33 button at anchor (62, 100).
	const bool live = (state != nullptr);
	const unsigned int s_shaped       = live ? state->shaped       : 0;
	const unsigned int s_flare        = live ? state->flare        : 0;
	const unsigned int s_poison_flare = live ? state->poison_flare : 0;
	const unsigned int s_neutron      = live ? state->neutron      : 0;
	const unsigned int * s_fires = live ? state->weapon_fires : nullptr;
	const unsigned int * s_hits  = live ? state->weapon_hits  : nullptr;
	const unsigned int * s_kills = live ? state->weapon_kills : nullptr;
	const int xp = live ? state->xp : 0;
	static const int default_levels[6] = {3, 0, 0, 3, 0, 0};
	const int * levels = live ? state->levels : default_levels;
	const bool show_banner = live ? state->show_banner : false;

	std::vector<Node> children;

	// Title: "Mission Summary" (15 chars * 12 width = 180; legacy centers
	// around x=192 → x = 192 - 90 = 102, y = 44).
	children.push_back(Label("Mission Summary", /*font_bank=*/135, /*font_width=*/12).at(102, 44));

	// XP text: "+ N XP" — text width 15, centered on x=467.
	{
		char xpbuf[24];
		snprintf(xpbuf, sizeof(xpbuf), "+ %d XP", xp);
		int len = (int)std::char_traits<char>::length(xpbuf);
		int xp_x = 467 - ((len * 15) / 2);
		children.push_back(Label(xpbuf, /*font_bank=*/136, /*font_width=*/15).at(xp_x, 45));
	}

	// "*NEW UPGRADE AVAILABLE*" banner — 23 chars * 6 width = 138, centered on x=467.
	if(show_banner){
		const char * banner = "*NEW UPGRADE AVAILABLE*";
		int len = (int)std::char_traits<char>::length(banner);
		int b_x = 467 - ((len * 6) / 2);
		children.push_back(Label(banner, /*font_bank=*/133, /*font_width=*/6).at(b_x, 77));
	}

	// Textbox visible window. Each entry is the text rendered at line `i`
	// (y = 92 + i*11). Lines with no glyphs (blanks, deque[36]/[42]/[48]/
	// [54]) emit a Label with empty text — DrawText short-circuits on
	// strlen=0, so we skip those.
	auto tb_label = [](const std::string & s, int line) {
		return Label(s, /*font_bank=*/133, /*font_width=*/6).at(89, 92 + line * 11);
	};
	int line = 0;
	children.push_back(tb_label(SummaryLine("  Shaped:", s_shaped), line++));              // deque[32]
	children.push_back(tb_label(SummaryLine("  Flare:", s_flare), line++));                // [33]
	children.push_back(tb_label(SummaryLine("  Poison Flare:", s_poison_flare), line++));  // [34]
	children.push_back(tb_label(SummaryLine("  Neutron:", s_neutron), line++));            // [35]
	line++;                                                                                 // [36] blank
	static const char * weapon_names[4] = {"Blaster", "Laser", "Rocket", "Flamer"};
	for(int w = 0; w < 4; w++){
		const unsigned int fires = s_fires ? s_fires[w] : 0;
		const unsigned int hits  = s_hits  ? s_hits[w]  : 0;
		const unsigned int kills = s_kills ? s_kills[w] : 0;
		children.push_back(tb_label(weapon_names[w], line++));                              // name
		children.push_back(tb_label(SummaryLine("  Shots fired:", fires), line++));
		children.push_back(tb_label(SummaryLine("  Hits:", hits), line++));
		children.push_back(tb_label(SummaryLine("  Accuracy:", Accuracy(fires, hits), true), line++));
		children.push_back(tb_label(SummaryLine("  Player kills:", kills), line++));
		if(w < 3) line++;                                                                   // blank between
	}

	// 6 "Current X Level:" left labels + level values. Level text x =
	// 556 - text.length()*6.
	static const char * level_labels[6] = {
		"Current Endurance Level:",
		"Current Shield Level:",
		"Current Jetpack Level:",
		"Current Tech Slot Level:",
		"Current Hacking Level:",
		"Current Contacts Level:",
	};
	for(int i = 0; i < 6; i++){
		int y = 97 + i * 46;
		children.push_back(Label(level_labels[i], /*font_bank=*/133, /*font_width=*/6).at(390, y));
		char numbuf[16];
		snprintf(numbuf, sizeof(numbuf), "%d", levels[i]);
		int len = (int)std::char_traits<char>::length(numbuf);
		int lvl_x = 556 - len * 6;
		children.push_back(Label(numbuf, /*font_bank=*/133, /*font_width=*/6).at(lvl_x, y));
	}

	// Upgrade buttons (uid 10..15 in legacy). B196x33 at (62, -180 + i*46).
	// Text strings padded to 12 chars for legacy chrome-centering parity.
	if(live){
		static const char * upgrade_texts[6] = {
			"+1 Endurance",
			"+1 Shield   ",
			"+1 Jetpack  ",
			"+1 Tech Slot",
			"+1 Hacking  ",
			"+1 Contacts ",
		};
		for(int i = 0; i < 6; i++){
			if(!state->show_upgrade[i]) continue;
			std::function<void()> on_click;
			if(handlers.on_upgrade){
				auto cb = handlers.on_upgrade;
				on_click = [cb, i](){ cb(i); };
			}
			children.push_back(
				Button(upgrade_texts[i], ButtonType::B196x33)
					.at(62, -180 + i * 46)
					.withKey(std::string("ms_up:") + std::to_string(i))
					.onClick(std::move(on_click))
			);
		}
	}

	// "Done" button — B196x33 at anchor (62, 100). uid=0; live engine wiring
	// maps this to GoToState(LOBBY) or MAINMENU depending on lobby state.
	children.push_back(
		Button("Done", ButtonType::B196x33).at(62, 100).onClick(handlers.on_done)
	);

	return Background(/*bank=*/6, /*index=*/0, {
		Sprite(/*bank=*/7, /*index=*/5),
		Group(std::move(children)),
	});
}

// -----------------------------------------------------------------------------
// MissionSummaryRuntime — engine wire-in for GameState::MISSIONSUMMARY.
// -----------------------------------------------------------------------------

namespace {

MissionSummaryHandlers BuildMissionSummaryHandlers(ScreenContext & sctx){
	MissionSummaryHandlers h;
	h.on_done = [&sctx](){
		World & w = sctx.world;
		if(w.lobby.state == Lobby::AUTHENTICATED){
			sctx.GoToState(GameState::LOBBY);
			w.lobby.JoinChannel(w.lobby.lastchannel);
		}else{
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	h.on_upgrade = [&sctx](int slot){
		World & w = sctx.world;
		User * user = w.lobby.GetUserInfo(w.lobby.accountid);
		if(!user) return;
		w.lobby.UpgradeStat(user->statsagency, (Uint8)slot);
	};
	return h;
}

// Mirrors MissionSummaryScreen::Refresh — builds the live state from
// world.lobby.GetUserInfo(). Returns false when user info hasn't arrived
// yet; caller should fall back to a null State (matches legacy's
// pre-Refresh appearance: build-time defaults, no banner, no upgrade
// buttons). The legacy build path also clears statupgraded after each
// successful Refresh; we do that here.
bool CurrentMissionSummary(World & world, MissionSummaryState & out){
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return false;
	Stats & st = user->statscopy;
	out.shaped       = st.shapedthrown;
	out.flare        = st.flaresthrown;
	out.poison_flare = st.poisonflaresthrown;
	out.neutron      = st.neutronsthrown;
	for(int i = 0; i < 4; i++){
		out.weapon_fires[i] = st.weaponfires[i];
		out.weapon_hits[i]  = st.weaponhits[i];
		out.weapon_kills[i] = st.playerkillsweapon[i];
	}
	out.xp = (int)st.CalculateExperience();
	auto & ag = user->agency[user->statsagency];
	out.levels[0] = ag.endurance;
	out.levels[1] = ag.shield;
	out.levels[2] = ag.jetpack;
	out.levels[3] = ag.techslots;
	out.levels[4] = ag.hacking;
	out.levels[5] = ag.contacts;
	if(user->retrieving){
		out.show_banner = false;
		for(int i = 0; i < 6; i++) out.show_upgrade[i] = false;
		return true;
	}
	int totalbonusupgrades = ag.endurance + ag.shield + ag.jetpack + ag.techslots + ag.hacking + ag.contacts;
	bool slot_room[6] = {
		ag.endurance < ag.maxendurance,
		ag.shield    < ag.maxshield,
		ag.jetpack   < ag.maxjetpack,
		ag.techslots < ag.maxtechslots,
		ag.hacking   < ag.maxhacking,
		ag.contacts  < ag.maxcontacts,
	};
	int maxupgrades = ag.level;
	int cap = user->TotalUpgradePointsPossible(user->statsagency);
	if(maxupgrades > cap) maxupgrades = cap;
	bool upgradeavailable = (totalbonusupgrades - ag.defaultbonuses) < maxupgrades;
	out.show_banner = upgradeavailable;
	for(int i = 0; i < 6; i++) out.show_upgrade[i] = upgradeavailable && slot_room[i];
	// Consume the upgrade-ack flag — legacy MissionSummaryScreen::Tick
	// cleared this after each Refresh; no other consumer reads it.
	world.lobby.statupgraded = false;
	return true;
}

}  // namespace

MissionSummaryRuntime::MissionSummaryRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void MissionSummaryRuntime::Render(Surface & target, ::Renderer & renderer,
                                    int mouse_x, int mouse_y, float dt){
	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(sctx_);
	MissionSummaryState live;
	bool have_state = CurrentMissionSummary(world_, live);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildMissionSummary(ctx, handlers, have_state ? &live : nullptr);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool MissionSummaryRuntime::DispatchMouseDown(int mouse_x, int mouse_y){
	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(sctx_);
	MissionSummaryState live;
	bool have_state = CurrentMissionSummary(world_, live);
	Node tree = BuildMissionSummary(ctx, handlers, have_state ? &live : nullptr);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

}  // namespace v2
}  // namespace ui
