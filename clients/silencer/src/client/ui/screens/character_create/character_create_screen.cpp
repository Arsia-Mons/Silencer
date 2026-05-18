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
#include "primitives/scroll_list.h"
#include "primitives/text.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstring>

namespace character_create_screen_detail {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;

constexpr uint16_t kPanelW = 382;
constexpr uint16_t kPanelH = 314;
constexpr uint16_t kPanelPad = 14;
constexpr uint16_t kTitleH = 30;
constexpr uint16_t kListW = 220;
constexpr uint16_t kListH = 156;
constexpr uint16_t kListLineH = 18;
constexpr uint8_t kScrollbarBank = 7;
constexpr int kMaxRows = 32;

constexpr const char * kActionAgentPrefix = "character_create.agent";
constexpr const char * kActionAgencyPrefix = "character_create.agency";
constexpr const char * kActionSelect = "character_create.select";
constexpr const char * kActionNext = "character_create.next";
constexpr const char * kActionBack = "character_create.back";
constexpr const char * kActionCreate = "character_create.create";
constexpr const char * kActionAlias = "character_create.alias";

enum : int {
	kAliasInputUid = 31,
};

struct AgencyDef {
	Uint8 agency;
	const char * name;
	const char * advantages;
	const char * description;
};

const AgencyDef kAgencies[5] = {
	{ Team::NOXIS,
	  "Noxis",
	  "Endurance +3    Jump +5",
	  "Terraforming security trained for long surface operations and fast terrain traversal." },
	{ Team::LAZARUS,
	  "Lazarus",
	  "Shield +3    Hacking +2",
	  "Medical black-ops agents using adaptive shielding and neural intrusion tools." },
	{ Team::CALIBER,
	  "Caliber",
	  "Contacts +2    Tech Slots +2",
	  "Procurement specialists with stronger field networks and flexible loadouts." },
	{ Team::STATIC,
	  "Static",
	  "Hacking +4    Shield +1",
	  "Signals operators built around surveillance, disruption, and electronic entry." },
	{ Team::BLACKROSE,
	  "Blackrose",
	  "Jetpack +3    Contacts +2",
	  "Covert mobility agents with rapid access routes and off-ledger support." },
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
	Text(text, { .size = TextSize::Title,
	             .effect = TextEffect::LegacyPalette(200) });
}

void Body(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .effect = TextEffect::LegacyPalette(129, 170, true) });
}

void ChromeButton(Clay_String id,
                  Clay_String label,
                  const char * action,
                  silencer::ui::UiInteractionRegistry& interactions,
                  int minWidth = 76)
{
	Button(id,
	       label,
	       ButtonOpts{ .variant = ButtonVariant::Chrome,
	                   .size = ButtonSize::Auto,
	                   .minWidth = minWidth },
	       ButtonHandle{ nullptr, action, &interactions });
}

void ButtonRow(silencer::ui::UiInteractionRegistry& interactions,
               bool primaryIsCreate)
{
	Clay_String primaryLabel = primaryIsCreate ? FromCStr("Create") : FromCStr("Next");
	const char * primaryAction = primaryIsCreate ? kActionCreate : kActionNext;
	CLAY({ .id = CLAY_ID("CharacterCreateButtons"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28) },
	           .childGap = 8,
	           .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		ChromeButton(CLAY_STRING("CharacterCreateBackButton"),
		             CLAY_STRING("Back"),
		             kActionBack,
		             interactions);
		ChromeButton(CLAY_STRING("CharacterCreatePrimaryButton"),
		             primaryLabel,
		             primaryAction,
		             interactions,
		             86);
	}
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
		const int visible = kListH / kListLineH;
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
	       },
	       .image = { .imageData = PackImageStretch(7, 1) } }) {
		CLAY({ .id = CLAY_ID("CharacterCreatePanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kPanelW),
		                       CLAY_SIZING_FIXED(kPanelH) },
		           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
		           .childGap = 10,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 2) } }) {
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

void CharacterCreateScreen::BuildSelectAgent(ScreenContext & ctx,
                                             silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	using namespace character_create_screen_detail;

	CLAY({ .id = CLAY_ID("CharacterCreateTitle"),
	       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) } } }) {
		Title(CLAY_STRING("Select Agent"));
	}

	for(int i = 0; i < static_cast<int>(agentRows.size()) && i < kMaxRows; ++i){
		agentItems[static_cast<size_t>(i)] = FromStd(agentRows[static_cast<size_t>(i)]);
	}
	ScrollListOpts listOpts;
	listOpts.width = kListW;
	listOpts.height = kListH;
	listOpts.lineHeight = kListLineH;
	listOpts.highlightColor = 180;
	listOpts.text = TextOpts{ .size = TextSize::Body };
	listOpts.scrollbarBank = kScrollbarBank;

	CLAY({ .id = CLAY_ID("CharacterCreateAgentBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		ScrollList(CLAY_STRING("CharacterCreateAgentList"),
		           agentItems.data(),
		           std::min(static_cast<int>(agentRows.size()), kMaxRows),
		           selectedAgentIndex,
		           agentScroll,
		           listOpts,
		           ScrollListHandle{ nullptr, kActionAgentPrefix, &interactions });
	}

	CLAY({ .id = CLAY_ID("CharacterCreateSelectButtons"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28) },
	           .childGap = 8,
	           .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		ChromeButton(CLAY_STRING("CharacterCreateSelectBack"),
		             CLAY_STRING("Back"),
		             kActionBack,
		             interactions);
		ChromeButton(CLAY_STRING("CharacterCreateSelect"),
		             CLAY_STRING("Select"),
		             kActionSelect,
		             interactions,
		             86);
	}
}

