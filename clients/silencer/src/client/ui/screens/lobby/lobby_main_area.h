#ifndef SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H
#define SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H

// Lobby body: character + chat (always on) plus the stepped right pane
// (upper shelf + tall column) that swaps between GameSelect / GameCreate /
// GameJoin / GameTech. Composes into LobbyRoot beneath the title bar.

class ScreenContext;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui {
class LobbyModel;
}

namespace silencer::client_ui::lobby {

struct CharacterPanelState;
struct ChatPanelState;
struct GameSelectPanelState;
struct GameCreatePanelState;
struct GameJoinPanelState;
struct GameTechPanelState;

struct LobbyMainAreaPanels {
	CharacterPanelState & character;
	ChatPanelState & chat;
	GameSelectPanelState & gameSelect;
	GameCreatePanelState & gameCreate;
	GameJoinPanelState & gameJoin;
	GameTechPanelState & gameTech;
	bool gameCreateActive;
	bool gameJoinActive;
	bool gameTechActive;
};

// Emits the LobbyBody subtree (character + chat + stepped right pane) into
// the current Clay frame.
void BuildLobbyMainArea(LobbyMainAreaPanels & panels,
                        ScreenContext & ctx,
                        LobbyModel & lobby,
                        int bodyX,
                        int bodyY,
                        int bodyW,
                        int bodyH,
                        int regionGap,
                        silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
