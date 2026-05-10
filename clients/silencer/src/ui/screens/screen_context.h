#ifndef SCREEN_CONTEXT_H
#define SCREEN_CONTEXT_H

#include <SDL3/SDL_scancode.h>
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
	              Updater & updater);

	World &   world;
	Renderer & renderer;
	Lobby &   lobby;
	KeyMap &  keymap;
	Updater & updater;

	// State-machine + screen-stack actions. Phase-1 stubs — wired up when the
	// first screen migrates.
	void GoToState(Uint8 newState);
	void GoBack();
	void RequestQuit();
	void PushScreen(std::unique_ptr<Screen> s);
	void PopScreen();
	void ReplaceScreen(std::unique_ptr<Screen> s);
	void ShowModal(std::unique_ptr<Modal> m);
	void ShowMessage(const char * msg, std::function<void(bool ok)> onClose);

	// Keybind/profile actions used by the controls options screen.
	void LoadActiveKeymap();
	void CycleKeybindPreset();
	void ForkActiveProfileIfBuiltin();

	// Display-side actions used by the display options screen. Touch the
	// SDL window / renderdevice on Game so screens stay free of platform
	// handles.
	void SetFullscreen(bool on);
	void SetScaleFilter(bool on);

	// Keyname lookup shared with tutorial overlays; kept on Game so screen
	// callers go through the context rather than reaching for Game.
	const char * KeyName(SDL_Scancode sc) const;

private:
	Game & game;
};

#endif
