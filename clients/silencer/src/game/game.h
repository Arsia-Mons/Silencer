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
#include "ui_state.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class Screen;
class Modal;
namespace ui { namespace v2 { struct Node; struct LobbyState; class Runtime; } }

// Live-engine v2 modal entry. Overlays the underlying state's v2 (or legacy)
// render. The stack is rendered + dispatched independently of any per-state
// path so a modal can be shown from any state. Mirrors what the legacy
// MessageModal / PasswordModal classes provided through the Screen stack —
// the v2 tree is rebuilt each frame from the current Text / password buffer.
struct V2ModalEntry {
	enum Kind { MESSAGE, PASSWORD };
	Kind                                       kind         = MESSAGE;
	std::string                                text;          // MESSAGE: body / PASSWORD: prompt
	bool                                       has_ok       = true;
	std::string                                password_buf;  // PASSWORD only
	std::function<void()>                      on_close;      // MESSAGE on-OK callback
	std::function<void(const std::string &)>   on_submit;     // PASSWORD on-OK callback
};

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
	const ui::v2::LobbyState* GetLobbyV2State() const;
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

	// Const accessor for read-only state queries.
	const class World & GetWorldConst() const { return world; }

	// Active v2 runtime accessor. Returns nullptr when no runtime is
	// instantiated (states not yet migrated, or between transitions).
	ui::v2::Runtime * GetActiveRuntime() const { return active_runtime.get(); }

	// LobbyScreen + per-panel interop. Public so panels can reach in via
	// `ScreenContext::game`.
	Uint16 currentinterface;
	Uint32 currentlobbygameid;
	bool minimized;
	bool creategameclicked;
	bool joininggame;
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	// Tear down a joined game's session/world state (Disconnect, switch
	// authority, destroy team overlays, rejoin previous chat channel). UI
	// concerns (panel swap, map-name overlay) stay on LobbyScreen.
	void LeaveJoinedGame();
	// Toggle in-lobby team overlay visibility. Called by LobbyScreen's
	// right-side panel swaps (ShowGameTech / TearDownRightPanels) when
	// entering / leaving the tech-choice surface.
	void ShowTeamOverlays(bool show);

	// Thin wrappers forwarding to the active LobbyRuntime — used by
	// events.cpp (text input + key routing) and controldispatch.cpp
	// (CLI smoke tests). Safe to call any time: no-ops / null returns
	// when the engine isn't in LOBBY or no runtime is instantiated.
	bool LobbyV2ChatActive() const;
	void LobbyV2ChatAppendChar(char c);
	void LobbyV2ChatBackspace();
	void LobbyV2ChatSubmit();
	bool LobbyV2CreateInputActive() const;
	void LobbyV2CreateAppendChar(char c);
	void LobbyV2CreateBackspace();
	void LobbyV2CreateSubmit();
	void LobbyV2NavPrev();
	void LobbyV2NavNext();

	// v2 modal stack — overlays the underlying state's render and intercepts
	// mouse / text input when non-empty. Lobby panels (P16g) and any other
	// caller that needs to surface a confirmation / progress / password
	// prompt without owning a legacy Screen subclass push entries here.
	void ShowV2Message(const std::string & text, std::function<void()> on_close = {});
	void ShowV2ProgressMessage(const std::string & text);  // No OK button — caller pops.
	void ShowV2PasswordModal(std::function<void(const std::string &)> on_submit);
	void SetV2ProgressText(const std::string & text);      // Updates text on top of stack.
	void PopV2Modal();
	bool IsV2ModalActive() const { return !ui_v2_modal_stack.empty(); }
	bool IsV2ProgressModalActive() const;

	// Preview-mode CLI flags. Set by Load() when --preview-screen is
	// passed; main.cpp dispatches to RunPreview() instead of the normal
	// Loop. Used by the ui/v2 harness for visual + PPM-diff verification.
	char preview_screen[64];        // empty = preview mode disabled
	char preview_impl[16];          // "v2" (default) or "legacy"
	char dump_ppm_path[256];        // empty = interactive window mode
	int  preview_scale;             // 1 if unset
	// Render the requested screen via either the new ui/v2 path or the
	// legacy widget path, then either dump a PPM (one-shot) or run an
	// SDL window loop. Defined in src/ui/v2/preview.cpp.
	int RunPreview();
	// Interactive component storybook — palette + canvas + properties
	// panel for the ui/v2 primitives. Launched via `--preview-screen
	// storybook`. Defined in src/ui/v2/storybook.cpp.
	int RunStorybook();

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

	std::vector<std::unique_ptr<Screen>> screenStack;
	ScreenContext screenContext;
	void TickActiveScreen();

	// ui/v2 state for menu surfaces that have migrated off the Screen stack.
	// Owns per-button hover-animation slots (hot_t) and gets BeginFrame /
	// EndFrame'd around each v2 render pass. Mouse position is tracked here
	// so events.cpp can populate Context.mouse_{x,y} on motion + dispatch.
	ui::v2::UIState ui_v2_state;
	int ui_v2_mouse_x = -1;
	int ui_v2_mouse_y = -1;
	Uint64 ui_v2_last_ticks = 0;
	// Per-engine-state Runtime — owns its Node tree, UIState, and
	// state struct. Swapped on state transition in SetRuntime(). Null
	// when the current state still uses the legacy per-state path on
	// Game (states being migrated one at a time).
	std::unique_ptr<ui::v2::Runtime> active_runtime;
	void SetRuntime(Uint8 new_state);
	// v2 modal stack render + dispatch hooks. RenderV2ModalOverlay() blits
	// the top modal on top of whatever the per-state render already drew;
	// DispatchV2Modal* return true when the event was consumed so the per-
	// state path can be skipped. SDL types are forward-declared to keep the
	// game.h dependency surface unchanged for non-event callers.
	void RenderV2ModalOverlay();
	bool DispatchV2ModalClick(int logical_x, int logical_y);
	bool DispatchV2ModalKey(int sdl_scancode);
	bool DispatchV2ModalText(char ascii);
	std::vector<V2ModalEntry> ui_v2_modal_stack;
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
