#include "game.h"
#include "audio.h"
#include "config.h"
#include "gasloader.h"
#include "interface.h"
#include "player.h"
#include "selectbox.h"
#include "team.h"
#include "textinput.h"
#include "world.h"
#include <cstring>

bool Game::LoadMap(const char * name){
	if(!world.map.Load(name, world)){
		return false;
	}
	if(!world.dedicatedserver.active){
		ambienceMixer.CreateAmbienceChannels();
		renderer.palette.SetParallaxColors(world.map.parallax);
	}
	return true;
}

void Game::UnloadGame(void){
	Audio::GetInstance().StopAll(GASLoader::Get().gameengine.audioStopAllFadeMs);
	currentlobbygameid = 0;
	for(int i = 0; i < sizeof(ambienceMixer.bgchannel) / sizeof(int); i++){
		ambienceMixer.bgchannel[i] = -1;
	}
	world.SwitchToLocalAuthorityMode();
	if(world.map.loaded){ // if the map was loaded, then the music was played
		Audio::GetInstance().StopMusic();
	}
	world.map.Unload();
	world.pancameraactive = false;
	world.pancamerareturn = false;
	world.pancamerareturncount = 0;
	world.message_i = 0;
	world.winningteamid = 0;
	world.DestroyAllObjects();
	world.chatlines.clear();
	world.messagetype = 0;
	world.highlightminimap = false;
	world.highlightsecrets = false;
	world.quitstate = 0;
	world.ingameusers.clear();
}

bool Game::CheckForQuit(void){
	if(keystate[SDL_SCANCODE_RETURN]){
		if(world.quitstate == 1 || world.quitstate == 2){
			world.quitstate = 0;
			return true;
		}
	}
	return false;
}

bool Game::CheckForEndOfGame(void){
	if(world.winningteamid){
		if(world.message_i == GASLoader::Get().gameengine.ticksPerSecond * 3){
			if(world.IsAuthority()){
				if(world.replay.IsRecording()){
					world.replay.EndRecording();
				}
				for(std::vector<Uint32>::iterator it = world.ingameusers.begin(); it != world.ingameusers.end(); it++){
					Uint32 accountid = *it;
					User * user = world.lobby.GetUserInfo(accountid);
					if(user){
						Uint8 won = 0;
						for(int i = 0; i < world.maxpeers; i++){
							Peer * peer = world.peerlist[i];
							if(peer){
								if(peer->accountid == user->accountid){
									Team * team = world.GetPeerTeam(peer->id);
									user->statscopy = peer->stats;
									user->statsagency = team->agency;
									user->teamnumber = team->number;
									world.SendStats(*peer);
									if(team && team->id == world.winningteamid){
										won = 1;
									}
								}
							}
						}
						world.lobby.RegisterStats(*user, won, world.gameinfo.id);
					}
				}
			}
		}
		if(world.message_i >= 240){
			return true;
		}
	}
	return false;
}

bool Game::CheckForConnectionLost(void){
	if(world.replay.IsPlaying()){
		return false;
	}
	if(world.state == World::IDLE && world.message_i >= 48){
		return true;
	}
	return false;
}

