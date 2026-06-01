#include "character_create_screen.h"

#include "client/ui/screens/character_create/character_create_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "lobby.h"
#include "peer.h"
#include "renderer.h"
#include "team.h"
#include "world.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int kMaxRows = 32;
constexpr const char * kActionAgentPrefix = "character_create.agent";
constexpr const char * kActionAgencyPrefix = "character_create.agency";
constexpr const char * kActionAlias = "character_create.alias";

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

Uint8 AgencyForIndex(int index)
{
	static const Uint8 agencies[5] = {
		Team::NOXIS,
		Team::LAZARUS,
		Team::CALIBER,
		Team::STATIC,
		Team::BLACKROSE,
	};
	if(index < 0 || index >= 5) return agencies[0];
	return agencies[index];
}

int AgencyIndex(Uint8 agency)
{
	static const Uint8 agencies[5] = {
		Team::NOXIS,
		Team::LAZARUS,
		Team::CALIBER,
		Team::STATIC,
		Team::BLACKROSE,
	};
	for(int i = 0; i < 5; i++){
		if(agencies[i] == agency) return i;
	}
	return 0;
}

int CreateRowIndex(size_t characterCount)
{
	return std::min(static_cast<int>(characterCount), kMaxRows - 1);
}

}  // namespace

void CharacterCreateScreen::Build(ScreenContext & ctx)
{
	ctx.world.GetAuthorityPeer()->controlledlist.clear();
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	step = Step::SelectAgent;
	selectedAgentIndex = 0;
	previewAgentIndex = -1;
	agentScroll = 0;
	agentScrollDelta = 0;
	selectedAgency = Team::NOXIS;
	previewAgencyIndex = -1;
	characterCountOnEntry = ctx.lobby.characters.size();
	waitingForCreate = false;
	waitingForRename = false;
	renameCharacterId = 0;
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
	if(waitingForRename){
		bool received = false;
		bool renamed = false;
		int renamedIndex = -1;
		ctx.lobby.LockMutex();
		received = ctx.lobby.charactersreceived;
		for(size_t i = 0; i < ctx.lobby.characters.size(); ++i){
			const Lobby::Character& ch = ctx.lobby.characters[i];
			if(ch.id == renameCharacterId){
				renamed = !ch.renameAvailable;
				renamedIndex = static_cast<int>(i);
				break;
			}
		}
		ctx.lobby.UnlockMutex();
		if(renamed){
			waitingForRename = false;
			renameCharacterId = 0;
			alias[0] = '\0';
			step = Step::SelectAgent;
			if(renamedIndex >= 0){
				selectedAgentIndex = renamedIndex;
				previewAgentIndex = renamedIndex;
			}
			return;
		}
		if(received){
			waitingForRename = false;
			ctx.ShowMessage("Could not rename character");
		}
	}
}

