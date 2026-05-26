#include "mission_summary_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "user.h"
#include "stats.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"
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
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::MeasureText;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOrigin;

constexpr int kStageW = 640;
constexpr int kStageH = 480;
constexpr int kFrameMarginLeft = 5;
constexpr int kFrameMarginRight = 7;
constexpr int kFrameMarginTop = 19;
constexpr int kFrameMarginBottom = 20;
constexpr int kPanelW = 628;
constexpr int kPanelH = 441;
constexpr uint16_t kSummaryW = 180;
constexpr uint16_t kSummaryH = 300;
// Legacy TextBox scroll math used the 300px logical viewport while drawing
// one extra line at the lower edge. Keep layout rendering separate from that
// scroll limit so the restored screen matches the pre-Clay frame.
constexpr uint16_t kSummaryRenderH = 308;
constexpr uint8_t kLineH = 11;
constexpr int kTitleCenterX = 192;
constexpr int kTitleY = 44;
constexpr int kSummaryX = 89;
constexpr int kSummaryY = 92;
constexpr int kXpCenterX = 467;
constexpr int kXpY = 45;
constexpr int kUpgradeBannerY = 77;
constexpr int kLevelLabelX = 390;
constexpr int kLevelValueRightX = 556;
constexpr int kLevelStartY = 97;
constexpr int kLevelRowGap = 46;
constexpr int kUpgradeButtonX = 372;
constexpr int kUpgradeButtonY = 108;
constexpr int kDoneButtonX = 372;
constexpr int kDoneButtonY = 388;
constexpr int kButtonW = 196;
constexpr int kButtonH = 33;
constexpr int kTextLineH = 14;
constexpr int kTitleW = 180;
constexpr int kTitleH = 24;
constexpr int kXpW = 160;
constexpr int kXpH = 28;
constexpr int kBannerW = 160;
constexpr int kBannerH = 16;
constexpr int kLevelLabelW = 160;
constexpr int kLevelValueW = 24;
constexpr int kMaxSummaryLines = 256;
constexpr const char * kActionDone = "mission_summary.done";
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

constexpr Lobby::StatID kUpgradeStatIds[6] = {
	Lobby::STAT_ENDURANCE,
	Lobby::STAT_SHIELD,
	Lobby::STAT_JETPACK,
	Lobby::STAT_TECHSLOTS,
	Lobby::STAT_HACKING,
	Lobby::STAT_CONTACTS,
};

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

template <typename Text>
bool StartsWith(const Text & value, const char * prefix)
{
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

template <typename Text>
int SuffixInt(const Text & value, const char * prefix)
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
		g_summarySlab[i].effect = TextEffect::Default();
		g_summarySlab[i].indent = 0;
	}
	return count;
}

int RelativeX(int screenX)
{
	return screenX - kFrameMarginLeft;
}

int RelativeY(int screenY)
{
	return screenY - kFrameMarginTop;
}

Clay_ElementDeclaration FloatingElement(Clay_ElementId id,
                                        int screenX,
                                        int screenY,
                                        int width,
                                        int height)
{
	return {
		.id = id,
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
			            CLAY_SIZING_FIXED(static_cast<float>(height)) },
		},
		.floating = {
			.offset = { static_cast<float>(RelativeX(screenX)),
			            static_cast<float>(RelativeY(screenY)) },
			.zIndex = 1,
			.attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
			                  .parent = CLAY_ATTACH_POINT_LEFT_TOP },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
			.attachTo = CLAY_ATTACH_TO_PARENT,
		},
	};
}

int TextPixelWidth(Clay_String value, TextSize size)
{
	return static_cast<int>(MeasureText(value, size).width);
}

int CenteredTextX(int centerX, Clay_String value, TextSize size)
{
	return centerX - TextPixelWidth(value, size) / 2;
}

int RightAlignedTextX(int rightX, Clay_String value, TextSize size)
{
	return rightX - TextPixelWidth(value, size);
}

void BuildUpgradeRow(int index,
                     int level,
                     bool canUpgrade,
                     silencer::ui::UiInteractionRegistry& interactions)
{
	const int rowY = kLevelStartY + index * kLevelRowGap;
	CLAY(FloatingElement(CLAY_IDI("MissionSummaryUpgradeLabel", (uint32_t)index),
	                     kLevelLabelX, rowY, kLevelLabelW, kTextLineH)) {
		Text(FromCStr(kLevelLabels[index]), { .size = TextSize::Body });
	}

	std::string levelText = std::to_string(level);
	const int valueX = RightAlignedTextX(kLevelValueRightX,
	                                     FromStd(levelText),
	                                     TextSize::Body);
	CLAY(FloatingElement(CLAY_IDI("MissionSummaryUpgradeValue", (uint32_t)index),
	                     valueX, rowY, kLevelValueW, kTextLineH)) {
		Text(FromStd(levelText), { .size = TextSize::Body });
	}

	if(!canUpgrade) return;

	std::string actionId = std::string(kActionUpgradePrefix) +
		std::to_string(index);
	std::string buttonId = "MissionSummaryUpgradeButton" +
		std::to_string(index);
	CLAY(FloatingElement(CLAY_IDI("MissionSummaryUpgradeButtonWrap",
	                              (uint32_t)index),
	                     kUpgradeButtonX,
	                     kUpgradeButtonY + index * kLevelRowGap,
	                     kButtonW,
	                     kButtonH)) {
		Button(FromStd(buttonId),
		       FromCStr(kUpgradeLabels[index]),
		       ButtonOpts{ .variant = ButtonVariant::Oval,
		                   .size = ButtonSize::Md },
		       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
	}
}

} // namespace mission_summary_screen_detail

