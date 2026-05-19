#include "character_create_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "lobby.h"
#include "renderer.h"
#include "surface.h"
#include "team.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"
#include "primitives/text_input.h"
#include "primitives/toggle.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <string>

namespace character_create_screen_detail {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleOpts;

constexpr int kStageW = 640;
constexpr int kStageH = 480;
constexpr int kFrameMarginLeft = 5;
constexpr int kFrameMarginRight = 7;
constexpr int kFrameMarginTop = 19;
constexpr int kFrameMarginBottom = 20;
constexpr int kPanelMinW = 628;
constexpr int kPanelMinH = 441;
constexpr int kContentPadLeft = 55;
constexpr int kContentPadRight = 22;
constexpr int kContentPadTop = 18;
constexpr int kContentPadBottom = 34;
constexpr int kColumnGap = 78;
constexpr int kLeftColumnW = 236;
constexpr int kRightColumnW = 196;
constexpr int kTitleH = 33;
constexpr int kTitleToRowsGap = 14;
constexpr int kRowH = 27;
constexpr int kRowGap = 5;
constexpr int kAgentRowsH = 272;
constexpr int kAgencyRowsH = 168;
constexpr int kDetailTopPad = 42;
constexpr int kAliasModalW = 284;
constexpr int kAliasBodyW = 284;
constexpr int kAliasModalH = 127;
constexpr int kAliasModalTop = 161;
constexpr int kAliasModalOffsetX = 0;
constexpr int kAliasBodyTopOffset = 15;
constexpr int kAliasModalBodyH = 94;
constexpr int kAliasInputFrameW = 236;
constexpr int kAliasInputW = 224;
constexpr int kMaxRows = 32;

constexpr const char * kActionAgentPrefix = "character_create.agent";
constexpr const char * kActionAgencyPrefix = "character_create.agency";
constexpr const char * kActionCreate = "character_create.create";
constexpr const char * kActionAlias = "character_create.alias";

enum : int {
	kAliasInputUid = 31,
};

struct AgencyDef {
	Uint8 agency;
	const char * name;
	const char * displayName;
	const char * advantages;
	const char * ability;
	const char * description;
};

const AgencyDef kAgencies[5] = {
	{ Team::NOXIS,
	  "Noxis",
	  "Noxis",
	  "Endurance +3\nJump +5",
	  "Health Packs",
	  "The Noxis corporation terraformed the majority of the initial habitable sectors of Mars, and continues to do so, as well as producing and selling 70 percent of the breathable oxygen. Since they are widely known to the populace and government, they have taken steps to bolster their agent's health so they are better able to avoid detection. Training in bio-sporria rich environments, and possessing suits with advanced oxygen processors and filters, has given these agents improved physical abilities such as higher jumps, more stamina, and enhanced durability." },
	{ Team::LAZARUS,
	  "Lazarus",
	  "Lazarus",
	  "Resurrection Ability",
	  "Resurrection",
	  "Like the mythical phoenix, the loose organization known as Lazarus has been resurrected every hundred years and causing chaos in the larger urban areas of Mars. Whether it's belief in some higher power, an understanding of mystical truths, or something else entirely, no one outside of the group is sure of this Lazarus. Numerous witnesses of Lazarus attacks claim to have seen supposedly fatally wounded agents continue themselves off and continue their efforts, virtually undamaged." },
	{ Team::CALIBER,
	  "Caliber",
	  "Caliber",
	  "Contacts +3",
	  "Security Passes",
	  "Caliber agents specialize in access, favors, and fast-moving procurement. Their contacts let them gather better mission rewards and keep security pressure away from their teams for longer than most agencies can manage." },
	{ Team::STATIC,
	  "Static",
	  "Static",
	  "Satellite Ability\nHacking +3",
	  "Virus",
	  "The Guv first realized that there was a brain-drain about four years ago. Silently, the youngest talented computer specialists and high level programmers were disappearing and leaving the Guv without the technical support it required. They ended up creating Static as a technological savvy, but anti-government agency, but many members are more interested in reinforcing their reputation as the best skilled hackers with the best skills and techniques." },
	{ Team::BLACKROSE,
	  "Black Rose",
	  "Black Rose",
	  "Stealth Ability\nShield +2",
	  "Poison Flare",
	  "More like a dark force than a loose collection of humans, little is known of Black Rose. Something vibrant was taken from these people, and left in its place was a new form of sustenance. Its members seem to be misanthropic and have a penchant for using illegal compounds such as Milloxthal injections to cause extreme pain and internal withering in their victims. Shunned by even the most avaristic mercenaries, these agents work alone and yet still prosper due to some combination of ego and superlative talent." },
};

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

bool StartsWith(const std::string& value, const char * prefix)
{
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string& value, const char * prefix)
{
	if(!StartsWith(value, prefix)) return -1;
	const char * suffix = value.c_str() + std::strlen(prefix);
	if(*suffix == '.') ++suffix;
	return std::atoi(suffix);
}

int AgencyIndex(Uint8 agency)
{
	for(int i = 0; i < 5; ++i){
		if(kAgencies[i].agency == agency) return i;
	}
	return 0;
}

const AgencyDef& SelectedAgency(Uint8 agency)
{
	return kAgencies[AgencyIndex(agency)];
}

void Title(Clay_String text)
{
	Text(text, { .size = TextSize::ScreenTitle,
	             .effect = TextEffect::LegacyPalette(0) });
}

void DetailHeading(Clay_String text)
{
	Text(text, { .size = TextSize::Heading,
	             .effect = TextEffect::LegacyPalette(129, 160, true) });
}

void DetailText(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .wrap = silencer::ui::primitives::TextWrap::Newlines,
	             .effect = TextEffect::LegacyPalette(129, 160, true) });
}

