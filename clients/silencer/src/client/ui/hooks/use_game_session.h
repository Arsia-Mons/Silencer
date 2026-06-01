#pragma once

#include "client/ui/providers/game_session_provider.h"

namespace silencer {
namespace client_ui {

class TutorialSessionModel {
public:
	explicit TutorialSessionModel(const GameSessionProviderValue& provider);

	void start() const;

private:
	GameSessionProviderValue provider_;
};

class GameSessionModel {
public:
	explicit GameSessionModel(const GameSessionProviderValue& provider);

	TutorialSessionModel tutorial;
};

GameSessionModel use_game_session(const GameSessionProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
