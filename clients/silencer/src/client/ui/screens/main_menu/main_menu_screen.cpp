#include "main_menu_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include <SDL3/SDL.h>

#include <string>

namespace main_menu_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;

constexpr uint16_t kRootPadY = 32;
constexpr uint16_t kLogoPadX = 7;
constexpr uint16_t kLogoNudgeY = 6;
constexpr uint16_t kLogoGroupWidth = 350;
constexpr uint16_t kActionGroupWidth = 290;
constexpr uint16_t kVersionFooterX = 10;
constexpr uint16_t kVersionFooterTopFromBottom = 17;
constexpr uint16_t kLegacyActionGap = 34;
constexpr float kActionStackRightInset = 54.0f;
constexpr float kActionStackCenterYOffset = 31.0f;
constexpr int kActionStaggerStep = 40;
constexpr int kActionExitOffset = -kActionStaggerStep;
constexpr int kActionColumnOffset = 0;
constexpr int kActionConnectOffset = kActionStaggerStep;
constexpr int kActionMinOffset = kActionExitOffset;
constexpr const char * kActionTutorial = "main_menu.tutorial";
constexpr const char * kActionLobby = "main_menu.lobby";
constexpr const char * kActionOptions = "main_menu.options";
constexpr const char * kActionExit = "main_menu.exit";

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

ButtonOpts MainMenuActionButtonOpts()
{
	return ButtonOpts{ .variant = ButtonVariant::Oval,
	                   .size = ButtonSize::Md };
}

void MainMenuVersionText(Clay_String text)
{
	Text(text, { .size = TextSize::Footer });
}

void MainMenuActionRow(Clay_String rowId,
                       Clay_String spacerId,
                       int columnOffset,
                       Clay_String buttonId,
                       Clay_String label,
                       const char * actionId,
                       silencer::ui::UiInteractionRegistry& interactions)
{
	const int normalizedOffset = columnOffset - kActionMinOffset;
	CLAY({ .id = CLAY_SID(rowId),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0),
	                       CLAY_SIZING_FIT(0) },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_SID(spacerId),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(normalizedOffset)),
		                       CLAY_SIZING_FIXED(1.0f) },
		       } }) {}
		Button(buttonId,
		       label,
		       MainMenuActionButtonOpts(),
		       ButtonHandle{ nullptr, actionId, &interactions });
	}
}

void MainMenuActionStack(silencer::ui::UiInteractionRegistry& interactions)
{
	CLAY({ .id = CLAY_ID("MainMenuActionStack"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0),
	                       CLAY_SIZING_FIT(0) },
	           .childGap = kLegacyActionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .floating = {
	           .offset = { -kActionStackRightInset,
	                       kActionStackCenterYOffset },
	           .zIndex = 1,
	           .attachPoints = { .element = CLAY_ATTACH_POINT_RIGHT_CENTER,
	                             .parent = CLAY_ATTACH_POINT_RIGHT_CENTER },
	           .attachTo = CLAY_ATTACH_TO_PARENT,
	       } }) {
		MainMenuActionRow(CLAY_STRING("MainMenuTutorialRow"),
		                  CLAY_STRING("MainMenuTutorialSpacer"),
		                  kActionColumnOffset,
		                  CLAY_STRING("MainMenuTutorialButton"),
		                  CLAY_STRING("Tutorial"),
		                  kActionTutorial,
		                  interactions);
		MainMenuActionRow(CLAY_STRING("MainMenuLobbyRow"),
		                  CLAY_STRING("MainMenuLobbySpacer"),
		                  kActionConnectOffset,
		                  CLAY_STRING("MainMenuLobbyButton"),
		                  CLAY_STRING("Connect To Lobby"),
		                  kActionLobby,
		                  interactions);
		MainMenuActionRow(CLAY_STRING("MainMenuOptionsRow"),
		                  CLAY_STRING("MainMenuOptionsSpacer"),
		                  kActionColumnOffset,
		                  CLAY_STRING("MainMenuOptionsButton"),
		                  CLAY_STRING("Options"),
		                  kActionOptions,
		                  interactions);
		MainMenuActionRow(CLAY_STRING("MainMenuExitRow"),
		                  CLAY_STRING("MainMenuExitSpacer"),
		                  kActionExitOffset,
		                  CLAY_STRING("MainMenuExitButton"),
		                  CLAY_STRING("Exit"),
		                  kActionExit,
		                  interactions);
	}
}
} // namespace main_menu_screen_detail

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetMenuPresentation(1);

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

	versionText_ = "Silencer v";
	versionText_ += ctx.world.GetVersion();

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
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
		       } }) {
			CLAY({ .id = CLAY_ID("MainMenuComposition"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0),
			                       CLAY_SIZING_GROW(0) },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				CLAY({ .id = CLAY_ID("MainMenuLogoGroup"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(
				                           main_menu_screen_detail::kLogoGroupWidth)),
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
				}

				CLAY({ .id = CLAY_ID("MainMenuActionGroup"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(
				                           main_menu_screen_detail::kActionGroupWidth)),
				                       CLAY_SIZING_GROW(0) },
				       } }) {
					main_menu_screen_detail::MainMenuActionStack(interactions);
				}
			}
		}

		CLAY({ .id = CLAY_ID("MainMenuVersion"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIT(0),
		                       CLAY_SIZING_FIT(0) },
		       },
		       .floating = {
		           .offset = {
		               static_cast<float>(main_menu_screen_detail::kVersionFooterX),
		               -static_cast<float>(main_menu_screen_detail::kVersionFooterTopFromBottom) },
		           .zIndex = 1,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
		                             .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
		           .attachTo = CLAY_ATTACH_TO_ROOT,
		       } }) {
			main_menu_screen_detail::MainMenuVersionText(main_menu_screen_detail::FromStd(versionText_));
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
