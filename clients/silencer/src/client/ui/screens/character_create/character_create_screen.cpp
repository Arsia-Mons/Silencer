#include "character_create_screen.h"

#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "lobby_connect_screen.h"
#include "lobby_screen.h"
#include "message_modal.h"
#include "screen_context.h"
#include "game.h"
#include "renderer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
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

int CreateRowIndex(size_t characterCount)
{
	return std::min(static_cast<int>(characterCount), kMaxRows - 1);
}

void ShowMessage(const silencer::client_ui::Navigation & navigation,
                 const char * message)
{
	navigation.push(std::make_unique<MessageModal>(message ? message : ""));
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
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	selectedAgency = lobby.character.agency_for_index(0);
	previewAgencyIndex = -1;
	characterCountOnEntry = lobby.agents.count();
	waitingForCreate = false;
	waitingForRename = false;
	renameCharacterId = 0;
	focusAliasRequested = false;
	alias[0] = '\0';
	RebuildAgentRows(lobby);
}

void CharacterCreateScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	if(waitingForCreate){
		const silencer::client_ui::LobbyAgentCreateStatus status =
			lobby.agents.create_status(characterCountOnEntry);
		if(status.created){
			waitingForCreate = false;
			navigation.reset_to(std::make_unique<LobbyScreen>());
			return;
		}
		if(status.received){
			waitingForCreate = false;
			ShowMessage(navigation, "Could not create character");
		}
	}
	if(waitingForRename){
		const silencer::client_ui::LobbyAgentRenameStatus status =
			lobby.agents.rename_status(renameCharacterId);
		if(status.renamed){
			waitingForRename = false;
			renameCharacterId = 0;
			alias[0] = '\0';
			step = Step::SelectAgent;
			if(status.renamed_index >= 0){
				selectedAgentIndex = status.renamed_index;
				previewAgentIndex = status.renamed_index;
			}
			return;
		}
		if(status.received){
			waitingForRename = false;
			ShowMessage(navigation, "Could not rename character");
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
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	if(lobby.agents.has_any()){
		navigation.reset_to(std::make_unique<LobbyScreen>());
	}else{
		lobby.connection.cancel();
		navigation.reset_to(std::make_unique<LobbyConnectScreen>());
	}
	return true;
}

bool CharacterCreateScreen::HandleUiIntent(ScreenContext & ctx,
                                           const silencer::ui::UiAction & action)
{
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
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
			selectedAgency = lobby.character.agency_for_index(agencyIndex);
			previewAgencyIndex = agencyIndex;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   step == Step::EnterAlias && action.id == kActionAlias){
		CopyAlias(action.value.c_str());
		if(IsRenaming()){
			RenameCurrentAgent(lobby, navigation);
		}else{
			AdvanceAliasStep();
		}
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate){
		return retainedFrame_.HandleUiIntent(action);
	}
	int agentIndex = SuffixInt(action.id, kActionAgentPrefix);
	if(step == Step::SelectAgent && agentIndex >= 0){
		selectedAgentIndex = agentIndex;
		previewAgentIndex = agentIndex;
		SelectCurrentAgent(lobby, navigation);
		return true;
	}
	int renameIndex = SuffixInt(action.id, kActionRenamePrefix);
	if(step == Step::SelectAgent && renameIndex >= 0){
		StartRenameAgent(lobby, renameIndex);
		return true;
	}
	int agencyIndex = SuffixInt(action.id, kActionAgencyPrefix);
	if(step == Step::SelectAgency && agencyIndex >= 0){
		if(waitingForCreate) return true;
		if(agencyIndex < 5){
			selectedAgency = lobby.character.agency_for_index(agencyIndex);
			previewAgencyIndex = agencyIndex;
			if(alias[0] != '\0'){
				CreateCurrentAgent(lobby, navigation);
			}
		}
		return true;
	}
	if(step == Step::SelectAgency && action.id == kActionCreate){
		if(waitingForCreate) return true;
		CreateCurrentAgent(lobby, navigation);
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

void CharacterCreateScreen::SelectCurrentAgent(
	const silencer::client_ui::LobbyModel & lobby,
	const silencer::client_ui::Navigation & navigation)
{
	const size_t agentCount = lobby.agents.count();
	const int createIndex = CreateRowIndex(agentCount);
	if(selectedAgentIndex == createIndex ||
	   selectedAgentIndex >= static_cast<int>(agentCount)){
		renameCharacterId = 0;
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}

	if(lobby.agents.select(selectedAgentIndex)){
		navigation.reset_to(std::make_unique<LobbyScreen>());
	}
}

void CharacterCreateScreen::CreateCurrentAgent(
	const silencer::client_ui::LobbyModel & lobby,
	const silencer::client_ui::Navigation & navigation)
{
	if(waitingForCreate){
		return;
	}
	if(alias[0] == '\0'){
		ShowMessage(navigation, "Enter an alias");
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}
	characterCountOnEntry = lobby.agents.count();
	lobby.agents.create(alias, selectedAgency);
	waitingForCreate = true;
}

void CharacterCreateScreen::StartRenameAgent(
	const silencer::client_ui::LobbyModel & lobby,
	int agentIndex)
{
	if(waitingForRename){
		return;
	}
	silencer::client_ui::LobbyAgentSummary agent;
	if(!lobby.agents.rename_start(agentIndex, agent)){
		return;
	}
	selectedAgentIndex = agentIndex;
	previewAgentIndex = agentIndex;
	renameCharacterId = agent.id;
	CopyAlias(agent.name.c_str());
	step = Step::EnterAlias;
	focusAliasRequested = true;
}

void CharacterCreateScreen::RenameCurrentAgent(
	const silencer::client_ui::LobbyModel & lobby,
	const silencer::client_ui::Navigation & navigation)
{
	if(waitingForRename){
		return;
	}
	if(alias[0] == '\0'){
		ShowMessage(navigation, "Enter an alias");
		focusAliasRequested = true;
		return;
	}
	if(renameCharacterId == 0){
		return;
	}
	lobby.agents.rename(renameCharacterId, alias);
	waitingForRename = true;
}

void CharacterCreateScreen::RebuildAgentRows(
	const silencer::client_ui::LobbyModel & lobby)
{
	agents = lobby.agents.list();
	agentRows.clear();
	for(const silencer::client_ui::LobbyAgentSummary& agent : agents){
		agentRows.push_back(agent.name);
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

void CharacterCreateScreen::CopyAlias(const char * value)
{
	const char * src = value ? value : "";
	size_t n = std::strlen(src);
	if(n > sizeof(alias) - 1) n = sizeof(alias) - 1;
	std::memcpy(alias, src, n);
	alias[n] = '\0';
}

void CharacterCreateScreen::AdvanceAliasStep()
{
	if(alias[0] == '\0'){
		return;
	}
	step = Step::SelectAgency;
	previewAgencyIndex = -1;
}
