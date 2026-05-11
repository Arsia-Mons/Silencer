#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyHandlers {
	std::function<void()> on_go_back;
};

// Lobby chrome only — background, "Silencer" title, version string, and
// the Go Back button. The character / chat / right-side panels live in
// separate Build functions (P10–P15) and get composed in once they land.
Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
