#include "client/ui/hooks/use_game_navigation.h"

#include "client/ui/deferred_ui_mutation.h"
#include "client/ui/screens/screen_context.h"
#include "game_state.h"

namespace silencer {
namespace client_ui {

namespace {

Uint8 game_state_for_destination(ScreenDestination destination) {
	switch(destination){
		case ScreenDestination::SinglePlayerGame:
			return GameState::SINGLEPLAYERGAME;
		case ScreenDestination::LobbyConnect:
			return GameState::LOBBYCONNECT;
		case ScreenDestination::Lobby:
			return GameState::LOBBY;
		case ScreenDestination::CreateCharacter:
			return GameState::CREATECHARACTER;
		case ScreenDestination::Updating:
			return GameState::UPDATING;
		case ScreenDestination::MissionSummary:
			return GameState::MISSIONSUMMARY;
		case ScreenDestination::Options:
			return GameState::OPTIONS;
		case ScreenDestination::OptionsControls:
			return GameState::OPTIONSCONTROLS;
		case ScreenDestination::OptionsDisplay:
			return GameState::OPTIONSDISPLAY;
		case ScreenDestination::OptionsAudio:
			return GameState::OPTIONSAUDIO;
		case ScreenDestination::MainMenu:
		default:
			return GameState::MAINMENU;
	}
}

}  // namespace

GameNavigation use_game_navigation() {
	internal::DeferredUiMutationSink mutations = internal::use_deferred_ui_mutations();
	if(!mutations) return {};
	return GameNavigation{
		.go_to = [mutations](ScreenDestination destination) {
			mutations.submit([destination](ScreenContext& ctx) {
				ctx.GoToState(game_state_for_destination(destination));
			});
		},
		.go_back = [mutations]() {
			mutations.submit([](ScreenContext& ctx) {
				ctx.GoBack();
			});
		},
		.request_quit = [mutations]() {
			mutations.submit([](ScreenContext& ctx) {
				ctx.RequestQuit();
			});
		},
	};
}

}  // namespace client_ui
}  // namespace silencer
