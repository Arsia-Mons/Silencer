#include "character_create_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "lobby.h"
#include "renderer.h"
#include "team.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int kMaxRows = 32;
constexpr const char * kActionAgentPrefix = "character_create.agent";
constexpr const char * kActionAgencyPrefix = "character_create.agency";
constexpr const char * kActionRenamePrefix = "character_create.rename";
constexpr const char * kActionCreate = "character_create.create";
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

int CreateRowIndex(size_t characterCount)
{
	return std::min(static_cast<int>(characterCount), kMaxRows - 1);
}

}  // namespace

void CharacterCreateScreen::Build(ScreenContext & ctx)
{
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
			focusAliasRequested = true;
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
		previewAgencyIndex = -1;
		focusAliasRequested = true;
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
	if(action.kind != silencer::ui::UiActionKind::Activate){
		return false;
	}
	int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
	if(step == Step::SelectAgent && agentIndex >= 0){
		selectedAgentIndex = agentIndex;
		previewAgentIndex = agentIndex;
		SelectCurrentAgent(ctx);
		return true;
	}
	int renameIndex = SuffixInt(action.id, kActionRenamePrefix);
	if(step == Step::SelectAgent && renameIndex >= 0){
		StartRenameAgent(ctx, renameIndex);
		return true;
	}
	int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
	if(step == Step::SelectAgency && agencyIndex >= 0){
		if(waitingForCreate) return true;
		if(agencyIndex < 5){
			selectedAgency = AgencyForIndex(agencyIndex);
			previewAgencyIndex = agencyIndex;
			if(alias[0] != '\0'){
				CreateCurrentAgent(ctx);
			}
		}
		return true;
	}
	if(step == Step::SelectAgency && action.id == kActionCreate){
		if(waitingForCreate) return true;
		CreateCurrentAgent(ctx);
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
	if(waitingForCreate){
		return;
	}
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
	focusAliasRequested = true;
}

void CharacterCreateScreen::RenameCurrentAgent(ScreenContext & ctx)
{
	if(waitingForRename){
		return;
	}
	if(alias[0] == '\0'){
		ctx.ShowMessage("Enter an alias");
		focusAliasRequested = true;
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
