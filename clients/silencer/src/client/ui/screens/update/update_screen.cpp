#include "update_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "updater.h"
#include "updaterstage2.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
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

constexpr uint16_t kDialogW = 352;
constexpr uint16_t kDialogH = 178;
constexpr uint16_t kDialogPadX = 34;
constexpr uint16_t kDialogPadY = 42;
constexpr uint16_t kButtonGap = 6;

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

void OnUpdateClicked(void * user)
{
	auto * screen = static_cast<UpdateScreen *>(user);
	if(screen) screen->NotifyUpdateClicked();
}

void OnCancelClicked(void * user)
{
	auto * screen = static_cast<UpdateScreen *>(user);
	if(screen) screen->NotifyCancelClicked();
}

void OnRetryClicked(void * user)
{
	auto * screen = static_cast<UpdateScreen *>(user);
	if(screen) screen->NotifyRetryClicked();
}

void OnDownloadClicked(void * user)
{
	auto * screen = static_cast<UpdateScreen *>(user);
	if(screen) screen->NotifyDownloadClicked();
}

void RegisterButton(const char * label,
                    int x,
                    int y,
                    void (*onClick)(void *),
                    UpdateScreen * screen)
{
	silencer::ui::automation::Widget w;
	w.label = label;
	w.kind = silencer::ui::automation::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	w.onClick = onClick;
	w.clickUser = screen;
	silencer::ui::automation::Register(w);
}

void RegisterWidgets(UpdateScreen * screen, int surfaceW, int surfaceH, Updater::State state, int retryCount)
{
	const int dialogX = (surfaceW - kDialogW) / 2;
	const int dialogY = (surfaceH - kDialogH) / 2;
	const int buttonY = dialogY + kDialogH - kDialogPadY - 21;
	const int leftX = dialogX + (kDialogW - (156 * 2 + kButtonGap)) / 2;
	const int rightX = leftX + 156 + kButtonGap;
	if(state == Updater::PROMPTING){
		RegisterButton("Update", leftX, buttonY, &OnUpdateClicked, screen);
	}else if(state == Updater::FAILED && retryCount < 3){
		RegisterButton("Retry", leftX, buttonY, &OnRetryClicked, screen);
	}else if(state == Updater::FAILED){
		RegisterButton("Download", leftX, buttonY, &OnDownloadClicked, screen);
	}
	if(state == Updater::PROMPTING || state == Updater::DOWNLOADING || state == Updater::FAILED){
		RegisterButton("Cancel", rightX, buttonY, &OnCancelClicked, screen);
	}
}

std::string StatusText(ScreenContext & ctx)
{
	switch(ctx.updater.GetState()){
		case Updater::PROMPTING:
			return "An update is required to play online.";
		case Updater::DOWNLOADING:{
			char buf[32];
			snprintf(buf, sizeof(buf), "%d%%", int(ctx.updater.GetProgress() * 100));
			return buf;
		}
		case Updater::VERIFYING:
			return "Verifying...";
		case Updater::STAGING:
			return "Restarting...";
		case Updater::FAILED:
			return ctx.updater.GetErrorMessage();
		case Updater::IDLE:
		case Updater::DONE:
		default:
			return "";
	}
}

std::string ProgressText(ScreenContext & ctx)
{
	if(ctx.updater.GetState() != Updater::DOWNLOADING) return "";
	int width = int(ctx.updater.GetProgress() * 20.0f);
	std::string bar = "[";
	for(int i = 0; i < 20; i++) bar += (i < width) ? "=" : " ";
	bar += "]";
	return bar;
}
}

void UpdateScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	updateClicked = false;
	cancelClicked = false;
	retryClicked = false;
	downloadClicked = false;
	silencer::ui::automation::BeginFrame();
	const Surface& surface = ctx.game.GetScreenBuffer();
	RegisterWidgets(this, surface.w, surface.h, ctx.updater.GetState(), ctx.updater.GetRetryCount());
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	Updater::State ustate = ctx.updater.GetState();
	if(updateClicked){
		updateClicked = false;
		if(ustate == Updater::PROMPTING) ctx.updater.Consent();
	}
	if(cancelClicked){
		cancelClicked = false;
		if(ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED){
			if(ustate == Updater::DOWNLOADING) ctx.updater.Cancel();
			ctx.GoToState(GameState::MAINMENU);
			return;
		}
	}
	if(retryClicked){
		retryClicked = false;
		if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
			ctx.updater.Retry();
		}
	}
	if(downloadClicked){
		downloadClicked = false;
		if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() >= 3){
			std::string url = ctx.updater.GetDownloadURL();
#ifdef _WIN32
			std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open '" + url + "'";
#else
			std::string cmd = "xdg-open '" + url + "' &";
#endif
			system(cmd.c_str());
			ctx.GoToState(GameState::MAINMENU);
			return;
		}
	}
	if(ctx.updater.GetState() == Updater::STAGING){
		std::string zippath =
#ifdef _WIN32
			std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
			"/tmp/silencer-update.zip";
#endif
		fprintf(stderr, "[updater] UpdateScreen invoking UpdaterStage2::Launch with zip=%s\n",
		        zippath.c_str());
		if(UpdaterStage2::Launch(zippath)){
			ctx.updater.MarkStage2Spawned();
			return;
		}
		fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
		ctx.GoToState(GameState::MAINMENU);
	}
}

void UpdateScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	ctx.BeginClayFrame(dst);

	BankButtonBeginFrame();
	BankTextBeginFrame();
	silencer::ui::automation::BeginFrame();

	std::string status = StatusText(ctx);
	std::string progress = ProgressText(ctx);
	Updater::State ustate = ctx.updater.GetState();

	ctx.BeginClayLayout();
	CLAY({ .id = CLAY_ID("UpdateRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("UpdateDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kDialogW),
		                       CLAY_SIZING_FIXED(kDialogH) },
		           .padding = { kDialogPadX, kDialogPadX,
		                        kDialogPadY, kDialogPadY },
		           .childGap = 12,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 4) } }) {
			BankText(FromStd(status), BankTextVariant::Heading, {});
			if(!progress.empty()){
				BankText(FromStd(progress), BankTextVariant::Heading, {});
			}
			CLAY({ .id = CLAY_ID("UpdateActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = kButtonGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				if(ustate == Updater::PROMPTING){
					BankButton(CLAY_STRING("Update"), BankButtonVariant::Chrome, {},
					           BankButtonHandle{ nullptr, &OnUpdateClicked, this });
				}else if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
					BankButton(CLAY_STRING("Retry"), BankButtonVariant::Chrome, {},
					           BankButtonHandle{ nullptr, &OnRetryClicked, this });
				}else if(ustate == Updater::FAILED){
					BankButton(CLAY_STRING("Download"), BankButtonVariant::Chrome, {},
					           BankButtonHandle{ nullptr, &OnDownloadClicked, this });
				}
				if(ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED){
					BankButton(CLAY_STRING("Cancel"), BankButtonVariant::Chrome, {},
					           BankButtonHandle{ nullptr, &OnCancelClicked, this });
				}
			}
		}
	}
	Clay_RenderCommandArray cmds = ctx.EndClayFrame();
	Render(ctx.game, &dst, cmds);
	RegisterWidgets(this, dst.w, dst.h, ustate, ctx.updater.GetRetryCount());
}

void UpdateScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
