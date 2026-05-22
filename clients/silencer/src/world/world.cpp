#include "world.h"
#include "serializer.h"
#include "player.h"
#include "civilian.h"
#include "robot.h"
#include "fixedcannon.h"
#include "walldefense.h"
#include "techstation.h"
#include "surveillancemonitor.h"
#include "team.h"
#include "objecttypes.h"
#include "terminal.h"
#include "basedoor.h"
#include "bodypart.h"
#include "gasloader.h"
#include "gamestateobject.h"
#include "text_wrap.h"
#include <algorithm>

#define DELTAENABLED 1

World::World(bool mode) : messaging(*this), objects(*this), network(*this), peers(*this), replication(*this), audio(Audio::GetInstance()), lobby(this){
	this->mode = mode;
	tickcount = 0;
	gravity         = GASLoader::Get().player.worldGravity;
	maxyvelocity    = GASLoader::Get().player.worldMaxYVelocity;
	minwalldistance = GASLoader::Get().world.minWallDistance;
	replaying = false;
	quitstate = 0;
	winningteamid = 0;
	gameplaystate = NONE;
	// LoadBuyableItems() is called from Game::Init() after GAS loads via resources.Load()
	highlightsecrets = false;
	highlightminimap = false;
	intutorialmode = false;
	choosingtech = false;
	ClearMapData();
	showteamcolors = false;
	showplayerlist = false;
	debugoverlay = false;
	pancameraactive = false;
	pancamerareturn = false;
	pancamerareturncount = 0;
	pancamerax = 0;
	pancameray = 0;
	viewedpeerid = 0;
	spectator.freecam = false;
	spectator.camx = 0;
	spectator.camy = 0;
	spectator.camvx = 0;
	spectator.camvy = 0;
	spectator.holdshowallnames = false;
	spectator.initialized = false;
	gameMode = GameModeFactory(GAMEMODE_DATA_RETRIEVAL);
}

World::~World(){
	Disconnect();
    shutdown(network.sockethandle, SHUT_RDWR);
    closesocket(network.sockethandle);
	delete gameMode;
	gameMode = nullptr;
	for(std::vector<BuyableItem *>::iterator it = buyableitems.begin(); it != buyableitems.end(); it++){
		delete (*it);
	}
	buyableitems.clear();
	DestroyAllObjects();
	for(unsigned int i = 0; i < maxpeers; i++){
		DeleteOldSnapshots(i);
		if(peers.peerlist[i]){
			delete peers.peerlist[i];
			peers.peerlist[i] = 0;
			peers.peercount--;
		}
	}
}

void World::Tick(void){
	if(mode == AUTHORITY){
		SendSnapshots();
	}
	if(mode == REPLICA){
		const int tps = GASLoader::Get().gameengine.ticksPerSecond;
		if(tickcount % tps == 0){
			//snapshotqueuemaxsize = ceil(float(AveragePingJitter()) / 42);
			CheckExists();
		}
		ProcessSnapshotQueue();
	}
	if(dedicatedserver.active){
		dedicatedserver.Tick(*this);
	}
	if(mode == REPLICA){
		DestroyMarkedObjects();
	}else{
		TickObjects();
		if(gameMode){
			gameMode->Tick(*this);
		}
		// Poll IsMatchOver + time limit each tick on authority.
		if(gameMode && !winningteamid && gameplaystate == INGAME){
			bool over = gameMode->IsMatchOver(*this);
			if(!over){
				const GameModeConfig* cfg = GASLoader::Get().GetGameModeConfig((int)gameMode->Id());
				const int tps = GASLoader::Get().gameengine.ticksPerSecond;
				if(cfg && cfg->timeLimitSecs > 0 && tps > 0 && (int)(tickcount / tps) >= cfg->timeLimitSecs){
					over = true;
				}
			}
			if(over){
				winningteamid = gameMode->WinningTeamId(*this);
				if(!winningteamid) winningteamid = 0xFFFF; // draw
			}
		}
		// Fire OnMatchEnd exactly once when the match transitions to over.
		if(gameMode && winningteamid && !matchEndCalled){
			matchEndCalled = true;
			gameMode->OnMatchEnd(*this);
		}
		// Create the replicated match-state object once per match on authority.
		if(gameplaystate == INGAME && objects.objectsbytype[ObjectTypes::GAMESTATEOBJ].empty()){
			GameStateObject* gso = (GameStateObject*)CreateObject(ObjectTypes::GAMESTATEOBJ);
			if(gso) gso->modeId = gameMode ? gameMode->Id() : GAMEMODE_DATA_RETRIEVAL;
		}
		for(int i = 0; i < maxpeers; i++){
			Peer * peer = peers.peerlist[i];
			if(peer){
				Player * player = GetPeerPlayer(peer->id);
				bool processed = false;
				while(player && player->CanExhaustInputQueue(*this, replication.inputqueue[peer->id].size()) && ProcessInputQueue(*peer)){ processed = true; };
				if(!processed){
					ProcessInputQueue(*peer);
				}
			}
		}
	}
	if(tickcount % GASLoader::Get().gameengine.ticksPerSecond == 0 && IsAuthority()){
		ActivateTerminals();
	}

	// Emit GAME_START on the very first tick so trigger scripts can respond.
	if (tickcount == 0 && IsAuthority()) {
		GameEvent ev;
		ev.type = EventType::GAME_START;
		triggerGraph.Bus().Emit(ev);
	}

	// Emit PLAYER_SPAWN the first time the local player exists in the world.
	if (!player_spawn_emitted && IsAuthority() && gameplaystate == World::INGAME) {
		Player* pp = GetPeerPlayer(peers.localpeerid);
		// Fallback for solo/TESTGAME mode where peerlist[localpeerid] may be null
		if (!pp && objects.objectsbytype[ObjectTypes::PLAYER].size() > 0) {
			pp = static_cast<Player*>(GetObjectFromId(objects.objectsbytype[ObjectTypes::PLAYER].front()));
		}
		if (pp != nullptr) {
			player_spawn_emitted = true;
			GameEvent ev;
			ev.type = EventType::PLAYER_SPAWN;
			triggerGraph.Bus().Emit(ev);
		}
	}

	// Countdown timer for camera return pan — releases input when it hits 0.
	if(pancamerareturncount > 0){
		pancamerareturncount--;
		if(pancamerareturncount == 0){
			pancamerareturn = false;
			input_locked = false;
		}
	}

	{
		const float dt = 1.f / GASLoader::Get().gameengine.ticksPerSecond;
		triggerGraph.Tick(*this, dt);
	}

	tickcount++;
	if(replay.IsRecording()){
		replay.WriteTick();
	}
	if(messaging.message_i){
		messaging.message_i++;
		if(messaging.message_i >= messaging.messagetime){
				}
	}
	if(messaging.topmessage_i){
		messaging.topmessage_i++;
	}
	if(messaging.showchat_i){
		messaging.showchat_i--;
	}
	for(std::deque<char *>::iterator it = messaging.statusmessages.begin(); it != messaging.statusmessages.end(); it++){
		char * status = *it;
		char * time = &status[strlen(status) + 1];
		(*time)--;
		if(*time == 0){
			delete[] status;
			messaging.statusmessages.erase(it);
			break;
		}
	}
}

void World::SetVersion(const char * version){
	strcpy(World::version, version);
}

