#include "mission_summary_screen.h"

#include "client/ui/screens/mission_summary/mission_summary_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "user.h"
#include "stats.h"
#include "renderer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

namespace mission_summary_screen_detail
{
constexpr uint16_t kSummaryW = 180;
constexpr uint16_t kSummaryH = 300;
constexpr uint8_t kLineH = 11;
constexpr const char * kActionDone = "mission_summary.done";
constexpr const char * kActionUpgradePrefix = "mission_summary.upgrade.";

constexpr Lobby::StatID kUpgradeStatIds[6] = {
	Lobby::STAT_ENDURANCE,
	Lobby::STAT_SHIELD,
	Lobby::STAT_JETPACK,
	Lobby::STAT_TECHSLOTS,
	Lobby::STAT_HACKING,
	Lobby::STAT_CONTACTS,
};

bool StartsWith(const std::string & value, const char * prefix)
{
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string & value, const char * prefix)
{
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

void UpgradeStat(World & world, int index)
{
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && index >= 0 && index < 6){
		world.lobby.UpgradeStat(user->selectedcharid, user->statsagency,
		                        kUpgradeStatIds[index]);
	}
}

silencer::client_ui::MissionSummaryDestination FinishMissionSummary(World & world)
{
	if(world.lobby.state == Lobby::AUTHENTICATED){
		world.lobby.JoinChannel(world.lobby.lastchannel);
		return silencer::client_ui::MissionSummaryDestination::Lobby;
	}
	return silencer::client_ui::MissionSummaryDestination::MainMenu;
}

} // namespace mission_summary_screen_detail

void MissionSummaryScreen::Build(ScreenContext & ctx)
{
	ctx.UnloadGame();
	ctx.world.Disconnect();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	infoLoaded = false;
	scrollDelta = 0;
	scrollPosition = 0;
	Refresh(ctx);
}

void MissionSummaryScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	if(scrollDelta != 0){
		int maxScroll = static_cast<int>(summaryLines.size()) - (mission_summary_screen_detail::kSummaryH / mission_summary_screen_detail::kLineH);
		if(maxScroll < 0) maxScroll = 0;
		scrollPosition = std::max(0, std::min(maxScroll, scrollPosition + scrollDelta));
		scrollDelta = 0;
	}
	if(world.lobby.statupgraded || !infoLoaded){
		User * user = world.lobby.GetUserInfo(world.lobby.accountid);
		if(user && !user->retrieving){
			Refresh(ctx);
			world.lobby.statupgraded = false;
		}
	}
}

bool MissionSummaryScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;

	std::array<const char *, silencer::client_ui::kMissionSummaryVisibleLines> lines = {};
	for(int i = 0; i < kVisibleSummaryLineCount; i++){
		const int source = scrollPosition + i;
		visibleSummaryLines[i] =
			(source >= 0 && source < static_cast<int>(summaryLines.size()))
				? summaryLines[source]
				: "";
		lines[i] = visibleSummaryLines[i].c_str();
	}

	xpText = "+ " + std::to_string(experience) + " XP";

	const silencer::client_ui::MissionSummaryContextValue context{
		.state = silencer::client_ui::MissionSummaryState{
			.xp = xpText.c_str(),
			.upgrade_banner = upgradeBanner,
			.summary_lines = lines,
			.levels = levels,
			.upgrades_available = upgradesAvailable,
		},
		.actions = silencer::client_ui::MissionSummaryActions{
			.upgrade = [world = &ctx.world](int index) {
				mission_summary_screen_detail::UpgradeStat(*world, index);
			},
			.done = [world = &ctx.world]() {
				return mission_summary_screen_detail::FinishMissionSummary(*world);
			},
		},
	};
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = ::ui::component(
		"MissionSummaryView",
		silencer::client_ui::MissionSummaryViewProps{
			.key = "mission-summary",
			.value = stored,
		},
		silencer::client_ui::MissionSummaryView);
	return true;
}

void MissionSummaryScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MissionSummaryScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel ||
	   (action.kind == silencer::ui::UiActionKind::Activate && action.id == mission_summary_screen_detail::kActionDone)){
		silencer::client_ui::MissionSummaryDestination destination =
			mission_summary_screen_detail::FinishMissionSummary(ctx.world);
		ctx.GoToState(destination == silencer::client_ui::MissionSummaryDestination::Lobby
		              ? GameState::LOBBY
		              : GameState::MAINMENU);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		scrollDelta += action.amount;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	int upgrade = mission_summary_screen_detail::SuffixInt(action.id, mission_summary_screen_detail::kActionUpgradePrefix);
	if(upgrade >= 0 && upgrade < 6){
		mission_summary_screen_detail::UpgradeStat(ctx.world, upgrade);
		return true;
	}
	return false;
}

