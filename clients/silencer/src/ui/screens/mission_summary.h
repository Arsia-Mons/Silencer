#ifndef SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H
#define SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H

#include "runtime.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

struct MissionSummaryHandlers {
	std::function<void()> on_done;
	// slot is 0..5 (endurance/shield/jetpack/techslots/hacking/contacts).
	std::function<void(int slot)> on_upgrade;
};

struct MissionSummaryState {
	// Stats subset rendered by the textbox: 4 grenade types + 4 weapons *
	// (fires/hits/kills, accuracy = hits/fires*100).
	unsigned int shaped = 0;
	unsigned int flare = 0;
	unsigned int poison_flare = 0;
	unsigned int neutron = 0;
	unsigned int weapon_fires[4] = {};
	unsigned int weapon_hits[4] = {};
	unsigned int weapon_kills[4] = {};
	int xp = 0;
	// Per-agency upgrade levels: endurance, shield, jetpack, techslots,
	// hacking, contacts. Defaults match Noxis (statsagency=0).
	int levels[6] = {3, 0, 0, 3, 0, 0};
	bool show_banner = false;
	bool show_upgrade[6] = {};
};

void RenderMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers,
                          const MissionSummaryState & state);

class MissionSummaryRuntime : public Runtime
{
public:
	MissionSummaryRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;

private:
	World &         world_;
	ScreenContext & sctx_;
};

}  // namespace v2
}  // namespace ui

#endif
