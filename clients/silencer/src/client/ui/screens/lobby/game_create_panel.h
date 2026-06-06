#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_CREATE_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_CREATE_PANEL_H

// Screen-side lobby GameCreatePanel: the game-options form on the right pane.
// The upper options pane is declared by retained cppx; this module owns the
// screen-local state, layout metrics, and retained frame metadata.
//
// Domain glue (CreateGame kickoff, config persistence, async map upload)
// lives behind LobbyProvider/use_lobby. Primitives stay screen-agnostic.

#include "shared.h"

#include <string>
#include <vector>

class MessageModal;

namespace silencer::ui {
struct UiInputState;
}

namespace silencer::client_ui {
class AppAudioModel;
class LobbyModel;
}

namespace silencer::client_ui::lobby {

constexpr const char * kGameCreateOptionsScrollId = "lobby.game_create.options";
constexpr const char * kGameCreateOptionsScrollLabel = "Game Options Form";

struct GameCreatePanelState {
	// Form fields — buffers sized to match the legacy TextInput maxchars.
	char name[36]      = {0};   // legacy maxchars = 35.
	char password[21]  = {0};   // legacy maxchars = 20.
	char minLevel[3]   = "0";   // legacy maxchars = 2.
	char maxLevel[3]   = "99";
	char maxPlayers[3] = "24";
	char maxTeams[3]   = "6";

	// Security cycler: 0=Off, 1=Low, 2=Medium (default), 3=High.
	Uint8 securityIndex = 2;
	bool  spectatable   = true;  // hydrated from LobbyCreateModel defaults on init.

	// Map list snapshot. Built once during Init from the local level dirs
	// + community server list. Display strings include any "[DL] " prefix
	// for remote-only maps.
	std::vector<std::string> maps;
	int    mapSelectedIndex = -1;
	Uint16 mapScrollPos     = 0;

	// Upper "Game Options" viewport state. The panel owns the visible-row
	// window and applies wheel/control-socket scroll in row units.
	Uint16 optionsScrollPosition = 0;
	int    optionsScrollDelta = 0;
	Uint16 optionsMaxScroll = 0;
	Uint8  optionsVisibleRows = 0;

	// Hover-preview cache for local map rows in the Create flow.
	int  lastHoveredMapIndex = -1;  // tracks hover changes to trigger sound once per row
	bool hoverPreviewVisible = false;
	int  hoverPreviewMapIndex = -1;
	std::string hoverPreviewName;
	std::string hoverPreviewDescription;
	std::vector<Uint8> hoverPreviewPixels;

	// Create/upload progress overlay owned by this workflow. Tracking the modal
	// here keeps lobby code from inspecting the global screen stack through Game.
	MessageModal * progressModal = nullptr;
};

struct GameCreateOptionsLayout {
	Uint16 titleHeight = 0;
	Uint16 viewportWidth = 0;
	Uint16 viewportHeight = 0;
	Uint16 valueColumnWidth = 0;
	Uint16 scrollMax = 0;
	Uint8 visibleRows = 0;
	bool showScrollbar = false;
};

struct GameCreateOptionsScrollbarLayout {
	Uint16 topSpacer = 0;
	Uint16 thumbHeight = 0;
	Uint16 bottomSpacer = 0;
};

struct GameCreateTallLayout {
	Uint16 listBoxWidth = 0;
	Uint16 listBoxHeight = 0;
	Uint16 listWidth = 0;
	Uint16 listHeight = 0;
	Uint16 inputWidth = 0;
	Uint8 visibleMapRows = 0;
};

struct GameCreatePreviewOverlayLayout {
	bool visible = false;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	int lineHeight = 0;
	int gap = 0;
	int bitmapWidth = 0;
	int bitmapHeight = 0;
};

enum class GameCreatePanelTextField {
	MinLevel,
	MaxLevel,
	MaxPlayers,
	MaxTeams,
	Name,
	Password,
};

// Hydrate state from LobbyCreateModel defaults and rebuild the map list.
void GameCreatePanelInit(GameCreatePanelState & state,
                         const LobbyModel & lobby);

// Per-frame pump for the deferred CreateGame state machine (map upload ->
// CreateGame -> CONNECTED -> joined-game pane handoff + progress-modal spinner
// update + create-failure unwind).
void GameCreatePanelTick(GameCreatePanelState & state,
                         LobbyModel & lobby);
void GameCreatePanelCycleSecurity(GameCreatePanelState & state);
void GameCreatePanelToggleSpectatable(GameCreatePanelState & state,
                                      const LobbyModel & lobby);
void GameCreatePanelSelectMap(GameCreatePanelState & state,
                              const LobbyModel & lobby,
                              int index);
void GameCreatePanelSubmit(GameCreatePanelState & state,
                           const LobbyModel & lobby);
MessageModal * GameCreatePanelProgressModal(GameCreatePanelState & state);
void GameCreatePanelDismissProgressModal(GameCreatePanelState & state);
void GameCreatePanelSetText(GameCreatePanelState & state,
                            GameCreatePanelTextField field,
                            const char * value);
void GameCreatePanelScrollOptions(GameCreatePanelState & state, int amount);

GameCreateOptionsLayout ResolveGameCreateOptionsLayout(Uint16 panelWidth,
                                                       Uint16 panelHeight);
GameCreateOptionsScrollbarLayout ResolveGameCreateOptionsScrollbarLayout(
	const GameCreateOptionsLayout & layout,
	const GameCreatePanelState & state);
void GameCreatePanelSyncOptionsLayout(GameCreatePanelState & state,
                                      Uint16 panelWidth,
                                      Uint16 panelHeight);
GameCreateTallLayout ResolveGameCreateTallLayout(Uint16 panelWidth,
                                                 Uint16 panelHeight);
void GameCreatePanelSyncTallLayout(GameCreatePanelState & state,
                                   const silencer::ui::UiInputState & input,
                                   const AppAudioModel & audio,
                                   LobbyModel & lobby,
                                   Uint16 panelWidth,
                                   Uint16 panelHeight,
                                   int panelX,
                                   int panelY);
GameCreatePreviewOverlayLayout ResolveGameCreatePreviewOverlayLayout(
	const GameCreatePanelState & state,
	const silencer::ui::UiInputState & input);

}  // namespace silencer::client_ui::lobby

#endif
