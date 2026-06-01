#include "game.h"

#include "audio.h"
#include "config.h"
#include "ui/input.h"
#include "world.h"
#include <cstring>

namespace {
// SDL keycode -> the runtime's text-editing UiKey vocabulary (src/ui/input.h).
// Only the keys the cppx Input cares about map; everything else stays Unknown
// (printable characters arrive as SDL_EVENT_TEXT_INPUT, not as key events).
::ui::UiKey UiKeyFromSdl(SDL_Keycode key){
switch(key){
case SDLK_BACKSPACE: return ::ui::UiKey::Backspace;
case SDLK_DELETE: return ::ui::UiKey::DeleteForward;
case SDLK_LEFT: return ::ui::UiKey::Left;
case SDLK_RIGHT: return ::ui::UiKey::Right;
case SDLK_HOME: return ::ui::UiKey::Home;
case SDLK_END: return ::ui::UiKey::End;
case SDLK_RETURN:
case SDLK_KP_ENTER: return ::ui::UiKey::Enter;
case SDLK_TAB: return ::ui::UiKey::Tab;
case SDLK_A: return ::ui::UiKey::A; // Ctrl/Cmd+A select-all
default: return ::ui::UiKey::Unknown;
}
}

uint16_t UiModsFromSdl(SDL_Keymod mod){
uint16_t out = ::ui::UI_KEY_MOD_NONE;
if(mod & SDL_KMOD_SHIFT) out |= ::ui::UI_KEY_MOD_SHIFT;
if(mod & SDL_KMOD_CTRL) out |= ::ui::UI_KEY_MOD_CTRL;
if(mod & SDL_KMOD_ALT) out |= ::ui::UI_KEY_MOD_ALT;
if(mod & SDL_KMOD_GUI) out |= ::ui::UI_KEY_MOD_SUPER;
return out;
}
} // namespace

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
// SIL-18: the single SDL-event site builds the per-frame UI input frame
// (src/game/CLAUDE.md). Gameplay shortcut keys + the keymap still flow
// through OnScancodeDown/Up; in parallel we collect nav/confirm/cancel +
// the text-editing key/text/editing channels into the cppx UiInputFrame the
// pipeline consumes. Printable characters arrive as TEXT_INPUT below.
case SDL_EVENT_KEY_DOWN:{
gameInput.OnScancodeDown(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = true;
::ui::UiInputFrame & ui = gameUiPipeline.UiInput();
ui.source = ::ui::UiFocusSource::Keyboard;
::ui::UiKey k = UiKeyFromSdl(event.key.key);
if(k != ::ui::UiKey::Unknown){
::ui::ui_input_push_key(ui, k, UiModsFromSdl(SDL_GetModState()),
                        event.key.repeat);
}
switch(event.key.key){
case SDLK_UP: ui.nav_up = true; break;
case SDLK_DOWN: ui.nav_down = true; break;
case SDLK_LEFT: ui.nav_left = true; break;
case SDLK_RIGHT: ui.nav_right = true; break;
case SDLK_RETURN:
case SDLK_KP_ENTER:
case SDLK_SPACE: ui.confirm_pressed = true; ui.confirm_down = true; break;
case SDLK_ESCAPE: ui.cancel_pressed = true; ui.cancel_down = true; break;
default: break;
}
}break;
case SDL_EVENT_KEY_UP:{
gameInput.OnScancodeUp(event.key.scancode);
gameInput.GetKeystate()[event.key.scancode] = false;
::ui::UiInputFrame & ui = gameUiPipeline.UiInput();
switch(event.key.key){
case SDLK_RETURN:
case SDLK_KP_ENTER:
case SDLK_SPACE: ui.confirm_released = true; break;
case SDLK_ESCAPE: ui.cancel_released = true; break;
default: break;
}
}break;
case SDL_EVENT_TEXT_INPUT:{
::ui::UiInputFrame & ui = gameUiPipeline.UiInput();
::ui::ui_input_push_text(ui, event.text.text);
ui.source = ::ui::UiFocusSource::Keyboard;
}break;
case SDL_EVENT_TEXT_EDITING:{
::ui::UiInputFrame & ui = gameUiPipeline.UiInput();
::ui::ui_input_push_editing(ui, event.edit.text, event.edit.start,
                            event.edit.length);
ui.source = ::ui::UiFocusSource::Keyboard;
}break;
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