void DetailParagraph(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .wrap = silencer::ui::primitives::TextWrap::Words,
	             .effect = TextEffect::LegacyPalette(129, 160, true) });
}

void ListButton(Clay_String id,
                Clay_String label,
                const char * action,
                silencer::ui::UiInteractionRegistry& interactions,
                bool selected = false)
{
	Button(id,
	       label,
	       ButtonOpts{ .variant = ButtonVariant::LegacyRow,
	                   .size = ButtonSize::Auto,
	                   .selected = selected,
	                   .minWidth = kLeftColumnW },
	       ButtonHandle{ nullptr, action, &interactions });
}

std::string AgentActionId(int index)
{
	std::string out = kActionAgentPrefix;
	out += ".";
	out += std::to_string(index);
	return out;
}

std::string AgencyActionId(int index)
{
	std::string out = kActionAgencyPrefix;
	out += ".";
	out += std::to_string(index);
	return out;
}

const AgencyDef& AgencyByIndex(int index)
{
	if(index < 0 || index >= 5) return kAgencies[0];
	return kAgencies[index];
}

int CreateRowIndex(size_t characterCount)
{
	return std::min(static_cast<int>(characterCount), kMaxRows - 1);
}

}  // namespace character_create_screen_detail

void CharacterCreateScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	ctx.renderer.camera.SetPosition(320, 240);
	step = Step::SelectAgent;
	selectedAgentIndex = 0;
	agentScroll = 0;
	agentScrollDelta = 0;
	selectedAgency = Team::NOXIS;
	characterCountOnEntry = ctx.lobby.characters.size();
	waitingForCreate = false;
	focusAliasRequested = false;
	alias[0] = '\0';
	RebuildAgentRows(ctx);
}

