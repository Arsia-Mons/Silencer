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

silencer::client_ui::Navigation Nav(ScreenContext & ctx)
{
	(void)ctx;
	return silencer::client_ui::use_navigation();
}

silencer::client_ui::LobbyModel UseLobby(ScreenContext & ctx)
{
	return silencer::client_ui::use_lobby(
		silencer::client_ui::MakeLobbyProvider(ctx));
}

Uint8 AgencyForIndex(ScreenContext & ctx, int index)
{
	return UseLobby(ctx).character.agency_for_index(index);
}

void ShowMessage(ScreenContext & ctx, const char * message)
{
	Nav(ctx).push(std::make_unique<MessageModal>(message ? message : ""));
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
	selectedAgency = AgencyForIndex(ctx, 0);
	previewAgencyIndex = -1;
	characterCountOnEntry = UseLobby(ctx).agents.count();
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
		const silencer::client_ui::LobbyAgentCreateStatus status =
			UseLobby(ctx).agents.create_status(characterCountOnEntry);
		if(status.created){
			waitingForCreate = false;
			Nav(ctx).reset_to(std::make_unique<LobbyScreen>());
			return;
		}
		if(status.received){
			waitingForCreate = false;
			ShowMessage(ctx, "Could not create character");
		}
	}
	if(waitingForRename){
		const silencer::client_ui::LobbyAgentRenameStatus status =
			UseLobby(ctx).agents.rename_status(renameCharacterId);
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
			ShowMessage(ctx, "Could not rename character");
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
	silencer::client_ui::LobbyModel lobby = UseLobby(ctx);
	if(lobby.agents.has_any()){
		Nav(ctx).reset_to(std::make_unique<LobbyScreen>());
	}else{
		lobby.connection.cancel();
		Nav(ctx).reset_to(std::make_unique<LobbyConnectScreen>());
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
			selectedAgency = AgencyForIndex(ctx, agencyIndex);
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
			selectedAgency = AgencyForIndex(ctx, agencyIndex);
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
	silencer::client_ui::LobbyModel lobby = UseLobby(ctx);
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
		Nav(ctx).reset_to(std::make_unique<LobbyScreen>());
	}
}

void CharacterCreateScreen::CreateCurrentAgent(ScreenContext & ctx)
{
	if(waitingForCreate){
		return;
	}
	if(alias[0] == '\0'){
		ShowMessage(ctx, "Enter an alias");
		step = Step::EnterAlias;
		focusAliasRequested = true;
		return;
	}
	silencer::client_ui::LobbyModel lobby = UseLobby(ctx);
	characterCountOnEntry = lobby.agents.count();
	lobby.agents.create(alias, selectedAgency);
	waitingForCreate = true;
}

void CharacterCreateScreen::StartRenameAgent(ScreenContext & ctx, int agentIndex)
{
	if(waitingForRename){
		return;
	}
	silencer::client_ui::LobbyAgentSummary agent;
	if(!UseLobby(ctx).agents.rename_start(agentIndex, agent)){
		return;
	}
	selectedAgentIndex = agentIndex;
	previewAgentIndex = agentIndex;
	renameCharacterId = agent.id;
	CopyAlias(agent.name);
	step = Step::EnterAlias;
	focusAliasRequested = true;
}

void CharacterCreateScreen::RenameCurrentAgent(ScreenContext & ctx)
{
	if(waitingForRename){
		return;
	}
	if(alias[0] == '\0'){
		ShowMessage(ctx, "Enter an alias");
		focusAliasRequested = true;
		return;
	}
	if(renameCharacterId == 0){
		return;
	}
	UseLobby(ctx).agents.rename(renameCharacterId, alias);
	waitingForRename = true;
}

void CharacterCreateScreen::RebuildAgentRows(ScreenContext & ctx)
{
	agents = UseLobby(ctx).agents.list();
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
