#include "input/game_input.h"

#include "game.h"
#include "audio.h"
#include "config.h"
#include "player.h"
#include <cstring>
#include <vector>

GameInput::GameInput(Game & g) : game(g), gamepad(nullptr) {
std::memset(keystate, 0, sizeof(keystate));
}

// Captureless lambdas decay to function pointers, so this table costs nothing
// at runtime vs. the old hand-written cascade. New actions get one row here
// (and one row in ACTION_TABLE) — that's the entire fan-out for adding one.
typedef bool& (*InputField)(Input&);
static const struct { Action a; InputField field; } INPUT_FIELDS[] = {
{ Action::MoveUp,        [](Input& i) -> bool& { return i.keymoveup;        } },
{ Action::MoveDown,      [](Input& i) -> bool& { return i.keymovedown;      } },
{ Action::MoveLeft,      [](Input& i) -> bool& { return i.keymoveleft;      } },
{ Action::MoveRight,     [](Input& i) -> bool& { return i.keymoveright;     } },
{ Action::LookUpLeft,    [](Input& i) -> bool& { return i.keylookupleft;    } },
{ Action::LookUpRight,   [](Input& i) -> bool& { return i.keylookupright;   } },
{ Action::LookDownLeft,  [](Input& i) -> bool& { return i.keylookdownleft;  } },
{ Action::LookDownRight, [](Input& i) -> bool& { return i.keylookdownright; } },
{ Action::Jump,          [](Input& i) -> bool& { return i.keyjump;          } },
{ Action::Jetpack,       [](Input& i) -> bool& { return i.keyjetpack;       } },
{ Action::Activate,      [](Input& i) -> bool& { return i.keyactivate;      } },
{ Action::Use,           [](Input& i) -> bool& { return i.keyuse;           } },
{ Action::Fire,          [](Input& i) -> bool& { return i.keyfire;          } },
{ Action::Chat,          [](Input& i) -> bool& { return i.keychat;          } },
{ Action::NextInv,       [](Input& i) -> bool& { return i.keynextinv;       } },
{ Action::NextCam,       [](Input& i) -> bool& { return i.keynextcam;       } },
{ Action::PrevCam,       [](Input& i) -> bool& { return i.keyprevcam;       } },
{ Action::Detonate,      [](Input& i) -> bool& { return i.keydetonate;      } },
{ Action::Disguise,      [](Input& i) -> bool& { return i.keydisguise;      } },
{ Action::NextWeapon,    [](Input& i) -> bool& { return i.keynextweapon;    } },
{ Action::Weapon1,       [](Input& i) -> bool& { return i.keyweapon[0];     } },
{ Action::Weapon2,       [](Input& i) -> bool& { return i.keyweapon[1];     } },
{ Action::Weapon3,       [](Input& i) -> bool& { return i.keyweapon[2];     } },
{ Action::Weapon4,       [](Input& i) -> bool& { return i.keyweapon[3];     } },
{ Action::UiUp,          [](Input& i) -> bool& { return i.keyup;            } },
{ Action::UiDown,        [](Input& i) -> bool& { return i.keydown;          } },
{ Action::UiLeft,        [](Input& i) -> bool& { return i.keyleft;          } },
{ Action::UiRight,       [](Input& i) -> bool& { return i.keyright;         } },
};

void GameInput::TickRumble(){
Player * localPlayer = game.world.GetPeerPlayer(game.world.peers.localpeerid);
if(!gamepad || game.world.gameplaystate != World::INGAME || !localPlayer) return;

if(localPlayer->rumbleFire){
localPlayer->rumbleFire = false;
SDL_RumbleGamepad(gamepad, 0, 12000, 80);
}
if(localPlayer->rumbleHit){
localPlayer->rumbleHit = false;
SDL_RumbleGamepad(gamepad, 30000, 15000, 200);
}
if(localPlayer->rumbleLand){
localPlayer->rumbleLand = false;
SDL_RumbleGamepad(gamepad, 18000, 0, 120);
}
}