void CharacterCreateScreen::Tick(ScreenContext & ctx)
{
	if(waitingForCreate){
		ctx.lobby.LockMutex();
		const bool received = ctx.lobby.charactersreceived;
		const bool created = ctx.lobby.characters.size() > characterCountOnEntry;
		ctx.lobby.UnlockMutex();
		if(created){
			waitingForCreate = false;
			ctx.GoToState(GameState::LOBBY);
			return;
		}
		if(received){
			waitingForCreate = false;
			ctx.ShowMessage("Could not create character");
		}
	}
}

void CharacterCreateScreen::BuildUi(ScreenContext & ctx,
                                    Surface & dst,
                                    float frametime,
                                    silencer::ui::UiInteractionRegistry& interactions)
{
	(void)dst;
	(void)frametime;
	using namespace silencer::clay_bridge;
	using namespace character_create_screen_detail;

	RebuildAgentRows(ctx);
	if(agentScrollDelta != 0){
		int next = static_cast<int>(agentScroll) + agentScrollDelta;
		const int visible = kAgentRowsH / (kRowH + kRowGap);
		const int maxScroll = std::max(0, static_cast<int>(agentRows.size()) - visible);
		if(next < 0) next = 0;
		if(next > maxScroll) next = maxScroll;
		agentScroll = static_cast<Uint16>(next);
		agentScrollDelta = 0;
	}

	CLAY({ .id = CLAY_ID("CharacterCreateRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterCreateStage"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kStageW),
		                       CLAY_SIZING_FIXED(kStageH) },
		           .padding = { kFrameMarginLeft,
		                        kFrameMarginRight,
		                        kFrameMarginTop,
		                        kFrameMarginBottom },
		       },
		       .image = { .imageData = PackImageStretch(6, 0) } }) {
			CLAY({ .id = CLAY_ID("CharacterCreatePanel"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(kPanelMinW),
			                       CLAY_SIZING_FIXED(kPanelMinH) },
			           .padding = { kContentPadLeft,
			                        kContentPadRight,
			                        kContentPadTop,
			                        kContentPadBottom },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			           .childGap = kColumnGap,
			       },
			       .image = { .imageData = PackImageStretch(7, 5) } }) {
				if(step == Step::SelectAgent){
					BuildSelectAgent(ctx, interactions);
				}else if(step == Step::EnterAlias){
					BuildEnterAlias(ctx, interactions);
				}else{
					BuildSelectAgency(ctx, interactions);
				}
			}
		}
	}
}

