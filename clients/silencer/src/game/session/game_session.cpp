#include "session/game_session.h"

#include "game.h"
#include "audio.h"
#include "buyableitem.h"
#include "config.h"
#include "gasloader.h"
#include "gamemode.h"
#include "lobbygame.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "user.h"
#include "world.h"
#include <cstring>
#include <vector>

GameSession::GameSession(Game & g)
: game(g), mapDownloader(g.world), ambienceMixer(g.world, g.renderer, mapDownloader, g.gameRenderer.FadePhaseRef()),
  currentlobbygameid(0), lastannouncedgameid(0), lastannouncedstatus(0),
  deploymessageshown(false), joininggame(false) {
}

bool GameSession::LoadMap(const char * name){
if(!game.world.map.Load(name, game.world)){
return false;
}
if(!game.world.dedicatedserver.active){
ambienceMixer.CreateAmbienceChannels();
game.renderer.palette.SetParallaxColors(game.world.map.parallax);
}
return true;
}

void GameSession::UnloadGame(){
Audio::GetInstance().StopAll(GASLoader::Get().gameengine.audioStopAllFadeMs);
currentlobbygameid = 0;
for(int i = 0; i < sizeof(ambienceMixer.bgchannel) / sizeof(int); i++){
ambienceMixer.bgchannel[i] = -1;
}
game.world.SwitchToLocalAuthorityMode();
if(game.world.map.loaded){
Audio::GetInstance().StopMusic();
}
game.world.map.Unload();
game.world.pancameraactive = false;
game.world.pancamerareturn = false;
game.world.pancamerareturncount = 0;
game.world.messaging.message_i = 0;
game.world.winningteamid = 0;
game.world.matchEndCalled = false;
game.world.DestroyAllObjects();
delete game.world.gameMode;
game.world.gameMode = GameModeFactory(GAMEMODE_DATA_RETRIEVAL);
game.world.messaging.chatlines.clear();
game.world.messaging.messagetype = 0;
game.world.highlightminimap = false;
game.world.highlightsecrets = false;
game.world.quitstate = 0;
game.world.ingameusers.clear();
}

void GameSession::JoinGame(LobbyGame & lobbygame, char * password){
strcpy(game.world.mapname, lobbygame.mapname);
Peer * peer = game.world.GetAuthorityPeer();
peer->ip = ntohl(inet_addr(lobbygame.hostname));
peer->port = lobbygame.port;
game.sharedstate = 0;
game.world.mode = World::REPLICA;
game.world.Connect(Config::GetInstance().defaultagency, game.world.lobby.accountid, password);
joininggame = true;
}

void GameSession::SpectateGame(LobbyGame & lobbygame, char * password){
strcpy(game.world.mapname, lobbygame.mapname);
Peer * peer = game.world.GetAuthorityPeer();
peer->ip = ntohl(inet_addr(lobbygame.hostname));
peer->port = lobbygame.port;
game.sharedstate = 0;
game.world.mode = World::REPLICA;
game.world.Connect(Config::GetInstance().defaultagency, game.world.lobby.accountid, password, true);
joininggame = true;
}

void GameSession::LeaveJoinedGame(){
game.world.Disconnect();
game.world.lobby.gamesprocessed = false;
game.world.lobby.channelchanged = true;
game.world.SwitchToLocalAuthorityMode();
game.sharedstate = 0;
for(std::list<Object *>::iterator it = game.world.objects.objectlist.begin(); it != game.world.objects.objectlist.end(); it++){
Object * object = *it;
if(object->type == ObjectTypes::TEAM){
game.world.MarkDestroyObject(object->id);
}
}
game.world.lobby.JoinChannel(game.world.lobby.lastchannel);
}

void GameSession::ShowDeployMessage(){
game.world.ShowMessage((char *)"STANDBY FOR TEAM DEPLOYMENT", 64, 1);
deploymessageshown = false;
}

void GameSession::GiveDefaultItems(Player & player){
Team * team = player.GetTeam(game.world);
if(team){
for(std::vector<BuyableItem *>::iterator it = game.world.buyableitems.begin(); it != game.world.buyableitems.end(); it++){
BuyableItem * buyableitem = *it;
switch(buyableitem->id){
case World::BUY_LASER:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("laser");
player.laserammo = def ? def->spawnAmmo : 5;
}
}break;
case World::BUY_ROCKET:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("rocket");
player.rocketammo = def ? def->spawnAmmo : 3;
}
}break;
case World::BUY_FLAMER:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("flamer");
player.flamerammo = def ? def->spawnAmmo : 15;
}
}break;
case World::BUY_HEALTH:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("healthpack");
int count = def ? def->spawnInventoryCount : 1;
for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_HEALTHPACK);
}
}break;
case World::BUY_VIRUS:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("virus");
int count = def ? def->spawnInventoryCount : 1;
for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_VIRUS);
}
}break;
case World::BUY_POISON:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("poison");
int count = def ? def->spawnInventoryCount : 2;
for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_POISON);
}
}break;
case World::BUY_TRACT:{
if(team->GetAvailableTech(game.world) & buyableitem->techchoice){
const ItemDef* def = GASLoader::Get().GetItemDef("lazarustract");
int count = def ? def->spawnInventoryCount : 1;
for(int i = 0; i < count; i++) player.AddInventoryItem(Player::INV_LAZARUSTRACT);
}
}break;
}
}
}
}

bool GameSession::CheckForQuit(){
if(game.gameInput.GetKeystate()[SDL_SCANCODE_RETURN]){
if(game.world.quitstate == 1 || game.world.quitstate == 2){
game.world.quitstate = 0;
return true;
}
}
return false;
}

bool GameSession::CheckForEndOfGame(){
if(game.world.winningteamid){
if(game.world.messaging.message_i == GASLoader::Get().gameengine.ticksPerSecond * 3){
if(game.world.IsAuthority()){
	if(game.world.replay.IsRecording()){
		game.world.replay.EndRecording();
	}
	for(std::vector<Uint32>::iterator it = game.world.ingameusers.begin(); it != game.world.ingameusers.end(); it++){
		Uint32 accountid = *it;
		User * user = game.world.lobby.GetUserInfo(accountid);
		if(user){
			Uint8 won = 0;
			for(int i = 0; i < game.world.maxpeers; i++){
				Peer * peer = game.world.peers.peerlist[i];
				if(peer){
					if(peer->accountid == user->accountid){
						Team * team = game.world.GetPeerTeam(peer->id);
						user->statscopy = peer->stats;
						user->statsagency = team->agency;
						user->teamnumber = team->number;
						game.world.SendStats(*peer);
						if(team && team->id == game.world.winningteamid){
							won = 1;
						}
					}
				}
			}
			game.world.lobby.RegisterStats(*user, won, game.world.gameinfo.id);
		}
	}
}
}
if(game.world.messaging.message_i >= 240){
return true;
}
}
return false;
}

bool GameSession::CheckForConnectionLost(){
if(game.world.replay.IsPlaying()){
return false;
}
if(game.world.network.state == World::IDLE && game.world.messaging.message_i >= 48){
return true;
}
return false;
}
