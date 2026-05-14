#include "game.h"
#include "audio.h"
#include "basedoor.h"
#include "buyableitem.h"
#include "config.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "runtime/UiAutomationRegistry.h"
#include "world.h"
#include <cstring>
#include <vector>

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

void Game::UpdateInGameOverlayState(void){
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(localplayer){
		if(localplayer->isbuying || localplayer->techstationactive){
			std::vector<BuyableItem *> items;
			localplayer->CollectBuyMenuItems(world, localplayer->techstationactive, items);
			int & selected = localplayer->isbuying ? localplayer->buyifacelastitem : localplayer->techifacelastitem;
			int & scrolled = localplayer->isbuying ? localplayer->buyifacelastscrolled : localplayer->techifacelastscrolled;
			if(items.empty()){
				selected = 0;
				scrolled = 0;
			}else{
				if(selected < 0) selected = 0;
				if(selected >= (int)items.size()) selected = (int)items.size() - 1;
				if(selected >= scrolled + 5) scrolled = selected - 4;
				if(selected < scrolled) scrolled = selected;
				if(scrolled < 0) scrolled = 0;
			}
		}
	}
}

namespace {

bool StartsWith(const std::string& value, const char * prefix) {
	return value.compare(0, std::strlen(prefix), prefix) == 0;
}

void ClampBuyTechSelection(Player& player, World& world) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	int & scrolled = player.isbuying ? player.buyifacelastscrolled : player.techifacelastscrolled;
	if(items.empty()){
		selected = 0;
		scrolled = 0;
		return;
	}
	if(selected < 0) selected = 0;
	if(selected >= static_cast<int>(items.size())) selected = static_cast<int>(items.size()) - 1;
	if(selected >= scrolled + 5) scrolled = selected - 4;
	if(selected < scrolled) scrolled = selected;
	if(scrolled < 0) scrolled = 0;
}

void SelectBuyTechRow(Player& player, World& world, int index) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	if(items.empty()) return;
	if(index < 0) index = 0;
	if(index >= static_cast<int>(items.size())) index = static_cast<int>(items.size()) - 1;
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	if(selected != index){
		Audio::GetInstance().Play(
			world.resources.soundbank[GASLoader::Get().player.soundRoundCountdown],
			64);
	}
	selected = index;
	ClampBuyTechSelection(player, world);
}

void ActivateBuyTechSelection(Player& player, World& world) {
	std::vector<BuyableItem *> items;
	bool tech = player.techstationactive;
	player.CollectBuyMenuItems(world, tech, items);
	if(items.empty()) return;
	int & selected = player.isbuying ? player.buyifacelastitem : player.techifacelastitem;
	if(selected < 0) selected = 0;
	if(selected >= static_cast<int>(items.size())) selected = static_cast<int>(items.size()) - 1;
	BuyableItem * buyableitem = items[selected];
	if(player.isbuying){
		player.BuyItem(world, buyableitem->id);
	}else if(player.InOwnBase(world)){
		player.RepairItem(world, buyableitem->id);
	}else{
		player.VirusItem(world, buyableitem->id);
	}
}

}  // namespace

bool Game::DispatchInGameUiActions(const std::vector<silencer::ui::UiAction>& actions) {
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(!localplayer) return false;

	bool handled = false;
	for(const silencer::ui::UiAction& action : actions){
		if(localplayer->chatActive &&
		   (action.id == "ingame.chat" || action.id == "ingame.chat.channel")){
			handled = true;
			if(action.kind == silencer::ui::UiActionKind::SetText &&
			   action.id == "ingame.chat"){
				int n = static_cast<int>(action.value.size());
				if(n > static_cast<int>(sizeof(localplayer->chatText)) - 1){
					n = static_cast<int>(sizeof(localplayer->chatText)) - 1;
				}
				std::memcpy(localplayer->chatText, action.value.data(), n);
				localplayer->chatText[n] = '\0';
			}else if(action.kind == silencer::ui::UiActionKind::SubmitText){
				if(std::strlen(localplayer->chatText) > 0){
					world.SendChat(localplayer->chatwithteam, localplayer->chatText);
				}
				localplayer->chatText[0] = '\0';
				localplayer->chatActive = false;
			}else if(action.kind == silencer::ui::UiActionKind::Cancel){
				localplayer->chatText[0] = '\0';
				localplayer->chatActive = false;
			}else if(action.kind == silencer::ui::UiActionKind::Navigate ||
			         action.kind == silencer::ui::UiActionKind::Activate){
				if(action.id == "ingame.chat.channel"){
					localplayer->chatwithteam = !localplayer->chatwithteam;
					silencer::ui::automation::FocusWidgetById("ingame.chat");
				}
			}
			continue;
		}

		if((localplayer->isbuying || localplayer->techstationactive) &&
		   StartsWith(action.id, "ingame.buytech.row.")){
			handled = true;
			if(action.index >= 0){
				SelectBuyTechRow(*localplayer, world, action.index);
			}
			if(action.kind == silencer::ui::UiActionKind::Select &&
			   action.value != "focus_next" && action.value != "focus_previous"){
				ActivateBuyTechSelection(*localplayer, world);
			}
			continue;
		}

		if((localplayer->isbuying || localplayer->techstationactive) &&
		   action.kind == silencer::ui::UiActionKind::Cancel){
			localplayer->isbuying = false;
			localplayer->techstationactive = false;
			handled = true;
		}
	}
	return handled;
}

