#ifndef SILENCER_CLIENT_UI_LOBBY_CHROME_H
#define SILENCER_CLIENT_UI_LOBBY_CHROME_H

// Lobby chrome: the top title bar (Silencer title + version, optional
// map-name overlay, Go Back button). Always a single row; smaller windows
// simply drop the map-name line instead of switching to a second layout.

#include <cstdint>
#include <string>

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

constexpr uint16_t kLobbyTitleBarH = 29;
uint16_t LobbyTitleBarHeight();

// Emits the title bar subtree into the current Clay frame and registers the
// Go Back button hit rect at its legacy screen coordinates.
void BuildLobbyTitleBar(const std::string & version,
                        const std::string & mapName,
                        int surfaceW,
                        silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
