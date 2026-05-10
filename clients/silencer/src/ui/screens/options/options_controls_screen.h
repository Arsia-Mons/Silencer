#ifndef OPTIONS_CONTROLS_SCREEN_H
#define OPTIONS_CONTROLS_SCREEN_H

#include "screen.h"
#include "keybinds.h"

#include <SDL3/SDL_scancode.h>

class Overlay;
class Button;

class OptionsControlsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	// Two-key view of an action's bindings. The on-disk model is
	// OR-of-AND with arbitrary list size; the UI shows a fixed
	// primary/secondary slot pair plus an OR/AND toggle. The two
	// helpers round-trip the shapes losslessly:
	//
	//   bindings = []                 → key1=UNK, key2=UNK
	//   bindings = [[k1]]             → key1=k1,  key2=UNK
	//   bindings = [[k1], [k2]]       → key1=k1,  key2=k2,  AND=false
	//   bindings = [[k1, k2]]         → key1=k1,  key2=k2,  AND=true
	//
	// CLI-set bindings of other shapes (mouse, gamepad, >2 keys) survive
	// a render but get clobbered on UI edit. Power-users use the CLI.
	struct LegacyView {
		SDL_Scancode key1 = SDL_SCANCODE_UNKNOWN;
		SDL_Scancode key2 = SDL_SCANCODE_UNKNOWN;
		bool         and_ = false;
	};
	static LegacyView ViewLegacy(const KeyMap & km, Action a);
	static void WriteLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_);

	Overlay * keynameoverlay[5] = {};
	Button *  c1button[5] = {};
	Button *  cobutton[5] = {};
	Button *  c2button[5] = {};
	Button *  presetbutton = nullptr;
	Uint32    optionscontrolstick = 0;
};

#endif
