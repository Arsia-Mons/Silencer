#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Node;
struct Context;

// One handler per options-router button. Any field left empty means "no
// action" — the button still renders + hovers, the click just does nothing.
// The PPM-dump preview path passes a default-constructed (all-empty) struct,
// which keeps the rendered output byte-identical to the legacy widget tree.
struct OptionsHandlers {
	std::function<void()> on_controls;
	std::function<void()> on_display;
	std::function<void()> on_audio;
	std::function<void()> on_go_back;
};

// Returns the declarative tree for the options sub-router. Pure function:
// same inputs produce the same tree. Layout values mirror the legacy
// OptionsScreen exactly (clients/silencer/src/ui/screens/options/
// options_screen.cpp) so the rendered output is byte-identical at scale=1.
//
// Uses absolute `.at()` positioning — the legacy anchor coords (x=-89,
// y=-142/-90/-38/15) bake in sprite-anchor offsets, which a Clay container
// would compute against the chrome top-left, so a VStack can't reproduce
// the exact pixels without runtime offset constants. The design doc's
// `.at()` escape hatch applies here.
Node BuildOptions(const Context & ctx, const OptionsHandlers & handlers = {});

// Engine-side runtime for GameState::OPTIONS. Constructed by Game::
// SetRuntime on state entry; owns the per-screen UIState slot and
// routes mouse-down clicks through DispatchClick.
class OptionsRuntime : public Runtime
{
public:
	OptionsRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y) override;

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;
};

}  // namespace v2
}  // namespace ui

#endif
