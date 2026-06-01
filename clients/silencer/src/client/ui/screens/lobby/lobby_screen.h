#ifndef SILENCER_CLIENT_UI_LOBBY_SCREEN_H
#define SILENCER_CLIENT_UI_LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
#include "client/ui/retained/RetainedFrame.h"
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
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState& input, Uint8 hudPhase, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	enum class LobbyRightPane {
		GameSelect,
		GameCreate,
		GameJoin,
		GameTech,
	};

	// Map-name overlay written by the join handoff and cleared by HandleBack.
	void SetMapNameOverlay(const char * name);

	// Right-side panel swaps used by lobby UI callbacks and provider pumps.
	void ShowGameSelect();
	void ShowGameCreate(const silencer::client_ui::LobbyModel & lobby);
	void ShowGameJoin();
	void ShowGameTech();

	// Per-frame state for the chrome tree. Strings live on the screen so the
	// layout pass can hold pointers that remain valid until the frame ends.
	// Version is cached once at Build; mapName is updated by SetMapNameOverlay.
	std::string version;
	std::string mapName;
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

	// Right-side pane state. `rightPane` decides which one owns the column.
	silencer::client_ui::lobby::GameCreatePanelState gameCreateState;
	silencer::client_ui::lobby::GameJoinPanelState gameJoinState;
	silencer::client_ui::lobby::GameTechPanelState gameTechState;
	LobbyRightPane rightPane = LobbyRightPane::GameSelect;

	silencer::client_ui::RetainedFrame chromeFrame_;
};

#endif
