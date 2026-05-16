#ifndef GAME_H
#define GAME_H

#include "renderdevice.h"
#include "renderer.h"
#include "input.h"
#include "keybinds.h"
#include "state.h"
#include "updater.h"
#include "controlserver.h"
#include "inputserver.h"
#include "screen_context.h"
#include "game_state.h"
#include "map_downloader.h"
#include "ambience_mixer.h"
#include "client/ui/ClayBridgeFrameBackend.h"
#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "client/ui/ingame/InGameUiController.h"
#include "clay/clay.h"
#include "ui/runtime/UiInputState.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class Screen;
class Modal;

class Game
{
public:
	Game();
	~Game();
	bool Load(char * cmdline);
	bool Loop(void);
	bool HandleSDLEvents(void);
	void LoadProgressCallback(int progress, int totalprogressitems);

	friend class Audio;
	friend class ScreenContext;

public:
	// Exposed for ControlDispatch (game-thread only).
	int GetFrameCount() const { return frames; }
	static const char* StateName(Uint8 s);
	Uint8 GetState() const { return state; }
	class World& GetWorld() { return world; }
	// Test/control-dispatch hook: gives ControlDispatch op handlers access
	// to the ScreenContext so they can invoke screen-side helpers (e.g.
	// LobbyScreen::ShowGameCreate from a CLI op when there's no widget
	// path to drive the click). Game-thread only.
	ScreenContext& GetScreenContext() { return screenContext; }
	struct WorldPeerSummary {
		int id = 0;
		unsigned int accountId = 0;
		bool observer = false;
		bool disconnected = false;
		std::vector<int> controlledList;
	};
	struct WorldPlayerSummary {
		int id = 0;
		int hp = 0;
		int x = 0;
		int y = 0;
	};
	struct WorldSummary {
		std::string map;
		int peers = 0;
		int localPeerId = 0;
		int viewedPeerId = 0;
		int authorityPeer = 0;
		unsigned int lobbyAccountId = 0;
		bool isLocalObserver = false;
		bool spectatorInitialized = false;
		bool spectatorFreecam = false;
		std::vector<WorldPeerSummary> peerList;
		std::vector<WorldPlayerSummary> players;
		int objectsCount = 0;
	};
	WorldSummary GetWorldSummary();
	const Surface& GetScreenBuffer() const { return screenbuffer; }
	const SDL_Color* GetPaletteColors() const { return palettecolors; }
	Renderer& GetRenderer() { return renderer; }
	silencer::client_ui::ClientUiInput& UiInput() { return clientUiInput; }
	const silencer::client_ui::ClientUiInput& UiInput() const { return clientUiInput; }
	silencer::ui::UiInteractionRegistry& UiInteractions() { return clientUi.Interactions(); }
	const silencer::ui::UiInteractionRegistry& UiInteractions() const { return clientUi.Interactions(); }
	silencer::client_ui::InGameUiController& InGameUi() { return inGameUiController; }
	bool ResizeRenderSurface(int width, int height);
	bool ResizeRenderSurfacePixels(int width, int height);
	bool SyncRenderSurfaceToWindowPixels();
	bool IsLiveMultiplayer() const;
	bool GoBack(void);
	struct PendingWait {
		ControlCommand cmd;
		Uint64 deadline_ms = 0;   // 0 = no wallclock deadline
		int frames_left = -1;     // <0 = no frame deadline
		std::string wait_state;   // for wait_for_state
	};
	std::vector<PendingWait> pendingWaits;
	bool quitRequested = false;
	bool paused;
	int stepFramesRemaining;
	Uint64 stepWallclockDeadlineMs;
	int controlPort;
	int tuiInputPort;
	bool headless;
	// TUI mode: audio enabled, no SDL window/events, frames stream to a TS
	// frontend over TCP via TUIBackend. Input arrives via the dedicated binary
	// input channel (InputServer, --tui-input-port) rather than SDL keyboard
	// polling. Edge-triggered key events from the control socket are queued
	// into the same normalized UI input state as SDL events.
	bool tui;

	// Client UI navigation requests. ClientUi owns the stack mechanics; Game
	// exposes these narrow wrappers for state transitions, screens, and control
	// socket handlers.
	void PushScreen(std::unique_ptr<Screen> s);
	void PopScreen();
	void ReplaceScreen(std::unique_ptr<Screen> s);
	Screen * GetTopScreen() const;
	bool HasUiInputTarget();

	// Keybind access for ControlDispatch.
	KeyMap& GetKeyMap() { return keymap; }
	const KeyMap& GetKeyMap() const { return keymap; }

	// Gamepad access for screens that need to capture rebind input or
	// query the connected pad type (e.g. OptionsControlsScreen).
	const GamepadState& GetGamepadState() const { return gamepadstate; }
	SDL_Gamepad * GetGamepad() const { return gamepad; }

