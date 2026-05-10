#ifndef SCREEN_CONTEXT_H
#define SCREEN_CONTEXT_H

#include <SDL3/SDL_stdinc.h>
#include <functional>
#include <memory>

class World;
class Renderer;
class Lobby;
class Updater;
class KeyMap;
class Screen;
class Modal;
class Game;
class AmbienceMixer;
class RenderDevice;
struct SDL_Window;

// Curated API screens use to talk to Game. Holds refs to subsystems and the
// state-machine / screen-stack actions Screens are allowed to invoke. No
// Game& accessor by design — session-specific data lives on the subsystem
// that owns it semantically.
class ScreenContext
{
public:
	ScreenContext(Game & game,
	              World & world,
	              Renderer & renderer,
	              Lobby & lobby,
	              KeyMap & keymap,
	              Updater & updater,
	              AmbienceMixer & ambienceMixer,
	              SDL_Window * & window,
	              RenderDevice * & renderdevice);

	World &   world;
	Renderer & renderer;
	Lobby &   lobby;
	KeyMap &  keymap;
	Updater & updater;
	AmbienceMixer & ambienceMixer;
	// Live refs to Game's SDL window + render device. Pointers because both
	// are nullable in headless / dedicated-server mode and are assigned
	// after ScreenContext is constructed (during SetupRenderDevice).
	SDL_Window * & window;
	RenderDevice * & renderdevice;

	// State-machine + screen-stack actions. ShowModal / ShowMessage are still
	// stubs — wired up when the first screen needs them.
	void GoToState(Uint8 newState);
	void GoBack();
	void RequestQuit();
	void PushScreen(std::unique_ptr<Screen> s);
	void PopScreen();
	void ReplaceScreen(std::unique_ptr<Screen> s);
	void ShowModal(std::unique_ptr<Modal> m);
	void ShowMessage(const char * msg, std::function<void(bool ok)> onClose);

	// Stage-A lobby adapter. Delegates to Game::CreateLobbyInterface and the
	// extracted Game::TickLobbyBody. Replaced piecewise across stages B–H as
	// each panel migrates onto LobbyScreen.
	Uint16 BuildLegacyLobbyInterface();
	void TickLegacyLobbyBody();

private:
	Game & game;
};

#endif
