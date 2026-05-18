#include "gamestateobject.h"
#include "gamemode.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "world.h"

GameStateObject::GameStateObject() : Object(ObjectTypes::GAMESTATEOBJ) {}

void GameStateObject::Serialize(bool write, Serializer& data, Serializer* old) {
	Object::Serialize(write, data, old);
	data.Serialize(write, (Uint8&)modeId,     old);
	data.Serialize(write, matchPhase,          old);
	data.Serialize(write, matchTimeSecs,       old);
	data.Serialize(write, winningTeamId,       old);
	for(int i = 0; i < 6; i++){
		data.Serialize(write, score[i], old);
	}
}

void GameStateObject::Tick(World& world) {
	const int tps = GASLoader::Get().gameengine.ticksPerSecond;
	if(tps > 0){
		matchTimeSecs = (Uint16)(world.tickcount / tps);
	}
	winningTeamId = world.winningteamid;
	if(world.winningteamid){
		matchPhase = 2;
	} else if(world.gameplaystate == World::INGAME){
		matchPhase = 1;
	} else {
		matchPhase = 0;
	}
	if(world.gameMode){
		world.gameMode->UpdateScores(*this, world);
	}
}