const char * GameInput::GetActionKeyDisplayName(Action a){
static thread_local char buf[32];
const auto& ab = keymap.Get(a);
const BindingKey* fallback = nullptr;
for(const auto& b : ab.bindings){
if(b.keys.empty()) continue;
const auto& k = b.keys[0];
if(k.device == BindingDevice::Keyboard){
return KeyMap::GetKeyName((SDL_Scancode)k.code);
}
if(!fallback) fallback = &k;
}
if(fallback){
std::string s = Stringify(*fallback);
auto colon = s.find(':');
std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
std::string label = GamepadShortLabel(raw, gamepad ? SDL_GetGamepadType(gamepad) : SDL_GAMEPAD_TYPE_UNKNOWN);
std::strncpy(buf, label.c_str(), sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';
return buf;
}
std::strncpy(buf, "(unbound)", sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';
return buf;
}

void GameInput::UpdateInputState(Input & input){
float mousex;
float mousey;
Uint32 mousestate = SDL_GetMouseState(&mousex, &mousey);
PollGamepadState();
gamepadstate.mouseButtons = 0;
for(int b = 1; b <= 5; ++b){
if(mousestate & SDL_BUTTON_MASK(b)) gamepadstate.mouseButtons |= (1u << (b - 1));
}
for(const auto& f : INPUT_FIELDS){
f.field(input) = keymap.IsPressed(f.a, keystate, gamepadstate);
}
input.mousex = (Uint16)mousex;
input.mousey = (Uint16)mousey;
input.mousedown = SDL_BUTTON_LEFT & mousestate ? true : false;

Player * localplayer = game.world.GetPeerPlayer(game.world.peers.localpeerid);
if(localplayer){
if(input.keynextweapon && !localplayer->input.keynextweapon){
switch(localplayer->currentweapon){
case 0:
if(localplayer->laserammo > 0){
input.keyweapon[1] = true;
}else if(localplayer->rocketammo > 0){
input.keyweapon[2] = true;
}else if(localplayer->flamerammo > 0){
input.keyweapon[3] = true;
}
break;
case 1:
if(localplayer->rocketammo > 0){
input.keyweapon[2] = true;
}else if(localplayer->flamerammo > 0){
input.keyweapon[3] = true;
}else{
input.keyweapon[0] = true;
}
break;
case 2:
if(localplayer->flamerammo > 0){
input.keyweapon[3] = true;
}else{
input.keyweapon[0] = true;
}
break;
case 3:
input.keyweapon[0] = true;
break;
}
}
if(!game.world.replay.IsPlaying()){
if(game.chatEnterDebounce && !keystate[SDL_SCANCODE_RETURN]){
game.chatEnterDebounce = false;
}
if(localplayer->chatActive || game.chatEnterDebounce){
Input zeroinput;
input = zeroinput;
game.chatEnterDebounce = true;
}
if(localplayer->isbuying || localplayer->techstationactive || game.chatEnterDebounce){
Input zeroinput;
zeroinput.keyactivate = input.keyactivate;
zeroinput.keymoveleft = input.keymoveleft;
zeroinput.keymoveright = input.keymoveright;
input = zeroinput;
game.chatEnterDebounce = true;
}
}
}
}

void GameInput::OpenFirstGamepad(){
if(gamepad){ SDL_CloseGamepad(gamepad); gamepad = nullptr; }
int count = 0;
SDL_JoystickID* ids = SDL_GetGamepads(&count);
if(ids){
if(count > 0) gamepad = SDL_OpenGamepad(ids[0]);
SDL_free(ids);
}
gamepadstate.connected = (gamepad != nullptr);
if(gamepadstate.connected){
const char* cur = Config::GetInstance().active_keybind_profile;
std::string curStr = (cur && *cur) ? cur : "default";
bool alreadyGamepad = (curStr.find("gamepad") != std::string::npos);
if(!alreadyGamepad){
prevGamepadProfile = curStr;
std::strncpy(Config::GetInstance().active_keybind_profile, "gamepad",
             sizeof(Config::GetInstance().active_keybind_profile) - 1);
Config::GetInstance().active_keybind_profile[
sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
LoadActiveKeymap(keymap);
}
}
}

void GameInput::PollGamepadState(){
if(!gamepad){
OpenFirstGamepad();
}
gamepadstate.connected = (gamepad != nullptr);
gamepadstate.buttons = 0;
for(auto& a : gamepadstate.axes) a = 0;
if(!gamepad) return;
for(int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b){
if(SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)b)){
gamepadstate.buttons |= (1u << b);
}
}
for(int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a){
gamepadstate.axes[a] = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)a);
}
}

void GameInput::OnScancodeDown(int sc){
if(sc == SDL_SCANCODE_F1){
game.world.SetShowingPlayerList(true);
}
if(sc == SDL_SCANCODE_F2){
game.world.SetShowingTeamColors(!game.world.IsShowingTeamColors());
}
if(sc == SDL_SCANCODE_F5){
game.gameSession.AmbienceMixerRef().LoadRandomGameMusic();
game.gameSession.AmbienceMixerRef().PlayMusic(game.world.resources.gamemusic);
}
if(sc == SDL_SCANCODE_F4){
if(Config::GetInstance().music){
if(Audio::GetInstance().MusicPaused()){
Audio::GetInstance().ResumeMusic();
game.world.ShowTopMessage("           MUSIC RESUMED");
}else{
Audio::GetInstance().PauseMusic();
game.world.ShowTopMessage("          *MUSIC PAUSED*");
}
}
}
if(sc == SDL_SCANCODE_F9){
game.world.debugoverlay = !game.world.debugoverlay;
game.world.ShowTopMessage(game.world.debugoverlay ? "        DEBUG OVERLAY ON" : "       DEBUG OVERLAY OFF");
}
}

void GameInput::OnScancodeUp(int sc){
if(sc == SDL_SCANCODE_F1){
game.world.SetShowingPlayerList(false);
}
}

