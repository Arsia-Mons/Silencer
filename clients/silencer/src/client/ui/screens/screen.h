#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL_stdinc.h>
#include "runtime/UiActionQueue.h"
#include "runtime/UiInputState.h"
#include "ui/runtime/element.h"

#include <cstdint>

class ScreenContext;

namespace silencer {
namespace client_ui {
using UiScreenEntryId = std::uint32_t;

enum class ScreenKind {
	Normal,
	Overlay,
};
}
}

// Top-level UI surface bound to a Game state. One Screen is active at a time
// (plus modals stacked on top). Lifecycle is owned by ClientUi's ScreenStack.
class Screen
{
public:
	virtual ~Screen() = default;

	silencer::client_ui::UiScreenEntryId EntryId() const { return entryId_; }
	void SetEntryId(silencer::client_ui::UiScreenEntryId id) { entryId_ = id; }
	silencer::client_ui::ScreenKind Kind() const {
		return IsOverlay() ? silencer::client_ui::ScreenKind::Overlay
		                   : silencer::client_ui::ScreenKind::Normal;
	}

	// Initialize screen-owned UI state. Called once on push.
	virtual void Build(ScreenContext & ctx) = 0;

	// Called once per frame while the screen is on top of the UI stack.
	virtual void Tick(ScreenContext & ctx) = 0;

	// Retained cppx entry point. ClientUi owns the UiElementFrame/reconciler
	// boundary; screens only return a component root built from props/children.
	virtual bool BuildElement(ScreenContext & ctx, ::ui::UiElement * out) = 0;

	// Tear down screen-owned UI state. Called on pop/replace.
	virtual void Destroy(ScreenContext & ctx) = 0;

	// Handle a back/cancel request (esc, right-click, "Go Back" button).
	// Return true if the screen consumed the request internally (e.g. swapped
	// a sub-panel) so Game should NOT fall through to its default action.
	// Return false to let Game decide what happens next (typically pop or reset
	// the screen stack).
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

private:
	silencer::client_ui::UiScreenEntryId entryId_ = 0;
};

#endif
