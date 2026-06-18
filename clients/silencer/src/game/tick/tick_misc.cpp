#include "game.h"
#include "world.h"

using namespace GameState;

void Game::TickFadeOut(){
	world.intutorialmode = false;
	gameRenderer.ApplyPaletteFade(true);
	if(gameRenderer.PaletteFadeFinished()){
		state = nextstate;
		gameRenderer.RestartPaletteFade();
		stateisnew = true;
	}
}
