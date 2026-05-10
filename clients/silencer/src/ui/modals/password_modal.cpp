#include "password_modal.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "interface.h"
#include "overlay.h"
#include "button.h"
#include "textinput.h"
#include "objecttypes.h"
#include "resources.h"

#include <cstring>

PasswordModal::PasswordModal(std::function<void(const char *)> onSubmit_)
    : onSubmit(std::move(onSubmit_))
{
}

void PasswordModal::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background->renderpass = 3;
	background->res_bank = 40;
	background->res_index = 2;
	background->x = 320 - (world.resources.spritewidth[background->res_bank][background->res_index] / 2);
	background->y = 240 - (world.resources.spriteheight[background->res_bank][background->res_index] / 2);

	Overlay * text = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	text->renderpass = 3;
	text->text = "This game requires a password";
	text->textbank = 134;
	text->textwidth = 8;
	text->x = 320 - ((int)(text->text.length() * text->textwidth) / 2);
	text->y = 196;

	TextInput * passwordinput = static_cast<TextInput *>(world.CreateObject(ObjectTypes::TEXTINPUT));
	passwordinput->renderpass = 3;
	passwordinput->x = 210;
	passwordinput->y = 243;
	passwordinput->width = 180;
	passwordinput->height = 14;
	passwordinput->res_bank = 135;
	passwordinput->fontwidth = 11;
	passwordinput->maxchars = 20;
	passwordinput->maxwidth = 20;
	passwordinput->password = true;
	passwordinput->uid = 1;
	inputId = passwordinput->id;

	Button * okbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	okbutton->renderpass = 3;
	okbutton->x = 242;
	okbutton->y = 267;
	okbutton->SetType(Button::B156x21);
	okbutton->uid = 2;
	strcpy(okbutton->text, "OK");
	okButtonId = okbutton->id;

	Interface * dialog = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	dialog->AddObject(background->id);
	dialog->AddObject(text->id);
	dialog->AddObject(passwordinput->id);
	dialog->AddObject(okbutton->id);
	dialog->AddTabObject(passwordinput->id);
	dialog->AddTabObject(okbutton->id);
	dialog->activeobject = passwordinput->id;
	dialog->buttonenter = okbutton->id;
	dialog->buttonescape = okbutton->id;
	dialog->modal = true;
	interfaceId = dialog->id;
}

void PasswordModal::Tick(ScreenContext & ctx)
{
	Button * okbutton = static_cast<Button *>(ctx.world.GetObjectFromId(okButtonId));
	if(!okbutton) return;
	if(!okbutton->clicked) return;
	okbutton->clicked = false;
	TextInput * input = static_cast<TextInput *>(ctx.world.GetObjectFromId(inputId));
	std::string captured = input ? input->text : "";
	auto cb = std::move(onSubmit);
	ctx.PopScreen();
	if(cb) cb(captured.c_str());
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	if(interfaceId){
		Interface * iface = static_cast<Interface *>(ctx.world.GetObjectFromId(interfaceId));
		if(iface) iface->DestroyInterface(ctx.world);
		interfaceId = 0;
	}
}
