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
class Surface;
struct SDL_Window;

// Bag of refs that legacy screens use to reach global subsystems while the
// retained UI port is underway. Screen-specific behavior should move behind
// provider-backed hooks; stack mutations should go through use_navigation().
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

	// Switch the renderer's active palette and clear the framebuffer. Called
	// from Screen::Build by every menu surface that owns its presentation.
	void ResetPresentation(int paletteIdx);

	// Clay frame ownership lives in Game/ClientUi. Screens declare UI through
	// Screen::BuildUi only; they must not begin/end or render Clay directly.
};

#endif
