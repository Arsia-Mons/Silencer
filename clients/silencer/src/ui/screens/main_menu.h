#ifndef SILENCER_UI_V2_SCREENS_MAIN_MENU_H
#define SILENCER_UI_V2_SCREENS_MAIN_MENU_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Node;
struct Context;

// One handler per main-menu button. Any field left empty means "no action"
// — the button still renders + hovers, the click just does nothing. The
// PPM-dump preview path passes a default-constructed (all-empty) struct,
// which keeps the rendered output byte-identical to the legacy widget tree.
struct MainMenuHandlers {
	std::function<void()> on_tutorial;
	std::function<void()> on_lobby;
	std::function<void()> on_options;
	std::function<void()> on_exit;
};

// Returns the declarative tree for the main menu. Pure function: same
// inputs produce the same tree. Layout values mirror the legacy
// MainMenuScreen exactly (clients/silencer/src/ui/screens/main_menu/
// main_menu_screen.cpp) so the rendered output is byte-identical at
// scale=1; the preview harness verifies this with PPM diff.
Node BuildMainMenu(const Context & ctx, const MainMenuHandlers & handlers = {});

// Engine-side runtime: owns the UIState slot for the main-menu surface,
// builds the tree each frame via BuildMainMenu, and routes mouse-down
// clicks through DispatchClick. Constructed by Game::SetRuntime when
// the engine enters GameState::MAINMENU; destroyed on exit.
class MainMenuRuntime : public Runtime
{
public:
	MainMenuRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;
};

}  // namespace v2
}  // namespace ui

#endif
