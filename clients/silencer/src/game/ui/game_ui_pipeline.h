#ifndef GAME_UI_PIPELINE_H
#define GAME_UI_PIPELINE_H

#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "client/ui/ingame/InGameUi.h"
#include "keybinds.h"
#include "surface.h"
#include "ui/runtime/UiInputState.h"
#include <memory>

class Game;
class Screen;

class GameUiPipeline
{
public:
explicit GameUiPipeline(Game & game);

void DrawInGameWorldInsets(Surface & surface, float frametime);
void RenderClientUiFrame(Surface & surface, float frametime);
void ResetUiFrameDeltas();
bool HasInputTarget();
bool TickScreenState(Uint8 state, bool entering);
void Push(std::unique_ptr<Screen> s);
void Pop();
void Replace(std::unique_ptr<Screen> s);
Screen * Top() const;
void QueueKeyboardInputForScancode(int scancode, const Uint8 * keystate,
const KeyMap & keymap, const GamepadState & gamepadstate);

silencer::client_ui::ClientUi & ClientUiRef() { return clientUi; }
const silencer::client_ui::ClientUi & ClientUiRef() const { return clientUi; }
silencer::client_ui::ClientUiInput & UiInput() { return clientUiInput; }
const silencer::client_ui::ClientUiInput & UiInput() const { return clientUiInput; }
const silencer::ui::UiInputState & CurrentUiInput() const { return preparedUiInput; }
const silencer::ui::UiInteractionRegistry & UiInteractions() const { return clientUi.Interactions(); }
void ClearUiFocus() { clientUi.ClearFocus(); }
silencer::client_ui::InGameUi & InGameUi() { return inGameUi; }

private:
Game & game;
silencer::client_ui::ClientUi clientUi;
silencer::client_ui::ClientUiInput clientUiInput;
silencer::client_ui::InGameUi inGameUi;
silencer::ui::UiInputState preparedUiInput;
bool hasPreparedUiInput;
Uint64 lastUiAnimationMs;
bool textInputFocused;

void PrepareClientUiFrame(Surface & surface);
void BeginPreparedClientUiFrame();
void EndClientUiFrame();
void BuildVisibleClientUi();
void PlayMenuMusicIfReady();
};

#endif
