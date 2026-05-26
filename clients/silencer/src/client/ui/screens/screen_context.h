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
class Surface;
class LobbyGame;
struct SDL_Window;

// Bag of refs that screens use to reach the global subsystems (World,
// Renderer, Lobby, Updater, KeyMap, AmbienceMixer, the SDL window /
// RenderDevice) plus narrow state-machine / screen-stack actions that touch
// Game itself. Per-screen behavior lives in the screen, not here. The `game`
// ref is transitional for state not yet moved behind ScreenContext; new Game
// or stack reads should be exposed through a narrow ScreenContext handoff.
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

	// State-machine + client UI navigation actions.
	void GoToState(Uint8 newState);
	bool GoBack();
	void RequestQuit();
	// Session-side cleanup when a screen leaves a joined game (handled by
	// Game/World, never by a screen reaching into the world directly).
	void LeaveJoinedGame();
	bool IsJoiningGame() const;
	void SetJoiningGame(bool joining);
	bool IsCreateGamePending() const;
	void SetCreateGamePending(bool pending);
	LobbyGame * CurrentLobbyGame() const;
	void JoinLobbyGame(LobbyGame & lobbyGame, char * password = nullptr);
	void SpectateLobbyGame(LobbyGame & lobbyGame, char * password = nullptr);
	bool PushScreen(std::unique_ptr<Screen> s);
	bool PopScreen();
	bool ReplaceScreen(std::unique_ptr<Screen> s);
	Screen * TopScreen() const;
	bool ShowModal(std::unique_ptr<Modal> m);
	bool ShowMessage(const char * msg, std::function<void()> onClose = nullptr);
	void ClearUiFocus();

	// Switch the renderer's active palette and clear the framebuffer. Called
	// from Screen::Build by every menu surface that owns its presentation.
	void ResetPresentation(int paletteIdx);

	// Clay frame ownership lives in Game/ClientUi. Screens declare UI through
	// Screen::BuildUi only; they must not begin/end or render Clay directly.
};

#endif
