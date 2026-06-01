#include "client/ui/hooks/use_mission_summary.h"

#include "lobby.h"
#include "screen_context.h"
#include "stats.h"
#include "user.h"
#include "world.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace silencer {
namespace client_ui {

struct MissionSummaryProviderState {
	World * world = nullptr;
};

MissionSummaryProviderValue MakeMissionSummaryProvider(ScreenContext& ctx) {
	MissionSummaryProviderValue value;
	value.state = std::make_shared<MissionSummaryProviderState>();
	value.state->world = &ctx.world;
	return value;
}

namespace mission_summary_provider_detail {

constexpr int kSummaryW = 180;

constexpr Lobby::StatID kUpgradeStatIds[6] = {
	Lobby::STAT_ENDURANCE,
	Lobby::STAT_SHIELD,
	Lobby::STAT_JETPACK,
	Lobby::STAT_TECHSLOTS,
	Lobby::STAT_HACKING,
	Lobby::STAT_CONTACTS,
};

World * WorldFor(const MissionSummaryProviderValue& provider) {
	return provider.state ? provider.state->world : nullptr;
}

void AddSummaryLine(std::vector<std::string>& lines,
                    const char * name,
                    Uint32 value,
                    bool percentage = false) {
	char string[256];
	char valuetext[64];
	snprintf(valuetext, sizeof(valuetext), "%d%s", value, percentage ? "%" : " ");
	int maxchars = kSummaryW / 6;
	int used = static_cast<int>(std::strlen(name) + std::strlen(valuetext));
	snprintf(string, sizeof(string), "%s", name);
	for(int i = 0; i < maxchars - used; i++){
		strncat(string, " ", sizeof(string) - std::strlen(string) - 1);
	}
	strncat(string, valuetext, sizeof(string) - std::strlen(string) - 1);
	lines.push_back(string);
}

}  // namespace mission_summary_provider_detail

MissionSummaryUpgradesModel::MissionSummaryUpgradesModel(
	const MissionSummaryProviderValue& provider)
	: provider_(provider) {}

void MissionSummaryUpgradesModel::apply(int index) const {
	World * world = mission_summary_provider_detail::WorldFor(provider_);
	if(!world || index < 0 || index >= 6) return;
	User * user = world->lobby.GetUserInfo(world->lobby.accountid);
	if(!user) return;
	world->lobby.UpgradeStat(
		user->selectedcharid,
		user->statsagency,
		mission_summary_provider_detail::kUpgradeStatIds[index]);
}

MissionSummaryModel::MissionSummaryModel(const MissionSummaryProviderValue& provider)
	: upgrades(provider), provider_(provider) {}

bool MissionSummaryModel::needs_refresh(bool info_loaded) const {
	World * world = mission_summary_provider_detail::WorldFor(provider_);
	return world && (world->lobby.statupgraded || !info_loaded);
}

MissionSummarySnapshot MissionSummaryModel::refresh() const {
	MissionSummarySnapshot snapshot;
	World * world = mission_summary_provider_detail::WorldFor(provider_);
	if(!world) return snapshot;
	User * user = world->lobby.GetUserInfo(world->lobby.accountid);
	if(!user || user->retrieving) return snapshot;

	snapshot.loaded = true;
	Stats & stats = user->statscopy;
	snapshot.experience = stats.CalculateExperience();

	auto& lines = snapshot.lines;
	mission_summary_provider_detail::AddSummaryLine(lines, "Kills:", stats.kills);
	mission_summary_provider_detail::AddSummaryLine(lines, "Deaths:", stats.deaths);
	mission_summary_provider_detail::AddSummaryLine(lines, "Suicides", stats.suicides);
	lines.push_back("");
	lines.push_back("Secrets");
	mission_summary_provider_detail::AddSummaryLine(lines, "  Returned:", stats.secretsreturned);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Stolen:", stats.secretsstolen);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Picked up:", stats.secretspickedup);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Fumbled:", stats.secretsdropped);
	lines.push_back("");
	mission_summary_provider_detail::AddSummaryLine(lines, "Civilians killed:", stats.civilianskilled);
	mission_summary_provider_detail::AddSummaryLine(lines, "Guards killed:", stats.guardskilled);
	mission_summary_provider_detail::AddSummaryLine(lines, "Robots killed:", stats.robotskilled);
	mission_summary_provider_detail::AddSummaryLine(lines, "Defenses destroyed:", stats.defensekilled);
	mission_summary_provider_detail::AddSummaryLine(lines, "Fixed Cannons destroyed:", stats.fixedcannonsdestroyed);
	lines.push_back("");
	lines.push_back("Files");
	mission_summary_provider_detail::AddSummaryLine(lines, "  Hacked:", stats.fileshacked);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Returned:", stats.filesreturned);
	lines.push_back("");
	mission_summary_provider_detail::AddSummaryLine(lines, "Powerups picked up:", stats.powerupspickedup);
	mission_summary_provider_detail::AddSummaryLine(lines, "Health packs used:", stats.healthpacksused);
	mission_summary_provider_detail::AddSummaryLine(lines, "Cameras placed:", stats.camerasplanted);
	mission_summary_provider_detail::AddSummaryLine(lines, "Detonators planted:", stats.detsplanted);
	mission_summary_provider_detail::AddSummaryLine(lines, "Fixed Cannons placed:", stats.fixedcannonsplaced);
	mission_summary_provider_detail::AddSummaryLine(lines, "Viruses used:", stats.virusesused);
	mission_summary_provider_detail::AddSummaryLine(lines, "Poisons:", stats.poisons);
	mission_summary_provider_detail::AddSummaryLine(lines, "Lazarus Tracts planted:", stats.tractsplanted);
	lines.push_back("");
	lines.push_back("Grenades thrown");
	mission_summary_provider_detail::AddSummaryLine(lines, "  E.M.P:", stats.empsthrown);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Plasma:", stats.plasmasthrown);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Shaped:", stats.shapedthrown);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Flare:", stats.flaresthrown);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Poison Flare:", stats.poisonflaresthrown);
	mission_summary_provider_detail::AddSummaryLine(lines, "  Neutron:", stats.neutronsthrown);
	for(int i = 0; i < 4; i++){
		lines.push_back("");
		switch(i){
			case 0: lines.push_back("Blaster"); break;
			case 1: lines.push_back("Laser"); break;
			case 2: lines.push_back("Rocket"); break;
			case 3: lines.push_back("Flamer"); break;
		}
		mission_summary_provider_detail::AddSummaryLine(lines, "  Shots fired:", stats.weaponfires[i]);
		mission_summary_provider_detail::AddSummaryLine(lines, "  Hits:", stats.weaponhits[i]);
		Uint32 accuracy = stats.weaponfires[i]
			? (Uint32)((float(stats.weaponhits[i]) / stats.weaponfires[i]) * 100)
			: 0;
		mission_summary_provider_detail::AddSummaryLine(lines, "  Accuracy:", accuracy, true);
		mission_summary_provider_detail::AddSummaryLine(lines, "  Player kills:", stats.playerkillsweapon[i]);
	}

	auto & ag = user->agency[user->statsagency];
	snapshot.levels[0] = ag.endurance;
	snapshot.levels[1] = ag.shield;
	snapshot.levels[2] = ag.jetpack;
	snapshot.levels[3] = ag.techslots;
	snapshot.levels[4] = ag.hacking;
	snapshot.levels[5] = ag.contacts;
	int totalbonusupgrades = ag.endurance + ag.shield + ag.jetpack
		+ ag.techslots + ag.hacking + ag.contacts;
	int maxupgrades = ag.level;
	if(maxupgrades > user->TotalUpgradePointsPossible(user->statsagency)){
		maxupgrades = user->TotalUpgradePointsPossible(user->statsagency);
	}
	snapshot.upgrade_banner = (totalbonusupgrades - ag.defaultbonuses < maxupgrades);
	if(snapshot.upgrade_banner){
		snapshot.upgrades_available[0] = ag.endurance < ag.maxendurance;
		snapshot.upgrades_available[1] = ag.shield < ag.maxshield;
		snapshot.upgrades_available[2] = ag.jetpack < ag.maxjetpack;
		snapshot.upgrades_available[3] = ag.techslots < ag.maxtechslots;
		snapshot.upgrades_available[4] = ag.hacking < ag.maxhacking;
		snapshot.upgrades_available[5] = ag.contacts < ag.maxcontacts;
	}
	world->lobby.statupgraded = false;
	return snapshot;
}

MissionSummaryDestination MissionSummaryModel::finish() const {
	World * world = mission_summary_provider_detail::WorldFor(provider_);
	if(world && world->lobby.state == Lobby::AUTHENTICATED){
		world->lobby.JoinChannel(world->lobby.lastchannel);
		return MissionSummaryDestination::Lobby;
	}
	return MissionSummaryDestination::MainMenu;
}

MissionSummaryModel use_mission_summary(
		const MissionSummaryProviderValue& provider) {
	return MissionSummaryModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
