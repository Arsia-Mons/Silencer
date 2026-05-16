#include "main_menu_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

#include <string>

namespace main_menu_screen_detail
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kRootPadX = 40;
constexpr uint16_t kRootPadY = 32;
constexpr uint16_t kButtonGap = 12;
constexpr int kMenuButtonW = 156;
constexpr int kMenuButtonH = 21;
constexpr int kMenuButtonCount = 4;
constexpr int kMenuButtonTotalH =
	kMenuButtonCount * kMenuButtonH + (kMenuButtonCount - 1) * kButtonGap;
constexpr const char * kActionTutorial = "main_menu.tutorial";
constexpr const char * kActionLobby = "main_menu.lobby";
constexpr const char * kActionOptions = "main_menu.options";
constexpr const char * kActionExit = "main_menu.exit";

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}
} // namespace main_menu_screen_detail

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns every visible main-menu element. No retained Interface/Object
	// widget graph is built for this screen. Button hit-test bounds are
	// resolved from Clay's real layout (ResolveClayBoundsFromClay), so no
	// absolute coordinates are registered here.

	tutorialClicked = false;
	lobbyClicked = false;
	optionsClicked = false;
	exitClicked = false;
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	if(tutorialClicked){
		tutorialClicked = false;
		ctx.GoToState(GameState::SINGLEPLAYERGAME);
		return;
	}
	if(lobbyClicked){
		lobbyClicked = false;
		ctx.GoToState(GameState::LOBBYCONNECT);
		return;
	}
	if(optionsClicked){
		optionsClicked = false;
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(exitClicked){
		exitClicked = false;
		ctx.RequestQuit();
		return;
	}
}

void MainMenuScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	std::string version = "Silencer v";
	version += ctx.world.GetVersion();

	// Fully responsive layout: the root fills Clay's virtual canvas (the
	// native surface / uiScale), the background covers it like a CSS
	// background-image, and a flex row places the logo + version on the
	// left and the button column centered on the right. No absolute
	// pixel positions — it reflows at any resolution.
	CLAY({ .id = CLAY_ID("MainMenuRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("MainMenuChrome"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_GROW(0) },
		           .padding = { main_menu_screen_detail::kRootPadX,
		                        main_menu_screen_detail::kRootPadX,
		                        main_menu_screen_detail::kRootPadY,
		                        main_menu_screen_detail::kRootPadY },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("MainMenuLeft"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_GROW(0) },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				// Width grows with the left column (never a rigid pixel
				// width that could overflow a narrow/portrait canvas and
				// push the button column off-screen); contain keeps the
				// logo's aspect at any size.
				CLAY({ .id = CLAY_ID("MainMenuLogo"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(160) },
				       },
				       .image = { .imageData = PackImageContain(208, 60) } }) {}

				CLAY({ .id = CLAY_ID("MainMenuVersion"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_GROW(0) },
				           .childAlignment = { .y = CLAY_ALIGN_Y_BOTTOM },
				       } }) {
					main_menu_screen_detail::BankText(main_menu_screen_detail::FromStd(version), main_menu_screen_detail::BankTextVariant::BodySm, {});
				}
			}

			CLAY({ .id = CLAY_ID("MainMenuRight"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_GROW(0) },
			           .childAlignment = { .x = CLAY_ALIGN_X_RIGHT,
			                               .y = CLAY_ALIGN_Y_CENTER },
			       } }) {
				CLAY({ .id = CLAY_ID("MainMenuButtons"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(main_menu_screen_detail::kMenuButtonW),
				                       CLAY_SIZING_FIXED(main_menu_screen_detail::kMenuButtonTotalH) },
				           .childGap = main_menu_screen_detail::kButtonGap,
				           .layoutDirection = CLAY_TOP_TO_BOTTOM,
				       } }) {
					main_menu_screen_detail::BankButton(CLAY_STRING("Tutorial"), main_menu_screen_detail::BankButtonVariant::Chrome, {},
					           main_menu_screen_detail::BankButtonHandle{ nullptr, main_menu_screen_detail::kActionTutorial, &interactions });
					main_menu_screen_detail::BankButton(CLAY_STRING("Connect To Lobby"), main_menu_screen_detail::BankButtonVariant::Chrome, {},
					           main_menu_screen_detail::BankButtonHandle{ nullptr, main_menu_screen_detail::kActionLobby, &interactions });
					main_menu_screen_detail::BankButton(CLAY_STRING("Options"), main_menu_screen_detail::BankButtonVariant::Chrome, {},
					           main_menu_screen_detail::BankButtonHandle{ nullptr, main_menu_screen_detail::kActionOptions, &interactions });
					main_menu_screen_detail::BankButton(CLAY_STRING("Exit"), main_menu_screen_detail::BankButtonVariant::Chrome, {},
					           main_menu_screen_detail::BankButtonHandle{ nullptr, main_menu_screen_detail::kActionExit, &interactions });
				}
			}
		}
	}
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		ctx.RequestQuit();
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == main_menu_screen_detail::kActionTutorial){
		tutorialClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionLobby){
		lobbyClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionOptions){
		optionsClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionExit){
		exitClicked = true;
		return true;
	}
	return false;
}
