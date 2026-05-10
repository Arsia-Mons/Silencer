#include "main_menu_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"

#include <cstring>

namespace
{
enum MainMenuButton : Uint8 {
	BTN_TUTORIAL    = 0,
	BTN_LOBBY       = 1,
	BTN_OPTIONS     = 2,
	BTN_EXIT        = 3,
};
}

void MainMenuScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;

	Overlay * logo = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	logo->res_bank = 208;

	Overlay * version = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	version->text = "Silencer v";
	version->text += world.GetVersion();
	version->textbank = 133;
	version->textwidth = 11;
	version->x = 10;
	version->y = 480 - 10 - 7;

	Button * startbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	startbutton->y = -134;
	startbutton->x = 40;
	startbutton->uid = BTN_TUTORIAL;
	strcpy(startbutton->text, "Tutorial");

	Button * lobbybutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	lobbybutton->y = -67;
	lobbybutton->x = 80;
	lobbybutton->uid = BTN_LOBBY;
	strcpy(lobbybutton->text, "Connect To Lobby");

	Button * optionsbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	strcpy(optionsbutton->text, "Options");
	optionsbutton->x = 40;
	optionsbutton->uid = BTN_OPTIONS;

	Button * exitbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	strcpy(exitbutton->text, "Exit");
	exitbutton->x = 0;
	exitbutton->y = 67;
	exitbutton->uid = BTN_EXIT;

	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	iface->AddObject(startbutton->id);
	iface->AddObject(lobbybutton->id);
	iface->AddObject(optionsbutton->id);
	iface->AddObject(exitbutton->id);
	iface->AddTabObject(startbutton->id);
	iface->AddTabObject(lobbybutton->id);
	iface->AddTabObject(optionsbutton->id);
	iface->AddTabObject(exitbutton->id);
	iface->activeobject = 0;
	iface->buttonescape = exitbutton->id;

	interfaceId = iface->id;
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	Interface * iface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!iface) return;
	for(Uint16 oid : iface->objects){
		Object * object = ctx.world.GetObjectFromId(oid);
		if(!object || object->type != ObjectTypes::BUTTON) continue;
		Button * button = static_cast<Button *>(object);
		if(!button->clicked) continue;
		switch(button->uid){
			case BTN_TUTORIAL: ctx.GoToState(GameState::SINGLEPLAYERGAME); break;
			case BTN_LOBBY:    ctx.GoToState(GameState::LOBBYCONNECT);    break;
			case BTN_OPTIONS:  ctx.GoToState(GameState::OPTIONS);         break;
			case BTN_EXIT:     ctx.RequestQuit();                         break;
		}
	}
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
