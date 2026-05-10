#include "options_audio_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "config.h"
#include "audio.h"

#include <cstring>

namespace
{
// Prefix every uid so anon-namespace enumerators don't collide with the
// display/controls screens under SILENCER_UNITY_BUILD (one merged TU).
enum AudioUid : Uint8 {
	AUD_BTN_MUSIC = 0,
	AUD_OVL_OFF   = 20,
	AUD_OVL_ON    = 40,
	AUD_BTN_SAVE  = 200,
	AUD_BTN_CANCEL = 201,
};

void ApplyMusicSetting(bool on)
{
	if(on){
		Audio::GetInstance().ResumeMusic();
	}else{
		Audio::GetInstance().PauseMusic();
	}
}
}

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;

	Overlay * title = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	title->text = "Audio Options";
	title->textbank = 135;
	title->textwidth = 12;
	title->x = 320 - ((title->text.length() * title->textwidth) / 2);
	title->y = 14;

	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);

	const char * names[] = {"Music"};
	for(int i = 0; i < 1; i++){
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
		off->uid = AUD_OVL_OFF + i;
		iface->AddObject(off->id);

		Overlay * on = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		on->y = 137 + (i * 53);
		on->x = 450;
		on->res_bank = 6;
		on->res_index = 14;
		on->uid = AUD_OVL_ON + i;
		iface->AddObject(on->id);
	}

	Button * savebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	savebutton->y = 117;
	savebutton->x = -200;
	savebutton->uid = AUD_BTN_SAVE;
	strcpy(savebutton->text, "Save");
	iface->AddObject(savebutton->id);
	iface->AddTabObject(savebutton->id);

	Button * cancelbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	cancelbutton->y = 117;
	cancelbutton->x = 20;
	cancelbutton->uid = AUD_BTN_CANCEL;
	strcpy(cancelbutton->text, "Cancel");
	iface->AddObject(cancelbutton->id);
	iface->AddTabObject(cancelbutton->id);

	iface->activeobject = 0;
	iface->buttonenter = savebutton->id;
	iface->buttonescape = cancelbutton->id;

	interfaceId = iface->id;
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	Interface * iface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!iface) return;
	iface->buttonenter = AUD_BTN_SAVE;
	for(Uint16 oid : iface->objects){
		Object * object = ctx.world.GetObjectFromId(oid);
		if(!object) continue;
		if(object->type == ObjectTypes::OVERLAY){
			Overlay * overlay = static_cast<Overlay *>(object);
			switch(overlay->uid){
				case AUD_OVL_OFF:
					overlay->res_index = Config::GetInstance().music ? 12 : 13;
					break;
				case AUD_OVL_ON:
					overlay->res_index = Config::GetInstance().music ? 15 : 14;
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
				case AUD_BTN_MUSIC:{
					Config & cfg = Config::GetInstance();
					cfg.music = !cfg.music;
					ApplyMusicSetting(cfg.music);
				}break;
				case AUD_BTN_SAVE:{
					Config::GetInstance().Save();
					ctx.GoToState(GameState::OPTIONS);
				}break;
				case AUD_BTN_CANCEL:{
					Config & cfg = Config::GetInstance();
					cfg.Load();
					ApplyMusicSetting(cfg.music);
					ctx.GoToState(GameState::OPTIONS);
				}break;
			}
			button->clicked = false;
		}
	}
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