void MissionSummaryScreen::Build(ScreenContext & ctx)
{
	ctx.ResetMenuPresentation(1);
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
			world.lobby.UpgradeStat(user->selectedcharid, user->statsagency,
			                        mission_summary_screen_detail::kUpgradeStatIds[idx]);
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
	const int titleX = mission_summary_screen_detail::CenteredTextX(
		mission_summary_screen_detail::kTitleCenterX,
		CLAY_STRING("Mission Summary"),
		mission_summary_screen_detail::TextSize::ScreenTitle);
	const int xpX = mission_summary_screen_detail::CenteredTextX(
		mission_summary_screen_detail::kXpCenterX,
		mission_summary_screen_detail::FromStd(xp),
		mission_summary_screen_detail::TextSize::Prompt);

	CLAY({ .id = CLAY_ID("MissionSummaryRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("MissionSummaryStage"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(mission_summary_screen_detail::kStageW),
		                       CLAY_SIZING_FIXED(mission_summary_screen_detail::kStageH) },
		           .padding = { mission_summary_screen_detail::kFrameMarginLeft,
		                        mission_summary_screen_detail::kFrameMarginRight,
		                        mission_summary_screen_detail::kFrameMarginTop,
		                        mission_summary_screen_detail::kFrameMarginBottom },
		       } }) {
			CLAY({ .id = CLAY_ID("MissionSummaryPanel"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(mission_summary_screen_detail::kPanelW),
			                       CLAY_SIZING_FIXED(mission_summary_screen_detail::kPanelH) },
			       },
			       .image = { .imageData = PackImageStretch(7, 5) } }) {
				CLAY(mission_summary_screen_detail::FloatingElement(
					CLAY_ID("MissionSummaryTitle"),
					titleX,
					mission_summary_screen_detail::kTitleY,
					mission_summary_screen_detail::kTitleW,
					mission_summary_screen_detail::kTitleH)) {
					mission_summary_screen_detail::Text(CLAY_STRING("Mission Summary"),
					                                    { .size = mission_summary_screen_detail::TextSize::ScreenTitle });
				}
				CLAY(mission_summary_screen_detail::FloatingElement(
					CLAY_ID("MissionSummaryStats"),
					mission_summary_screen_detail::kSummaryX,
					mission_summary_screen_detail::kSummaryY,
					mission_summary_screen_detail::kSummaryW,
					mission_summary_screen_detail::kSummaryRenderH)) {
					mission_summary_screen_detail::ScrollTextBox(
						CLAY_STRING("MissionSummaryStatsText"),
						mission_summary_screen_detail::g_summarySlab, lineCount,
						static_cast<Uint16>(scrollPosition),
						{ .width = mission_summary_screen_detail::kSummaryW,
						  .height = mission_summary_screen_detail::kSummaryRenderH,
						  .lineHeight = mission_summary_screen_detail::kLineH,
						  .text = { .size = mission_summary_screen_detail::TextSize::Body },
						  .origin = mission_summary_screen_detail::ScrollTextBoxOrigin::TopDown });
				}
				CLAY(mission_summary_screen_detail::FloatingElement(
					CLAY_ID("MissionSummaryDoneButtonWrap"),
					mission_summary_screen_detail::kDoneButtonX,
					mission_summary_screen_detail::kDoneButtonY,
					mission_summary_screen_detail::kButtonW,
					mission_summary_screen_detail::kButtonH)) {
					mission_summary_screen_detail::Button(
						CLAY_STRING("MissionSummaryDoneButton"), CLAY_STRING("Done"),
						mission_summary_screen_detail::ButtonOpts{
							.variant = mission_summary_screen_detail::ButtonVariant::Oval,
							.size = mission_summary_screen_detail::ButtonSize::Md },
						mission_summary_screen_detail::ButtonHandle{ nullptr, mission_summary_screen_detail::kActionDone, &interactions });
				}
				CLAY(mission_summary_screen_detail::FloatingElement(
					CLAY_ID("MissionSummaryXp"),
					xpX,
					mission_summary_screen_detail::kXpY,
					mission_summary_screen_detail::kXpW,
					mission_summary_screen_detail::kXpH)) {
					mission_summary_screen_detail::Text(
						mission_summary_screen_detail::FromStd(xp),
						{ .size = mission_summary_screen_detail::TextSize::Prompt });
				}
				if(upgradeBanner){
					const char * banner = "*NEW UPGRADE AVAILABLE*";
					const int bannerX = mission_summary_screen_detail::CenteredTextX(
						mission_summary_screen_detail::kXpCenterX,
						mission_summary_screen_detail::FromCStr(banner),
						mission_summary_screen_detail::TextSize::Body);
					CLAY(mission_summary_screen_detail::FloatingElement(
						CLAY_ID("MissionSummaryUpgradeBanner"),
						bannerX,
						mission_summary_screen_detail::kUpgradeBannerY,
						mission_summary_screen_detail::kBannerW,
						mission_summary_screen_detail::kBannerH)) {
						mission_summary_screen_detail::Text(
							mission_summary_screen_detail::FromCStr(banner),
							{ .size = mission_summary_screen_detail::TextSize::Body,
							  .effect = mission_summary_screen_detail::TextEffect::LegacyPalette(
								  129, static_cast<Uint8>(160), true) });
					}
				}
				for(int i = 0; i < 6; i++){
					mission_summary_screen_detail::BuildUpgradeRow(
						i, levels[i], upgradesAvailable[i], interactions);
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
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		scrollDelta += action.amount;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
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
