#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct LobbyProviderState;

struct LobbyProviderValue {
	std::shared_ptr<LobbyProviderState> state;
};

LobbyProviderValue MakeLobbyProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
