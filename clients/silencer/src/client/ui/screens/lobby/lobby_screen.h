#ifndef SILENCER_CLIENT_UI_LOBBY_SCREEN_H
#define SILENCER_CLIENT_UI_LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
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

	// Map-name overlay written by the join handoff and cleared by HandleBack.
	void SetMapNameOverlay(const char * name);

	// Right-side panel swaps used by the lobby provider.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);
	void ShowGameJoin(ScreenContext & ctx);
	void ShowGameTech(ScreenContext & ctx);

private:
	// Per-frame state for the chrome tree. Strings live on the screen so the
	// layout pass can hold pointers that remain valid until the frame ends.
	// Version is cached once at Build; mapName is updated by SetMapNameOverlay.
	std::string version;
	std::string mapName;
	bool goBackClicked = false;
	bool disconnectMessageOpen = false;

	// CharacterPanel state. The lobby provider persists agency selection.
	silencer::client_ui::lobby::CharacterPanelState characterState;

	// ChatPanel state — chat scrollback + presence list + input buffer +
	// cached channel name.
	silencer::client_ui::lobby::ChatPanelState chatState;

	// GameSelect state — snapshot of the games list + selection + scroll
	// + per-frame click flags. Always-on right-pane surface; suppressed
	// when another right-side panel is active.
	silencer::client_ui::lobby::GameSelectPanelState gameSelectState;

	// GameCreate state + active flag. When `gameCreateActive` is true the
	// create panel owns the right column (suppresses the games-list tree)
	// and the provider pumps the deferred create-game state machine.
	silencer::client_ui::lobby::GameCreatePanelState gameCreateState;
	bool gameCreateActive = false;

	// GameJoin state + active flag.
	silencer::client_ui::lobby::GameJoinPanelState gameJoinState;
	bool gameJoinActive = false;

	// GameTech state + active flag.
	silencer::client_ui::lobby::GameTechPanelState gameTechState;
	bool gameTechActive = false;
};

#endif
