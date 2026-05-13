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
#include <array>
#include <memory>
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
	// LobbyClayScreen::ShowGameCreate from a CLI op when there's no widget
	// path to drive the click). Game-thread only.
	ScreenContext& GetScreenContext() { return screenContext; }
	nlohmann::json GetWorldSummary();
	const Surface& GetScreenBuffer() const { return screenbuffer; }
	const SDL_Color* GetPaletteColors() const { return palettecolors; }
	Renderer& GetRenderer() { return renderer; }
	bool IsLiveMultiplayer() const;
	bool HandleInGameMenuKey(char ascii);
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
	// polling. Edge-triggered key events for menu nav still use the control
	// socket "key" op.
	bool tui;

	// Screen-stack ops. Every menu surface is a Screen; the stack drives
	// rendering and input via TickActiveScreen() at the top of Tick().
	void PushScreen(std::unique_ptr<Screen> s);
	void PopScreen();
	void ReplaceScreen(std::unique_ptr<Screen> s);
	Screen * GetTopScreen() const;

	// Keybind access for ControlDispatch.
	KeyMap& GetKeyMap() { return keymap; }
	const KeyMap& GetKeyMap() const { return keymap; }

	// Gamepad access for screens that need to capture rebind input or
	// query the connected pad type (e.g. OptionsControlsScreen).
	const GamepadState& GetGamepadState() const { return gamepadstate; }
	SDL_Gamepad * GetGamepad() const { return gamepad; }

	// LobbyClayScreen + per-panel interop. Public so panels can reach in via
	// `ScreenContext::game`.
	Uint32 currentlobbygameid;
	bool minimized;
	bool creategameclicked;
	bool joininggame;
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	void SpectateGame(LobbyGame & lobbygame, char * password = 0);
	// Tear down a joined game's session/world state (Disconnect, switch
	// authority, destroy team overlays, rejoin previous chat channel). UI
	// concerns (panel swap, map-name overlay) stay on LobbyClayScreen.
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
	static Uint32 TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval);
	void SetColors(SDL_Color * colors);
	void UpdateInputState(Input & input);
	bool LoadMap(const char * name);
	void UnloadGame(void);
	bool CheckForQuit(void);
	bool CheckForEndOfGame(void);
	bool CheckForConnectionLost(void);
	void ProcessInGameInterfaces(void);
	void ShowDeployMessage(void);
	void GiveDefaultItems(Player & player);
	void GoToState(Uint8 newstate);
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
	int frames;
	int fps;
	Uint64 lasttick;
	Uint16 sharedstate;
	int oldselecteditem;
	Uint8 singleplayermessage;
	bool updatetitle;
	Uint32 lastannouncedgameid;
	Uint8 lastannouncedstatus;
	bool deploymessageshown;
	int quitscancode;
	bool interfaceenterfix;
	bool fullscreentoggled;
	char * replayfile;
	ControlServer controlserver;
	InputServer inputserver;
	// TUI mouse edge-detection state. Tracks the last (x, y, down) so
	// control clients drive the same screen-level mouse hooks as SDL.
	Uint16 tui_prev_mouse_x;
	Uint16 tui_prev_mouse_y;
	bool   tui_prev_mouse_down;
	bool   tui_have_prev_mouse;
	void DrainControlQueue();
	void PostFrameReplies();

	// mapDownloader must be declared before ambienceMixer — the latter's
	// constructor captures it by reference (for ListFiles in music selection).
	MapDownloader mapDownloader;
	AmbienceMixer ambienceMixer;

	std::vector<std::unique_ptr<Screen>> screenStack;
	ScreenContext screenContext;
	void TickActiveScreen();
	// Set by GoToState; processed at the next Tick() entry to pop screens
	// safely after the active screen's Tick has returned. Avoids destroying
	// a screen mid-Tick when a button click triggers a state transition.
	bool screenStackPendingTeardown = false;

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
