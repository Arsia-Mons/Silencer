#ifndef SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H
#define SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H

// Lobby body: character + chat (always on) + the panel-switching right
// column (GameSelect / GameCreate / GameJoin / GameTech). Composes into
// LobbyRoot beneath the title bar.

class World;
class Resources;
class LobbyScreen;

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

// Emits the LobbyBody subtree (narrow stack OR wide left/middle/right
// columns) into the current Clay frame.
void BuildLobbyMainArea(LobbyMainAreaPanels & panels,
                        World & world,
                        Resources & resources,
                        LobbyScreen & owner,
                        bool narrow,
                        int bodyH);

}  // namespace silencer::client_ui::lobby

#endif
