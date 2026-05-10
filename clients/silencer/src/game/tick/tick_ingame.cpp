#include "game.h"
#include "audio.h"
#include "interface.h"
#include "player.h"
#include "team.h"
#include "world.h"
#include <stdio.h>

using namespace GameState;

void Game::TickInGame(){
if(/*!world.map.loaded && */stateisnew){
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		switch(object->type){
			case ObjectTypes::TEAM:{
				Team * team = static_cast<Team *>(object);
				team->DestroyOverlays(world);
			}break;
			case ObjectTypes::INTERFACE:{
				Interface * iface = static_cast<Interface *>(object);
				iface->DestroyInterface(world, iface);
			}break;
		}
	}
	screenbuffer.Clear(0);
	//char mapname[7 + 256];
	//sprintf(mapname, "level/%s", world.gameinfo.mapname);
	if(!LoadMap(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).c_str())){
		printf("Unable to load map\n");
		if(world.replay.IsPlaying()){
			world.replay.EndPlaying();
		}
		world.Disconnect();
		if(world.lobby.state == Lobby::AUTHENTICATED){
			world.lobby.JoinChannel(world.lobby.lastchannel);
			GoToState(LOBBY);
		}else{
			GoToState(MAINMENU);
		}
		return;
	}
	State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
	if(sharedstateobject){
		sharedstateobject->state = 2;
	}
	if(world.replay.IsRecording()){
		world.replay.WriteStart();
	}
	ShowDeployMessage();
	Audio::GetInstance().StopMusic();
	world.gameplaystate = World::INGAME;
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		switch(object->type){
			case ObjectTypes::TEAM:{
				Team * team = static_cast<Team *>(object);
				for(int i = 0; i < team->numpeers; i++){
					Peer * peer = world.peerlist[team->peers[i]];
					if(peer){
						world.ingameusers.push_back(peer->accountid);
						User * user = world.lobby.GetUserInfo(peer->accountid);
						if(user){
							user->statsagency = team->agency;
							user->teamnumber = team->number;
						}
						Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
						if(player){
							world.map.RandomPlayerStartLocation(world, player->x, player->y);
							player->oldx = player->x;
							player->oldy = player->y;
							Uint8 teamcolor = team->GetColor();
							player->suitcolor = teamcolor;//(((teamcolor >> 4) - i) << 4) + (teamcolor & 0xF);
							peer->controlledlist.clear();
							peer->controlledlist.push_back(player->id);
							GiveDefaultItems(*player);
						}
					}
				}
			}break;
		}
	}
	world.SendPeerList();
	currentinterface = 0;
	renderer.palette.SetPalette(0);
	renderer.palette.SetParallaxColors(world.map.parallax);
	screenbuffer.Clear(0);
	SetColors(renderer.palette.GetColors());
	ambienceMixer.LoadRandomGameMusic();
	stateisnew = false;
}else{
	if(ambienceMixer.FadedIn()){
		//Audio::GetInstance().ambienceMixer.PlayMusic(world.resources.gamemusic);
		ambienceMixer.PlayMusic(world.resources.gamemusic);
	}
	if(world.replay.IsPlaying()){
		// replay controls
		if(world.localpeerid == world.authoritypeer && !deploymessageshown){
			for(int i = 0; i < world.maxpeers; i++){
				if(world.peerlist[i] && i != world.authoritypeer){
					world.localpeerid = i;
					break;
				}
			}
		}
		world.replay.oldx = world.replay.x;
		world.replay.oldy = world.replay.y;
		if(world.localinput.keymoveleft || world.localinput.keymoveright || world.localinput.keymoveup || world.localinput.keymovedown){
			world.localpeerid = world.authoritypeer;
			bool inbase = false;
			if(world.replay.y > world.map.height * 64){
				inbase = true;
			}
			if(world.localinput.keymoveleft){
				if(world.replay.xv > 0){
					world.replay.xv = 0;
				}
				world.replay.xv -= 3;
				world.replay.x += world.replay.xv;
				if(world.replay.x < 320){
					world.replay.x = 320;
				}
			}else
			if(world.localinput.keymoveright){
				if(world.replay.xv < 0){
					world.replay.xv = 0;
				}
				world.replay.xv += 3;
				world.replay.x += world.replay.xv;
				if(world.replay.x > ((inbase ? world.map.expandedwidth : world.map.width) * 64) - 320){
					world.replay.x = ((inbase ? world.map.expandedwidth : world.map.width) * 64) - 320;
				}
			}else{
				world.replay.xv = 0;
			}
			if(world.localinput.keymoveup){
				if(world.replay.yv > 0){
					world.replay.yv = 0;
				}
				world.replay.yv -= 3;
				world.replay.y += world.replay.yv;
				if(world.replay.y < 240){
					world.replay.y = 240;
				}
			}else
			if(world.localinput.keymovedown){
				if(world.replay.yv < 0){
					world.replay.yv = 0;
				}
				world.replay.yv += 3;
				world.replay.y += world.replay.yv;
				if(world.replay.y > ((inbase ? world.map.expandedheight : world.map.height) * 64) - 240){
					world.replay.y = ((inbase ? world.map.expandedheight : world.map.height) * 64) - 240;
				}
			}else{
				world.replay.yv = 0;
			}
		}else{
			world.replay.xv = 0;
			world.replay.yv = 0;
		}
		if(world.localinput.keyprevcam && !world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory].keyprevcam){
			for(int i = world.localpeerid - 1; i > 0; i--){
				if(world.peerlist[i] && i != world.authoritypeer){
					world.localpeerid = i;
					break;
				}
			}
		}
		if(world.localinput.keynextcam && !world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory].keynextcam){
			for(int i = world.localpeerid + 1; i < world.maxpeers; i++){
				if(world.peerlist[i] && i != world.authoritypeer){
					world.localpeerid = i;
					break;
				}
			}

		}
		world.replay.speed = 1;
		if(world.localinput.keydetonate){
			world.replay.speed = 0.05;
		}
		if(world.localinput.keyuse){
			world.replay.speed = 2;
		}
		if(world.localinput.keyjump){
			world.replay.showallnames = true;
		}else{
			world.replay.showallnames = false;
		}
		//
		if(!world.replay.ReadToNextTick(world)){
			world.replay.EndPlaying();
			GoToState(MAINMENU);
		}
	}
	Peer * localpeer = world.peerlist[world.localpeerid];
	if(localpeer && world.localpeerid != world.authoritypeer){
		if(localpeer->controlledlist.size() == 0){
			world.RequestPeerList();
		}
	}
	if(!deploymessageshown && world.messagetype == 1 && world.message_i == 63){
		world.ShowMessage((char *)"Location : Base Arsia Mons, Surface Temperature : -7C", 96, 1);
		deploymessageshown = true;
	}
	if(CheckForQuit()){
		world.Disconnect();
		if(world.lobby.state == Lobby::AUTHENTICATED){
			GoToState(LOBBY);
			world.lobby.JoinChannel(world.lobby.lastchannel);
		}else{
			if(world.replay.IsPlaying()){
				world.replay.EndPlaying();
			}
			GoToState(MAINMENU);
		}
	}
	if(CheckForEndOfGame()){
		if(world.lobby.state == Lobby::AUTHENTICATED){
			GoToState(MISSIONSUMMARY);
		}else{
			if(world.replay.IsPlaying()){
				world.replay.EndPlaying();
			}
			GoToState(MAINMENU);
		}
	}
	if(CheckForConnectionLost()){
		if(world.lobby.state == Lobby::AUTHENTICATED){
			GoToState(LOBBY);
			world.lobby.JoinChannel(world.lobby.lastchannel);
		}else{
			GoToState(MAINMENU);
		}
	}
}
}
