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
#include "primitives/button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

#include <string>

namespace main_menu_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kRootPadX = 40;
constexpr uint16_t kRootPadY = 32;
constexpr uint16_t kLogoPadX = 7;
constexpr uint16_t kLogoNudgeY = 6;
constexpr float kBrandColumnPercent = 0.625f;
constexpr uint16_t kButtonGap = 12;
constexpr int kMenuButtonW = 196;
constexpr int kMenuButtonH = 33;
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
	logo.Reset();
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

	// Flex-first layout: legacy positions are expressed as column sizes,
	// padding, and alignment so the menu still reflows with Clay.
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
		           .padding = { 0,
		                        0,
		                        main_menu_screen_detail::kRootPadY,
		                        main_menu_screen_detail::kRootPadY },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("MainMenuLeft"),
			       .layout = {
			           .sizing = { CLAY_SIZING_PERCENT(main_menu_screen_detail::kBrandColumnPercent),
			                       CLAY_SIZING_GROW(0) },
			           .padding = { main_menu_screen_detail::kLogoPadX,
			                        0,
			                        0,
			                        0 },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				CLAY({ .id = CLAY_ID("MainMenuLogoRegion"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_GROW(0) },
				           .padding = { 0,
				                        0,
				                        main_menu_screen_detail::kLogoNudgeY,
				                        0 },
				           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
				                               .y = CLAY_ALIGN_Y_CENTER },
				       } }) {
					logo.Build(ctx.world.resources);
				}

				CLAY({ .id = CLAY_ID("MainMenuVersion"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(14) },
				           .childAlignment = { .y = CLAY_ALIGN_Y_BOTTOM },
				       } }) {
					main_menu_screen_detail::BankText(main_menu_screen_detail::FromStd(version), main_menu_screen_detail::BankTextVariant::BodySm, {});
				}
			}

			CLAY({ .id = CLAY_ID("MainMenuRight"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_GROW(0) },
			           .padding = { 0,
			                        main_menu_screen_detail::kRootPadX,
			                        0,
			                        0 },
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
					main_menu_screen_detail::Button(CLAY_STRING("MainMenuTutorialButton"), CLAY_STRING("Tutorial"),
					           main_menu_screen_detail::ButtonOpts{ .variant = main_menu_screen_detail::ButtonVariant::Oval,
					                                               .size = main_menu_screen_detail::ButtonSize::Md },
					           main_menu_screen_detail::ButtonHandle{ nullptr, main_menu_screen_detail::kActionTutorial, &interactions });
					main_menu_screen_detail::Button(CLAY_STRING("MainMenuLobbyButton"), CLAY_STRING("Connect To Lobby"),
					           main_menu_screen_detail::ButtonOpts{ .variant = main_menu_screen_detail::ButtonVariant::Oval,
					                                               .size = main_menu_screen_detail::ButtonSize::Md },
					           main_menu_screen_detail::ButtonHandle{ nullptr, main_menu_screen_detail::kActionLobby, &interactions });
					main_menu_screen_detail::Button(CLAY_STRING("MainMenuOptionsButton"), CLAY_STRING("Options"),
					           main_menu_screen_detail::ButtonOpts{ .variant = main_menu_screen_detail::ButtonVariant::Oval,
					                                               .size = main_menu_screen_detail::ButtonSize::Md },
					           main_menu_screen_detail::ButtonHandle{ nullptr, main_menu_screen_detail::kActionOptions, &interactions });
					main_menu_screen_detail::Button(CLAY_STRING("MainMenuExitButton"), CLAY_STRING("Exit"),
					           main_menu_screen_detail::ButtonOpts{ .variant = main_menu_screen_detail::ButtonVariant::Oval,
					                                               .size = main_menu_screen_detail::ButtonSize::Md },
					           main_menu_screen_detail::ButtonHandle{ nullptr, main_menu_screen_detail::kActionExit, &interactions });
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
