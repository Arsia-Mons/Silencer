#include "ingame_chat.h"

#include "world.h"

#include <SDL3/SDL_scancode.h>
#include <cstring>

namespace ui { namespace v2 {

IngameChat::IngameChat(World & world)
	: world_(world) {}

void IngameChat::Activate(){
	world_.ingame_chat_active = true;
	world_.ingame_chat_text[0] = 0;
	// Surface the chat scrollback the same way the legacy
	// chatinterfaceid path implicitly did via the renderer's
	// `world.showchat_i || player->chatinterfaceid` predicate.
}

void IngameChat::Cancel(){
	world_.ingame_chat_active = false;
	world_.ingame_chat_text[0] = 0;
}

void IngameChat::Submit(){
	if(world_.ingame_chat_text[0] != 0){
		world_.SendChat(world_.ingame_chat_with_team, world_.ingame_chat_text);
	}
	Cancel();
}

void IngameChat::ToggleTeam(){
	world_.ingame_chat_with_team = !world_.ingame_chat_with_team;
}

void IngameChat::AppendChar(char ascii){
	if(!world_.ingame_chat_active) return;
	size_t len = strlen(world_.ingame_chat_text);
	if((int)len >= kMaxChars) return;
	world_.ingame_chat_text[len]     = ascii;
	world_.ingame_chat_text[len + 1] = 0;
}

void IngameChat::Backspace(){
	if(!world_.ingame_chat_active) return;
	size_t len = strlen(world_.ingame_chat_text);
	if(len == 0) return;
	world_.ingame_chat_text[len - 1] = 0;
}

bool IngameChat::Active() const          { return world_.ingame_chat_active;     }
bool IngameChat::WithTeam() const        { return world_.ingame_chat_with_team;  }
const char * IngameChat::Text() const    { return world_.ingame_chat_text;       }

bool IngameChat::DispatchKey(int sdl_scancode){
	if(!Active()) return false;
	switch(sdl_scancode){
		case SDL_SCANCODE_RETURN:    Submit();      return true;
		case SDL_SCANCODE_ESCAPE:    Cancel();      return true;
		case SDL_SCANCODE_TAB:       ToggleTeam();  return true;
		case SDL_SCANCODE_BACKSPACE: Backspace();   return true;
		default: break;
	}
	// Swallow all keys while the overlay is active so they don't leak
	// to gameplay input (e.g. movement keys while typing).
	return true;
}

bool IngameChat::DispatchText(char ascii){
	if(!Active()) return false;
	AppendChar(ascii);
	return true;
}

}}  // namespace ui::v2
