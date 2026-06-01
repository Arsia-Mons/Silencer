#include "client/ui/hooks/use_game_session.h"

#include "game.h"
#include "screen_context.h"

namespace silencer {
namespace client_ui {

GameSessionProviderValue MakeGameSessionProvider(ScreenContext& ctx) {
	GameSessionProviderValue value;
	value.game = &ctx.game;
	return value;
}

TutorialSessionModel::TutorialSessionModel(const GameSessionProviderValue& provider)
	: provider_(provider) {}

void TutorialSessionModel::start() const {
	if(provider_.game){
		provider_.game->StartTutorial();
	}
}

GameSessionModel::GameSessionModel(const GameSessionProviderValue& provider)
	: tutorial(provider) {}

GameSessionModel use_game_session(const GameSessionProviderValue& provider) {
	return GameSessionModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
