#ifndef SILENCER_CLIENT_UI_LOBBY_CHROME_H
#define SILENCER_CLIENT_UI_LOBBY_CHROME_H

// Lobby chrome: the top title bar (Silencer title + version, optional
// map-name overlay, Go Back button). The layout splits into a single row
// at standard widths and a two-row stack on narrow widths so the map name
// remains readable.

#include <cstdint>
#include <string>

namespace silencer::client_ui::lobby {

constexpr uint16_t kLobbyTitleBarH = 29;
constexpr uint16_t kLobbyTitleBarMapH = 48;
constexpr uint16_t kLobbyNarrowBreakpointW = 560;

bool LobbyUseNarrowLayout(int surfaceW);
uint16_t LobbyTitleBarHeight(bool narrow, const std::string & mapName);

// Emits the title bar subtree into the current Clay frame and registers the
// Go Back button hit rect at its legacy screen coordinates.
void BuildLobbyTitleBar(const std::string & version,
                        const std::string & mapName,
                        bool narrow,
                        int surfaceW);

}  // namespace silencer::client_ui::lobby

#endif
