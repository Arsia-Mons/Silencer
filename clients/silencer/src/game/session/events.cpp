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
// SIL-15: the legacy Clay UI-input collection (text/wheel/pointer-window
// events feeding silencer::client_ui::ClientUiInput) is gone with the Clay
// layer. The live cppx UI polls the pointer directly in
// GameUiPipeline::RenderCppxClientUiFrame; nav/text routing for the
// interactive cppx screens is wired with those screens (SIL-18+). Gameplay
// shortcut keys + the keymap still flow through OnScancodeDown/Up below.
case SDL_EVENT_KEY_DOWN:
gameInput.OnScancodeDown(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = true;
break;
case SDL_EVENT_KEY_UP:
gameInput.OnScancodeUp(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = false;
break;
case SDL_EVENT_GAMEPAD_ADDED:
if(!gameInput.GetGamepad()) gameInput.OpenFirstGamepad();
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
