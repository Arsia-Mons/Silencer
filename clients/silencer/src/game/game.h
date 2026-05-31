#ifndef GAME_H
#define GAME_H

#include "controlserver.h"
#include "input/game_input.h"
#include "inputserver.h"
#include "render/game_renderer.h"
#include "renderer.h"
#include "screen_context.h"
#include "game_summary.h"
#include "state.h"
#include "state/game_state.h"
#include "session/game_session.h"
#include "ui/game_ui_pipeline.h"
#include "updater.h"
#include "world.h"
#include <memory>
#include <string>
#include <vector>

class Screen;
class Modal;
class LobbyGame;

class Game
{
public:
Game();
~Game();
bool Load(char * cmdline);
bool Loop();
bool HandleSDLEvents();
void LoadProgressCallback(int progress, int totalprogressitems);

friend class Audio;
friend class ScreenContext;
friend class GameRenderer;
friend class GameInput;
friend class GameUiPipeline;
friend class GameSession;

int GetFrameCount() const { return frames; }
static const char * StateName(Uint8 s);
Uint8 GetState() const { return state; }
World & GetWorld() { return world; }
ScreenContext & GetScreenContext() { return screenContext; }
WorldSummary GetWorldSummary();
Surface & GetScreenBuffer() { return gameRenderer.GetScreenBuffer(); }
const Surface & GetScreenBuffer() const { return gameRenderer.GetScreenBuffer(); }
const SDL_Color * GetPaletteColors() const { return gameRenderer.GetPaletteColors(); }
Renderer & GetRenderer() { return renderer; }
silencer::client_ui::ClientUiInput & UiInput() { return gameUiPipeline.UiInput(); }
const silencer::client_ui::ClientUiInput & UiInput() const { return gameUiPipeline.UiInput(); }
const silencer::ui::UiInputState & CurrentUiInput() const { return gameUiPipeline.CurrentUiInput(); }
silencer::ui::UiInteractionRegistry & UiInteractions() { return gameUiPipeline.UiInteractions(); }
const silencer::ui::UiInteractionRegistry & UiInteractions() const { return gameUiPipeline.UiInteractions(); }
silencer::client_ui::InGameUiController & InGameUi() { return gameUiPipeline.InGameUi(); }
bool ResizeRenderSurface(int width, int height);
bool ResizeRenderSurfacePixels(int width, int height);
bool SyncRenderSurfaceToWindowPixels();
bool IsLiveMultiplayer() const;
bool GoBack();
struct PendingWait {
ControlCommand cmd;
Uint64 deadline_ms = 0;
int frames_left = -1;
std::string wait_state;
};
std::vector<PendingWait> pendingWaits;
bool quitRequested = false;
bool paused;
int stepFramesRemaining;
Uint64 stepWallclockDeadlineMs;
int controlPort;
int tuiInputPort;
bool headless;
bool tui;

void PushScreen(std::unique_ptr<Screen> s);
void PopScreen();
void ReplaceScreen(std::unique_ptr<Screen> s);
Screen * GetTopScreen() const;
bool HasUiInputTarget();
bool HasVisibleUiScreen() const;

KeyMap & GetKeyMap() { return gameInput.GetKeyMap(); }
const KeyMap & GetKeyMap() const { return gameInput.GetKeyMap(); }
const GamepadState & GetGamepadState() const { return gameInput.GetGamepadState(); }
SDL_Gamepad * GetGamepad() const { return gameInput.GetGamepad(); }

void JoinGame(LobbyGame & lobbygame, char * password = 0);
void SpectateGame(LobbyGame & lobbygame, char * password = 0);
void LeaveJoinedGame();

private:
bool Tick();
void TickFadeOut();
void TickInGame();
void TickSinglePlayerGame();
void TickHostGame();
void TickJoinGame();
void TickTestGame();
void TickReplayGame();
void Present();
void GoToState(Uint8 newstate);
void DrainControlQueue();
void PostFrameReplies();

World world;
Renderer renderer;
GameRenderer gameRenderer;
GameInput gameInput;
GameUiPipeline gameUiPipeline;
GameSession gameSession;

public:
bool minimized;
bool creategameclicked;
Uint32 & currentlobbygameid;
bool & joininggame;

private:
Updater updater;
ScreenContext screenContext;
ControlServer controlserver;
InputServer inputserver;
Uint8 state;
Uint8 nextstate;
bool stateisnew;
bool nextstateprocessed;
Uint16 sharedstate;
Uint8 singleplayermessage;
bool updatetitle;
int quitscancode;
bool chatEnterDebounce;
bool fullscreentoggled;
char * replayfile;
int frames;
int fps;
Uint64 lasttick;
};

#endif
