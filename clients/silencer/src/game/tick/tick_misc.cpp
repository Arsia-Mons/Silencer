#include "game.h"
#include "world.h"

using namespace GameState;

void Game::TickFadeOut(){
	world.intutorialmode = false;
	ApplyPaletteFade(true);
	if(PaletteFadeFinished()){
		gameUiPipeline.ClientUiRef().RequestClearScreens();
		state = nextstate;
		RestartPaletteFade();
		stateisnew = true;
	}
}
