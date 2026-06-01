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
explicit GameInput(Game & game);

void UpdateInputState(Input & input);
void TickRumble();
void OpenFirstGamepad();
void PollGamepadState();
void OnScancodeDown(int scancode);
void OnScancodeUp(int scancode);
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
};

#endif
