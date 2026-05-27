#ifndef SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H
#define SILENCER_CLIENT_UI_LOBBY_MAIN_AREA_H

// Lobby body: character + chat (always on) plus the stepped right pane
// (upper shelf + tall column) that swaps between GameSelect / GameCreate /
// GameJoin / GameTech. Composes into LobbyRoot beneath the title bar.

#include <functional>

class ScreenContext;

namespace silencer::client_ui::lobby {

using LobbyMainAreaChild = std::function<void(int width, int height)>;

// Emits the LobbyBody subtree (character + chat + stepped right pane) into
// the current Clay frame.
void BuildLobbyMainArea(ScreenContext & ctx,
                        int bodyX,
                        int bodyY,
                        int bodyW,
                        int bodyH,
                        int regionGap,
                        const LobbyMainAreaChild & buildCharacter,
                        const LobbyMainAreaChild & buildChat,
                        const LobbyMainAreaChild & buildRightUpper,
                        const LobbyMainAreaChild & buildRightTall);

}  // namespace silencer::client_ui::lobby

#endif
