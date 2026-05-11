#include "mission_summary.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

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
#include <cstring>
#include <functional>
#include <string>

namespace ui {
namespace v2 {

namespace {

constexpr size_t kFrameStringBytes = 4096;
thread_local char   g_frame_strings[kFrameStringBytes];
thread_local size_t g_frame_off = 0;

Clay_String FrameStr(const char * s) {
	size_t n = std::strlen(s);
	if(g_frame_off + n + 1 > kFrameStringBytes) g_frame_off = 0;
	char * p = &g_frame_strings[g_frame_off];
	std::memcpy(p, s, n);
	p[n] = '\0';
	g_frame_off += n + 1;
	Clay_String out{};
	out.length = (int32_t)n;
	out.chars  = p;
	return out;
}

void OnButtonClick(Clay_ElementId, Clay_PointerData p, intptr_t user) {
	if(p.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * h = reinterpret_cast<const std::function<void()> *>(user);
	if(h && *h) (*h)();
}

// Per-slot upgrade closures. Same pattern as options_controls — thread_local
// addresses outlive the DispatchMouseDown stack frame that fires SetPointerState
// after Render returns. Rebound every frame at the top of RenderMissionSummary.
thread_local std::function<void()> g_upgrade_handlers[6];

void Button(const char * key, const char * label, const std::function<void()> & handler,
            float width, float height) {
	Clay_String key_s   = FrameStr(key);
	Clay_String label_s = FrameStr(label);
	CLAY({
		.id = Clay_GetElementId(key_s),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height) },
			.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = Clay_Hovered() ? ::ui::kColorSelectHi : ::ui::kColorScrollbarFill,
		.cornerRadius    = ::ui::kCornerSmall,
	}) {
		Clay_OnHover(OnButtonClick, (intptr_t)&handler);
		CLAY_TEXT(label_s, CLAY_TEXT_CONFIG(::ui::kFontHeading));
	}
}

unsigned int Accuracy(unsigned int fires, unsigned int hits)
{
	if(!fires) return 0;
	return (unsigned int)((float(hits) / fires) * 100);
}

void StatLine(const char * name, unsigned int value, bool percentage = false) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%s %u%s", name, value, percentage ? "%" : "");
	CLAY({ .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() } } }) {
		CLAY_TEXT(FrameStr(buf), CLAY_TEXT_CONFIG(::ui::kFontBody));
	}
}

void PlainLine(const char * s, const Clay_TextElementConfig & font) {
	CLAY({ .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() } } }) {
		CLAY_TEXT(FrameStr(s), CLAY_TEXT_CONFIG(font));
	}
}

}  // namespace

void RenderMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers,
                          const MissionSummaryState & state) {
	(void)ctx;
	g_frame_off = 0;

	// Bind upgrade closures so OnHover userData has stable addresses.
	for(int i = 0; i < 6; i++){
		auto cb = handlers.on_upgrade;
		int slot = i;
		g_upgrade_handlers[i] = [cb, slot](){ if(cb) cb(slot); };
	}

	CLAY({
		.id = CLAY_ID("MissionSummaryRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.padding         = CLAY_PADDING_ALL(12),
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		PlainLine("Mission Summary", ::ui::kFontTitle);

		{
			char xpbuf[32];
			std::snprintf(xpbuf, sizeof(xpbuf), "+ %d XP", state.xp);
			PlainLine(xpbuf, ::ui::kFontStat);
		}

		if(state.show_banner){
			PlainLine("*NEW UPGRADE AVAILABLE*", ::ui::kFontBody);
		}

		// Body section: textbox on the left, levels + upgrade chips on the right.
		CLAY({
			.id = CLAY_ID("MissionSummaryBody"),
			.layout = {
				.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() },
				.childGap = 24,
				.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		}) {
			// Stats textbox column.
			CLAY({
				.id = CLAY_ID("MissionSummaryStats"),
				.layout = {
					.sizing = { CLAY_SIZING_FIXED(280), CLAY_SIZING_FIT() },
					.childGap = 2,
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
			}) {
				StatLine("Shaped:",       state.shaped);
				StatLine("Flare:",        state.flare);
				StatLine("Poison Flare:", state.poison_flare);
				StatLine("Neutron:",      state.neutron);

				static const char * weapon_names[4] = {"Blaster", "Laser", "Rocket", "Flamer"};
				for(int w = 0; w < 4; w++){
					PlainLine(weapon_names[w], ::ui::kFontBody);
					StatLine("  Shots fired:", state.weapon_fires[w]);
					StatLine("  Hits:",        state.weapon_hits[w]);
					StatLine("  Accuracy:",    Accuracy(state.weapon_fires[w], state.weapon_hits[w]), true);
					StatLine("  Player kills:", state.weapon_kills[w]);
				}
			}

			// Levels column: 6 rows, each "Foo: N" plus an optional Upgrade button.
			CLAY({
				.id = CLAY_ID("MissionSummaryLevels"),
				.layout = {
					.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() },
					.childGap = 6,
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
			}) {
				static const char * level_labels[6] = {
					"Endurance",
					"Shield",
					"Jetpack",
					"Tech Slot",
					"Hacking",
					"Contacts",
				};
				static const char * upgrade_labels[6] = {
					"+1 Endurance",
					"+1 Shield",
					"+1 Jetpack",
					"+1 Tech Slot",
					"+1 Hacking",
					"+1 Contacts",
				};
				static const char * upgrade_keys[6] = {
					"ms_up:0", "ms_up:1", "ms_up:2", "ms_up:3", "ms_up:4", "ms_up:5",
				};
				for(int i = 0; i < 6; i++){
					char row_key[24];
					std::snprintf(row_key, sizeof(row_key), "ms_lvl_%d", i);
					CLAY({
						.id = Clay_GetElementId(FrameStr(row_key)),
						.layout = {
							.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIXED(33) },
							.childGap = 8,
							.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
							.layoutDirection = CLAY_LEFT_TO_RIGHT,
						},
					}) {
						char rowbuf[64];
						std::snprintf(rowbuf, sizeof(rowbuf), "%s Level: %d", level_labels[i], state.levels[i]);
						CLAY({
							.layout = {
								.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(33) },
								.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
							},
						}) {
							CLAY_TEXT(FrameStr(rowbuf), CLAY_TEXT_CONFIG(::ui::kFontBody));
						}
						if(state.show_upgrade[i]){
							Button(upgrade_keys[i], upgrade_labels[i], g_upgrade_handlers[i], 160, 33);
						}
					}
				}
			}
		}

		// Done button.
		static const std::function<void()> noop;
		Button("ms_done", "Done", handlers.on_done ? handlers.on_done : noop, 196, 33);
	}
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

// Mirrors MissionSummaryScreen::Refresh — builds live state from
// world.lobby.GetUserInfo(). Returns default-constructed state when user
// info hasn't arrived yet (matches legacy's pre-Refresh appearance:
// build-time defaults, no banner, no upgrade buttons).
MissionSummaryState CurrentMissionSummary(World & world){
	MissionSummaryState out;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return out;
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
	if(user->retrieving) return out;
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
	// Mirror legacy MissionSummaryScreen::Tick — clears the upgrade-ack flag.
	world.lobby.statupgraded = false;
	return out;
}

}  // namespace

MissionSummaryRuntime::MissionSummaryRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void MissionSummaryRuntime::Render(Surface & target, ::Renderer & renderer,
                                    int mouse_x, int mouse_y, float dt,
                              int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.dt      = dt;

	target.Clear(0);

	EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(sctx_);
	MissionSummaryState live = CurrentMissionSummary(world_);
	RenderMissionSummary(ctx, handlers, live);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
}

bool MissionSummaryRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
                                int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;

	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	MissionSummaryHandlers handlers = BuildMissionSummaryHandlers(sctx_);
	MissionSummaryState live = CurrentMissionSummary(world_);
	RenderMissionSummary(ctx, handlers, live);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

}  // namespace v2
}  // namespace ui
