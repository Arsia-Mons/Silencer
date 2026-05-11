#include "game.h"
#include "runtime.h"
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
		// Drop any active v2 runtime — the new state's stateisnew block
		// re-instantiates one via SetRuntime() if it has been migrated.
		active_runtime.reset();
	}
}
