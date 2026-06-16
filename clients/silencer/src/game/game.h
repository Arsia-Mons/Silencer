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
    // Screenshot the final composited frame (world + cppx UI overlay) by capturing
    // the GPU swapchain; falls back to the indexed Surface when the device can't
    // capture (TUI/headless). SIL-11.
    bool CaptureCompositedFrame(const char *path);
    Renderer &GetRenderer() { return renderer; }
    // The cppx UI composition root. Public so the control socket can introspect the
    // retained UI tree and inject automation input (SIL-18) without a friend grant;
    // gameplay code drives navigation through the session-phase reconciler.
    GameUiPipeline &GetUiPipeline() { return gameUiPipeline; }
    // Public so the control socket can drive the self-updater into a static phase
    // (show_update_screen test op, SIL-212) without a friend grant.
    Updater &GetUpdater() { return updater; }
    bool ResizeRenderSurface(int width, int height);
    bool ResizeRenderSurfacePixels(int width, int height);
    bool SyncRenderSurfaceToWindowPixels();
    bool IsLiveMultiplayer() const;
    bool GoBack();
    // State transition entry point. Public so the UI layer can drive navigation
    // without a friend grant (SIL-8); ~21 internal callers unchanged.
    void GoToState(Uint8 newstate);
    // Push the active palette colors into the render backend. Hides the private
    // gameRenderer behind a public command (closed the old screen-context's
    // ResetPresentation seam).
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
    // Leave the in-progress match to the menu (origin CheckForQuit outcome): drop
    // the connection, then return to the lobby + rejoin the channel if
    // authenticated, else end any replay and go to the main menu. Driven by the
    // UI-layer PauseScreen via use_session().leave_match.
    void LeaveMatchToMenu();

    // The LobbyConnect status log, accumulated by the connect flow on the tick.
    // Public so the cppx composition root can snapshot it without a friend grant.
    const std::vector<std::string> &LobbyConnectLog() const { return lobbyConnectFlow.Log(); }
    // The lobby chat scrollback, drained from the lobby message queue on the tick
    // (the queue would otherwise grow unboundedly). Snapshotted by the comp root.
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
    // The state a FADEOUT transition is leaving. The session-phase projection holds
    // this (not nextstate) while state==FADEOUT so the outgoing screen stays
    // mounted and fades to black before the switch.
    Uint8 fadefromstate;
    // True on the previous in-game tick if the transition palette fade was still
    // dimming (FadePhase < 16). Used to detect the fade's falling edge so the
    // ambience palette is recomposed once over the canonical base palette when the
    // fade settles, instead of latching a stale, half-dimmed fade palette (the
    // level-entry lighting flicker).
    bool ambienceFadeWasActive = false;
    bool stateisnew;
    bool nextstateprocessed;
    // Roster size captured on entering CREATECHARACTER; when the roster grows past
    // it (a create round-tripped through the lobby), the tick routes to LOBBY.
    size_t charCreateCountOnEntry = 0;
    // Lobby chat scrollback, drained from world.lobby.chatmessages on the LOBBY
    // tick (cleared on entry). The cppx ChatPanel reads it via LobbyChatLog().
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
