#include "main_menu_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "clay_inspector.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

namespace
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kRootPadX = 40;
constexpr uint16_t kRootPadY = 32;
constexpr uint16_t kButtonGap = 12;
constexpr int kMenuButtonW = 156;
constexpr int kMenuButtonH = 21;
constexpr int kMenuButtonCount = 4;
constexpr int kMenuButtonTotalH =
	kMenuButtonCount * kMenuButtonH + (kMenuButtonCount - 1) * kButtonGap;

struct MainMenuButtonLayout
{
	int x = 0;
	int y = 0;
};

void MainMenuTutorialClicked(void * user)
{
	auto * screen = static_cast<MainMenuScreen *>(user);
	if(screen) screen->NotifyTutorialClicked();
}

void MainMenuLobbyClicked(void * user)
{
	auto * screen = static_cast<MainMenuScreen *>(user);
	if(screen) screen->NotifyLobbyClicked();
}

void MainMenuOptionsClicked(void * user)
{
	auto * screen = static_cast<MainMenuScreen *>(user);
	if(screen) screen->NotifyOptionsClicked();
}

void MainMenuExitClicked(void * user)
{
	auto * screen = static_cast<MainMenuScreen *>(user);
	if(screen) screen->NotifyExitClicked();
}

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

MainMenuButtonLayout ComputeButtonLayout(int width, int height)
{
	int maxX = std::max(0, width - kMenuButtonW - 8);
	int maxY = std::max(0, height - kMenuButtonTotalH - 24);
	bool narrow = width < 560;

	MainMenuButtonLayout layout;
	if(narrow){
		layout.x = (width - kMenuButtonW) / 2;
		layout.y = (height - kMenuButtonTotalH) / 2;
		layout.y = std::max(layout.y, 184);
	}else{
		layout.x = static_cast<int>(width * 0.62f);
		layout.y = (height - kMenuButtonTotalH) / 2;
	}

	layout.x = std::max(8, std::min(layout.x, maxX));
	layout.y = std::max(24, std::min(layout.y, maxY));
	return layout;
}

void RegisterButton(const char * label,
                    int x,
                    int y,
                    void (*onClick)(void *),
                    MainMenuScreen * screen)
{
	silencer::ui::clay_inspector::Widget w;
	w.label = label;
	w.kind = silencer::ui::clay_inspector::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	w.onClick = onClick;
	w.clickUser = screen;
	silencer::ui::clay_inspector::Register(w);
}

void RegisterMainMenuButtons(const MainMenuButtonLayout & layout,
                             MainMenuScreen * screen)
{
	RegisterButton("Tutorial", layout.x, layout.y,
	               &MainMenuTutorialClicked, screen);
	RegisterButton("Connect To Lobby", layout.x,
	               layout.y + kMenuButtonH + kButtonGap,
	               &MainMenuLobbyClicked, screen);
	RegisterButton("Options", layout.x,
	               layout.y + (kMenuButtonH + kButtonGap) * 2,
	               &MainMenuOptionsClicked, screen);
	RegisterButton("Exit", layout.x,
	               layout.y + (kMenuButtonH + kButtonGap) * 3,
	               &MainMenuExitClicked, screen);
}
}

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns every visible main-menu element. No retained Interface/Object
	// widget graph is built for this screen.

	tutorialClicked = false;
	lobbyClicked = false;
	optionsClicked = false;
	exitClicked = false;

	silencer::ui::clay_inspector::BeginFrame();
	const Surface & screenbuffer = ctx.game.GetScreenBuffer();
	RegisterMainMenuButtons(ComputeButtonLayout(screenbuffer.w, screenbuffer.h),
	                        this);
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

void MainMenuScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	ctx.BeginClayFrame(dst);

	BankTextBeginFrame();
	BankButtonBeginFrame();
	silencer::ui::clay_inspector::BeginFrame();

	std::string version = "Silencer v";
	version += ctx.world.GetVersion();
	const MainMenuButtonLayout buttons = ComputeButtonLayout(dst.w, dst.h);

	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("MainMenuRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("MainMenuChrome"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_GROW(0) },
		           .padding = { kRootPadX, kRootPadX, kRootPadY, kRootPadY },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			CLAY({ .id = CLAY_ID("MainMenuLeft"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_GROW(0) },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				CLAY({ .id = CLAY_ID("MainMenuLogo"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(360),
				                       CLAY_SIZING_FIXED(160) },
				       },
				       .image = { .imageData = PackImage(208, 60) } }) {}

				CLAY({ .id = CLAY_ID("MainMenuVersion"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_GROW(0) },
				           .childAlignment = { .y = CLAY_ALIGN_Y_BOTTOM },
				       } }) {
					BankText(FromStd(version), BankTextVariant::BodySm, {});
				}
			}
		}

		CLAY({ .id = CLAY_ID("MainMenuButtons"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kMenuButtonW),
		                       CLAY_SIZING_FIXED(kMenuButtonTotalH) },
		           .childGap = kButtonGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .floating = {
		           .offset = { (float)buttons.x, (float)buttons.y },
		           .attachTo = CLAY_ATTACH_TO_ROOT,
		       } }) {
			BankButton(CLAY_STRING("Tutorial"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &MainMenuTutorialClicked, this });
			BankButton(CLAY_STRING("Connect To Lobby"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &MainMenuLobbyClicked, this });
			BankButton(CLAY_STRING("Options"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &MainMenuOptionsClicked, this });
			BankButton(CLAY_STRING("Exit"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &MainMenuExitClicked, this });
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();

	Render(ctx.game, &dst, cmds);

	RegisterMainMenuButtons(buttons, this);
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::HandleKeyPress(ScreenContext & ctx, char ascii)
{
	if(ascii == 0x1B){
		ctx.RequestQuit();
		return true;
	}
	return Screen::HandleKeyPress(ctx, ascii);
}
