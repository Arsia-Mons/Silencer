#ifndef SILENCER_CLIENT_UI_LOBBY_SCREEN_H
#define SILENCER_CLIENT_UI_LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
#include <cstdint>
#include <functional>
#include <string>

class Surface;

// Top-level lobby surface. The chrome (background image, "Silencer" title,
// version string, map-name overlay, "Go Back" button), the four right-side
// panels, and the always-on character + chat panels are emitted by per-screen
// state structs; no retained world UI objects are created for the lobby UI.
class LobbyScreen : public Screen
{
public:
	LobbyScreen();
	~LobbyScreen() override;

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

	// Map-name overlay (uid 8 on the legacy chrome) — written by the
	// CONNECTED→GameJoin handoff and cleared by HandleBack.
	void SetMapNameOverlay(const char * name);

	// Right-side panel swap helpers. Called by lobby panels (GameSelect's
	// "Create Game" button, GameJoin's "Choose Tech", GameTech's "Back To
	// Teams") and by the `lobby_show_panel` CLI op.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);
	void ShowGameJoin(ScreenContext & ctx);
	void ShowGameTech(ScreenContext & ctx);

private:
	void QueueGoBack();
	uint32_t SelectedLobbyGameId() const;

	// Per-frame state for the chrome tree. Strings live on the screen so the
	// layout pass can hold pointers that remain valid until the frame ends.
	// Version is cached once at Build; mapName is updated by SetMapNameOverlay.
	std::string version;
	std::string mapName;
	std::function<void()> goBack;
	std::function<void()> showGameCreateQueued;
	std::function<void()> showGameJoinQueued;
	std::function<void()> showGameTechQueued;
	std::function<void()> sendGameJoinReady;
	std::function<void()> changeGameJoinTeam;
	std::function<void()> beginGameTechSelection;
	std::function<void(int)> toggleGameTechChoice;
	std::function<void(uint32_t)> joinLobbyGame;
	std::function<void(uint32_t)> spectateLobbyGame;
	bool goBackQueued = false;

	// CharacterPanel state — declaration reads live lobby data through UseLobby;
	// pending intent + agency sync are flushed from the lobby lifecycle.
	silencer::client_ui::lobby::CharacterPanelState characterState;
	int lastSyncedCharacterAgency = -1;

	// ChatPanel state — chat scrollback + presence list + input buffer +
	// cached channel name.
	silencer::client_ui::lobby::ChatPanelState chatState;

	// GameSelect state — snapshot of the games list + selection + scroll.
	// Always-on right-pane surface; suppressed when another right-side panel
	// is active.
	silencer::client_ui::lobby::GameSelectPanelState gameSelectState;

	// GameCreate state + active flag. When `gameCreateActive` is true the
	// create panel owns the right column (suppresses the games-list tree)
	// and the screen's Tick pumps the deferred CreateGame state
	// machine.
	silencer::client_ui::lobby::GameCreatePanelState gameCreateState;
	bool gameCreateActive = false;

	// GameJoin state + active flag.
	silencer::client_ui::lobby::GameJoinPanelState gameJoinState;
	bool gameJoinActive = false;

	// GameTech state + active flag. Lobby write hooks set
	// `world.choosingtech` before this panel is shown from UI.
	silencer::client_ui::lobby::GameTechPanelState gameTechState;
	bool gameTechActive = false;
};

#endif