void CharacterCreateScreen::BuildSelectAgent(ScreenContext & ctx,
                                             silencer::ui::UiInteractionRegistry& interactions)
{
	using namespace character_create_screen_detail;

	CLAY({ .id = CLAY_ID("CharacterCreateAgentColumn"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kLeftColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	           .childGap = kRowGap,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterCreateAgentTitle"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_CENTER },
		} }) {
			Title(CLAY_STRING("SELECT AGENT"));
		}
		CLAY({ .id = CLAY_ID("CharacterCreateAgentTitleSpacer"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleToRowsGap) },
		       } }) {}
		CLAY({ .id = CLAY_ID("CharacterCreateAgentRows"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kAgentRowsH) },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		           .childGap = kRowGap,
		       } }) {
			const int visibleRows = kAgentRowsH / (kRowH + kRowGap);
			const int count = std::min(static_cast<int>(agentRows.size()), kMaxRows);
			for(int row = 0; row < visibleRows; ++row){
				const int index = static_cast<int>(agentScroll) + row;
				if(index < 0 || index >= count) break;
				const std::string id = std::string("CharacterCreateAgentRow") + std::to_string(index);
				const std::string action = AgentActionId(index);
				ListButton(FromStd(id),
				           FromStd(agentRows[static_cast<size_t>(index)]),
				           action.c_str(),
				           interactions,
				           index == selectedAgentIndex);
			}
		}
	}

	CLAY({ .id = CLAY_ID("CharacterCreateAgentDetails"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kRightColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, kDetailTopPad, 0 },
	           .childGap = 8,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		const int createIndex = CreateRowIndex(ctx.lobby.characters.size());
		if(selectedAgentIndex >= 0 &&
		   selectedAgentIndex < static_cast<int>(ctx.lobby.characters.size()) &&
		   selectedAgentIndex != createIndex){
			const Lobby::Character& ch = ctx.lobby.characters[static_cast<size_t>(selectedAgentIndex)];
			const AgencyDef& agency = AgencyByIndex(static_cast<int>(ch.agencyIdx));
			CLAY({ .id = CLAY_ID("CharacterCreateAgentAgencyRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24) },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			           .childGap = 8,
			           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
			                               .y = CLAY_ALIGN_Y_CENTER },
			       } }) {
				DetailText(CLAY_STRING("Agency"));
				DetailText(FromCStr(agency.displayName));
				Toggle(CLAY_STRING("CharacterCreateAgentAgencyIcon"),
				       181,
				       agency.agency,
				       true,
				       ToggleOpts{ .width = 20,
				                   .height = 20,
				                   .effectColor = 112,
				                   .selectedBrightness = 128,
				                   .unselectedBrightness = 128 });
			}
			std::string security = std::string("Security Level ") + std::to_string(ch.stats.level);
			DetailText(FromStd(security));
			std::string tech = std::string("Technology Slots: ") + std::to_string(ch.stats.techslots);
			std::string points = std::string("Agency Points: ") + std::to_string(ch.stats.xp);
			std::string next = std::string("Next Level At: ") + std::to_string((ch.stats.level + 1) * 100);
			std::string missions = std::string("Successful Missions: ") + std::to_string(ch.stats.wins);
			DetailText(FromStd(tech));
			DetailText(FromStd(points));
			DetailText(FromStd(next));
			DetailText(FromStd(missions));
			CLAY({ .id = CLAY_ID("CharacterCreateAgentAdvantages"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			           .padding = { 0, 0, 10, 0 },
			           .childGap = 2,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				DetailHeading(CLAY_STRING("Advantages"));
				DetailText(FromCStr(agency.advantages));
				DetailText(FromCStr(agency.ability));
			}
		}
	}
}

void CharacterCreateScreen::BuildEnterAlias(ScreenContext & ctx,
                                            silencer::ui::UiInteractionRegistry& interactions)
{
	using namespace character_create_screen_detail;
	const bool focused = interactions.IsTextInputFocused(kAliasInputUid);
	const bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

	BuildSelectAgent(ctx, interactions);

	CLAY({ .id = CLAY_ID("CharacterAliasModalWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kAliasModalW),
	                       CLAY_SIZING_FIXED(kAliasModalH) },
	           .childGap = 0,
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .floating = {
	           .offset = { static_cast<float>(kAliasModalOffsetX),
	                       static_cast<float>(kAliasModalTop +
	                                          (kAliasModalH / 2) -
	                                          kFrameMarginTop -
	                                          (kPanelMinH / 2)) },
	           .zIndex = 2,
	           .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_CENTER,
	                             .parent = CLAY_ATTACH_POINT_CENTER_CENTER },
	           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
	           .attachTo = CLAY_ATTACH_TO_PARENT,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterAliasTitleFloat"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kAliasModalW),
		                       CLAY_SIZING_FIXED(kTitleH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_TOP },
		       },
		       .floating = {
		           .offset = { 0.0f, 0.0f },
		           .zIndex = 3,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_TOP,
		                             .parent = CLAY_ATTACH_POINT_CENTER_TOP },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
		           .attachTo = CLAY_ATTACH_TO_PARENT,
		       } }) {
			Button(CLAY_STRING("CharacterAliasTitle"),
			       CLAY_STRING("SILENCER ALIAS"),
			       ButtonOpts{ .variant = ButtonVariant::Oval,
			                   .size = ButtonSize::Auto,
			                   .minWidth = 254,
			                   .textEffect = TextEffect::LegacyPalette(0) });
		}
		CLAY({ .id = CLAY_ID("CharacterAliasBodyOffset"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kAliasBodyTopOffset) },
		       } }) {}
		CLAY({ .id = CLAY_ID("CharacterAliasModalBody"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kAliasBodyW),
		                       CLAY_SIZING_FIXED(kAliasModalBodyH) },
		           .padding = { 24, 24, 31, 0 },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImageStretch(7, 27) } }) {
			CLAY({ .id = CLAY_ID("CharacterAliasInputFrame"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(kAliasInputFrameW),
			                       CLAY_SIZING_FIXED(kRowH) },
			           .padding = { 6, 6, 0, 0 },
			       },
			       .image = { .imageData = silencer::clay_bridge::PackImageStretch(7, 24) } }) {
				TextInput(CLAY_STRING("CharacterAliasInput"),
				          alias,
				          TextInputOpts{ .widthPx = kAliasInputW,
				                         .heightPx = kRowH,
				                         .textSize = TextSize::Body,
				                         .showCaret = focused && blink },
				          TextInputHandle{ nullptr,
				                           kActionAlias,
				                           "Alias",
				                           &interactions,
				                           kAliasInputUid,
				                           16,
				                           true });
			}
		}
		if(focusAliasRequested){
			ctx.game.UiInteractions().FocusTextInputByUid(kAliasInputUid);
			focusAliasRequested = false;
		}
	}
}

