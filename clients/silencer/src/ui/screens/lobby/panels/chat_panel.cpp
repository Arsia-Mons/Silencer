#include "chat_panel.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "objecttypes.h"
#include "interface.h"
#include "overlay.h"
#include "textbox.h"
#include "textinput.h"
#include "scrollbar.h"

#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include "cocoawrapper.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum ChatPanelOverlay : Uint8 {
	CHT_OVL_CHANNEL = 1,
};
enum ChatPanelTextbox : Uint8 {
	CHT_TB_PRESENCE = 9,
};
}

void ChatPanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	Interface * chatinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	chatinterface->x = 15;
	chatinterface->y = 216;
	chatinterface->width = 368;
	chatinterface->height = 234;
	Overlay * chatborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	chatborder->res_bank = 7;
	chatborder->res_index = 11;
	Overlay * chatinputborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	chatinputborder->res_bank = 7;
	chatinputborder->res_index = 14;
	Overlay * channeltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	channeltext->uid = CHT_OVL_CHANNEL;
	channeltext->textbank = 134;
	channeltext->textwidth = 8;
	channeltext->x = 15;
	channeltext->y = 200;
	TextBox * textbox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	textbox->x = 19;
	textbox->y = 220;
	textbox->width = 242;
	textbox->height = 207;
	textbox->res_bank = 133;
	textbox->lineheight = 11;
	textbox->fontwidth = 6;
	textbox->bottomtotop = true;
	TextBox * presencebox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	presencebox->x = 267;
	presencebox->y = 220;
	presencebox->width = 110;
	presencebox->height = 207;
	presencebox->res_bank = 133;
	presencebox->lineheight = 11;
	presencebox->fontwidth = 6;
	presencebox->bottomtotop = false;
	presencebox->uid = CHT_TB_PRESENCE;
	TextInput * chatinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	chatinput->x = 18;
	chatinput->y = 437;
	chatinput->width = 360;
	chatinput->height = 14;
	chatinput->res_bank = 133;
	chatinput->fontwidth = 6;
	chatinput->maxchars = 200;
	chatinput->maxwidth = 60;
	chatinput->uid = 1;
	ScrollBar * chatscrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	chatscrollbar->res_index = 12;
	chatscrollbar->barres_index = 13;
	chatscrollbar->scrollpixels = textbox->lineheight;
	chatscrollbar->scrollposition = textbox->scrolled;
	chatinterface->AddObject(chatborder->id);
	chatinterface->AddObject(chatinputborder->id);
	chatinterface->AddObject(channeltext->id);
	chatinterface->AddObject(textbox->id);
	chatinterface->AddObject(presencebox->id);
	chatinterface->AddObject(chatinput->id);
	chatinterface->AddObject(chatscrollbar->id);
	chatinterface->AddTabObject(chatinput->id);
	chatinterface->scrollbar = chatscrollbar->id;

	interfaceId = chatinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
		parent->activeobject = interfaceId;
		parent->ActiveChanged(world, parent, false);
	}
	// Mirror onto Game so the legacy ProcessLobbyInterface walk and the
	// joining/create flows can still find the chat iface id during the
	// multi-stage migration. Removed in stage H.
	ctx.game.chatinterface = interfaceId;
}

void ChatPanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object) continue;
		switch(object->type){
			case ObjectTypes::TEXTINPUT:{
				TextInput * textinput = static_cast<TextInput *>(object);
				if(iface->activeobject == textinput->id){
					if(textinput->enterpressed && strlen(textinput->text) > 0){
						world.lobby.SendChat(world.lobby.channel, textinput->text);
						textinput->Clear();
					}
				}
				if(textinput->enterpressed){
					textinput->enterpressed = false;
				}
			}break;
			case ObjectTypes::TEXTBOX:{
				TextBox * textbox = static_cast<TextBox *>(object);
				if(textbox->uid == CHT_TB_PRESENCE){
					if(world.lobby.presencechanged || !world.lobby.gamesprocessed){
						textbox->text.clear();
						textbox->scrolled = 0;
						struct Row { Uint8 group; std::string label; };
						std::vector<Row> rows;
						for(auto & kv : world.lobby.presence){
							Lobby::PresenceEntry & e = kv.second;
							Row r;
							r.label = e.name;
							r.group = (e.status <= 2) ? e.status : 0;
							if(e.gameid != 0){
								LobbyGame * g = world.lobby.GetGameById(e.gameid);
								if(g){
									r.label += " [";
									r.label += g->name;
									r.label += "]";
								}
							}
							rows.push_back(r);
						}
						std::sort(rows.begin(), rows.end(), [](const Row & a, const Row & b){
							if(a.group != b.group) return a.group < b.group;
							return a.label < b.label;
						});
						Uint8 lastgroup = 255;
						for(auto & r : rows){
							if(r.group != lastgroup){
								const char * header = (r.group == 0) ? "In Lobby" : (r.group == 1) ? "Pregame" : "Playing";
								textbox->AddText(header, 0, 128 + 32, 0, false);
								lastgroup = r.group;
							}
							textbox->AddText(r.label.c_str(), 0, 128, 2, false);
						}
						world.lobby.presencechanged = false;
					}
				}else{
					Object * sbobject = world.GetObjectFromId(iface->scrollbar);
					ScrollBar * scrollbar = static_cast<ScrollBar *>(sbobject);
					if(ctx.game.minimized && !world.lobby.chatmessages.empty()){
#ifdef _WIN32
						HWND hwnd = ctx.window ? (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(ctx.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL) : NULL;
						if(hwnd){
							FLASHWINFO flashinfo;
							flashinfo.cbSize = sizeof(flashinfo);
							flashinfo.hwnd = hwnd;
							flashinfo.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
							flashinfo.uCount = 0xFFFFFFFF;
							flashinfo.dwTimeout = 0;
							FlashWindowEx(&flashinfo);
						}
#endif
#ifdef __APPLE__
						RequestUserAttention();
#endif
					}
					bool scroll = false;
					while(!world.lobby.chatmessages.empty()){
						auto message = world.lobby.chatmessages.front();
						Uint8 color = message[strlen(message.data()) + 1];
						Uint8 brightness = message[strlen(message.data()) + 2];
						if(scrollbar && scrollbar->scrollposition == scrollbar->scrollmax){
							scroll = true;
						}
						textbox->AddText(message.data(), color, brightness, 2, scroll);
						world.lobby.chatmessages.pop_front();
						if(scrollbar){
							scrollbar->scrollposition = textbox->scrolled;
						}
					}
					if(scrollbar){
						textbox->scrolled = scrollbar->scrollposition;
						if(textbox->text.size() > std::ceil(float(textbox->height) / textbox->lineheight)){
							scrollbar->draw = true;
							scrollbar->scrollmax = textbox->text.size() - std::ceil(float(textbox->height) / textbox->lineheight);
						}else{
							scrollbar->draw = false;
						}
					}
				}
			}break;
			case ObjectTypes::OVERLAY:{
				Overlay * overlay = static_cast<Overlay *>(object);
				if(overlay->uid == CHT_OVL_CHANNEL && world.lobby.channelchanged){
					if(world.lobby.lastchannel[0] == '\0'){
						strcpy(world.lobby.lastchannel, world.lobby.channel);
					}
					overlay->text = world.lobby.channel;
					world.lobby.channelchanged = false;
				}
			}break;
		}
	}
}

void ChatPanel::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
