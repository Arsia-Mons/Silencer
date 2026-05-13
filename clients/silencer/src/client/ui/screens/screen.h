#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_scancode.h>
#include "runtime/UiAutomationRegistry.h"

class ScreenContext;
class Surface;

// Top-level UI surface bound to a Game state. One Screen is active at a time
// (plus modals stacked on top). Lifecycle is owned by Game's screenStack.
class Screen
{
public:
	virtual ~Screen() = default;

	// Initialize screen-owned UI state. Called once on push.
	virtual void Build(ScreenContext & ctx) = 0;

	// Called once per Game::Tick while the screen is on top of the stack.
	virtual void Tick(ScreenContext & ctx) = 0;

	// Render-phase hook. Called from the game's render loop AFTER the
	// screenbuffer is cleared and BEFORE Renderer::Draw walks the world.
	// Screens that emit a Clay tree dispatch its render commands here so
	// Clay-painted pixels are drawn first (background image, chrome) before
	// Renderer::Draw handles game-world content. Default no-op.
	virtual void Draw(ScreenContext & ctx, Surface & dst, float frametime)
	{ (void)ctx; (void)dst; (void)frametime; }

	// Tear down screen-owned UI state. Called on pop/replace.
	virtual void Destroy(ScreenContext & ctx) = 0;

	// Handle a back/cancel request (esc, right-click, "Go Back" button).
	// Return true if the screen consumed the request internally (e.g. swapped
	// a sub-panel) so Game should NOT fall through to its default action.
	// Return false to let Game decide what happens next (typically pop or
	// transition to MAINMENU).
	virtual bool HandleBack(ScreenContext & ctx) { (void)ctx; return false; }

	// Optional input hooks for Clay-driven screens. Screens consume here and
	// keep their state in caller-owned buffers/callbacks.
	virtual bool HandleTextInput(ScreenContext & ctx, char ascii)
	{ (void)ctx; (void)ascii; return false; }
	virtual bool HandleKeyPress(ScreenContext & ctx, char ascii)
	{
		(void)ctx;
		switch(ascii){
			case 2:
			case '\t':
			case 4:
				return silencer::ui::automation::FocusNextInteractive();
			case 1:
			case 3:
				return silencer::ui::automation::FocusPreviousInteractive();
			case '\n':
				return silencer::ui::automation::ActivateFocused();
			default:
				return silencer::ui::automation::DispatchKeyPress(ascii);
		}
	}
	virtual bool HandleScancodeDown(ScreenContext & ctx, SDL_Scancode scancode)
	{ (void)ctx; (void)scancode; return false; }
	virtual bool HandleMousePress(ScreenContext & ctx, bool pressed, Uint16 x, Uint16 y)
	{
		(void)ctx;
		return pressed ? silencer::ui::automation::InvokeAt(x, y) : false;
	}
	virtual bool HandleMouseMove(ScreenContext & ctx, Uint16 x, Uint16 y)
	{ (void)ctx; (void)x; (void)y; return false; }

	// Modals draw the screen below them; non-modal Screens hide what's beneath.
	virtual bool IsOverlay() const { return false; }
};

#endif
