#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyConnectHandlers {
	std::function<void()> on_login;
	std::function<void()> on_cancel;
};

Node BuildLobbyConnect(const Context & ctx, const LobbyConnectHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