bool CharacterCreateScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	RebuildAgentRows(ctx);
	if(agentScrollDelta != 0){
		int next = static_cast<int>(agentScroll) + agentScrollDelta;
		const int visible = silencer::client_ui::kCharacterCreateVisibleAgentRows;
		const int maxScroll = std::max(0, static_cast<int>(agentRows.size()) - visible);
		if(next < 0) next = 0;
		if(next > maxScroll) next = maxScroll;
		agentScroll = static_cast<Uint16>(next);
		agentScrollDelta = 0;
	}

	silencer::client_ui::CharacterCreateViewStep viewStep =
		silencer::client_ui::CharacterCreateViewStep::SelectAgent;
	if(step == Step::EnterAlias){
		viewStep = silencer::client_ui::CharacterCreateViewStep::EnterAlias;
	}else if(step == Step::SelectAgency){
		viewStep = silencer::client_ui::CharacterCreateViewStep::SelectAgency;
	}

	int detailAgentIndex = previewAgentIndex >= 0
		? previewAgentIndex
		: selectedAgentIndex;
	int detailAgencyIndex = AgencyIndex(selectedAgency);
	bool detailRenameAvailable = false;
	detailStats.fill("");
	if(detailAgentIndex >= 0 &&
	   detailAgentIndex < static_cast<int>(ctx.lobby.characters.size())){
		const Lobby::Character& ch =
			ctx.lobby.characters[static_cast<size_t>(detailAgentIndex)];
		detailAgencyIndex = AgencyIndex(ch.agencyIdx);
		detailRenameAvailable = ch.renameAvailable;
		detailStats[0] = "Security Level " + std::to_string(ch.stats.level);
		detailStats[1] = "Technology Slots: " + std::to_string(ch.stats.techslots);
		detailStats[2] = "Agency Points: " + std::to_string(ch.stats.xp);
		detailStats[3] = "Successful Missions: " + std::to_string(ch.stats.wins);
	}else{
		detailAgentIndex = -1;
	}
	std::array<const char *, silencer::client_ui::kCharacterCreateStatLineCount> stats = {};
	for(int i = 0; i < silencer::client_ui::kCharacterCreateStatLineCount; i++){
		stats[i] = detailStats[i].c_str();
	}

	silencer::client_ui::CharacterCreateContextValue context{
		.state = {
			.step = viewStep,
			.agent_rows = &agentRows,
			.agent_scroll = static_cast<int>(agentScroll),
			.selected_agent_index = selectedAgentIndex,
			.preview_agent_index = previewAgentIndex,
			.detail_agent_index = detailAgentIndex,
			.detail_agency_index = detailAgencyIndex,
			.detail_rename_available = detailRenameAvailable,
			.detail_stats = stats,
			.alias = alias,
			.alias_renaming = IsRenaming(),
			.waiting = waitingForCreate || waitingForRename,
			.selected_agency_index = AgencyIndex(selectedAgency),
			.preview_agency_index = previewAgencyIndex,
		},
		.actions = {
			.preview_agent = [this](int index) {
				previewAgentIndex = index;
			},
			.activate_agent = [this, screenContext = &ctx](int index) {
				ActivateAgent(*screenContext, index);
			},
			.rename_agent = [this, screenContext = &ctx](int index) {
				StartRenameAgent(*screenContext, index);
			},
			.set_alias = [this](const std::string& value) {
				CopyAlias(value);
			},
			.submit_alias = [this, screenContext = &ctx]() {
				SubmitAlias(*screenContext);
			},
			.preview_agency = [this](int index) {
				previewAgencyIndex = index;
			},
			.activate_agency = [this, screenContext = &ctx](int index) {
				ActivateAgency(*screenContext, index);
			},
		},
	};
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = silencer::client_ui::CharacterCreateScreenView(
		silencer::client_ui::CharacterCreateScreenViewProps{
			.key = "character-create",
			.value = stored,
		});
	return true;
}

void CharacterCreateScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool CharacterCreateScreen::HandleBack(ScreenContext & ctx)
{
	if(step == Step::SelectAgency){
		step = Step::EnterAlias;
		previewAgencyIndex = -1;
		return true;
	}
	if(step == Step::EnterAlias){
		if(waitingForRename){
			return true;
		}
		if(IsRenaming()){
			renameCharacterId = 0;
			alias[0] = '\0';
		}
		step = Step::SelectAgent;
		previewAgentIndex = -1;
		return true;
	}
	if(!ctx.lobby.characters.empty()){
		ctx.GoToState(GameState::LOBBY);
	}else{
		ctx.lobby.Disconnect();
		ctx.GoToState(GameState::LOBBYCONNECT);
	}
	return true;
}