void CharacterCreateScreen::BuildSelectAgency(ScreenContext & ctx,
                                              silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	using namespace character_create_screen_detail;
	const AgencyDef& agency = SelectedAgency(selectedAgency);

	CLAY({ .id = CLAY_ID("CharacterAgencyColumn"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kLeftColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	           .childGap = kRowGap,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterAgencyTitle"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_CENTER },
		} }) {
			Title(CLAY_STRING("SELECT AGENCY"));
		}
		CLAY({ .id = CLAY_ID("CharacterAgencyTitleSpacer"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleToRowsGap) },
		       } }) {}
		CLAY({ .id = CLAY_ID("CharacterAgencyRows"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kAgencyRowsH) },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		           .childGap = kRowGap,
		       } }) {
			for(int i = 0; i < 5; ++i){
				const std::string id = std::string("CharacterAgencyRow") + std::to_string(i);
				const std::string action = AgencyActionId(i);
				ListButton(FromStd(id),
				           FromCStr(kAgencies[i].displayName),
				           action.c_str(),
				           interactions,
				           kAgencies[i].agency == selectedAgency);
			}
		}
	}

	CLAY({ .id = CLAY_ID("CharacterAgencyInfo"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kRightColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, kDetailTopPad, 0 },
	           .childGap = 8,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		DetailHeading(CLAY_STRING("Advantages"));
		DetailText(FromCStr(agency.advantages));
		DetailHeading(CLAY_STRING("Description"));
		CLAY({ .id = CLAY_ID("CharacterAgencyDescription"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		       } }) {
			DetailParagraph(FromCStr(agency.description));
		}
	}
}

void CharacterCreateScreen::Destroy(ScreenContext & ctx)
{
	ctx.game.UiInteractions().ClearFocus();
}

bool CharacterCreateScreen::HandleBack(ScreenContext & ctx)
{
	if(step == Step::SelectAgency){
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return true;
	}
	if(step == Step::EnterAlias){
		step = Step::SelectAgent;
		return true;
	}
	if(!ctx.lobby.characters.empty()){
		ctx.GoToState(GameState::LOBBY);
	}else{
		ctx.GoToState(GameState::LOBBYCONNECT);
	}
	return true;
}

