#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "keybinds.h"
#include <string>

class Game;
class Input;
class Player;

class GameInput
{
public:
static constexpr Uint32 GAMEPAD_NAV_DELAY_MS  = 300;
static constexpr Uint32 GAMEPAD_NAV_REPEAT_MS = 120;

struct GamepadNavDir {
bool   held = false;
Uint32 nextfire = 0;
};

explicit GameInput(Game & game);

void UpdateInputState(Input & input);
void TickGamepadMenuNav();
void TickRumble();
void OpenFirstGamepad();
void PollGamepadState();
void OnScancodeDown(int scancode);
void OnScancodeUp(int scancode);
void QueueUiKeyboardInputForScancode(int scancode);
const char * GetActionKeyDisplayName(Action a);

KeyMap & GetKeyMap() { return keymap; }
const KeyMap & GetKeyMap() const { return keymap; }
GamepadState & GetGamepadStateMutable() { return gamepadstate; }
const GamepadState & GetGamepadState() const { return gamepadstate; }
SDL_Gamepad * GetGamepad() const { return gamepad; }
SDL_Gamepad * & GamepadRef() { return gamepad; }
Uint8 * GetKeystate() { return keystate; }
const Uint8 * GetKeystate() const { return keystate; }
std::string & PrevGamepadProfileRef() { return prevGamepadProfile; }

private:
Game & game;
Uint8 keystate[SDL_SCANCODE_COUNT];
KeyMap keymap;
GamepadState gamepadstate;
SDL_Gamepad * gamepad;
std::string prevGamepadProfile;
GamepadNavDir gamepadNavUp;
GamepadNavDir gamepadNavDown;
GamepadNavDir gamepadNavLeft;
GamepadNavDir gamepadNavRight;
};

#endif
