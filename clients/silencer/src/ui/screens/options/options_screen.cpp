#include "options_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"

#include <cstring>

namespace
{
enum OptionsButton : Uint8 {
	BTN_GO_BACK  = 0,
	BTN_CONTROLS = 1,
	BTN_DISPLAY  = 2,
	BTN_AUDIO    = 3,
};
}

void OptionsScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;

	Button * controlsbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	controlsbutton->y = -142;
	controlsbutton->x = -89;
	controlsbutton->uid = BTN_CONTROLS;
	strcpy(controlsbutton->text, "Controls");

	Button * displaybutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	displaybutton->y = -90;
	displaybutton->x = -89;
	displaybutton->uid = BTN_DISPLAY;
	strcpy(displaybutton->text, "Display");

	Button * audiobutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	audiobutton->y = -38;
	audiobutton->x = -89;
	audiobutton->uid = BTN_AUDIO;
	strcpy(audiobutton->text, "Audio");

	Button * gobackbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gobackbutton->y = 15;
	gobackbutton->x = -89;
	gobackbutton->uid = BTN_GO_BACK;
	strcpy(gobackbutton->text, "Go Back");

	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	iface->AddObject(controlsbutton->id);
	iface->AddObject(displaybutton->id);
	iface->AddObject(audiobutton->id);
	iface->AddObject(gobackbutton->id);
	iface->AddTabObject(controlsbutton->id);
	iface->AddTabObject(displaybutton->id);
	iface->AddTabObject(audiobutton->id);
	iface->AddTabObject(gobackbutton->id);
	iface->activeobject = 0;
	iface->buttonescape = gobackbutton->id;

	interfaceId = iface->id;
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	Interface * iface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!iface) return;
	for(Uint16 oid : iface->objects){
		Object * object = ctx.world.GetObjectFromId(oid);
		if(!object || object->type != ObjectTypes::BUTTON) continue;
		Button * button = static_cast<Button *>(object);
		if(!button->clicked) continue;
		switch(button->uid){
			case BTN_GO_BACK:  ctx.GoToState(GameState::MAINMENU);       break;
			case BTN_CONTROLS: ctx.GoToState(GameState::OPTIONSCONTROLS); break;
			case BTN_DISPLAY:  ctx.GoToState(GameState::OPTIONSDISPLAY);  break;
			case BTN_AUDIO:    ctx.GoToState(GameState::OPTIONSAUDIO);    break;
		}
		button->clicked = false;
	}
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
