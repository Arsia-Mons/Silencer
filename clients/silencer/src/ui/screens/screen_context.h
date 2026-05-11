#ifndef SCREEN_CONTEXT_H
#define SCREEN_CONTEXT_H

#include <SDL3/SDL_stdinc.h>

class World;
class Renderer;
class Lobby;
class Updater;
class KeyMap;
class Game;
class AmbienceMixer;
class MapDownloader;
class RenderDevice;
struct SDL_Window;

// Bag of refs that v2 screens use to reach the global subsystems (World,
// Renderer, Lobby, Updater, KeyMap, AmbienceMixer, the SDL window /
// RenderDevice) plus the state-machine actions that touch Game itself.
// Per-screen behavior lives in the screen's Runtime, not here — when a
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

	void GoToState(Uint8 newState);
	void GoBack();
	void RequestQuit();

	// Switch the renderer's active palette and clear the framebuffer. Called
	// from v2 Runtime constructors by every menu surface that owns its
	// presentation.
	void ResetPresentation(int paletteIdx);
};

#endif
