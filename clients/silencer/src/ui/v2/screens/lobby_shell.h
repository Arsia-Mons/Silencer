#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H

#include "runtime.h"
#include "ui_state.h"

#include "shared.h"
#include "lobby_character.h"
#include "lobby_chat.h"
#include "lobby_create.h"
#include "lobby_join.h"
#include "lobby_select.h"
#include "lobby_tech.h"

#include <functional>
#include <string>

class World;
class ScreenContext;
class Game;

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyHandlers {
	std::function<void()> on_go_back;
	CharacterPanelHandlers character;
	GameCreateHandlers game_create;
	GameJoinHandlers game_join;
	GameSelectHandlers game_select;
	GameTechHandlers game_tech;
};

// Which right-side panel the lobby is currently displaying. Mirrors the
// legacy LobbyScreen unique_ptr<> swap (gameSelect / gameCreate /
// gameJoin / gameTech) — `None` is the chrome-only state used by the
// initial "lobby" preview before any panel ports landed.
enum class LobbyActivePanel : Uint8 {
	None,
	GameCreate,
	GameJoin,
	GameSelect,
	GameTech,
};

// Engine-supplied dynamic state for the lobby chrome + embedded panels.
// Kept separate from LobbyHandlers because handlers are pure callbacks
// while these are read on every Build.
struct LobbyState {
	// Which right-side panel is currently active (drives which Build
	// helper gets composed in BuildLobby + whether the chat caret renders).
	LobbyActivePanel active_panel = LobbyActivePanel::None;
	// Character panel state (selected agency + username + stat readouts).
	CharacterPanelState character;
	// Chat panel state (channel name, scrollback, presence, input).
	// Always rendered (the chat panel sits in the lobby chrome alongside
	// the right-side panel swap).
	ChatPanelState chat;
	// Per-panel state. Only the field matching `active_panel` is read.
	GameCreateState game_create;
	GameJoinState game_join;
	GameSelectState game_select;
	GameTechState game_tech;
	// Map-name overlay (legacy LobbyScreen uid 8). Empty = no label.
	// Truncated to 25 chars by the live engine to mirror the legacy
	// SetMapNameOverlay clamp.
	std::string map_name;
	// Gamepad / keyboard menu-nav cursor. v2 equivalent of the legacy
	// Interface->activeobject + tabobjects pair. -1 = no focus (default);
	// 0 = Character, 1 = Chat, 2 = RightPanel (currently active right-side
	// panel). LobbyV2NavPrev/Next on Game advance/retreat with wrap; first
	// press lifts -1 to the first/last region. No rendering is keyed off
	// this — preview PPMs stay byte-identical regardless of cursor state.
	int nav_cursor = -1;
};

// nav_cursor value enum. Stored as int on LobbyState to keep the field
// trivially serialisable into the JSON-lines control "state" op.
constexpr int kLobbyNavCount       = 3;
constexpr int kLobbyNavCharacter   = 0;
constexpr int kLobbyNavChat        = 1;
constexpr int kLobbyNavRightPanel  = 2;

// Lobby chrome plus the character panel. Remaining panels (chat /
// right-side game lists) land in P11+ and get composed in here as they
// do.
Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers = {},
                const LobbyState & state = {});

// Engine-side runtime for GameState::LOBBY. Owns the LobbyState struct
// + per-panel refresh helpers + the (deferred CreateGame / joining /
// progress / CONNECTED transition / disconnect) Tick state machine.
// Public methods are called by events.cpp (chat / create input
// routing, nav cursor) and controldispatch.cpp (CLI smoke tests).
class LobbyRuntime : public Runtime
{
public:
	LobbyRuntime(World & world, ScreenContext & sctx, Game & game);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y) override;
	bool DispatchKeyDown(int sdl_scancode) override;
	bool DispatchTextInput(char ascii) override;
	void Tick() override;

	const LobbyState & GetState() const { return state_data_; }

	// Public surface (matches the legacy Game::LobbyV2* method names).
	void HandleBack();
	void ShowGameSelect();
	void ShowGameCreate();
	void ShowGameJoin();
	void ShowGameTech();
	void SelectAgency(Uint8 agency);
	bool ChatActive() const;
	void ChatAppendChar(char c);
	void ChatBackspace();
	void ChatSubmit();
	void GameSelectRow(int index);
	void GameSelectJoin();
	bool CreateInputActive() const;
	void CreateAppendChar(char c);
	void CreateBackspace();
	void CreateSubmit();
	void CreateCycleSecurity();
	void CreateCycleMinLevel();
	void CreateCycleMaxLevel();
	void CreateCycleMaxPlayers();
	void CreateCycleMaxTeams();
	void CreateGame();
	void CreateSelectMap(int row);
	void CreateDownloadMap(int row);
	void GameJoinReady();
	void GameJoinChangeTeam();
	void GameTechToggle(int item_index);
	void GameTechSelect(int item_index);
	void NavPrev();
	void NavNext();

private:
	void RefreshCharacter();
	void RefreshChat();
	void RefreshGameSelect();
	void RefreshGameCreate();
	void RefreshGameJoin();
	void RefreshGameTech();

	World &         world_;
	ScreenContext & sctx_;
	Game &          game_;
	UIState         state_;
	LobbyState      state_data_;
	int             create_active_field_ = 1;  // 0=none, 1=name, 2=password
};

}  // namespace v2
}  // namespace ui

#endif
