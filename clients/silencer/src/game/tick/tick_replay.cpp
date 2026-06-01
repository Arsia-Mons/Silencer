#include "game.h"
#include "world.h"
#include <stdio.h>

using namespace GameState;

void Game::TickReplayGame(){
	if(stateisnew){
		world.Disconnect();
		world.lobby.Disconnect();
		gameSession.UnloadGame();
		world.GetAuthorityPeer()->controlledlist.clear();
		world.DestroyAllObjects();
		stateisnew = false;
		world.gameplaystate = World::INLOBBY;
		world.replay.BeginPlaying(replayfile);
		if((world.replay.IsPlaying() && !world.replay.ReadHeader(world)) || !world.replay.IsPlaying()){
			printf("Replay error\n");
			world.replay.EndPlaying();
			screenContext.ShowMainMenu();
		}
	}else{
		while(world.replay.ReadToNextTick(world)){
			if(world.replay.GameStarted()){
				GoToState(INGAME);
				break;
			}
			world.Tick();
		}
		if(!world.replay.GameStarted()){
			world.replay.EndPlaying();
			screenContext.ShowMainMenu();
		}
	}
}
