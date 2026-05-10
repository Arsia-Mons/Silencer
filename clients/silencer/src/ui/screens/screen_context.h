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
class MapDownloader;
class RenderDevice;
struct SDL_Window;

// Bag of refs that screens use to reach the global subsystems (World,
// Renderer, Lobby, Updater, KeyMap, AmbienceMixer, the SDL window /
// RenderDevice) plus the state-machine / screen-stack actions that touch
// Game itself. Per-screen behavior lives in the screen, not here — when a
// screen needs Game state directly, reach through the `game` ref.
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
	              MapDownloader & mapDownloader,
	              SDL_Window * & window,
	              RenderDevice * & renderdevice);

	Game &     game;
	World &    world;
	Renderer & renderer;
	Lobby &    lobby;
	KeyMap &   keymap;
	Updater &  updater;
	AmbienceMixer & ambienceMixer;
	MapDownloader & mapDownloader;
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
};

#endif
