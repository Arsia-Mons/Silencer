#include "game.h"
#include "audio.h"
#include "gasloader.h"
#include "lobbygame.h"
#include "os.h"
#include "player.h"
#include "team.h"
#include "terminal.h"
#include "world.h"
#include <stdio.h>

using namespace GameState;

void Game::TickSinglePlayerGame(){
if(stateisnew){
	world.GetAuthorityPeer()->controlledlist.clear();
	world.DestroyAllObjects();
	world.gameplaystate = World::INGAME;
	world.intutorialmode = true;
	world.GetAuthorityPeer()->techchoices = World::BUY_LASER | World::BUY_ROCKET;
	//world.Listen(23456);
	Team * team = (Team *)world.CreateObject(ObjectTypes::TEAM);
	team->AddPeer(world.GetAuthorityPeer()->id);
	team->agency = Team::NOXIS;
	team->color = ((8 << 4) + 13);
	Player * player = (Player *)world.CreateObject(ObjectTypes::PLAYER);
	player->suitcolor = team->color;
	player->laserammo = 0;
	player->credits = GASLoader::Get().player.startingCredits;
	player->RemoveInventoryItem(Player::INV_BASEDOOR);
	gameSession.ShowDeployMessage();
	world.GetAuthorityPeer()->controlledlist.push_back(player->id);
	world.gameinfo.securitylevel = LobbyGame::SECNONE;
	if(!gameSession.LoadMap((GetResDir() + "AGENCY04.SIL").c_str())){
		GoToState(MAINMENU);
	}
	Audio::GetInstance().StopMusic();
	world.map.RandomPlayerStartLocation(world, player->x, player->y);
	player->oldx = player->x;
	player->oldy = player->y;
	renderer.palette.SetPalette(0);
	renderer.palette.SetParallaxColors(world.map.parallax);
	GetScreenBuffer().Clear(0);
	gameRenderer.SetColors(renderer.palette.GetColors());
	// SIL-236: init the lighted palette and invalidate the cached ambience level
	// so the game loop applies the ambience palette once the fade-in settles —
	// the slow map load otherwise skips that refresh on entry, leaving the world
	// too bright with a black sky and no weather (see tick_ingame.cpp).
	Uint8 ambiencelevel = renderer.GetAmbienceLevel();
	renderer.palette.CalculateLighted(ambiencelevel);
	gameSession.AmbienceMixerRef().oldambiencelevel = ambiencelevel ^ 0xFF;
	singleplayermessage = 0;
	stateisnew = false;
	gameSession.AmbienceMixerRef().LoadRandomGameMusic();
}
if(gameSession.AmbienceMixerRef().FadedIn()){
	//Audio::GetInstance().ambienceMixer.PlayMusic(world.resources.gamemusic);
	gameSession.AmbienceMixerRef().PlayMusic(world.resources.gamemusic);
}
Player * player = world.GetPeerPlayer(world.peers.localpeerid);
if(player){
	switch(singleplayermessage){
		case 0:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Move your agent left and right\nBy tapping %s and %s.", gameInput.GetActionKeyDisplayName(Action::MoveLeft), gameInput.GetActionKeyDisplayName(Action::MoveRight));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::RUNNING){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 1:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Make your agent jump by striking %s.", gameInput.GetActionKeyDisplayName(Action::Jump));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::JUMPING){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 2:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"If you hold the %s key down, it will\nactivate your agent's jet-pack.", gameInput.GetActionKeyDisplayName(Action::Jetpack));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::JETPACK){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 3:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Make your agent kneel by holding %s.", gameInput.GetActionKeyDisplayName(Action::MoveDown));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::CROUCHED){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 4:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Make your agent roll by kneeling,\nthen striking %s or %s.", gameInput.GetActionKeyDisplayName(Action::MoveLeft), gameInput.GetActionKeyDisplayName(Action::MoveRight));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::ROLLING){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 5:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"To disguise as a civilian, press the %s key.", gameInput.GetActionKeyDisplayName(Action::Disguise));
				world.ShowMessage(text, 128);
			}
			if(player->IsDisguised()){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 6:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"To return to normal, press the %s key again.", gameInput.GetActionKeyDisplayName(Action::Disguise));
				world.ShowMessage(text, 128);
			}
			if(!player->IsDisguised()){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 7:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"The %s key fires your current weapon,\nthe Blaster.", gameInput.GetActionKeyDisplayName(Action::Fire));
				world.ShowMessage(text, 128);
			}
			if(player->state == Player::STANDINGSHOOT || player->state == Player::CROUCHEDSHOOT || player->state == Player::FALLINGSHOOT || player->state == Player::JETPACKSHOOT || player->state == Player::LADDERSHOOT){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 8:{
			if(!world.messaging.message_i){
				char text[256];
#ifdef OUYA
				snprintf(text, sizeof text,"To change weapons, press %s", gameInput.GetActionKeyDisplayName(Action::NextWeapon));
#else
				snprintf(text, sizeof text,"To change weapons, press the 1, 2, 3, or 4 keys");
#endif
				world.ShowMessage(text, 128);
				
			}
			if(player->laserammo == 0){
				player->laserammo = 15;
			}
			if(player->currentweapon != 0){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 9:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Good job, agent.\n\nYou have been given a base-building device.");
				world.ShowMessage(text, 128);
				player->AddInventoryItem(Player::INV_BASEDOOR);
				singleplayermessage++;
			}
		}break;
		case 10:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Hit %s to build your base.", gameInput.GetActionKeyDisplayName(Action::Use));
				world.ShowMessage(text, 128);
			}
			Team * team = player->GetTeam(world);
			if(team && team->basedoorid){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 11:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"To enter your base, hit %s when your\nSilencer is at the base entrance.", gameInput.GetActionKeyDisplayName(Action::Activate));
				world.ShowMessage(text, 128);
			}
			if(player->InBase(world)){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 12:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"You are now inside your agent's secret base.\nWalk right to the flashing green computer screen\nand hit %s to activate it.", gameInput.GetActionKeyDisplayName(Action::Activate));
				world.ShowMessage(text, 255);
			}
			if(!player->InBase(world)){
				singleplayermessage = 11;
				world.messaging.message_i = 0;
			}
			if(player->isbuying){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 13:{
			if(!world.messaging.message_i){
				char text[256];
#ifdef OUYA
				const char * button = "O";
#else
				const char * button = "Enter";
#endif
				snprintf(text, sizeof text,"Use the Up and Down keys to select Rocket ammo\nand press %s to purchase.", button);
				world.ShowMessage(text, 255);
			}
			if(!player->InBase(world)){
				singleplayermessage = 11;
				world.messaging.message_i = 0;
			}
			if(player->credits < 250){
			player->credits = GASLoader::Get().player.creditFloor;
			}
			if(player->rocketammo > 0){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 14:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Good, now hit %s or %s to exit the menu.", gameInput.GetActionKeyDisplayName(Action::MoveLeft), gameInput.GetActionKeyDisplayName(Action::MoveRight));
				world.ShowMessage(text, 128);
			}
			if(player->rocketammo > 0){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 15:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"To leave your base, walk Left through\nthe door you entered.");
				world.ShowMessage(text, 128);
			}
			if(!player->InBase(world)){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 16:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"In the playfield, you need to hack into\ndata terminals to collect information.");
				world.ShowMessage(text, 192);
			}
			if(world.messaging.message_i >= 192 - 1){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 17:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Walk around until you see a\nflashing green data port.\nStanding in front of the data port, hit %s\nto initiate hacking.", gameInput.GetActionKeyDisplayName(Action::Activate));
				world.ShowMessage(text, 255);
			}
			if(player->state == Player::HACKING && player->files >= 100){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 18:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Return with the information to your base door,\nhitting %s to enter the base.", gameInput.GetActionKeyDisplayName(Action::Activate));
				world.ShowMessage(text, 128);
			}
			if(player->InBase(world)){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 19:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Walk to the Right, through the agency receiver\nto deliver the information to your agency.");
				world.ShowMessage(text, 128);
			}
			if(!player->InBase(world)){
				singleplayermessage = 18;
				world.messaging.message_i = 0;
			}
			if(!player->files){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 20:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Good job, agent.\nYou're ready for the final training exercise.");
				world.ShowMessage(text, 128);
			}
			if(world.messaging.message_i >= 128 - 1){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 21:{
			if(!world.messaging.message_i){
				world.highlightsecrets = true;
				char text[256];
				snprintf(text, sizeof text,"This indicator shows your progress towards\ndiscovering the location of a secret.\nKeep collecting files\nuntil all stages are lit.");
				world.ShowMessage(text, 255);
			}
			Team * team = player->GetTeam(world);
			if(team && team->beamingterminalid){
				world.highlightsecrets = false;
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 22:{
			if(!world.messaging.message_i){
				world.highlightminimap = true;
				char text[256];
				snprintf(text, sizeof text,"The narrowing circle on your radar shows\nyour agency acquiring a lock on the secret.\nWhen the lock is completed, the\nsecret can be picked up by your team.");
				world.ShowMessage(text, 255);
			}
			Team * team = player->GetTeam(world);
			bool advance22 = player->hassecret || (team && team->secrets > 0);
			if(!advance22 && team && team->beamingterminalid){
				Terminal * terminal = static_cast<Terminal *>(world.GetObjectFromId(team->beamingterminalid));
				if(terminal){
					if(terminal->beamingtime > 45){
						terminal->beamingtime = 45;
					}
					if(terminal->state == Terminal::SECRETREADY){
						advance22 = true;
					}
				}
			}
			if(advance22){
				world.highlightminimap = false;
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 23:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Pick up the secret at the location shown\non your radar map");
				world.ShowMessage(text, 128);
			}
			Team * team23 = player->GetTeam(world);
			if(player->hassecret || (team23 && team23->secrets > 0)){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 24:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Now, you must return the secret to your base.\nIf this were a real government secret,\nyou would have limited time before\nthe government traced your location.");
				world.ShowMessage(text, 255);
			}
			Team * team24 = player->GetTeam(world);
			if(player->InBase(world) || (team24 && team24->secrets > 0)){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 25:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"To stash the secret data, it must be brought\nto the memory bank at the\nfar right of your base.");
				world.ShowMessage(text, 128);
			}
			Team * team = player->GetTeam(world);
			if(team && team->secrets > 0){
				singleplayermessage++;
				world.messaging.message_i = 0;
			}
		}break;
		case 26:{
			if(!world.messaging.message_i){
				char text[256];
				snprintf(text, sizeof text,"Good job, agent.\n\nYou're ready to begin real agency missions.");
				world.ShowMessage(text, 255);
			}
			if(world.messaging.message_i == 12){
				player->state = Player::UNDEPLOYING;
				player->state_i = 0;
			}
			if(world.messaging.message_i >= 128 - 1){
				GoToState(MAINMENU);
			}
		}break;
	}
}
if(gameSession.CheckForEndOfGame()){
	GoToState(MAINMENU);
}
}
