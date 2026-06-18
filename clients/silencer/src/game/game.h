#ifndef GAME_H
#define GAME_H

#include "controlserver.h"
#include "input/game_input.h"
#include "inputserver.h"
#include "render/game_renderer.h"
#include "renderer.h"
#include "game_summary.h"
#include "state.h"
#include "state/game_state.h"
#include "session/game_session.h"
#include "session/lobby_connect_flow.h"
#include "ui/game_ui_pipeline.h"
#include "updater.h"
#include "world.h"
#include <memory>
#include <string>
#include <vector>

class LobbyGame;

class Game {
public:
    Game();
    ~Game();
    bool Load(char *cmdline);
    bool Loop();
    bool HandleSDLEvents();
    void LoadProgressCallback(int progress, int totalprogressitems);

    friend class Audio;
    friend class GameRenderer;
    friend class GameInput;
    friend class GameUiPipeline;
    friend class GameSession;

    int GetFrameCount() const { return frames; }
    static const char *StateName(Uint8 s);
    Uint8 GetState() const { return state; }
    World &GetWorld() { return world; }
    WorldSummary GetWorldSummary();
    Surface &GetScreenBuffer() { return gameRenderer.GetScreenBuffer(); }
    const Surface &GetScreenBuffer() const { return gameRenderer.GetScreenBuffer(); }
    const SDL_Color *GetPaletteColors() const { return gameRenderer.GetPaletteColors(); }
    bool CaptureCompositedFrame(const char *path);
    Renderer &GetRenderer() { return renderer; }
    GameUiPipeline &GetUiPipeline() { return gameUiPipeline; }
    Updater &GetUpdater() { return updater; }
    bool ResizeRenderSurface(int width, int height);
    bool ResizeRenderSurfacePixels(int width, int height);
    bool SyncRenderSurfaceToWindowPixels();
    bool IsLiveMultiplayer() const;
    bool GoBack();
    void GoToState(Uint8 newstate);
    void SetRenderPaletteColors(SDL_Color *colors) { gameRenderer.SetColors(colors); }
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

    KeyMap &GetKeyMap() { return gameInput.GetKeyMap(); }
    const KeyMap &GetKeyMap() const { return gameInput.GetKeyMap(); }
    const GamepadState &GetGamepadState() const { return gameInput.GetGamepadState(); }
    SDL_Gamepad *GetGamepad() const { return gameInput.GetGamepad(); }

    void JoinGame(LobbyGame &lobbygame, char *password = 0);
    void SpectateGame(LobbyGame &lobbygame, char *password = 0);
    void LeaveJoinedGame();
    void LeaveMatchToMenu();

    const std::vector<std::string> &LobbyConnectLog() const { return lobbyConnectFlow.Log(); }
    const std::vector<std::string> &LobbyChatLog() const { return lobbyChatLog; }

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
    void DrainControlQueue();
    void PostFrameReplies();

    World world;
    Renderer renderer;
    GameRenderer gameRenderer;
    GameInput gameInput;
    GameUiPipeline gameUiPipeline;
    GameSession gameSession;
    LobbyConnectFlow lobbyConnectFlow;

public:
    bool minimized;
    bool creategameclicked;
    Uint32 &currentlobbygameid;
    bool &joininggame;

private:
    Updater updater;
    ControlServer controlserver;
    InputServer inputserver;
    Uint8 state;
    Uint8 nextstate;
    // The state a FADEOUT transition is leaving; the session-phase projection
    // reads this (not nextstate) to keep the outgoing screen mounted through the fade.
    Uint8 fadefromstate;
    // Previous-tick fade-active flag, for falling-edge detection: recompose the
    // ambience palette once when the fade settles instead of latching a stale,
    // half-dimmed fade palette (the level-entry lighting flicker).
    bool ambienceFadeWasActive = false;
    bool stateisnew;
    bool nextstateprocessed;
    // Roster size on entering CREATECHARACTER; when it grows (a create
    // round-tripped through the lobby), the tick routes to LOBBY.
    size_t charCreateCountOnEntry = 0;
    std::vector<std::string> lobbyChatLog;
    Uint16 sharedstate;
    Uint8 singleplayermessage;
    bool updatetitle;
    bool chatEnterDebounce;
    bool fullscreentoggled;
    char *replayfile;
    int frames;
    int fps;
    Uint64 lasttick;
};

#endif
