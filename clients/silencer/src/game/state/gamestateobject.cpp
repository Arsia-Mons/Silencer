#include "gamestateobject.h"
#include "gamemode.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "world.h"

GameStateObject::GameStateObject() : Object(ObjectTypes::GAMESTATEOBJ) {
	// Replicate for real: without this the object never enters snapshots
	// (Object defaults requiresauthority=false) and lobby clients can never
	// see the match result — a time-limit match end then falls through to
	// the CONNECTION LOST path instead of MISSIONSUMMARY.
	requiresauthority = true;
	snapshotinterval = 0; // RelevantToPlayer: include every authority tick
}

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
	// Replicas: every field here is authority-written and arrives via
	// snapshot — recomputing locally would clobber it (world.winningteamid
	// is never set on replicas). Adopt the match result instead so
	// CheckForEndOfGame fires client-side, same as the secrets win path
	// does via Team::Tick.
	if(world.mode == World::REPLICA){
		if(winningTeamId && !world.winningteamid){
			world.winningteamid = winningTeamId;
		}
		return;
	}
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
