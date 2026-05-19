#include "game.h"

#include "audio.h"
#include "config.h"
#include "world.h"
#include <cstring>

bool Game::HandleSDLEvents(){
if(world.dedicatedserver.active){
return true;
}
if(headless || tui){
return true;
}
SDL_Event event;
while(SDL_PollEvent(&event) > 0){
switch(event.type){
case SDL_EVENT_WINDOW_RESIZED:
case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
SyncRenderSurfaceToWindowPixels();
break;
case SDL_EVENT_WINDOW_FOCUS_GAINED:
if(!world.replay.IsPlaying()) Audio::GetInstance().Unmute();
minimized = false;
break;
case SDL_EVENT_WINDOW_FOCUS_LOST:
if(!world.replay.IsPlaying()) Audio::GetInstance().Mute(25);
minimized = true;
break;
case SDL_EVENT_WINDOW_MINIMIZED:
minimized = true;
break;
case SDL_EVENT_WINDOW_MAXIMIZED:
case SDL_EVENT_WINDOW_RESTORED:
minimized = false;
break;
case SDL_EVENT_TEXT_INPUT: {
char ascii = event.text.text[0] & 0x7F;
UiInput().QueueTextInput(ascii);
} break;
case SDL_EVENT_KEY_DOWN:
OnScancodeDown(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = true;
QueueUiKeyboardInputForScancode(event.key.scancode);
break;
case SDL_EVENT_KEY_UP:
OnScancodeUp(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = false;
break;
case SDL_EVENT_MOUSE_WHEEL:
UiInput().AddWheelDelta(event.wheel.x, event.wheel.y);
break;
case SDL_EVENT_MOUSE_BUTTON_DOWN:
if(event.button.button == SDL_BUTTON_LEFT){
int windowW = 0;
int windowH = 0;
SDL_GetWindowSize(gameRenderer.GetWindow(), &windowW, &windowH);
UiInput().QueuePointerWindowEvent(
event.button.x, event.button.y, windowW, windowH,
GetScreenBuffer().w, GetScreenBuffer().h, true, false);
}
break;
case SDL_EVENT_MOUSE_BUTTON_UP:
if(event.button.button == SDL_BUTTON_LEFT){
int windowW = 0;
int windowH = 0;
SDL_GetWindowSize(gameRenderer.GetWindow(), &windowW, &windowH);
UiInput().QueuePointerWindowEvent(
event.button.x, event.button.y, windowW, windowH,
GetScreenBuffer().w, GetScreenBuffer().h, false, true);
}
break;
case SDL_EVENT_MOUSE_MOTION: {
int windowW = 0;
int windowH = 0;
SDL_GetWindowSize(gameRenderer.GetWindow(), &windowW, &windowH);
UiInput().QueuePointerWindowEvent(
event.motion.x, event.motion.y, windowW, windowH,
GetScreenBuffer().w, GetScreenBuffer().h, false, false);
} break;
case SDL_EVENT_GAMEPAD_ADDED:
if(!gameInput.GetGamepad()) OpenFirstGamepad();
break;
case SDL_EVENT_GAMEPAD_REMOVED:
if(gameInput.GetGamepad() && event.gdevice.which == SDL_GetGamepadID(gameInput.GetGamepad())){
SDL_CloseGamepad(gameInput.GetGamepad());
gameInput.GamepadRef() = nullptr;
gameInput.GetGamepadStateMutable().connected = false;
if(!gameInput.PrevGamepadProfileRef().empty()){
std::strncpy(Config::GetInstance().active_keybind_profile,
             gameInput.PrevGamepadProfileRef().c_str(),
             sizeof(Config::GetInstance().active_keybind_profile) - 1);
Config::GetInstance().active_keybind_profile[
sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
LoadActiveKeymap(gameInput.GetKeyMap());
gameInput.PrevGamepadProfileRef().clear();
}
}
break;
case SDL_EVENT_QUIT:
return false;
}
}
return true;
}
