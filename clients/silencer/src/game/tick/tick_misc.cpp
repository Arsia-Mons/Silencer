#include "game.h"
#include "world.h"

using namespace GameState;

void Game::TickFadeOut(){
	world.intutorialmode = false;
	SDL_Color * fadedpalette = renderer.palette.CopyWithBrightness(renderer.palette.GetColors(), (15 - fade_i) * 8);
	SetColors(fadedpalette);
	if(fade_i >= 16){
		state = nextstate;
		fade_i = 0;
		stateisnew = true;
	}
}
