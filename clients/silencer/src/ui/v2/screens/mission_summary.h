#ifndef SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H
#define SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct MissionSummaryHandlers {
	std::function<void()> on_done;
	// slot is 0..5 (endurance/shield/jetpack/techslots/hacking/contacts).
	std::function<void(int slot)> on_upgrade;
};

// Live state injected by Game::RenderMissionSummaryV2 each frame from
// world.lobby.GetUserInfo(accountid). `state == nullptr` keeps the
// build-time preview defaults (stats=0, levels=Noxis-agency-defaults,
// banner+upgrade-buttons hidden) for the byte-identical preview gate.
struct MissionSummaryState {
	// Stats subset rendered by the textbox's visible (post-auto-scroll)
	// window deque[32..59]: 4 grenade types + 4 weapons * (fires/hits/kills,
	// accuracy = hits/fires*100).
	unsigned int shaped = 0;
	unsigned int flare = 0;
	unsigned int poison_flare = 0;
	unsigned int neutron = 0;
	unsigned int weapon_fires[4] = {};
	unsigned int weapon_hits[4] = {};
	unsigned int weapon_kills[4] = {};
	// XP from stats.CalculateExperience().
	int xp = 0;
	// Per-agency upgrade levels: endurance, shield, jetpack, techslots,
	// hacking, contacts. Defaults match Noxis (statsagency=0).
	int levels[6] = {3, 0, 0, 3, 0, 0};
	// Refresh-derived flags: banner is `upgradeavailable`; show_upgrade[i] is
	// `upgrades_available[i] && upgradeavailable`.
	bool show_banner = false;
	bool show_upgrade[6] = {};
};

Node BuildMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers = {},
                         const MissionSummaryState * state = nullptr);

// Engine-side runtime for GameState::MISSIONSUMMARY. Builds the live
// state from world.lobby.GetUserInfo(accountid) each frame.
class MissionSummaryRuntime : public Runtime
{
public:
	MissionSummaryRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y) override;

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;
};

}  // namespace v2
}  // namespace ui

#endif