void CharacterCreateScreen::BuildEnterAlias(ScreenContext & ctx,
                                            silencer::ui::UiInteractionRegistry& interactions)
{
	using namespace character_create_screen_detail;
	const bool focused = interactions.IsTextInputFocused(kAliasInputUid);
	const bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

	CLAY({ .id = CLAY_ID("CharacterAliasTitle"),
	       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) } } }) {
		Title(CLAY_STRING("Silencer Alias"));
	}

	CLAY({ .id = CLAY_ID("CharacterAliasBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .childGap = 12,
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		Body(CLAY_STRING("Enter a name for this agent."));
		TextInput(CLAY_STRING("CharacterAliasInput"),
		          alias,
		          TextInputOpts{ .widthPx = 190,
		                         .heightPx = 23,
		                         .textSize = TextSize::Body,
		                         .showCaret = focused && blink,
		                         .contentInsetX = 7 },
		          TextInputHandle{ nullptr,
		                           kActionAlias,
		                           "Alias",
		                           &interactions,
		                           kAliasInputUid,
		                           16,
		                           true });
		if(focusAliasRequested){
			ctx.game.UiInteractions().FocusTextInputByUid(kAliasInputUid);
			focusAliasRequested = false;
		}
	}

	ButtonRow(interactions, false);
}

void CharacterCreateScreen::BuildSelectAgency(ScreenContext & ctx,
                                              silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	using namespace character_create_screen_detail;
	const AgencyDef& agency = SelectedAgency(selectedAgency);
	Clay_String agencyItems[5];
	for(int i = 0; i < 5; ++i){
		agencyItems[i] = FromCStr(kAgencies[i].name);
	}

	CLAY({ .id = CLAY_ID("CharacterAgencyTitle"),
	       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) } } }) {
		Title(CLAY_STRING("Select Agency"));
	}

	CLAY({ .id = CLAY_ID("CharacterAgencyBody"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .childGap = 14,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		ScrollListOpts listOpts;
		listOpts.width = 130;
		listOpts.height = 110;
		listOpts.lineHeight = kListLineH;
		listOpts.highlightColor = 180;
		listOpts.text = TextOpts{ .size = TextSize::Body };
		listOpts.scrollbarBank = kScrollbarBank;

		ScrollList(CLAY_STRING("CharacterAgencyList"),
		           agencyItems,
		           5,
		           AgencyIndex(selectedAgency),
		           0,
		           listOpts,
		           ScrollListHandle{ nullptr, kActionAgencyPrefix, &interactions });

		CLAY({ .id = CLAY_ID("CharacterAgencyInfo"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		           .childGap = 7,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			Body(FromCStr(agency.name));
			Body(FromCStr(agency.advantages));
			Body(FromCStr(agency.description));
		}
	}

	ButtonRow(interactions, true);
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
	if(action.kind == silencer::ui::UiActionKind::Select){
		if(StartsWith(action.id, kActionAgentPrefix)){
			selectedAgentIndex = action.index;
			return true;
		}
		if(StartsWith(action.id, kActionAgencyPrefix)){
			if(action.index >= 0 && action.index < 5){
				selectedAgency = kAgencies[action.index].agency;
			}
			return true;
		}
		return false;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate){
		return false;
	}
	if(action.id == kActionBack){
		return HandleBack(ctx);
	}
	if(action.id == kActionSelect){
		SelectCurrentAgent(ctx);
		return true;
	}
	if(action.id == kActionNext){
		if(step == Step::SelectAgent){
			SelectCurrentAgent(ctx);
		}else if(step == Step::EnterAlias){
			AdvanceAliasStep(ctx);
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
	if(selectedAgentIndex <= 0){
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}

	Uint32 charId = 0;
	ctx.lobby.LockMutex();
	const int charIndex = selectedAgentIndex - 1;
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
	agentRows.push_back("Create New Character");
	for(const Lobby::Character& ch : ctx.lobby.characters){
		agentRows.push_back(ch.name);
	}
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
