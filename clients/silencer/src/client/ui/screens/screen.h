#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL_stdinc.h>
#include "runtime/UiInputState.h"

class ScreenContext;
class Surface;

// Top-level UI surface bound to a Game state. One Screen is active at a time
// (plus modals stacked on top). Lifecycle is owned by ClientUi's ScreenStack.
class Screen
{
public:
	virtual ~Screen() = default;

	// Initialize screen-owned UI state. Called once on push.
	virtual void Build(ScreenContext & ctx) = 0;

	// Called once per frame while the screen is on top of the UI stack.
	virtual void Tick(ScreenContext & ctx) = 0;

	// Declare this screen's UI into the current ClientUi frame. Screens do not
	// begin/end Clay, render Clay commands, or reset primitive arenas; ClientUi
	// owns the frame lifecycle for every visible UI surface.
	virtual void BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
	{ (void)ctx; (void)dst; (void)frametime; }

	// Tear down screen-owned UI state. Called on pop/replace.
	virtual void Destroy(ScreenContext & ctx) = 0;

	// Handle a back/cancel request (esc, right-click, "Go Back" button).
	// Return true if the screen consumed the request internally (e.g. swapped
	// a sub-panel) so Game should NOT fall through to its default action.
	// Return false to let Game decide what happens next (typically pop or
	// transition to MAINMENU).
	virtual bool HandleBack(ScreenContext & ctx) { (void)ctx; return false; }

	// Semantic UI input fallback. Device-specific input is normalized by
	// ClientUi before it reaches screens.
	virtual bool HandleUiAction(ScreenContext & ctx, silencer::ui::UiNavAction action)
	{ (void)ctx; (void)action; return false; }

	// Narrow escape hatch for controls rebinding. Normal UI navigation must not
	// use raw key codes.
	virtual bool CaptureRawKeyDown(ScreenContext & ctx, int keyCode)
	{ (void)ctx; (void)keyCode; return false; }

	// Modals draw the screen below them; non-modal Screens hide what's beneath.
	virtual bool IsOverlay() const { return false; }
};

#endif