nlohmann::json Game::ConfigureInGameUiForControl(const std::string& mode) {
	nlohmann::json r;
	r["mode"] = mode;
	r["ok"] = false;

	Player * player = world.GetPeerPlayer(world.viewedpeerid);
	if(!player){
		r["error"] = "no viewed player";
		return r;
	}

	auto clear = [&]() {
		player->chatActive = false;
		player->chatText[0] = '\0';
		player->isbuying = false;
		player->techstationactive = false;
		world.showchat_i = 0;
		world.showplayerlist = false;
	};

	if(mode == "clear"){
		clear();
		r["ok"] = true;
		return r;
	}

	if(mode != "status") clear();
	if(mode == "chat" || mode == "all"){
		player->chatActive = true;
		player->chatwithteam = false;
		std::strncpy(player->chatText, "clay chat smoke", sizeof(player->chatText) - 1);
		player->chatText[sizeof(player->chatText) - 1] = '\0';
		world.showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
	}
	if(mode == "buy" || mode == "all"){
		player->isbuying = true;
		player->buyifacelastitem = 0;
		player->buyifacelastscrolled = 0;
	}
	if(mode == "tech" || mode == "all"){
		Team * team = player->GetTeam(world);
		if(!team && !world.objectsbytype[ObjectTypes::TEAM].empty()){
			team = static_cast<Team *>(world.GetObjectFromId(world.objectsbytype[ObjectTypes::TEAM][0]));
		}
		if(!team){
			team = static_cast<Team *>(world.CreateObject(ObjectTypes::TEAM));
			if(team){
				team->agency = Team::NOXIS;
				team->number = 0;
				team->color = ((8 << 4) + 13);
			}
		}
		if(team){
			player->teamid = team->id;
			team->AddPeer(world.localpeerid);
		}
		BaseDoor * door = nullptr;
		if(team){
			team->disabledtech = 0xffffffff;
			door = static_cast<BaseDoor *>(world.GetObjectFromId(team->basedoorid));
			for(Uint16 objectid : world.objectsbytype[ObjectTypes::BASEDOOR]){
				auto * candidate = static_cast<BaseDoor *>(world.GetObjectFromId(objectid));
				if(candidate && candidate->teamid == team->id){
					door = candidate;
					break;
				}
			}
		}
		if(door){
			player->basedoorentering = door->id;
		}
		if(team){
			int baseY = ((world.map.height + 10) * 64) + (team->number * 26 * 64) + 64;
			player->y = static_cast<Sint16>(baseY);
		}
		player->techstationactive = true;
		player->techifacelastitem = 0;
		player->techifacelastscrolled = 0;
	}
	if(mode == "playerlist" || mode == "all"){
		world.showplayerlist = true;
	}

	std::vector<BuyableItem *> buyItems;
	std::vector<BuyableItem *> techItems;
	player->CollectBuyMenuItems(world, false, buyItems);
	player->CollectBuyMenuItems(world, true, techItems);
	r["ok"] = true;
	r["chat_active"] = player->chatActive;
	r["buy_active"] = player->isbuying;
	r["tech_active"] = player->techstationactive;
	r["show_chat_ticks"] = world.showchat_i;
	r["show_player_list"] = world.showplayerlist;
	r["buy_item_count"] = static_cast<int>(buyItems.size());
	r["tech_item_count"] = static_cast<int>(techItems.size());
	r["buy_selected_index"] = player->buyifacelastitem;
	r["tech_selected_index"] = player->techifacelastitem;
	return r;
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

void Game::SpectateGame(LobbyGame & lobbygame, char * password){
	strcpy(world.mapname, lobbygame.mapname);
	Peer * peer = world.GetAuthorityPeer();
	peer->ip = ntohl(inet_addr(lobbygame.hostname));
	peer->port = lobbygame.port;
	sharedstate = 0;
	world.mode = World::REPLICA;
	world.Connect(Config::GetInstance().defaultagency, world.lobby.accountid, password, true);
	joininggame = true;
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
			world.MarkDestroyObject(object->id);
		}
	}
	world.lobby.JoinChannel(world.lobby.lastchannel);
}
