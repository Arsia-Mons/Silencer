#include "mission_summary_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "user.h"
#include "stats.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/bank_text.h"
#include "primitives/scroll_text_box.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace mission_summary_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxBeginFrame;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOrigin;

constexpr uint16_t kRootPadX = 52;
constexpr uint16_t kRootPadY = 42;
constexpr uint16_t kLeftW = 242;
constexpr uint16_t kRightW = 258;
constexpr uint16_t kSummaryW = 180;
constexpr uint16_t kSummaryH = 300;
constexpr uint8_t kLineH = 11;
constexpr int kMaxSummaryLines = 256;
constexpr const char * kActionDone = "mission_summary.done";
constexpr const char * kActionScrollUp = "mission_summary.scroll_up";
constexpr const char * kActionScrollDown = "mission_summary.scroll_down";
constexpr const char * kActionUpgradePrefix = "mission_summary.upgrade.";
ScrollTextBoxLine g_summarySlab[kMaxSummaryLines];

const char * kUpgradeLabels[6] = {
	"+1 Endurance",
	"+1 Shield",
	"+1 Jetpack",
	"+1 Tech Slot",
	"+1 Hacking",
	"+1 Contacts",
};

const char * kLevelLabels[6] = {
	"Current Endurance Level:",
	"Current Shield Level:",
	"Current Jetpack Level:",
	"Current Tech Slot Level:",
	"Current Hacking Level:",
	"Current Contacts Level:",
};

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

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

int FillSummarySlab(const std::vector<std::string> & lines)
{
	int count = static_cast<int>(lines.size());
	if(count > kMaxSummaryLines) count = kMaxSummaryLines;
	for(int i = 0; i < count; i++){
		g_summarySlab[i].text = FromStd(lines[i]);
		g_summarySlab[i].effectColor = 0;
		g_summarySlab[i].brightness = 128;
		g_summarySlab[i].indent = 0;
	}
	return count;
}
} // namespace mission_summary_screen_detail

void MissionSummaryScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	infoLoaded = false;
	doneClicked = false;
	upgradeClicked = -1;
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
	if(upgradeClicked >= 0){
		int idx = upgradeClicked;
		upgradeClicked = -1;
		User * user = world.lobby.GetUserInfo(world.lobby.accountid);
		if(user && idx >= 0 && idx < 6){
			world.lobby.UpgradeStat(user->statsagency, idx);
		}
	}
	if(doneClicked){
		doneClicked = false;
		if(world.lobby.state == Lobby::AUTHENTICATED){
			ctx.GoToState(GameState::LOBBY);
			world.lobby.JoinChannel(world.lobby.lastchannel);
		}else{
			ctx.GoToState(GameState::MAINMENU);
		}
	}
}

void MissionSummaryScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	int lineCount = mission_summary_screen_detail::FillSummarySlab(summaryLines);
	std::string xp = "+ " + std::to_string(experience) + " XP";

	CLAY({ .id = CLAY_ID("MissionSummaryRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { mission_summary_screen_detail::kRootPadX, mission_summary_screen_detail::kRootPadX, mission_summary_screen_detail::kRootPadY, mission_summary_screen_detail::kRootPadY },
	           .childGap = 42,
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("MissionSummaryPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(mission_summary_screen_detail::kLeftW),
		                       CLAY_SIZING_FIXED(390) },
		           .padding = { 26, 20, 20, 20 },
		           .childGap = 12,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 5) } }) {
			mission_summary_screen_detail::BankText(CLAY_STRING("Mission Summary"), mission_summary_screen_detail::BankTextVariant::Title, {});
			CLAY({ .id = CLAY_ID("MissionSummaryStatsRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = 10,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				mission_summary_screen_detail::ScrollTextBox(CLAY_STRING("MissionSummaryStats"),
				              mission_summary_screen_detail::g_summarySlab, lineCount,
				              static_cast<Uint16>(scrollPosition),
				              { .width = mission_summary_screen_detail::kSummaryW,
				                .height = mission_summary_screen_detail::kSummaryH,
				                .lineHeight = mission_summary_screen_detail::kLineH,
				                .textVariant = mission_summary_screen_detail::BankTextVariant::Body,
				                .origin = mission_summary_screen_detail::ScrollTextBoxOrigin::TopDown });
				CLAY({ .id = CLAY_ID("MissionSummaryScrollControls"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(34), CLAY_SIZING_FIXED(mission_summary_screen_detail::kSummaryH) },
				           .childGap = 20,
				           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_TOP_TO_BOTTOM,
				       } }) {
					mission_summary_screen_detail::Button(CLAY_STRING("MissionSummaryScrollUpButton"), CLAY_STRING("Up"),
					           mission_summary_screen_detail::ButtonOpts{ .variant = mission_summary_screen_detail::ButtonVariant::Ghost,
					                                                  .size = mission_summary_screen_detail::ButtonSize::Auto,
					                                                  .minWidth = 34,
					                                                  .paddingY = 4 },
					           mission_summary_screen_detail::ButtonHandle{ nullptr, mission_summary_screen_detail::kActionScrollUp, &interactions });
					mission_summary_screen_detail::Button(CLAY_STRING("MissionSummaryScrollDownButton"), CLAY_STRING("Down"),
					           mission_summary_screen_detail::ButtonOpts{ .variant = mission_summary_screen_detail::ButtonVariant::Ghost,
					                                                  .size = mission_summary_screen_detail::ButtonSize::Auto,
					                                                  .minWidth = 48,
					                                                  .paddingY = 4 },
					           mission_summary_screen_detail::ButtonHandle{ nullptr, mission_summary_screen_detail::kActionScrollDown, &interactions });
				}
			}
			mission_summary_screen_detail::Button(CLAY_STRING("MissionSummaryDoneButton"), CLAY_STRING("Done"),
			           mission_summary_screen_detail::ButtonOpts{ .variant = mission_summary_screen_detail::ButtonVariant::Chrome,
			                                                   .size = mission_summary_screen_detail::ButtonSize::Compact },
			           mission_summary_screen_detail::ButtonHandle{ nullptr, mission_summary_screen_detail::kActionDone, &interactions });
		}
		CLAY({ .id = CLAY_ID("MissionSummaryUpgrades"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(mission_summary_screen_detail::kRightW),
		                       CLAY_SIZING_FIXED(390) },
		           .padding = { 12, 12, 6, 0 },
		           .childGap = 16,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			mission_summary_screen_detail::BankText(mission_summary_screen_detail::FromStd(xp), mission_summary_screen_detail::BankTextVariant::Title, {});
			if(upgradeBanner){
				mission_summary_screen_detail::BankText(CLAY_STRING("*NEW UPGRADE AVAILABLE*"),
				         mission_summary_screen_detail::BankTextVariant::Body,
				         { .effectColor = 129, .brightness = static_cast<Uint8>(160),
				           .colorRamp = true });
			}
			for(int i = 0; i < 6; i++){
				CLAY({ .id = CLAY_IDI("UpgradeRow", (uint32_t)i),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30) },
				           .childGap = 8,
				           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_IDI("UpgradeLabel", (uint32_t)i),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
					       } }) {
						mission_summary_screen_detail::BankText(mission_summary_screen_detail::FromCStr(mission_summary_screen_detail::kLevelLabels[i]), mission_summary_screen_detail::BankTextVariant::Body, {});
					}
					std::string level = std::to_string(levels[i]);
					mission_summary_screen_detail::BankText(mission_summary_screen_detail::FromStd(level), mission_summary_screen_detail::BankTextVariant::Body, {});
				}
				if(upgradesAvailable[i]){
					std::string actionId = std::string(mission_summary_screen_detail::kActionUpgradePrefix) + std::to_string(i);
					std::string buttonId = "MissionSummaryUpgradeButton" + std::to_string(i);
					mission_summary_screen_detail::Button(mission_summary_screen_detail::FromStd(buttonId),
					           mission_summary_screen_detail::FromCStr(mission_summary_screen_detail::kUpgradeLabels[i]),
					           mission_summary_screen_detail::ButtonOpts{ .variant = mission_summary_screen_detail::ButtonVariant::Oval,
					                                                   .size = mission_summary_screen_detail::ButtonSize::Auto,
					                                                   .minWidth = 156 },
					           mission_summary_screen_detail::ButtonHandle{ nullptr, actionId.c_str(), &interactions });
				}
			}
		}
	}
}

void MissionSummaryScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MissionSummaryScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel ||
	   (action.kind == silencer::ui::UiActionKind::Activate && action.id == mission_summary_screen_detail::kActionDone)){
		doneClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == mission_summary_screen_detail::kActionScrollUp){
		scrollDelta--;
		return true;
	}
	if(action.id == mission_summary_screen_detail::kActionScrollDown){
		scrollDelta++;
		return true;
	}
	int upgrade = mission_summary_screen_detail::SuffixInt(action.id, mission_summary_screen_detail::kActionUpgradePrefix);
	if(upgrade >= 0 && upgrade < 6){
		upgradeClicked = upgrade;
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
