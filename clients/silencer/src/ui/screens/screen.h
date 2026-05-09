#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL_stdinc.h>

class ScreenContext;

// Top-level UI surface bound to a Game state. One Screen is active at a time
// (plus modals stacked on top). Lifecycle is owned by Game's screenStack.
class Screen
{
public:
	virtual ~Screen() = default;

	// Build widgets and add the root Interface to world. Called once on push.
	virtual void Build(ScreenContext & ctx) = 0;

	// Called once per Game::Tick while the screen is on top of the stack.
	virtual void Tick(ScreenContext & ctx) = 0;

	// Tear down widgets. Called on pop/replace.
	virtual void Destroy(ScreenContext & ctx) = 0;

	// Optional: handle a back/cancel request (esc, right-click). Default = pop.
	virtual bool HandleBack(ScreenContext & ctx) { (void)ctx; return true; }

	// Modals draw the screen below them; non-modal Screens hide what's beneath.
	virtual bool IsOverlay() const { return false; }

	Uint16 interfaceId = 0;
};

#endif
