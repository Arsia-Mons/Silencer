#include "game.h"
#include "audio.h"
#include "gasloader.h"
#include "lobbygame.h"
#include "player.h"
#include "playerai.h"
#include "team.h"
#include "world.h"
#include <cstring>

using namespace GameState;

void Game::TickHostGame(){
	if(stateisnew){
		world.network.lagsimulator.Activate(200, 200, 0.0f);
		world.Listen(12456);
		world.DestroyAllObjects();
		Audio::GetInstance().StopMusic();
		world.gameplaystate = World::INLOBBY;
		world.gameinfo.accountid = 1;
		world.dedicatedserver.active = true;
		world.dedicatedserver.accountid = 1;
		State * newsharedstateobject = static_cast<State *>(world.CreateObject(ObjectTypes::STATE));
		sharedstate = newsharedstateobject->id;
		newsharedstateobject->state = 0;
		world.replay.BeginRecording("testrecording.zsr");
		if(world.replay.IsRecording()){
			world.replay.WriteHeader(world);
			world.replay.WriteGameInfo(world.gameinfo);
		}
		stateisnew = false;
	}
	gameSession.MapDownloaderRef().ProcessMapDownload();
	/*if(world.tickcount % 48 == 0){
		world.SendPeerList();
	}*/
	if(!world.map.loaded && world.peers.peercount >= 1 && world.AllPeersLoadedGameInfo() && world.AllPeersDownloadedMap()){
		GetScreenBuffer().Clear(0);
		if(world.replay.IsRecording()){
			world.replay.WriteStart();
		}
		//char mapname[256];
		//sprintf(mapname, "level/%s", world.gameinfo.mapname);
		gameSession.LoadMap(gameSession.MapDownloaderRef().FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str());
		renderer.palette.SetPalette(0);
		renderer.palette.SetParallaxColors(world.map.parallax);
		gameRenderer.SetColors(renderer.palette.GetColors());
		State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
		if(sharedstateobject){
			sharedstateobject->state = 2;
		}
		//world.GetAuthorityPeer()->controlledlist.clear();
		world.gameplaystate = World::INGAME;
		for(std::list<Object *>::iterator it = world.objects.objectlist.begin(); it != world.objects.objectlist.end(); it++){
			Object * object = *it;
			switch(object->type){
				case ObjectTypes::TEAM:{
					Team * team = static_cast<Team *>(object);
					if(team){
						for(int i = 0; i < team->numpeers; i++){
							Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
							if(player){
								world.map.RandomPlayerStartLocation(world, player->x, player->y);
								player->oldx = player->x;
								player->oldy = player->y;
								//player->AddInventoryItem(Player::INV_VIRUS);
								player->credits = GASLoader::Get().player.startingCredits;
								Uint8 teamcolor = team->GetColor();
								player->suitcolor = (((teamcolor >> 4) - i) << 4) + (teamcolor & 0xF);
								for(int j = 0; j < 5; j++){
									world.peers.peerlist[team->peers[i]]->techchoices |= 1 << (rand() % 10);
								}
								world.peers.peerlist[team->peers[i]]->techchoices = 0xffffffff;
								world.peers.peerlist[team->peers[i]]->controlledlist.clear();
								world.peers.peerlist[team->peers[i]]->controlledlist.push_back(player->id);
								gameSession.GiveDefaultItems(*player);
							}
						}
					}
				}break;
			}
		}
		world.SendPeerList();
	}
}

void Game::TickJoinGame(){
	if(stateisnew){
		strcpy(world.gameinfo.mapname, "STAR72.SIL");
		gameSession.MapDownloaderRef().CalculateMapHash(gameSession.MapDownloaderRef().FindMap(world.gameinfo.mapname).c_str(), &world.gameinfo.maphash);
		world.gameinfo.accountid = 1;
		world.gameinfo.loaded = true;
		sharedstate = 0;
		Peer * authoritypeer = world.GetAuthorityPeer();
		authoritypeer->ip = ntohl(inet_addr("127.0.0.1"));
		authoritypeer->port = 12456;
		world.Connect(rand() % 5, 1);
		gameSession.MapDownloaderRef().LoadMapData(gameSession.MapDownloaderRef().FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str());
		//printf("map data: %d %d\n", world.currentmapdatalength, world.currentmapdatamax);
		Audio::GetInstance().StopMusic();
		world.DestroyAllObjects();
		stateisnew = false;
	}else{
		State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
		if(sharedstateobject && sharedstateobject->state == 2){
			world.gameplaystate = World::INGAME;
		}
		gameSession.MapDownloaderRef().ProcessMapDownload();
	}
}

