#include "game_create_panel.h"

#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "screen_context.h"
#include "message_modal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace silencer::client_ui::lobby {

namespace game_create_panel_detail {

constexpr const char * kActionSecurity = "lobby.game_create.security";
constexpr const char * kActionSpectatable = "lobby.game_create.spectatable";
constexpr const char * kActionCreate = "lobby.game_create.create";
constexpr const char * kActionMapPrefix = "lobby.game_create.map";
constexpr const char * kActionMinLevel = "lobby.game_create.min_level";
constexpr const char * kActionMaxLevel = "lobby.game_create.max_level";
constexpr const char * kActionMaxPlayers = "lobby.game_create.max_players";
constexpr const char * kActionMaxTeams = "lobby.game_create.max_teams";
constexpr const char * kActionName = "lobby.game_create.name";
constexpr const char * kActionPassword = "lobby.game_create.password";
constexpr const char * kActionOptionsScroll = kGameCreateOptionsScrollId;

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

}  // namespace game_create_panel_detail

static void DismissProgressModal(GameCreatePanelState & state,
                                 ScreenContext & ctx) {
	if(!state.progressModal) return;
	silencer::client_ui::use_navigation().pop_top();
	state.progressModal = nullptr;
}

void GameCreatePanelInit(GameCreatePanelState & state, ScreenContext & ctx) {
	state = GameCreatePanelState{};
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	const LobbyCreateModel::Defaults defaults = lobby.create.defaults();
	state.spectatable = defaults.spectatable;
	std::strncpy(state.name, defaults.game_name.c_str(), sizeof(state.name) - 1);
	state.name[sizeof(state.name) - 1] = '\0';
	state.maps = defaults.maps;
	lobby.create.reset();
}

void GameCreatePanelTick(GameCreatePanelState & state,
                         ScreenContext & ctx,
                         LobbyModel & lobby) {
	if(state.mapRowClickedIndex >= 0){
		state.mapSelectedIndex = state.mapRowClickedIndex;
		lobby.create.select_map(state.mapRowClickedIndex);
		state.mapRowClickedIndex = -1;
	}
	if(state.securityClicked){
		state.securityClicked = false;
		state.securityIndex = static_cast<Uint8>((state.securityIndex + 1) % 4);
	}
	if(state.spectatableClicked){
		state.spectatableClicked = false;
		state.spectatable = !state.spectatable;
		lobby.create.set_spectatable(state.spectatable);
	}

	const LobbyCreateModel::PumpResult pump = lobby.create.pump();
	if(pump.dismiss_progress){
		DismissProgressModal(state, ctx);
		if(!pump.message.empty()){
			lobby.modal.show_message(pump.message.c_str());
		}
	}

	if(!state.createClicked) return;
	state.createClicked = false;

	LobbyCreateModel::Request request;
	request.game_name = state.name;
	request.password = state.password;
	if(state.mapSelectedIndex >= 0 && state.mapSelectedIndex < (int)state.maps.size()){
		request.map_name = state.maps[state.mapSelectedIndex];
	}
	request.security_index = state.securityIndex;
	Uint8 maxplayers = static_cast<Uint8>(atoi(state.maxPlayers)); if(maxplayers <= 0) maxplayers = 1;
	Uint8 maxteams   = static_cast<Uint8>(atoi(state.maxTeams));   if(maxteams   <= 0) maxteams   = 1;
	request.min_level = static_cast<Uint8>(atoi(state.minLevel));
	request.max_level = static_cast<Uint8>(atoi(state.maxLevel));
	request.max_players = maxplayers;
	request.max_teams = maxteams;
	request.spectatable = state.spectatable;

	const LobbyCreateModel::StartResult started = lobby.create.start(request);
	if(!started.message.empty()){
		lobby.modal.show_message(started.message.c_str());
		return;
	}
	if(!started.started) return;
	std::unique_ptr<MessageModal> progress = MessageModal::Progress("Uploading map...");
	state.progressModal = progress.get();
	silencer::client_ui::use_navigation().push(std::move(progress));
}

bool GameCreatePanelHandleUiIntent(GameCreatePanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || action.id == game_create_panel_detail::kActionOptionsScroll){
			state.optionsScrollDelta += action.amount;
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == game_create_panel_detail::kActionMinLevel){
			game_create_panel_detail::CopyUiText(state.minLevel, static_cast<int>(sizeof(state.minLevel)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxLevel){
			game_create_panel_detail::CopyUiText(state.maxLevel, static_cast<int>(sizeof(state.maxLevel)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxPlayers){
			game_create_panel_detail::CopyUiText(state.maxPlayers, static_cast<int>(sizeof(state.maxPlayers)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxTeams){
			game_create_panel_detail::CopyUiText(state.maxTeams, static_cast<int>(sizeof(state.maxTeams)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionName){
			game_create_panel_detail::CopyUiText(state.name, static_cast<int>(sizeof(state.name)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionPassword){
			game_create_panel_detail::CopyUiText(state.password, static_cast<int>(sizeof(state.password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == game_create_panel_detail::kActionSecurity){
			state.securityClicked = true;
			return true;
		}
		if(action.id == game_create_panel_detail::kActionSpectatable){
			state.spectatableClicked = true;
			return true;
		}
		if(action.id == game_create_panel_detail::kActionCreate){
			state.createClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   game_create_panel_detail::StartsWith(action.id, game_create_panel_detail::kActionMapPrefix)){
		state.mapRowClickedIndex = action.index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