bool CharacterCreateScreen::HandleUiIntent(ScreenContext & ctx,
                                           const silencer::ui::UiAction & action)
{
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
	if(action.kind == silencer::ui::UiActionKind::Navigate){
		int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
		if(step == Step::SelectAgent && agentIndex >= 0){
			previewAgentIndex = agentIndex;
			return true;
		}
		int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
		if(step == Step::SelectAgency && agencyIndex >= 0 && agencyIndex < 5){
			previewAgencyIndex = agencyIndex;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select){
		int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
		if(step == Step::SelectAgent && agentIndex >= 0){
			selectedAgentIndex = agentIndex;
			previewAgentIndex = agentIndex;
			return true;
		}
		int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
		if(step == Step::SelectAgency && agencyIndex >= 0 && agencyIndex < 5){
			if(waitingForCreate) return true;
			selectedAgency = AgencyForIndex(agencyIndex);
			previewAgencyIndex = agencyIndex;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::SetText &&
	   step == Step::EnterAlias && action.id == kActionAlias){
		CopyAlias(action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   step == Step::EnterAlias && action.id == kActionAlias){
		CopyAlias(action.value);
		if(IsRenaming()){
			RenameCurrentAgent(ctx);
		}else{
			AdvanceAliasStep(ctx);
		}
		return true;
	}
	return false;
}

void CharacterCreateScreen::SelectCurrentAgent(ScreenContext & ctx)
{
	const int createIndex = CreateRowIndex(ctx.lobby.characters.size());
	if(selectedAgentIndex == createIndex ||
	   selectedAgentIndex >= static_cast<int>(ctx.lobby.characters.size())){
		renameCharacterId = 0;
		step = Step::EnterAlias;
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

void CharacterCreateScreen::ActivateAgent(ScreenContext & ctx, int agentIndex)
{
	selectedAgentIndex = agentIndex;
	previewAgentIndex = agentIndex;
	SelectCurrentAgent(ctx);
}

void CharacterCreateScreen::CreateCurrentAgent(ScreenContext & ctx)
{
	if(waitingForCreate){
		return;
	}
	if(alias[0] == '\0'){
		ctx.ShowMessage("Enter an alias");
		step = Step::EnterAlias;
		return;
	}
	ctx.lobby.LockMutex();
	characterCountOnEntry = ctx.lobby.characters.size();
	ctx.lobby.charactersreceived = false;
	ctx.lobby.CreateCharacter(alias, selectedAgency);
	ctx.lobby.UnlockMutex();
	waitingForCreate = true;
}

void CharacterCreateScreen::ActivateAgency(ScreenContext & ctx, int agencyIndex)
{
	if(waitingForCreate){
		return;
	}
	selectedAgency = AgencyForIndex(agencyIndex);
	previewAgencyIndex = agencyIndex;
	if(alias[0] != '\0'){
		CreateCurrentAgent(ctx);
	}
}

void CharacterCreateScreen::StartRenameAgent(ScreenContext & ctx, int agentIndex)
{
	if(waitingForRename){
		return;
	}
	Uint32 charID = 0;
	char currentName[17] = {};
	ctx.lobby.LockMutex();
	if(agentIndex >= 0 && agentIndex < static_cast<int>(ctx.lobby.characters.size())){
		const Lobby::Character& ch = ctx.lobby.characters[static_cast<size_t>(agentIndex)];
		if(ch.renameAvailable){
			charID = ch.id;
			std::strncpy(currentName, ch.name, sizeof(currentName) - 1);
			selectedAgentIndex = agentIndex;
			previewAgentIndex = agentIndex;
		}
	}
	ctx.lobby.UnlockMutex();
	if(charID == 0){
		return;
	}
	renameCharacterId = charID;
	CopyAlias(currentName);
	step = Step::EnterAlias;
}

void CharacterCreateScreen::RenameCurrentAgent(ScreenContext & ctx)
{
	if(waitingForRename){
		return;
	}
	if(alias[0] == '\0'){
		ctx.ShowMessage("Enter an alias");
		return;
	}
	if(renameCharacterId == 0){
		return;
	}
	ctx.lobby.LockMutex();
	ctx.lobby.charactersreceived = false;
	ctx.lobby.RenameCharacter(renameCharacterId, alias);
	ctx.lobby.UnlockMutex();
	waitingForRename = true;
}

void CharacterCreateScreen::SubmitAlias(ScreenContext & ctx)
{
	if(IsRenaming()){
		RenameCurrentAgent(ctx);
	}else{
		AdvanceAliasStep(ctx);
	}
}

void CharacterCreateScreen::RebuildAgentRows(ScreenContext & ctx)
{
	agentRows.clear();
	for(const Lobby::Character& ch : ctx.lobby.characters){
		agentRows.push_back(ch.name);
	}
	agentRows.push_back("Create New Character");
	const int maxIndex = std::max(0, std::min(static_cast<int>(agentRows.size()), kMaxRows) - 1);
	if(selectedAgentIndex > maxIndex){
		selectedAgentIndex = maxIndex;
	}
	if(previewAgentIndex > maxIndex){
		previewAgentIndex = -1;
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
	previewAgencyIndex = -1;
}
