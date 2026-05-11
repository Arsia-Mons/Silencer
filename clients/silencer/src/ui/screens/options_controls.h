#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_CONTROLS_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_CONTROLS_H

#include "runtime.h"
#include "ui_state.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdint>
#include <functional>
#include <string>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Per-row text feed (computed engine-side from the active KeyMap + the
// current rebind-capture status). Empty strings render no pixels.
struct OptionsControlsRowState {
	std::string keyname;   // e.g. "Move Up:" — empty = no label rendered
	std::string c1_text;   // primary key label, "-" while rebinding
	std::string c2_text;   // secondary key label, "-" while rebinding
	std::string op_text;   // "OR" or "AND" — empty = no toggle drawn
};

struct OptionsControlsState {
	std::string preset_text;            // active profile label
	OptionsControlsRowState rows[5];    // VISIBLE_ROWS — preset takes slot 0
};

// Handlers for the controls sub-screen. Empty handlers are valid — the
// matching button still renders and hit-tests, the click just does
// nothing. Rebind-key fires when the user clicks a primary (slot=0) or
// secondary (slot=1) key chip; engine wiring captures the next SDL key /
// gamepad input and writes back to the KeyMap.
struct OptionsControlsHandlers {
	std::function<void()> on_preset;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
	std::function<void(int row, int slot)> on_rebind_key;
};

// Returns the declarative tree for the controls-options sub-screen.
// When `state == nullptr`, mirrors legacy OptionsControlsScreen::Build
// (pre-Tick) for the byte-identical PPM gate: all per-row text empty,
// preset button empty, OR/AND overlays omitted. When `state` is supplied
// (live engine path), per-row keyname / key labels / OR-AND / preset
// text render through the state struct.
Node BuildOptionsControls(const Context & ctx,
                          const OptionsControlsHandlers & handlers = {},
                          const OptionsControlsState * state = nullptr);

// Engine-side runtime for GameState::OPTIONSCONTROLS. Owns the rebind
// capture state machine (primary / secondary slot, gamepad button +
// axis snapshot, pending scancode). DispatchKeyDown latches the next
// SDL scancode while a slot is armed; Tick() commits + clears.
class OptionsControlsRuntime : public Runtime
{
public:
	OptionsControlsRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;
	bool DispatchKeyDown(int sdl_scancode) override;
	void Tick() override;

	// Called from BuildHandlers' on_rebind_key when the user clicks a
	// key chip. Arms the capture state machine for the next Tick.
	void StartRebind(int row, int slot);

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;

	// Active rebind slot in legacy uid encoding: row for primary (0..99)
	// or 100+row for secondary (100..149); -1 = no active capture.
	int      rebind_active_slot_      = -1;
	uint32_t rebind_start_tick_       = 0;
	int      rebind_pending_scancode_ = -1;
	uint32_t rebind_gamepad_buttons_  = 0;
	int16_t  rebind_gamepad_axes_[SDL_GAMEPAD_AXIS_COUNT] = {};
};

}  // namespace v2
}  // namespace ui

#endif
