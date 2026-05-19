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

World::World(bool mode) : lobby(this), lagsimulator(&sockethandle), audio(Audio::GetInstance()){
	this->mode = mode;
	sockethandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	unsigned long iomode = 1;
    ioctl(sockethandle, FIONBIO, &iomode);
	currentid = 1;
	memset(peerlist, 0, sizeof(peerlist));
	peercount = 0;
	authoritypeer = 0;
	localpeerid = 0;
	tickcount = 0;
	memset(oldsnapshots, 0, sizeof(oldsnapshots));
	totalsnapshots = 0;
	totalinputpackets = 0;
	gravity         = GASLoader::Get().player.worldGravity;
	maxyvelocity    = GASLoader::Get().player.worldMaxYVelocity;
	minwalldistance = GASLoader::Get().world.minWallDistance;
	replaying = false;
	state = IDLE;
	illuminate = 0;
	totalbytesread = 0;
	totalbytessent = 0;
	quitstate = 0;
	winningteamid = 0;
	message[0] = 0;
	message_i = 0;
	showchat_i = 0;
	gameplaystate = NONE;
	// LoadBuyableItems() is called from Game::Init() after GAS loads via resources.Load()
	lastpingsent = 0;
	pingtime = 0;
	highlightsecrets = false;
	highlightminimap = false;
	intutorialmode = false;
	choosingtech = false;
	boundport = 0;
	snapshotqueueminsize = GASLoader::Get().gameengine.snapshotQueueMinSize;
	snapshotqueuemaxsize = GASLoader::Get().gameengine.snapshotQueueInitMaxSize;
	lastsnapshotqueueadjust = 0;
	for(int i = 0; i < sizeof(pinghistory) / sizeof(int); i++){
		pinghistory[i] = 0;
	}
	lastpingid = 0;
	ClearMapData();
	showteamcolors = false;
	showplayerlist = false;
	debugoverlay = false;
	memset(topmessage, 0, sizeof(topmessage));
	topmessage_i = 0;
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
    shutdown(sockethandle, SHUT_RDWR);
    closesocket(sockethandle);
	delete gameMode;
	gameMode = nullptr;
	for(std::vector<BuyableItem *>::iterator it = buyableitems.begin(); it != buyableitems.end(); it++){
		delete (*it);
	}
	buyableitems.clear();
	DestroyAllObjects();
	for(unsigned int i = 0; i < maxpeers; i++){
		DeleteOldSnapshots(i);
		if(peerlist[i]){
			delete peerlist[i];
			peerlist[i] = 0;
			peercount--;
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
		if(gameplaystate == INGAME && objectsbytype[ObjectTypes::GAMESTATEOBJ].empty()){
			GameStateObject* gso = (GameStateObject*)CreateObject(ObjectTypes::GAMESTATEOBJ);
			if(gso) gso->modeId = gameMode ? gameMode->Id() : GAMEMODE_DATA_RETRIEVAL;
		}
		for(int i = 0; i < maxpeers; i++){
			Peer * peer = peerlist[i];
			if(peer){
				Player * player = GetPeerPlayer(peer->id);
				bool processed = false;
				while(player && player->CanExhaustInputQueue(*this, inputqueue[peer->id].size()) && ProcessInputQueue(*peer)){ processed = true; };
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
		Player* pp = GetPeerPlayer(localpeerid);
		// Fallback for solo/TESTGAME mode where peerlist[localpeerid] may be null
		if (!pp && objectsbytype[ObjectTypes::PLAYER].size() > 0) {
			pp = static_cast<Player*>(GetObjectFromId(objectsbytype[ObjectTypes::PLAYER].front()));
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
	if(message_i){
		message_i++;
		if(message_i >= messagetime){
			message_i = 0;
		}
	}
	if(topmessage_i){
		topmessage_i++;
	}
	if(showchat_i){
		showchat_i--;
	}
	for(std::deque<char *>::iterator it = statusmessages.begin(); it != statusmessages.end(); it++){
		char * status = *it;
		char * time = &status[strlen(status) + 1];
		(*time)--;
		if(*time == 0){
			delete[] status;
			statusmessages.erase(it);
			break;
		}
	}
}

void World::SetVersion(const char * version){
	strcpy(World::version, version);
}

