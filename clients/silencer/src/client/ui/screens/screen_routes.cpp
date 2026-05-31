#include "client/ui/screens/screen_routes.h"

#include "character_create/character_create_screen.h"
#include "game_state.h"
#include "lobby_connect/lobby_connect_screen.h"
#include "main_menu/main_menu_screen.h"
#include "mission_summary/mission_summary_screen.h"
#include "options/options_audio_screen.h"
#include "options/options_controls_screen.h"
#include "options/options_display_screen.h"
#include "options/options_screen.h"
#include "update/update_screen.h"
#ifdef SILENCER_HAVE_LOBBY_UI
#include "lobby/lobby_screen.h"
#endif

namespace silencer {
namespace client_ui {

bool IsScreenState(Uint8 state) {
	using namespace GameState;
	switch(state){
	case MAINMENU:
	case LOBBYCONNECT:
	case LOBBY:
	case CREATECHARACTER:
	case UPDATING:
	case MISSIONSUMMARY:
	case OPTIONS:
	case OPTIONSCONTROLS:
	case OPTIONSDISPLAY:
	case OPTIONSAUDIO:
		return true;
	default:
		return false;
	}
}

bool ScreenStatePlaysMenuMusic(Uint8 state) {
	using namespace GameState;
	switch(state){
	case MAINMENU:
	case LOBBYCONNECT:
	case LOBBY:
	case CREATECHARACTER:
	case UPDATING:
	case MISSIONSUMMARY:
		return true;
	default:
		return false;
	}
}

std::unique_ptr<Screen> CreateScreenForState(Uint8 state) {
	using namespace GameState;
	switch(state){
	case MAINMENU:
		return std::make_unique<MainMenuScreen>();
	case LOBBYCONNECT:
		return std::make_unique<LobbyConnectScreen>();
	case LOBBY:
#ifdef SILENCER_HAVE_LOBBY_UI
		return std::make_unique<LobbyScreen>();
#else
		return nullptr;
#endif
	case CREATECHARACTER:
		return std::make_unique<CharacterCreateScreen>();
	case UPDATING:
		return std::make_unique<UpdateScreen>();
	case MISSIONSUMMARY:
		return std::make_unique<MissionSummaryScreen>();
	case OPTIONS:
		return std::make_unique<OptionsScreen>();
	case OPTIONSCONTROLS:
		return std::make_unique<OptionsControlsScreen>();
	case OPTIONSDISPLAY:
		return std::make_unique<OptionsDisplayScreen>();
	case OPTIONSAUDIO:
		return std::make_unique<OptionsAudioScreen>();
	default:
		return nullptr;
	}
}

}  // namespace client_ui
}  // namespace silencer
