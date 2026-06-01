#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <SDL3/SDL_stdinc.h>

namespace GameState {
enum : Uint8 {
	NONE,
	FADEOUT,
	FRONTEND,
	INGAME,
	SINGLEPLAYERGAME,
	HOSTGAME,
	JOINGAME,
	REPLAYGAME,
	TESTGAME,
};
}

#endif
