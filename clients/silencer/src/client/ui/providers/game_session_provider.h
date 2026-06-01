#pragma once

class Game;
class ScreenContext;

namespace silencer {
namespace client_ui {

struct GameSessionProviderValue {
	Game * game = nullptr;
};

GameSessionProviderValue MakeGameSessionProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
