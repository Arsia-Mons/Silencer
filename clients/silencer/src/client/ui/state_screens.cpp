#include "client/ui/state_screens.h"

#include "client/ui/ClientUi.h"
#include "character_create_screen.h"
#include "game_state.h"
#include "lobby_connect_screen.h"
#include "main_menu_screen.h"
#include "mission_summary_screen.h"
#include "options_audio_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "options_screen.h"
#include "screen_context.h"
#include "update_screen.h"
#ifdef SILENCER_HAVE_LOBBY_UI
#include "lobby_screen.h"
#endif

#include <memory>

namespace silencer {
namespace client_ui {

void ShowStateScreen(ClientUi & clientUi, ScreenContext & ctx, Uint8 uiState) {
	switch(uiState){
	case GameState::MAINMENU:
		clientUi.PushScreen(std::make_unique<MainMenuScreen>(), ctx);
		break;
	case GameState::LOBBYCONNECT:
		clientUi.PushScreen(std::make_unique<LobbyConnectScreen>(), ctx);
		break;
	case GameState::LOBBY:
#ifdef SILENCER_HAVE_LOBBY_UI
		clientUi.PushScreen(std::make_unique<LobbyScreen>(), ctx);
#endif
		break;
	case GameState::CREATECHARACTER:
		clientUi.PushScreen(std::make_unique<CharacterCreateScreen>(), ctx);
		break;
	case GameState::UPDATING:
		clientUi.PushScreen(std::make_unique<UpdateScreen>(), ctx);
		break;
	case GameState::MISSIONSUMMARY:
		clientUi.PushScreen(std::make_unique<MissionSummaryScreen>(), ctx);
		break;
	case GameState::OPTIONS:
		clientUi.PushScreen(std::make_unique<OptionsScreen>(), ctx);
		break;
	case GameState::OPTIONSCONTROLS:
		clientUi.PushScreen(std::make_unique<OptionsControlsScreen>(), ctx);
		break;
	case GameState::OPTIONSDISPLAY:
		clientUi.PushScreen(std::make_unique<OptionsDisplayScreen>(), ctx);
		break;
	case GameState::OPTIONSAUDIO:
		clientUi.PushScreen(std::make_unique<OptionsAudioScreen>(), ctx);
		break;
	default:
		break;
	}
}

}  // namespace client_ui
}  // namespace silencer
