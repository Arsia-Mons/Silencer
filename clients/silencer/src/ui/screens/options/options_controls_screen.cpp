#include "options_controls_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "scrollbar.h"
#include "config.h"

#include <cstring>
#include <string>

namespace
{
constexpr int VISIBLE_ROWS = 5;
constexpr int PRIMARY_SLOT_BASE   = 0;    // uids 0..99   = primary key buttons
constexpr int SECONDARY_SLOT_BASE = 100;  // uids 100..149 = secondary key buttons
constexpr int OR_AND_TOGGLE_BASE  = 150;  // uids 150..199 = OR/AND toggle
constexpr int SAVE_BTN_UID    = 200;
constexpr int CANCEL_BTN_UID  = 201;
constexpr int PRESET_BTN_UID  = 250;
constexpr int REBIND_TIMEOUT_TICKS = 72;

// Mirrors the screen-private "is this a built-in profile?" predicate used by
// keybinds.cpp's ForkActiveProfileIfBuiltin. Kept renamed to dodge file-static
// collision under SILENCER_UNITY_BUILD (keybinds.cpp has a `static
// IsBuiltinProfile`).
bool IsBuiltinKeybindProfile(const std::string & name)
{
	return name == "default" || name == "wasd" || name == "gamepad";
}
}

OptionsControlsScreen::LegacyView OptionsControlsScreen::ViewLegacy(const KeyMap & km, Action a)
{
	LegacyView v;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return v;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
		v.key2 = (SDL_Scancode)b0.keys[1].code;
		v.and_ = true;
		return v;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			v.key2 = (SDL_Scancode)b1.keys[0].code;
		}
	}
	return v;
}

void OptionsControlsScreen::WriteLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_)
{
	auto & ab = km.Get(a);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = (int)sc;
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1)); b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

void OptionsControlsScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 6;
	background->res_index = 0;

	Overlay * background2 = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background2->res_bank = 7;
	background2->res_index = 7;

	Overlay * title = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	title->text = "Configure Controls";
	title->textbank = 135;
	title->textwidth = 12;
	title->x = 320 - ((title->text.length() * title->textwidth) / 2);
	title->y = 14;

	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);

	// Preset cycle row. Static "Preset:" label on the left at the same x/y as
	// binding row labels; cycle button on the right carrying the profile name.
	// B220x33 keeps the same vertical sprite anchor as the (now-shifted)
	// binding row buttons (B112x33), so y=0 places it where row 0 used to
	// render. The visible button is left-aligned with c1button[0]; both render
	// with bank-6 sprites but at different indices, so we add the sprite-offset
	// delta (B220x33 res_index 23 vs B112x33 res_index 28) to c1's obj-space x
	// — see resources.cpp:66 for the header read.
	Overlay * presetlabel = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	presetlabel->text = "Preset:";
	presetlabel->textbank  = 134;
	presetlabel->textwidth = 10;
	presetlabel->x = 80;
	presetlabel->y = 95;

	presetbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	presetbutton->SetType(Button::B220x33);
	presetbutton->x = -30 + (world.resources.spriteoffsetx[6][23] - world.resources.spriteoffsetx[6][28]);
	presetbutton->y = 0;
	presetbutton->uid = PRESET_BTN_UID;
	strcpy(presetbutton->text, "");
	iface->AddObject(presetbutton->id);
	iface->AddTabObject(presetbutton->id);

	for(int i = 0; i < VISIBLE_ROWS; i++){
		int slot = i + 1;  // visual row index — preset takes slot 0

		Overlay * name = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		keynameoverlay[i] = name;
		name->textbank = 134;
		name->textwidth = 10;
		name->x = 80;
		name->y = 95 + (slot * 53);

		Button * c1 = (Button *)world.CreateObject(ObjectTypes::BUTTON);
		c1button[i] = c1;
		c1->SetType(Button::B112x33);
		c1->y = 0 + (slot * 53);
		c1->x = -30;
		c1->uid = PRIMARY_SLOT_BASE + i;
		strcpy(c1->text, "");
		iface->AddObject(c1->id);
		iface->AddTabObject(c1->id);

		Button * op = (Button *)world.CreateObject(ObjectTypes::BUTTON);
		cobutton[i] = op;
		op->SetType(Button::BNONE);
		op->y = 95 + (slot * 53);
		op->x = 383;
		op->uid = OR_AND_TOGGLE_BASE + i;
		op->width = 40;
		op->height = 30;
		op->textbank = 134;
		op->textwidth = 9;
		strcpy(op->text, "");
		iface->AddObject(op->id);

		Button * c2 = (Button *)world.CreateObject(ObjectTypes::BUTTON);
		c2button[i] = c2;
		c2->SetType(Button::B112x33);
		c2->y = 0 + (slot * 53);
		c2->x = 120;
		c2->uid = SECONDARY_SLOT_BASE + i;
		strcpy(c2->text, "");
		iface->AddObject(c2->id);
		iface->AddTabObject(c2->id);
	}
	iface->objectupscroll = c1button[0]->id;
	iface->objectdownscroll = c2button[VISIBLE_ROWS - 1]->id;

	ScrollBar * scrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	scrollbar->res_index = 9;
	scrollbar->scrollpixels = 53;
	scrollbar->scrollposition = 0;
	scrollbar->scrollmax = (int)Action::Count - VISIBLE_ROWS;
	iface->AddObject(scrollbar->id);
	iface->scrollbar = scrollbar->id;

	Button * savebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	savebutton->y = 117;
	savebutton->x = -200;
	savebutton->uid = SAVE_BTN_UID;
	strcpy(savebutton->text, "Save");
	iface->AddObject(savebutton->id);
	iface->AddTabObject(savebutton->id);

	Button * cancelbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	cancelbutton->y = 117;
	cancelbutton->x = 20;
	cancelbutton->uid = CANCEL_BTN_UID;
	strcpy(cancelbutton->text, "Cancel");
	iface->AddObject(cancelbutton->id);
	iface->AddTabObject(cancelbutton->id);

	iface->activeobject = 0;
	iface->buttonenter = savebutton->id;
	iface->buttonescape = cancelbutton->id;

	interfaceId = iface->id;
}

void OptionsControlsScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	KeyMap & keymap = ctx.keymap;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;
	ScrollBar * scrollbar = (ScrollBar *)world.GetObjectFromId(iface->scrollbar);

	if(!iface->disabled && scrollbar){
		for(int i = 0; i < VISIBLE_ROWS; i++){
			int row = i + scrollbar->scrollposition;
			if(row < 0 || row >= (int)Action::Count) continue;
			Action a = ACTION_TABLE[row].action;
			LegacyView v = ViewLegacy(keymap, a);
			keynameoverlay[i]->text = std::string(GetActionInfo(a).label) + ":";
			strcpy(c1button[i]->text, KeyMap::GetKeyName(v.key1));
			strcpy(c2button[i]->text, KeyMap::GetKeyName(v.key2));
		}
		// Preset button text reflects the active profile's label.
		// The static "Preset:" overlay to its left supplies the noun.
		std::string presetText = !keymap.label.empty() ? keymap.label
		                       : !keymap.name.empty()  ? keymap.name
		                       : std::string(Config::GetInstance().active_keybind_profile);
		if(presetText.size() >= sizeof(presetbutton->text)){
			presetText.resize(sizeof(presetbutton->text) - 1);
		}
		strcpy(presetbutton->text, presetText.c_str());
	}

	iface->buttonenter = SAVE_BTN_UID;
	for(Uint16 oid : iface->objects){
		Object * object = world.GetObjectFromId(oid);
		if(!object || object->type != ObjectTypes::BUTTON) continue;
		Button * button = static_cast<Button *>(object);

		if(button->state == Button::ACTIVE || button->state == Button::ACTIVATING){
			if((button->uid >= 0 && button->uid < SAVE_BTN_UID) || button->uid == PRESET_BTN_UID){
				iface->buttonenter = button->id;
			}
		}

		// OR/AND toggle text follows the active binding shape every tick.
		if(button->uid >= OR_AND_TOGGLE_BASE && button->uid < SAVE_BTN_UID && scrollbar){
			int row = button->uid - OR_AND_TOGGLE_BASE + scrollbar->scrollposition;
			if(row >= 0 && row < (int)Action::Count){
				Action a = ACTION_TABLE[row].action;
				strcpy(button->text, ViewLegacy(keymap, a).and_ ? "AND" : "OR");
			}
		}

		// Capture the next keypress for whichever key-slot button was clicked
		// and put the UI back into edit mode. iface->disabled gates the rest of
		// the UI while we wait for input.
		if(button->uid >= PRIMARY_SLOT_BASE && button->uid < OR_AND_TOGGLE_BASE && scrollbar){
			if(iface->disabled && button->state == Button::ACTIVE &&
			   (iface->lastsym != SDL_SCANCODE_UNKNOWN || world.tickcount - optionscontrolstick > REBIND_TIMEOUT_TICKS)){
				int slot = button->uid;     // 0..99 = primary; 100..149 = secondary
				int row  = (slot < SECONDARY_SLOT_BASE ? slot : slot - SECONDARY_SLOT_BASE) + scrollbar->scrollposition;
				SDL_Scancode sym = iface->lastsym;
				if(world.tickcount - optionscontrolstick > REBIND_TIMEOUT_TICKS){
					sym = SDL_SCANCODE_UNKNOWN;
				}
#ifndef OUYA
				if(sym == SDL_SCANCODE_ESCAPE){
					sym = SDL_SCANCODE_UNKNOWN;
				}
#endif
				strcpy(button->text, KeyMap::GetKeyName(sym));
				if(row >= 0 && row < (int)Action::Count){
					Action a = ACTION_TABLE[row].action;
					LegacyView v = ViewLegacy(keymap, a);
					if(slot < SECONDARY_SLOT_BASE) v.key1 = sym; else v.key2 = sym;
					ForkActiveProfileIfBuiltin(ctx.keymap);
					WriteLegacy(keymap, a, v.key1, v.key2, v.and_);
				}
				iface->disabled = false;
			}
		}

		if(!button->clicked) continue;

		if(button->uid >= PRIMARY_SLOT_BASE && button->uid < OR_AND_TOGGLE_BASE){
			strcpy(button->text, "-");
			iface->disabled = true;
			optionscontrolstick = world.tickcount;
			iface->lastsym = SDL_SCANCODE_UNKNOWN;
		}

		if(button->uid >= OR_AND_TOGGLE_BASE && button->uid < SAVE_BTN_UID && scrollbar){
			int row = button->uid - OR_AND_TOGGLE_BASE + scrollbar->scrollposition;
			if(row >= 0 && row < (int)Action::Count){
				Action a = ACTION_TABLE[row].action;
				LegacyView v = ViewLegacy(keymap, a);
				v.and_ = !v.and_;
				ForkActiveProfileIfBuiltin(ctx.keymap);
				WriteLegacy(keymap, a, v.key1, v.key2, v.and_);
			}
		}

		if(button->uid == PRESET_BTN_UID){
			CycleKeybindPreset(ctx.keymap);
		}

		switch(button->uid){
			case SAVE_BTN_UID:{
				// Persist the live keymap to the active profile's writable file,
				// then save the rest of Config (which is now just the
				// active_keybind_profile pointer plus graphics/audio/network).
				// Skip writing built-ins — any edit forks via
				// ForkActiveProfileIfBuiltin, so a built-in name here means
				// cycle-only (no edits) and shouldn't shadow the on-disk
				// built-in with a copy.
				const std::string active = Config::GetInstance().active_keybind_profile;
				if(!IsBuiltinKeybindProfile(active)){
					keymap.SaveFile(WritableProfilePath(active));
				}
				Config::GetInstance().Save();
				ctx.GoToState(GameState::OPTIONS);
			}break;
			case CANCEL_BTN_UID:{
				// Reload the keymap from disk so any in-UI edits are discarded.
				LoadActiveKeymap(ctx.keymap);
				Config::GetInstance().Load();
				ctx.GoToState(GameState::OPTIONS);
			}break;
		}
		button->clicked = false;
	}
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
