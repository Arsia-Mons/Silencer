#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL_stdinc.h>
#include "runtime/UiActionQueue.h"
#include "runtime/UiInputState.h"

class ScreenContext;
class Surface;

namespace ui {
struct DrawCommandList;
}

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
}

// Top-level legacy UI surface owned by ClientUi's ScreenStack. One normal
// screen is visible at a time, with overlay screens stacked above it.
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
	virtual void BuildUi(ScreenContext & ctx,
	                     Surface & dst,
	                     float frametime,
	                     const silencer::ui::UiInputState& input,
	                     Uint8 hudPhase,
	                     silencer::ui::UiInteractionRegistry& interactions)
	{ (void)ctx; (void)dst; (void)frametime; (void)input; (void)hudPhase; (void)interactions; }

	// Tear down screen-owned UI state. Called on pop/replace.
	virtual void Destroy(ScreenContext & ctx) = 0;

	// Handle a back/cancel request (esc, right-click, "Go Back" button).
	// Return true if the screen consumed the request internally (e.g. swapped
	// a sub-panel) so Game should NOT fall through to its default action.
	// Return false to let Game decide what happens next.
	virtual bool HandleBack(ScreenContext & ctx) { (void)ctx; return false; }

	// Typed UI intent emitted by the runtime input router. Screens own all
	// application state mutations and route by stable widget/action IDs.
	virtual bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
	{
		if(action.kind == silencer::ui::UiActionKind::Cancel){
			return HandleBack(ctx);
		}
		(void)ctx;
		return false;
	}

	// Modals draw the screen below them; non-modal Screens hide what's beneath.
	virtual bool IsOverlay() const { return false; }

	virtual const ::ui::DrawCommandList * RetainedDrawCommands() const
	{
		return nullptr;
	}
};

#endif
