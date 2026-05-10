#ifndef GAME_H
#define GAME_H

#include "renderdevice.h"
#include "renderer.h"
#include "input.h"
#include "keybinds.h"
#include "state.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"
#include "textbox.h"
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
	Uint16 GetCurrentInterfaceId() const { return currentinterface; }
	class World& GetWorld() { return world; }
	nlohmann::json GetWorldSummary();
	const Surface& GetScreenBuffer() const { return screenbuffer; }
	const SDL_Color* GetPaletteColors() const { return palettecolors; }
	Renderer& GetRenderer() { return renderer; }
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
	// polling. Edge-triggered key events for menu nav still use the control
	// socket "key" op.
	bool tui;

	// Screen-stack ops. The stack starts empty and stays empty until screens
	// migrate over from the legacy Create*/Process*Interface helpers.
	void PushScreen(std::unique_ptr<Screen> s);
	void PopScreen();
	void ReplaceScreen(std::unique_ptr<Screen> s);

	// Keybind access for ControlDispatch.
	KeyMap& GetKeyMap() { return keymap; }
	const KeyMap& GetKeyMap() const { return keymap; }

	// LobbyScreen + per-panel interop. Public during the multi-stage migration
	// of Game::Create*Interface / ProcessLobbyInterface onto Screen/Panel
	// classes — panels reach in via `ScreenContext::game` rather than going
	// through narrow shim methods. Moves toward fully encapsulating once stage
	// H lands and the legacy helpers are deleted.
	Interface * CreateLobbyInterface(void);
	void TickLobbyBody();
	Uint16 lobbyinterface;
	Uint16 chatinterface;
	Uint16 gameselectinterface;
	Uint16 gamecreateinterface;
	Uint16 gamejoininterface;       // mirrored by GameJoinPanel; removed in stage H.
	Uint16 gametechinterface;       // mirrored by GameTechPanel; removed in stage H.
	Uint16 mappreviewinterface;
	Uint16 currentinterface;
	Uint32 currentlobbygameid;
	bool minimized;
	bool creategameclicked;
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	Interface * CreateModalDialog(const char * message, bool ok = true);
	Interface * CreatePasswordDialog(void);
	Interface * CreateMapPreview(const char * filename);
	// Toggle in-lobby team overlay visibility. Called by LobbyScreen's
	// right-side panel swaps (ShowGameTech / TearDownRightPanels) when
	// entering / leaving the tech-choice surface. Removed in stage H once
	// the panel can reach world.objectlist directly.
	void ShowTeamOverlays(bool show);

private:
	bool Tick(void);
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
	Interface * CreateGameSummaryInterface(Stats & stats, Uint8 agency);
	void DestroyModalDialog(void);
	Uint16 gamesummaryinterface;
	Uint16 modalinterface;
	Uint16 passwordinterface;
	Updater updater;
	bool ProcessLobbyInterface(Interface * iface);
	void ProcessGameSummaryInterface(Interface * iface);
	void UpdateLobbyMapName(const char * name);
	void UpdateGameSummaryInterface(void);
	void AddSummaryLine(TextBox & textbox, const char * name, Uint32 value, bool percentage = false);
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
	Uint16 aftermodalinterface;
	Uint16 sharedstate;
	int oldselecteditem;
	Uint8 singleplayermessage;
	bool updatetitle;
	Uint32 lastannouncedgameid;
	Uint8 lastannouncedstatus;
	bool gamesummaryinfoloaded;
	bool modaldialoghasok;
	bool joininggame;
	bool deploymessageshown;
	int quitscancode;
	bool interfaceenterfix;
	bool fullscreentoggled;
	char * replayfile;
	ControlServer controlserver;
	InputServer inputserver;
	// TUI mouse edge-detection state. Tracks the last (x, y, down) we
	// applied so the TUI loop can synthesize ProcessMousePress / Move
	// calls on transitions, mirroring HandleSDLEvents on native.
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

	// Stack-based UI. Starts empty; legacy Create*/Process*Interface helpers
	// drive the menus until screens migrate over.
	std::vector<std::unique_ptr<Screen>> screenStack;
	ScreenContext screenContext;
	void TickActiveScreen();
	// Set by GoToState; processed at the next Tick() entry to pop screens
	// safely after the active screen's Tick has returned. Avoids destroying
	// a screen mid-Tick when a button click triggers a state transition.
	bool screenStackPendingTeardown = false;
};

#endif
