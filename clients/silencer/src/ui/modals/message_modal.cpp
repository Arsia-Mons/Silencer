#include "message_modal.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "interface.h"
#include "overlay.h"
#include "button.h"
#include "objecttypes.h"

#include <cstring>

MessageModal::MessageModal(std::string message_, std::function<void()> onClose_)
    : message(std::move(message_)), hasOk(true), onClose(std::move(onClose_))
{
}

MessageModal::MessageModal(std::string message_, bool ok, std::function<void()> onClose_)
    : message(std::move(message_)), hasOk(ok), onClose(std::move(onClose_))
{
}

std::unique_ptr<MessageModal> MessageModal::Progress(std::string message)
{
	return std::unique_ptr<MessageModal>(new MessageModal(std::move(message), false, nullptr));
}

void MessageModal::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	Overlay * background = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background->renderpass = 3;
	background->res_bank = 40;
	background->res_index = 4;

	Overlay * text = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	text->renderpass = 3;
	text->text = message;
	text->textbank = 134;
	text->textwidth = 8;
	text->x = 320 - ((int)(text->text.length() * text->textwidth) / 2);
	text->y = hasOk ? 200 : 218;
	textOverlayId = text->id;

	Interface * dialog = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	if(hasOk){
		Button * okbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
		okbutton->renderpass = 3;
		okbutton->x = 242;
		okbutton->y = 230;
		okbutton->SetType(Button::B156x21);
		okbutton->uid = 1;
		strcpy(okbutton->text, "OK");
		dialog->AddObject(okbutton->id);
		dialog->buttonenter = okbutton->id;
		okButtonId = okbutton->id;
	}
	dialog->AddObject(background->id);
	dialog->AddObject(text->id);
	dialog->modal = true;
	interfaceId = dialog->id;
}

void MessageModal::Tick(ScreenContext & ctx)
{
	if(!hasOk || !okButtonId) return;
	Button * okbutton = static_cast<Button *>(ctx.world.GetObjectFromId(okButtonId));
	if(okbutton && okbutton->clicked){
		okbutton->clicked = false;
		auto cb = std::move(onClose);
		ctx.PopScreen();
		// `this` is destroyed past PopScreen; invoke the user callback last.
		if(cb) cb();
	}
}

void MessageModal::Destroy(ScreenContext & ctx)
{
	if(interfaceId){
		Interface * iface = static_cast<Interface *>(ctx.world.GetObjectFromId(interfaceId));
		if(iface) iface->DestroyInterface(ctx.world);
		interfaceId = 0;
	}
}

void MessageModal::SetText(ScreenContext & ctx, const std::string & text)
{
	if(!textOverlayId) return;
	Overlay * overlay = static_cast<Overlay *>(ctx.world.GetObjectFromId(textOverlayId));
	if(overlay) overlay->text = text;
}