bool CharacterCreateScreen::HandleUiIntent(ScreenContext & ctx,
                                           const silencer::ui::UiAction & action)
{
	using namespace character_create_screen_detail;

	if(action.kind == silencer::ui::UiActionKind::Cancel){
		return HandleBack(ctx);
	}
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(step == Step::SelectAgent){
			agentScrollDelta += action.amount;
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::Navigate ||
	   action.kind == silencer::ui::UiActionKind::Select){
		int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
		if(step == Step::SelectAgent && agentIndex >= 0){
			selectedAgentIndex = agentIndex;
			return true;
		}
		int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
		if(step == Step::SelectAgency && agencyIndex >= 0 && agencyIndex < 5){
			selectedAgency = kAgencies[agencyIndex].agency;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::SetText &&
	   action.id == kActionAlias){
		CopyAlias(action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   action.id == kActionAlias){
		CopyAlias(action.value);
		AdvanceAliasStep(ctx);
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate){
		return false;
	}
	int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
	if(agentIndex >= 0){
		selectedAgentIndex = agentIndex;
		SelectCurrentAgent(ctx);
		return true;
	}
	int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
	if(agencyIndex >= 0){
		if(agencyIndex < 5){
			if(kAgencies[agencyIndex].agency == selectedAgency && alias[0] != '\0'){
				CreateCurrentAgent(ctx);
			}else{
				selectedAgency = kAgencies[agencyIndex].agency;
			}
		}
		return true;
	}
	if(action.id == kActionCreate){
		CreateCurrentAgent(ctx);
		return true;
	}
	return false;
}

void CharacterCreateScreen::SelectCurrentAgent(ScreenContext & ctx)
{
	const int createIndex = character_create_screen_detail::CreateRowIndex(
		ctx.lobby.characters.size());
	if(selectedAgentIndex == createIndex ||
	   selectedAgentIndex >= static_cast<int>(ctx.lobby.characters.size())){
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}

	Uint32 charId = 0;
	ctx.lobby.LockMutex();
	const int charIndex = selectedAgentIndex;
	if(charIndex >= 0 && charIndex < static_cast<int>(ctx.lobby.characters.size())){
		const Lobby::Character& ch = ctx.lobby.characters[static_cast<size_t>(charIndex)];
		charId = ch.id;
		ctx.lobby.selectedcharid = ch.id;
		ctx.lobby.selectedagency = ch.agencyIdx;
	}
	if(charId != 0){
		ctx.lobby.SelectCharacter(charId);
	}
	ctx.lobby.UnlockMutex();
	if(charId != 0){
		ctx.GoToState(GameState::LOBBY);
	}
}

void CharacterCreateScreen::CreateCurrentAgent(ScreenContext & ctx)
{
	if(alias[0] == '\0'){
		ctx.ShowMessage("Enter an alias");
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}
	ctx.lobby.LockMutex();
	characterCountOnEntry = ctx.lobby.characters.size();
	ctx.lobby.charactersreceived = false;
	ctx.lobby.CreateCharacter(alias, selectedAgency);
	ctx.lobby.UnlockMutex();
	waitingForCreate = true;
}

void CharacterCreateScreen::RebuildAgentRows(ScreenContext & ctx)
{
	agentRows.clear();
	for(const Lobby::Character& ch : ctx.lobby.characters){
		agentRows.push_back(ch.name);
	}
	agentRows.push_back("Create New Character");
	const int maxIndex = std::max(0, std::min(static_cast<int>(agentRows.size()), character_create_screen_detail::kMaxRows) - 1);
	if(selectedAgentIndex > maxIndex){
		selectedAgentIndex = maxIndex;
	}
}

void CharacterCreateScreen::CopyAlias(const std::string& value)
{
	size_t n = value.size();
	if(n > sizeof(alias) - 1) n = sizeof(alias) - 1;
	std::memcpy(alias, value.data(), n);
	alias[n] = '\0';
}

void CharacterCreateScreen::AdvanceAliasStep(ScreenContext & ctx)
{
	(void)ctx;
	if(alias[0] == '\0'){
		return;
	}
	step = Step::SelectAgency;
}
