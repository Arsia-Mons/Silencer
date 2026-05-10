#include "options_display_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "config.h"

#include <cstring>

namespace
{
// Prefix every uid so anon-namespace enumerators don't collide with the
// audio/controls screens under SILENCER_UNITY_BUILD (one merged TU).
enum DisplayUid : Uint8 {
	DSP_BTN_FULLSCREEN     = 0,
	DSP_BTN_SMOOTH_SCALING = 1,
	DSP_OVL_OFF_BASE       = 20,  // 20 = fullscreen indicator, 21 = smooth scaling
	DSP_OVL_ON_BASE        = 40,
	DSP_BTN_SAVE           = 200,
	DSP_BTN_CANCEL         = 201,
};
}

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;

	Overlay * title = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	title->text = "Display Options";
	title->textbank = 135;
	title->textwidth = 12;
	title->x = 320 - ((title->text.length() * title->textwidth) / 2);
	title->y = 14;

	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);

	const char * names[] = {"Fullscreen", "Smooth Scaling"};
	for(int i = 0; i < 2; i++){
		Button * c1button = (Button *)world.CreateObject(ObjectTypes::BUTTON);
		c1button->SetType(Button::B220x33);
		c1button->y = 50 + (i * 53);
		c1button->x = 100;
		c1button->uid = i;
		strcpy(c1button->text, names[i]);
		iface->AddObject(c1button->id);
		iface->AddTabObject(c1button->id);

		Overlay * off = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		off->y = 137 + (i * 53);
		off->x = 420;
		off->res_bank = 6;
		off->res_index = 12;
		off->uid = DSP_OVL_OFF_BASE + i;
		iface->AddObject(off->id);

		Overlay * on = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		on->y = 137 + (i * 53);
		on->x = 450;
		on->res_bank = 6;
		on->res_index = 14;
		on->uid = DSP_OVL_ON_BASE + i;
		iface->AddObject(on->id);
	}

	Button * savebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	savebutton->y = 117;
	savebutton->x = -200;
	savebutton->uid = DSP_BTN_SAVE;
	strcpy(savebutton->text, "Save");
	iface->AddObject(savebutton->id);
	iface->AddTabObject(savebutton->id);

	Button * cancelbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	cancelbutton->y = 117;
	cancelbutton->x = 20;
	cancelbutton->uid = DSP_BTN_CANCEL;
	strcpy(cancelbutton->text, "Cancel");
	iface->AddObject(cancelbutton->id);
	iface->AddTabObject(cancelbutton->id);

	iface->activeobject = 0;
	iface->buttonenter = savebutton->id;
	iface->buttonescape = cancelbutton->id;

	interfaceId = iface->id;
}

void OptionsDisplayScreen::Tick(ScreenContext & ctx)
{
	Interface * iface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!iface) return;
	iface->buttonenter = DSP_BTN_SAVE;
	for(Uint16 oid : iface->objects){
		Object * object = ctx.world.GetObjectFromId(oid);
		if(!object) continue;
		if(object->type == ObjectTypes::OVERLAY){
			Overlay * overlay = static_cast<Overlay *>(object);
			switch(overlay->uid){
				case DSP_OVL_OFF_BASE + 0:  // fullscreen
					overlay->res_index = Config::GetInstance().fullscreen ? 12 : 13;
					break;
				case DSP_OVL_OFF_BASE + 1:  // smooth scaling
					overlay->res_index = Config::GetInstance().scalefilter ? 12 : 13;
					break;
				case DSP_OVL_ON_BASE + 0:
					overlay->res_index = Config::GetInstance().fullscreen ? 15 : 14;
					break;
				case DSP_OVL_ON_BASE + 1:
					overlay->res_index = Config::GetInstance().scalefilter ? 15 : 14;
					break;
			}
		}else if(object->type == ObjectTypes::BUTTON){
			Button * button = static_cast<Button *>(object);
			if(button->state == Button::ACTIVE || button->state == Button::ACTIVATING){
				if(button->uid >= 0 && button->uid < 200){
					iface->buttonenter = button->id;
				}
			}
			if(!button->clicked) continue;
			switch(button->uid){
				case DSP_BTN_FULLSCREEN:{
					Config & cfg = Config::GetInstance();
					cfg.fullscreen = !cfg.fullscreen;
					ctx.SetFullscreen(cfg.fullscreen);
				}break;
				case DSP_BTN_SMOOTH_SCALING:{
					Config & cfg = Config::GetInstance();
					cfg.scalefilter = !cfg.scalefilter;
					ctx.SetScaleFilter(cfg.scalefilter);
				}break;
				case DSP_BTN_SAVE:{
					Config::GetInstance().Save();
					ctx.GoToState(GameState::OPTIONS);
				}break;
				case DSP_BTN_CANCEL:{
					Config & cfg = Config::GetInstance();
					cfg.Load();
					ctx.SetScaleFilter(cfg.scalefilter);
					ctx.SetFullscreen(cfg.fullscreen);
					ctx.GoToState(GameState::OPTIONS);
				}break;
			}
			button->clicked = false;
		}
	}
}

void OptionsDisplayScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
