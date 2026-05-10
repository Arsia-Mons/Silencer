#include "update_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "updater.h"
#include "updaterstage2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling screen .cpp files.
enum UpdateButton : Uint16 {
	UPD_BTN_UPDATE   = 250,
	UPD_BTN_CANCEL   = 251,
	UPD_BTN_RETRY    = 252,
	UPD_BTN_DOWNLOAD = 253,
};
enum UpdateOverlay : Uint8 {
	UPD_OVERLAY_STATUS   = 1,
	UPD_OVERLAY_PROGRESS = 2,
};
}

void UpdateScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(2);

	Interface * iface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	// Background (reuse modal-dialog sprite — same one CreateModalDialog uses;
	// its baked-in offsets center it, so no x/y needed).
	Overlay * background = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background->renderpass = 3;
	background->res_bank = 40;
	background->res_index = 4;
	// Status line (mutated each frame; x re-centered in Tick since text length
	// varies with state).
	Overlay * status = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	status->renderpass = 3;
	status->text = "";
	status->textbank = 134;
	status->textwidth = 8;
	status->x = 320;
	status->y = 200;
	status->uid = UPD_OVERLAY_STATUS;
	// Progress bar (sized by Tick while DOWNLOADING).
	Overlay * progress = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	progress->renderpass = 3;
	progress->text = "";
	progress->textbank = 134;
	progress->textwidth = 8;
	progress->x = 320;
	progress->y = 215;
	progress->uid = UPD_OVERLAY_PROGRESS;
	// Action buttons — UPD_BTN_UPDATE/RETRY/DOWNLOAD share the left slot;
	// exactly one is visible based on Updater state. UPD_BTN_CANCEL is always
	// on the right. Two B156x21 side-by-side centered on x=320 (mirrors
	// horizontal pair in LobbyConnectScreen, scaled up).
	Button * updatebutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	updatebutton->renderpass = 3;
	updatebutton->x = 161;
	updatebutton->y = 230;
	updatebutton->SetType(Button::B156x21);
	updatebutton->uid = UPD_BTN_UPDATE;
	strcpy(updatebutton->text, "Update");
	Button * cancelbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	cancelbutton->renderpass = 3;
	cancelbutton->x = 322;
	cancelbutton->y = 230;
	cancelbutton->SetType(Button::B156x21);
	cancelbutton->uid = UPD_BTN_CANCEL;
	strcpy(cancelbutton->text, "Cancel");
	Button * retrybutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	retrybutton->renderpass = 3;
	retrybutton->x = 161;
	retrybutton->y = 230;
	retrybutton->SetType(Button::B156x21);
	retrybutton->uid = UPD_BTN_RETRY;
	strcpy(retrybutton->text, "Retry");
	Button * openbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	openbutton->renderpass = 3;
	openbutton->x = 161;
	openbutton->y = 230;
	openbutton->SetType(Button::B156x21);
	openbutton->uid = UPD_BTN_DOWNLOAD;
	strcpy(openbutton->text, "Download");
	iface->AddObject(background->id);
	iface->AddObject(status->id);
	iface->AddObject(progress->id);
	iface->AddObject(updatebutton->id);
	iface->AddObject(cancelbutton->id);
	iface->AddObject(retrybutton->id);
	iface->AddObject(openbutton->id);
	iface->modal = true;

	interfaceId = iface->id;
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(interfaceId));
	if(!iface) return;

	Updater::State ustate = ctx.updater.GetState();
	// Pass 1: update status/progress overlays and button visibility.
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object){
			continue;
		}
		if(object->type == ObjectTypes::OVERLAY){
			Overlay * overlay = static_cast<Overlay *>(object);
			if(overlay->uid == UPD_OVERLAY_STATUS){
				switch(ustate){
					case Updater::PROMPTING:
						overlay->text = "An update is required to play online.";
					break;
					case Updater::DOWNLOADING:{
						char buf[32];
						snprintf(buf, sizeof(buf), "%d%%", int(ctx.updater.GetProgress() * 100));
						overlay->text = buf;
					}break;
					case Updater::VERIFYING:
						overlay->text = "Verifying...";
					break;
					case Updater::STAGING:
						overlay->text = "Restarting...";
					break;
					case Updater::FAILED:
						overlay->text = ctx.updater.GetErrorMessage();
					break;
					case Updater::IDLE:
					case Updater::DONE:
					default:
						overlay->text = "";
					break;
				}
				overlay->x = 320 - ((overlay->text.length() * overlay->textwidth) / 2);
			}else if(overlay->uid == UPD_OVERLAY_PROGRESS){
				// Simple textual progress indicator for now; real bar rendering
				// can be introduced later without re-touching the state wiring.
				if(ustate == Updater::DOWNLOADING){
					int width = int(ctx.updater.GetProgress() * 20.0f);
					std::string bar = "[";
					for(int i = 0; i < 20; i++){
						bar += (i < width) ? "=" : " ";
					}
					bar += "]";
					overlay->text = bar;
				}else{
					overlay->text = "";
				}
				overlay->x = 320 - ((overlay->text.length() * overlay->textwidth) / 2);
			}
		}else if(object->type == ObjectTypes::BUTTON){
			Button * button = static_cast<Button *>(object);
			bool active = false;
			switch(button->uid){
				case UPD_BTN_UPDATE:
					active = (ustate == Updater::PROMPTING);
				break;
				case UPD_BTN_CANCEL:
					active = (ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED);
				break;
				case UPD_BTN_RETRY:
					active = (ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3);
				break;
				case UPD_BTN_DOWNLOAD:
					active = (ustate == Updater::FAILED && ctx.updater.GetRetryCount() >= 3);
				break;
				default:
					continue;
			}
			// UPD_BTN_UPDATE/RETRY/DOWNLOAD overlap (same x/y); only the active
			// one draws. UPD_BTN_CANCEL always draws; the click handler below
			// guards by ustate so stale clicks during VERIFYING/STAGING are
			// no-ops. Don't touch button->state here — Interface::Tick owns
			// hover activation, and fighting it causes the hover sound to spam
			// every frame the mouse isn't over the button.
			button->draw = active || (button->uid == UPD_BTN_CANCEL);
		}
	}
	// Pass 2: dispatch button clicks.
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object || object->type != ObjectTypes::BUTTON){
			continue;
		}
		Button * button = static_cast<Button *>(object);
		if(!button->clicked){
			continue;
		}
		switch(button->uid){
			case UPD_BTN_UPDATE:{
				if(ustate == Updater::PROMPTING){
					ctx.updater.Consent();
				}
			}break;
			case UPD_BTN_CANCEL:{
				if(ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED){
					if(ustate == Updater::DOWNLOADING){
						ctx.updater.Cancel();
					}
					ctx.GoToState(GameState::MAINMENU);
				}
			}break;
			case UPD_BTN_RETRY:{
				if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
					ctx.updater.Retry();
				}
			}break;
			case UPD_BTN_DOWNLOAD:{
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
				}
			}break;
		}
		button->clicked = false;
	}
	// If the updater reached STAGING, spawn the stage-2 child. On success,
	// flag the Updater so Game::Loop returns false next tick and main()
	// unwinds — that lets ~Game tear down SDL/audio cleanly before the new
	// process opens the device (skipping it produces an audible pop).
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

void UpdateScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