void Game::TickTestGame(){
	if(stateisnew){
		world.GetAuthorityPeer()->controlledlist.clear();
		world.DestroyAllObjects();
		world.gameplaystate = World::INGAME;
		Audio::GetInstance().StopMusic();
		world.GetAuthorityPeer()->techchoices = 0xffffffff;//World::BUY_LASER | World::BUY_ROCKET;
		Team * team = (Team *)world.CreateObject(ObjectTypes::TEAM);
		team->AddPeer(world.GetAuthorityPeer()->id);
		team->agency = Team::LAZARUS;
		//team->color = ((8 << 4) + 13);
		Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
		player->suitcolor = team->GetColor();
		player->laserammo = 0;
		player->credits = GASLoader::Get().player.creditCap;
		player->oldx = player->x;
		player->oldy = player->y;
		world.GetAuthorityPeer()->controlledlist.push_back(player->id);
		gameSession.GiveDefaultItems(*player);
		int botnum = 0;
		// Spawn a mix of difficulties: 4 easy, 4 medium, 2 hard
		const PlayerAI::Difficulty diffs[10] = {
			PlayerAI::EASY, PlayerAI::EASY, PlayerAI::EASY, PlayerAI::EASY,
			PlayerAI::MEDIUM, PlayerAI::MEDIUM, PlayerAI::MEDIUM, PlayerAI::MEDIUM,
			PlayerAI::HARD, PlayerAI::HARD
		};
		for(int i = 0; i < 10; i++){
			Uint8 agency;
			do{
				agency = rand() % 5;
			}while(agency == Team::BLACKROSE);
			Peer * botpeer = world.AddBot(agency);
			if(botpeer){
				botpeer->accountid = 0xFFFFFFFF - botnum;
				Team * botteam = world.GetPeerTeam(botpeer->id);
				Player * botplayer = (Player *)world.CreateObject(ObjectTypes::PLAYER);
				botplayer->suitcolor = botteam->GetColor();
				botplayer->laserammo = 0;
				botplayer->credits = GASLoader::Get().player.startingCredits;
				botplayer->ai = new PlayerAI(*botplayer, diffs[botnum]);
				botpeer->controlledlist.push_back(botplayer->id);
				world.map.RandomPlayerStartLocation(world, botplayer->x, botplayer->y);
				botplayer->oldx = botplayer->x;
				botplayer->oldy = botplayer->y;
				botnum++;
			}
		}
		world.gameinfo.securitylevel = LobbyGame::SECHIGH;
		gameSession.LoadMap("level/ALLY10c.sil");
		for(std::list<Object *>::iterator it = world.objects.objectlist.begin(); it != world.objects.objectlist.end(); it++){
			if((*it)->type == ObjectTypes::PLAYER){
				Player * player = static_cast<Player *>(*it);
				world.map.RandomPlayerStartLocation(world, player->x, player->y);
			}
		}
		gameSession.ShowDeployMessage();
		renderer.palette.SetPalette(0);
		renderer.palette.SetParallaxColors(world.map.parallax);
		GetScreenBuffer().Clear(0);
		gameRenderer.SetColors(renderer.palette.GetColors());
		singleplayermessage = 0;
		stateisnew = false;
	}else{
		/*Player * localplayer = world.GetPeerPlayer(world.peers.authoritypeer);
		if(localplayer){
			if(localplayer->state == Player::STANDINGSHOOT){
				for(std::list<Object *>::iterator it = world.objects.objectlist.begin(); it != world.objects.objectlist.end(); it++){
					if((*it)->type == ObjectTypes::PLAYER){
						Player * player = static_cast<Player *>(*it);
						if(player->ai){
							//player->ai->SetTarget(world, localplayer->x, localplayer->y);
							player->hassecret = true;
							break;
						}
					}
				}
			}
		}*/
		if(gameSession.CheckForQuit() || gameSession.CheckForEndOfGame()){
			GoToState(MAINMENU);
		}
	}
}
