#include "client/ui/hooks/use_game_session.h"

#include "game.h"
#include "screen_context.h"

#include <memory>

namespace silencer {
namespace client_ui {

struct GameSessionProviderState {
	Game * game = nullptr;
};

GameSessionProviderValue MakeGameSessionProvider(ScreenContext& ctx) {
	GameSessionProviderValue value;
	value.state = std::make_shared<GameSessionProviderState>();
	value.state->game = &ctx.game;
	return value;
}

namespace game_session_provider_detail {

Game * GameFor(const GameSessionProviderValue& provider) {
	return provider.state ? provider.state->game : nullptr;
}

}  // namespace game_session_provider_detail

TutorialSessionModel::TutorialSessionModel(const GameSessionProviderValue& provider)
	: provider_(provider) {}

void TutorialSessionModel::start() const {
	if(Game * game = game_session_provider_detail::GameFor(provider_)){
		game->StartTutorial();
	}
}

GameSessionModel::GameSessionModel(const GameSessionProviderValue& provider)
	: tutorial(provider) {}

GameSessionModel use_game_session(const GameSessionProviderValue& provider) {
	return GameSessionModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
