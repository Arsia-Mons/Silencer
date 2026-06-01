#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct GameSessionProviderState;

struct GameSessionProviderValue {
	std::shared_ptr<GameSessionProviderState> state;
};

GameSessionProviderValue MakeGameSessionProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
