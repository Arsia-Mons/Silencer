#include "game_create_panel.h"

#include "screen_context.h"
#include "screen.h"
#include "message_modal.h"

#include <cstdlib>
#include <cstring>
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

template <typename Text>
bool StartsWith(const Text & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

template <typename Text>
void CopyUiText(char * dst, int dstLen, const Text & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void BuildMapList(GameCreatePanelState & state, ScreenContext & ctx) {
	state.maps = ctx.CreateGameMapLabels();
}

}  // namespace game_create_panel_detail

void GameCreatePanelInit(GameCreatePanelState & state, ScreenContext & ctx) {
	state = GameCreatePanelState{};
	ScreenContext::CreateGameDefaults defaults = ctx.CurrentCreateGameDefaults();
	state.spectatable = defaults.spectatable;
	std::strncpy(state.name, defaults.name.c_str(), sizeof(state.name) - 1);
	state.name[sizeof(state.name) - 1] = '\0';
	game_create_panel_detail::BuildMapList(state, ctx);
	ctx.SelectCreateGameMap(-1);
	ctx.SetCreateGamePending(false);
}

void GameCreatePanelTick(GameCreatePanelState & state,
                         ScreenContext & ctx) {
	if(state.mapRowClickedIndex >= 0){
		state.mapSelectedIndex = state.mapRowClickedIndex;
		ctx.SelectCreateGameMap(state.mapRowClickedIndex);
		state.mapRowClickedIndex = -1;
	}
	if(state.securityClicked){
		state.securityClicked = false;
		state.securityIndex = static_cast<Uint8>((state.securityIndex + 1) % 4);
	}
	if(state.spectatableClicked){
		state.spectatableClicked = false;
		state.spectatable = !state.spectatable;
		ctx.SetCreateGameSpectatableDefault(state.spectatable);
	}

	ScreenContext::CreateGameMapUploadResult uploadResult =
		ctx.ConsumeCreateGameMapUploadResult();
	if(uploadResult == ScreenContext::CreateGameMapUploadResult::Failed){
		ctx.SetCreateGamePending(false);
		Screen * top = ctx.TopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not upload map");
	}
	ScreenContext::CreateLobbyGameResult createGameResult =
		ctx.ConsumeCreateLobbyGameResult();
	if(createGameResult == ScreenContext::CreateLobbyGameResult::Failed){
		Screen * top = ctx.TopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not create game");
	}

	if(!state.createClicked) return;
	state.createClicked = false;
	if(ctx.IsCreateGamePending()) return;

	if(strlen(state.name) == 0){ ctx.ShowMessage("No game name"); return; }
	if(state.mapSelectedIndex < 0 || state.mapSelectedIndex >= (int)state.maps.size()){
		ctx.ShowMessage("No map selected"); return;
	}
	std::string mapname = state.maps[state.mapSelectedIndex];
	if(ctx.IsServerMapLabel(mapname)){
		ctx.ShowMessage("Download the map first"); return;
	}

	Uint8 maxplayers = static_cast<Uint8>(atoi(state.maxPlayers)); if(maxplayers <= 0) maxplayers = 1;
	Uint8 maxteams   = static_cast<Uint8>(atoi(state.maxTeams));   if(maxteams   <= 0) maxteams   = 1;

	ctx.BeginCreateGameMapUpload(state.name,
	                             mapname,
	                             state.password,
	                             state.securityIndex,
	                             static_cast<Uint8>(atoi(state.minLevel)),
	                             static_cast<Uint8>(atoi(state.maxLevel)),
	                             maxplayers,
	                             maxteams,
	                             state.spectatable);
	ctx.StartCreateGameRequest();
	ctx.SaveDefaultCreateGameName(state.name);
	ctx.PushScreen(MessageModal::Progress("Uploading map..."));
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