void Game::ProcessInGameInterfaces(void){
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(localplayer){
		if(localplayer->buyinterfaceid || localplayer->techinterfaceid){
			currentinterface = localplayer->buyinterfaceid;
			if(!currentinterface){
				currentinterface = localplayer->techinterfaceid;
			}
		}else{
			oldselecteditem = 0;
		}
		if(!localplayer->buyinterfaceid && !localplayer->techinterfaceid){
			currentinterface = 0;
		}
		Interface * iface = (Interface *)world.GetObjectFromId(currentinterface);
		if(iface){
			if(iface->id == localplayer->buyinterfaceid || iface->id == localplayer->techinterfaceid){
				bool buying = false;
				if(iface->id == localplayer->buyinterfaceid){
					buying = true;
				}
				SelectBox * selectbox = (SelectBox *)iface->GetObjectWithUid(world, 1);
				if(selectbox){
					if(selectbox->selecteditem != oldselecteditem){
						Audio::GetInstance().Play(world.resources.soundbank[GASLoader::Get().player.soundRoundCountdown], 64);
						oldselecteditem = selectbox->selecteditem;
						if(buying){
							localplayer->buyifacelastitem = selectbox->selecteditem;
						}
					}
					if(selectbox->selecteditem >= selectbox->scrolled + 5){
						selectbox->scrolled = selectbox->selecteditem - 4;
					}
					if(selectbox->selecteditem < selectbox->scrolled){
						selectbox->scrolled = selectbox->selecteditem;
					}
					if(buying){
						localplayer->buyifacelastscrolled = selectbox->scrolled;
					}
					if(selectbox->enterpressed){
						BuyableItem * buyableitem = 0;
						for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
							if((*it)->id == selectbox->IndexToId(selectbox->selecteditem)){
								buyableitem = (*it);
								break;
							}
						}
						if(buyableitem){
							if(buying){
								localplayer->BuyItem(world, buyableitem->id);
							}else{
								if(localplayer->InOwnBase(world)){
									localplayer->RepairItem(world, buyableitem->id);
								}else{
									localplayer->VirusItem(world, buyableitem->id);
								}
							}
						}
						selectbox->enterpressed = false;
					}
				}
			}
		}
	}
}

void Game::ShowDeployMessage(void){
	world.ShowMessage((char *)"STANDBY FOR TEAM DEPLOYMENT", 64, 1);
	deploymessageshown = false;
}

void Game::GiveDefaultItems(Player & player){
	Team * team = player.GetTeam(world);
	if(team){
		for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
			BuyableItem * buyableitem = *it;
			switch(buyableitem->id){
				case World::BUY_LASER:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("laser");
						player.laserammo = def ? def->spawnAmmo : 5;
					}
				}break;
				case World::BUY_ROCKET:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("rocket");
						player.rocketammo = def ? def->spawnAmmo : 3;
					}
				}break;
				case World::BUY_FLAMER:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("flamer");
						player.flamerammo = def ? def->spawnAmmo : 15;
					}
				}break;
				case World::BUY_HEALTH:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("healthpack");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_HEALTHPACK);
					}
				}break;
				case World::BUY_VIRUS:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("virus");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_VIRUS);
					}
				}break;
				case World::BUY_POISON:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("poison");
						int count = def ? def->spawnInventoryCount : 2;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_POISON);
					}
				}break;
				case World::BUY_TRACT:{
					if(team->GetAvailableTech(world) & buyableitem->techchoice){
						const ItemDef* def = GASLoader::Get().GetItemDef("lazarustract");
						int count = def ? def->spawnInventoryCount : 1;
						for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_LAZARUSTRACT);
					}
				}break;
			}
		}
	}
}

void Game::JoinGame(LobbyGame & lobbygame, char * password){
	strcpy(world.mapname, lobbygame.mapname);
	Peer * peer = world.GetAuthorityPeer();
	peer->ip = ntohl(inet_addr(lobbygame.hostname));
	//peer->ip = ntohl(inet_addr("127.0.0.1")); // temporary
	peer->port = lobbygame.port;
	sharedstate = 0;
	world.mode = World::REPLICA;
	world.Connect(Config::GetInstance().defaultagency, world.lobby.accountid, password);
	joininggame = true;
}

void Game::ShowTeamOverlays(bool show){
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		if(object->type == ObjectTypes::TEAM){
			Team * team = static_cast<Team *>(object);
			team->ShowOverlays(world, show);
		}
	}
}

void Game::LeaveJoinedGame(){
	world.Disconnect();
	world.lobby.gamesprocessed = false;
	world.lobby.channelchanged = true;
	world.SwitchToLocalAuthorityMode();
	sharedstate = 0;
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		if(object->type == ObjectTypes::TEAM){
			Team * team = static_cast<Team *>(object);
			team->DestroyOverlays(world);
			world.MarkDestroyObject(object->id);
		}
	}
	world.lobby.JoinChannel(world.lobby.lastchannel);
}