	// LobbyScreen + per-panel interop. Public so panels can reach in via
	// `ScreenContext::game`.
	Uint32 currentlobbygameid;
	bool minimized;
	bool creategameclicked;
	bool joininggame;
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	void SpectateGame(LobbyGame & lobbygame, char * password = 0);
	// Tear down a joined game's session/world state (Disconnect, switch
	// authority, destroy team overlays, rejoin previous chat channel). UI
	// concerns (panel swap, map-name overlay) stay on LobbyScreen.
	void LeaveJoinedGame();
private:
	bool Tick(void);
	// Gameplay-state Tick bodies — one per state. Each lives in its own
	// src/game/tick/tick_*.cpp file. The switch in Tick() dispatches to
	// these. Menu/screen states (MAINMENU, LOBBY, OPTIONS*, …) stay inline
	// in the dispatcher — they're trivial PushScreen wrappers.
	void TickFadeOut();
	void TickInGame();
	void TickSinglePlayerGame();
	void TickHostGame();
	void TickJoinGame();
	void TickTestGame();
	void TickReplayGame();
	void Present(void);
	bool SetupRenderDevice(void);
	// Edge-triggered scancode handlers. Called from HandleSDLEvents on real
	// SDL key events, and from Loop()'s TUI branch when comparing the
	// previous scancode bitmask against a freshly received one. Centralising
	// them keeps the in-game ESC quitstate machine, F1 player-list overlay,
	// debug toggles, etc. working identically in both paths.
	void OnScancodeDown(int scancode);
	void OnScancodeUp(int scancode);
	void QueueUiKeyboardInputForScancode(int scancode);
	static Uint32 TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval);
	void SetColors(SDL_Color * colors);
	void UpdateInputState(Input & input);
	bool LoadMap(const char * name);
	void UnloadGame(void);
	bool CheckForQuit(void);
	bool CheckForEndOfGame(void);
	bool CheckForConnectionLost(void);
	void ShowDeployMessage(void);
	void GiveDefaultItems(Player & player);
	void GoToState(Uint8 newstate);
	void PrepareClientUiFrame(Surface& surface);
	void BeginPreparedClientUiFrame();
	Clay_RenderCommandArray EndClientUiFrame();
	void RenderClientUiFrame(Surface& surface, float frametime);
	void ResetUiFrameDeltas();
	void BuildVisibleClientUi(Surface& surface, float frametime);
	void DrawInGameWorldInsets(Surface& surface, float frametime);
	Updater updater;
	// Display name for the first key bound to an action; "(unbound)" if none.
	// Used by tutorial overlays that say "press %s to fire".
	const char * GetActionKeyDisplayName(Action a);
	KeyMap keymap;
	GamepadState gamepadstate;
	SDL_Gamepad * gamepad;
	void OpenFirstGamepad(void);
	void PollGamepadState(void);
	Uint8 keystate[SDL_SCANCODE_COUNT];
	Uint8 state;
	Uint8 nextstate;
	Uint8 fade_i;
	bool stateisnew;
	bool nextstateprocessed;
	class World world;
	Renderer renderer;
	SDL_Window * window;
	RenderDevice * renderdevice;
	SDL_Color palettecolors[256]; // CPU copy — for ffmpeg replay pixel export
	Surface screenbuffer;
	// The game world always renders at the legacy 640x480 and is then
	// nearest-upscaled into a centered, aspect-correct region of screenbuffer,
	// so pixel art stays square while the Clay UI composites crisp on top.
	Surface worldSurface;
	silencer::client_ui::ClayBridgeFrameBackend uiClayBackend;
	silencer::ui::ClayService uiClayService;
	silencer::client_ui::ClientUi clientUi;
	silencer::client_ui::ClientUiInput clientUiInput;
	silencer::client_ui::InGameUiController inGameUiController;
	silencer::ui::UiInputState preparedUiInput;
	bool hasPreparedUiInput = false;
	int frames;
	int fps;
	Uint64 lasttick;
	Uint16 sharedstate;
	Uint8 singleplayermessage;
	bool updatetitle;
	Uint32 lastannouncedgameid;
	Uint8 lastannouncedstatus;
	bool deploymessageshown;
	int quitscancode;
	bool chatEnterDebounce;
	bool fullscreentoggled;
	char * replayfile;
	ControlServer controlserver;
	InputServer inputserver;
	void DrainControlQueue();
	void PostFrameReplies();

	// mapDownloader must be declared before ambienceMixer — the latter's
	// constructor captures it by reference (for ListFiles in music selection).
	MapDownloader mapDownloader;
	AmbienceMixer ambienceMixer;

	ScreenContext screenContext;

	// Profile to restore when a gamepad disconnects.  Stays empty when the
	// active profile was already "gamepad" before the pad was connected.
	std::string prevGamepadProfile;

	// Per-direction software-repeat state for gamepad menu navigation.
	// Gamepad events are polled each frame, not event-driven, so we
	// synthesise key-repeat manually: first press fires immediately, further
	// repeats fire after GAMEPAD_NAV_DELAY_MS then every GAMEPAD_NAV_REPEAT_MS.
	static constexpr Uint32 GAMEPAD_NAV_DELAY_MS  = 300;
	static constexpr Uint32 GAMEPAD_NAV_REPEAT_MS = 120;
	struct GamepadNavDir {
		bool       held     = false;
		Uint32     nextfire = 0;  // SDL_GetTicks() value at which next repeat fires
	};
	GamepadNavDir gamepadNavUp;
	GamepadNavDir gamepadNavDown;
	GamepadNavDir gamepadNavLeft;
	GamepadNavDir gamepadNavRight;
	void TickGamepadMenuNav();
	// Trigger SDL_RumbleGamepad for fire/hit/land events on the local player.
	void TickRumble();
};

#endif