void MissionSummaryScreen::Refresh(ScreenContext & ctx)
{
	World & world = ctx.world;
	summaryLines.clear();
	upgradesAvailable.fill(false);
	levels.fill(0);
	upgradeBanner = false;
	experience = 0;

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user || user->retrieving) return;
	infoLoaded = true;
	Stats & stats = user->statscopy;
	experience = stats.CalculateExperience();

	AddSummaryLine("Kills:", stats.kills);
	AddSummaryLine("Deaths:", stats.deaths);
	AddSummaryLine("Suicides", stats.suicides);
	summaryLines.push_back("");
	summaryLines.push_back("Secrets");
	AddSummaryLine("  Returned:", stats.secretsreturned);
	AddSummaryLine("  Stolen:", stats.secretsstolen);
	AddSummaryLine("  Picked up:", stats.secretspickedup);
	AddSummaryLine("  Fumbled:", stats.secretsdropped);
	summaryLines.push_back("");
	AddSummaryLine("Civilians killed:", stats.civilianskilled);
	AddSummaryLine("Guards killed:", stats.guardskilled);
	AddSummaryLine("Robots killed:", stats.robotskilled);
	AddSummaryLine("Defenses destroyed:", stats.defensekilled);
	AddSummaryLine("Fixed Cannons destroyed:", stats.fixedcannonsdestroyed);
	summaryLines.push_back("");
	summaryLines.push_back("Files");
	AddSummaryLine("  Hacked:", stats.fileshacked);
	AddSummaryLine("  Returned:", stats.filesreturned);
	summaryLines.push_back("");
	AddSummaryLine("Powerups picked up:", stats.powerupspickedup);
	AddSummaryLine("Health packs used:", stats.healthpacksused);
	AddSummaryLine("Cameras placed:", stats.camerasplanted);
	AddSummaryLine("Detonators planted:", stats.detsplanted);
	AddSummaryLine("Fixed Cannons placed:", stats.fixedcannonsplaced);
	AddSummaryLine("Viruses used:", stats.virusesused);
	AddSummaryLine("Poisons:", stats.poisons);
	AddSummaryLine("Lazarus Tracts planted:", stats.tractsplanted);
	summaryLines.push_back("");
	summaryLines.push_back("Grenades thrown");
	AddSummaryLine("  E.M.P:", stats.empsthrown);
	AddSummaryLine("  Plasma:", stats.plasmasthrown);
	AddSummaryLine("  Shaped:", stats.shapedthrown);
	AddSummaryLine("  Flare:", stats.flaresthrown);
	AddSummaryLine("  Poison Flare:", stats.poisonflaresthrown);
	AddSummaryLine("  Neutron:", stats.neutronsthrown);
	for(int i = 0; i < 4; i++){
		summaryLines.push_back("");
		switch(i){
			case 0: summaryLines.push_back("Blaster"); break;
			case 1: summaryLines.push_back("Laser"); break;
			case 2: summaryLines.push_back("Rocket"); break;
			case 3: summaryLines.push_back("Flamer"); break;
		}
		AddSummaryLine("  Shots fired:", stats.weaponfires[i]);
		AddSummaryLine("  Hits:", stats.weaponhits[i]);
		Uint32 accuracy = stats.weaponfires[i]
			? (Uint32)((float(stats.weaponhits[i]) / stats.weaponfires[i]) * 100)
			: 0;
		AddSummaryLine("  Accuracy:", accuracy, true);
		AddSummaryLine("  Player kills:", stats.playerkillsweapon[i]);
	}

	auto & ag = user->agency[user->statsagency];
	levels[0] = ag.endurance;
	levels[1] = ag.shield;
	levels[2] = ag.jetpack;
	levels[3] = ag.techslots;
	levels[4] = ag.hacking;
	levels[5] = ag.contacts;
	int totalbonusupgrades = ag.endurance + ag.shield + ag.jetpack
		+ ag.techslots + ag.hacking + ag.contacts;
	int maxupgrades = ag.level;
	if(maxupgrades > user->TotalUpgradePointsPossible(user->statsagency)){
		maxupgrades = user->TotalUpgradePointsPossible(user->statsagency);
	}
	upgradeBanner = (totalbonusupgrades - ag.defaultbonuses < maxupgrades);
	if(upgradeBanner){
		upgradesAvailable[0] = ag.endurance < ag.maxendurance;
		upgradesAvailable[1] = ag.shield < ag.maxshield;
		upgradesAvailable[2] = ag.jetpack < ag.maxjetpack;
		upgradesAvailable[3] = ag.techslots < ag.maxtechslots;
		upgradesAvailable[4] = ag.hacking < ag.maxhacking;
		upgradesAvailable[5] = ag.contacts < ag.maxcontacts;
	}
	int maxScroll = static_cast<int>(summaryLines.size()) - (mission_summary_screen_detail::kSummaryH / mission_summary_screen_detail::kLineH);
	if(maxScroll < 0) maxScroll = 0;
	scrollPosition = std::max(0, std::min(maxScroll, scrollPosition));
}

void MissionSummaryScreen::AddSummaryLine(const char * name, Uint32 value, bool percentage)
{
	char string[256];
	char valuetext[64];
	snprintf(valuetext, sizeof(valuetext), "%d%s", value, percentage ? "%" : " ");
	int maxchars = mission_summary_screen_detail::kSummaryW / 6;
	int used = static_cast<int>(std::strlen(name) + std::strlen(valuetext));
	snprintf(string, sizeof(string), "%s", name);
	for(int i = 0; i < maxchars - used; i++) strncat(string, " ", sizeof(string) - std::strlen(string) - 1);
	strncat(string, valuetext, sizeof(string) - std::strlen(string) - 1);
	summaryLines.push_back(string);
}
