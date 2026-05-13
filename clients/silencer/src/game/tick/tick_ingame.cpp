#include "game.h"
#include "audio.h"
#include "player.h"
#include "team.h"
#include "world.h"
#include <stdio.h>

using namespace GameState;

void Game::TickInGame(){
if(/*!world.map.loaded && */stateisnew){
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
					world.viewedpeerid = i;
					break;
				}
			}
		}
		world.replay.oldx = world.replay.x;
		world.replay.oldy = world.replay.y;
		if(world.localinput.keymoveleft || world.localinput.keymoveright || world.localinput.keymoveup || world.localinput.keymovedown){
			world.localpeerid = world.authoritypeer;
			world.viewedpeerid = world.authoritypeer;
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
					world.viewedpeerid = i;
					break;
				}
			}
		}
		if(world.localinput.keynextcam && !world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory].keynextcam){
			for(int i = world.localpeerid + 1; i < world.maxpeers; i++){
				if(world.peerlist[i] && i != world.authoritypeer){
					world.localpeerid = i;
					world.viewedpeerid = i;
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
	// Spectator controls. Mirrors the replay block above but reads from
	// localinput edges and writes to World::viewedpeerid + World::spectator,
	// not localpeerid (network identity stays put for observers).
	if(world.IsLocalObserver()){
		Input & prevtick = world.localinputhistory[(world.tickcount - 1) % world.maxlocalinputhistory];
		// Default-mode follow: first applicable tick after a candidate exists.
		if(!world.spectator.initialized){
			Uint8 picked = world.viewedpeerid;
			bool found = false;
			for(int i = 0; i < (int)world.maxpeers; i++){
				Peer * p = world.peerlist[i];
				if(!p) continue;
				if(i == (int)world.authoritypeer) continue;
				if(p->observer) continue;
				if(p->controlledlist.empty()) continue;
				Player * pl = world.GetPeerPlayer((Uint8)i);
				if(pl && pl->state != Player::DEAD){
					picked = (Uint8)i;
					found = true;
					break;
				}
			}
			if(!found){
				for(int i = 0; i < (int)world.maxpeers; i++){
					Peer * p = world.peerlist[i];
					if(!p) continue;
					if(i == (int)world.authoritypeer) continue;
					if(p->observer) continue;
					if(p->controlledlist.empty()) continue;
					picked = (Uint8)i;
					found = true;
					break;
				}
			}
			if(found){
				world.viewedpeerid = picked;
				world.spectator.initialized = true;
			}
		}
		// If currently-followed peer became invalid (disconnected, lost
		// its Player), auto-step to the next valid candidate so the
		// spectator's view doesn't freeze.
		if(world.spectator.initialized && !world.spectator.freecam){
			Peer * cur = world.peerlist[world.viewedpeerid];
			bool stale = !cur || cur->observer || cur->controlledlist.empty()
				|| world.viewedpeerid == (Uint8)world.authoritypeer;
			if(stale){
				for(int step = 1; step <= (int)world.maxpeers; step++){
					int i = (world.viewedpeerid + step) % world.maxpeers;
					Peer * p = world.peerlist[i];
					if(!p) continue;
					if(i == (int)world.authoritypeer) continue;
					if(p->observer) continue;
					if(p->controlledlist.empty()) continue;
					world.viewedpeerid = (Uint8)i;
					break;
				}
			}
		}
		// Move Right cycles to the next player; Move Left to the previous.
		// Reusing movement bindings means observers don't need a separate
		// keybind row — the inputs are unused for them otherwise.
		if(world.localinput.keymoveright && !prevtick.keymoveright){
			for(int step = 1; step <= (int)world.maxpeers; step++){
				int i = (world.viewedpeerid + step) % world.maxpeers;
				Peer * p = world.peerlist[i];
				if(!p) continue;
				if(i == (int)world.authoritypeer) continue;
				if(p->observer) continue;
				if(p->controlledlist.empty()) continue;
				world.viewedpeerid = (Uint8)i;
				break;
			}
		}
		if(world.localinput.keymoveleft && !prevtick.keymoveleft){
			for(int step = 1; step <= (int)world.maxpeers; step++){
				int i = ((int)world.viewedpeerid - step + (int)world.maxpeers) % world.maxpeers;
				Peer * p = world.peerlist[i];
				if(!p) continue;
				if(i == (int)world.authoritypeer) continue;
				if(p->observer) continue;
				if(p->controlledlist.empty()) continue;
				world.viewedpeerid = (Uint8)i;
				break;
			}
		}
		// Hold Activate to reveal all player names. Free-cam state/renderer
		// path is preserved but no input drives it.
		world.spectator.holdshowallnames = world.localinput.keyactivate;
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
