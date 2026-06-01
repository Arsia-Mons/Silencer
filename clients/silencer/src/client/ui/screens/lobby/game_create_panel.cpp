#include "game_create_panel.h"

#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "message_modal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace silencer::client_ui::lobby {

namespace game_create_panel_detail {
void CopyUiText(char * dst, int dstLen, const char * value)
{
	if(!dst || dstLen <= 0) return;
	const char * src = value ? value : "";
	int n = static_cast<int>(std::strlen(src));
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, src, n);
	dst[n] = '\0';
}

}  // namespace game_create_panel_detail

static void DismissProgressModal(GameCreatePanelState & state) {
	if(!state.progressModal) return;
	silencer::client_ui::use_navigation().pop_top();
	state.progressModal = nullptr;
}

void GameCreatePanelInit(GameCreatePanelState & state,
                         const LobbyModel & lobby) {
	state = GameCreatePanelState{};
	const LobbyCreateModel::Defaults defaults = lobby.create.defaults();
	state.spectatable = defaults.spectatable;
	std::strncpy(state.name, defaults.game_name.c_str(), sizeof(state.name) - 1);
	state.name[sizeof(state.name) - 1] = '\0';
	state.maps = defaults.maps;
	lobby.create.reset();
}

void GameCreatePanelCycleSecurity(GameCreatePanelState & state) {
	state.securityIndex = static_cast<Uint8>((state.securityIndex + 1) % 4);
}

void GameCreatePanelToggleSpectatable(GameCreatePanelState & state,
                                      const LobbyModel & lobby) {
	state.spectatable = !state.spectatable;
	lobby.create.set_spectatable(state.spectatable);
}

void GameCreatePanelSelectMap(GameCreatePanelState & state,
                              const LobbyModel & lobby,
                              int index) {
	if(index < 0 || index >= static_cast<int>(state.maps.size())) return;
	state.mapSelectedIndex = index;
	lobby.create.select_map(index);
}

void GameCreatePanelSubmit(GameCreatePanelState & state,
                           const LobbyModel & lobby) {
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

void GameCreatePanelSetText(GameCreatePanelState & state,
                            GameCreatePanelTextField field,
                            const char * value) {
	switch(field){
		case GameCreatePanelTextField::MinLevel:
			game_create_panel_detail::CopyUiText(
				state.minLevel, static_cast<int>(sizeof(state.minLevel)), value);
			break;
		case GameCreatePanelTextField::MaxLevel:
			game_create_panel_detail::CopyUiText(
				state.maxLevel, static_cast<int>(sizeof(state.maxLevel)), value);
			break;
		case GameCreatePanelTextField::MaxPlayers:
			game_create_panel_detail::CopyUiText(
				state.maxPlayers, static_cast<int>(sizeof(state.maxPlayers)), value);
			break;
		case GameCreatePanelTextField::MaxTeams:
			game_create_panel_detail::CopyUiText(
				state.maxTeams, static_cast<int>(sizeof(state.maxTeams)), value);
			break;
		case GameCreatePanelTextField::Name:
			game_create_panel_detail::CopyUiText(
				state.name, static_cast<int>(sizeof(state.name)), value);
			break;
		case GameCreatePanelTextField::Password:
			game_create_panel_detail::CopyUiText(
				state.password, static_cast<int>(sizeof(state.password)), value);
			break;
	}
}

void GameCreatePanelScrollOptions(GameCreatePanelState & state, int amount) {
	state.optionsScrollDelta += amount;
}

void GameCreatePanelTick(GameCreatePanelState & state,
                         LobbyModel & lobby) {
	const LobbyCreateModel::PumpResult pump = lobby.create.pump();
	if(pump.dismiss_progress){
		DismissProgressModal(state);
		if(!pump.message.empty()){
			lobby.modal.show_message(pump.message.c_str());
		}
	}
}

}  // namespace silencer::client_ui::lobby
