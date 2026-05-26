#ifndef OPTIONS_CONTROLS_SCREEN_H
#define OPTIONS_CONTROLS_SCREEN_H

#include "controls_keybind_list.h"
#include "screen.h"

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>

#include <string>

class Overlay;
class OptionsControlsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	// The screen owns only the in-flight row/slot state. Binding labels and
	// writes are requested through ScreenContext.
	void BeginRebindFromVisibleRow(int row, int slot);
	void ToggleOperatorFromVisibleRow(int row);
	int MaxScroll() const;

	int scrollPosition = 0;
	int rebindRow = -1;
	int rebindSlot = -1;
	Uint32    optionscontrolstick = 0;
	bool presetClicked = false;
	bool saveClicked = false;
	bool cancelClicked = false;
	int scrollDelta = 0;
	int operatorClickedRow = -1;
	int visibleRowCapacity_ = silencer::client_ui::options::kKeybindListMinVisibleRows;
	silencer::client_ui::options::KeybindListView keybindListView_;
};

#endif
