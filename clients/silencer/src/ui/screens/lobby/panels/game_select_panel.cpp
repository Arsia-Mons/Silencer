#include "game_select_panel.h"

#include "screen_context.h"
#include "lobby_screen.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "config.h"
#include "user.h"
#include "objecttypes.h"
#include "interface.h"
#include "overlay.h"
#include "button.h"
#include "selectbox.h"
#include "scrollbar.h"
#include "message_modal.h"
#include "password_modal.h"

#include <memory>

#include <cmath>
#include <cstring>
#include <deque>
#include <list>
#include <string>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum GameSelectOverlay : Uint8 {
	GSEL_OVL_NAME    = 1,
	GSEL_OVL_MAP     = 2,
	GSEL_OVL_INFO    = 3,
	GSEL_OVL_CREATOR = 4,
	GSEL_OVL_LIMITS  = 5,
};
enum GameSelectButton : Uint8 {
	GSEL_BTN_JOIN     = 20,
	GSEL_BTN_SPECTATE = 21,
	GSEL_BTN_CREATE   = 30,
};
enum GameSelectSelect : Uint8 {
	GSEL_SEL_GAMES = 10,
};
}

GameSelectPanel::GameSelectPanel(LobbyScreen & owner_) : owner(owner_) {}

void GameSelectPanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	Interface * gameselectinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gameselectinterface->x = 403;
	gameselectinterface->y = 87;
	gameselectinterface->width = 222;
	gameselectinterface->height = 267;
	Overlay * rightborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	rightborder->res_bank = 7;
	rightborder->res_index = 8;
	Overlay * gamestext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamestext->text = "Active Games";
	gamestext->textbank = 134;
	gamestext->textwidth = 8;
	gamestext->x = 405;
	gamestext->y = 70;
	SelectBox * gameselect = (SelectBox *)world.CreateObject(ObjectTypes::SELECTBOX);
	gameselect->x = 407;
	gameselect->y = 89;
	gameselect->width = 214;
	gameselect->height = 265;
	gameselect->lineheight = 14;
	gameselect->uid = GSEL_SEL_GAMES;
	ScrollBar * gamescrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	gamescrollbar->res_index = 9;
	gamescrollbar->scrollpixels = gameselect->lineheight;
	gamescrollbar->scrollposition = gameselect->scrolled;
	Overlay * gamenametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamenametext->textbank = 133;
	gamenametext->textwidth = 6;
	gamenametext->x = 405;
	gamenametext->y = 358;
	gamenametext->uid = GSEL_OVL_NAME;
	Overlay * gamemaptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamemaptext->textbank = 133;
	gamemaptext->textwidth = 6;
	gamemaptext->x = 405;
	gamemaptext->y = 370;
	gamemaptext->uid = GSEL_OVL_MAP;
	Overlay * gameplayerstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gameplayerstext->textbank = 133;
	gameplayerstext->textwidth = 6;
	gameplayerstext->x = 405;
	gameplayerstext->y = 382;
	gameplayerstext->uid = GSEL_OVL_INFO;
	Overlay * gamecreatortext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gamecreatortext->textbank = 133;
	gamecreatortext->textwidth = 6;
	gamecreatortext->x = 405;
	gamecreatortext->y = 394;
	gamecreatortext->uid = GSEL_OVL_CREATOR;
	Overlay * gameinfotext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	gameinfotext->textbank = 133;
	gameinfotext->textwidth = 6;
	gameinfotext->x = 405;
	gameinfotext->y = 406;
	gameinfotext->uid = GSEL_OVL_LIMITS;

	Button * gamejoinbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamejoinbutton->y = 430;
	gamejoinbutton->x = 436;
	gamejoinbutton->SetType(Button::B156x21);
	gamejoinbutton->uid = GSEL_BTN_JOIN;
	strcpy(gamejoinbutton->text, "Join Game");
	Button * gamespectatebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamespectatebutton->y = 408;
	gamespectatebutton->x = 436;
	gamespectatebutton->SetType(Button::B156x21);
	gamespectatebutton->uid = GSEL_BTN_SPECTATE;
	strcpy(gamespectatebutton->text, "Spectate");
	gamespectatebutton->draw = false;
	Button * gamecreatebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamecreatebutton->y = 68;
	gamecreatebutton->x = 242;
	gamecreatebutton->SetType(Button::B156x21);
	gamecreatebutton->uid = GSEL_BTN_CREATE;
	strcpy(gamecreatebutton->text, "Create Game");
	gameselectinterface->AddObject(rightborder->id);
	gameselectinterface->AddObject(gamestext->id);
	gameselectinterface->AddObject(gamejoinbutton->id);
	gameselectinterface->AddObject(gamespectatebutton->id);
	gameselectinterface->AddObject(gamecreatebutton->id);
	gameselectinterface->AddObject(gameselect->id);
	gameselectinterface->AddObject(gamescrollbar->id);
	gameselectinterface->AddObject(gamenametext->id);
	gameselectinterface->AddObject(gamemaptext->id);
	gameselectinterface->AddObject(gameplayerstext->id);
	gameselectinterface->AddObject(gamecreatortext->id);
	gameselectinterface->AddObject(gameinfotext->id);
	gameselectinterface->buttonenter = gamejoinbutton->id;
	gameselectinterface->scrollbar = gamescrollbar->id;

	interfaceId = gameselectinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
}

void GameSelectPanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

	// Sync selectbox scroll + game-info overlays.
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object) continue;
		if(object->type == ObjectTypes::SELECTBOX){
			SelectBox * selectbox = static_cast<SelectBox *>(object);
			if(selectbox->uid != GSEL_SEL_GAMES) continue;
			Object * sbobj = world.GetObjectFromId(iface->scrollbar);
			ScrollBar * scrollbar = static_cast<ScrollBar *>(sbobj);
			if(scrollbar){
				selectbox->scrolled = scrollbar->scrollposition;
				if(selectbox->items.size() > std::ceil(float(selectbox->height) / selectbox->lineheight)){
					scrollbar->draw = true;
					scrollbar->scrollmax = selectbox->items.size() - std::ceil(float(selectbox->height) / selectbox->lineheight);
				}else{
					scrollbar->draw = false;
				}
			}

			if(!world.lobby.gamesprocessed){
				bool deleted;
				do{
					deleted = false;
					unsigned int index = 0;
					for(std::deque<Uint32>::iterator it2 = selectbox->itemids.begin(); it2 != selectbox->itemids.end(); it2++, index++){
						Uint32 gameid = (*it2);
						if(!world.lobby.GetGameById(gameid)){
							selectbox->DeleteItem(index);
							deleted = true;
							break;
						}
					}
				}while(deleted);
				for(std::list<LobbyGame *>::iterator it2 = world.lobby.games.begin(); it2 != world.lobby.games.end(); it2++){
					LobbyGame * lobbygame = (*it2);
					if(selectbox->IdToIndex(lobbygame->id) == -1){
						selectbox->AddItem(lobbygame->name, lobbygame->id);
					}
				}
				world.lobby.gamesprocessed = true;
			}

			LobbyGame * lobbygame = world.lobby.GetGameById(selectbox->IndexToId(selectbox->selecteditem));
			Object * tobject = iface->GetObjectWithUid(world, GSEL_OVL_NAME);
			if(tobject && tobject->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(tobject);
				overlay->text = lobbygame ? lobbygame->name : "";
			}
			tobject = iface->GetObjectWithUid(world, GSEL_OVL_MAP);
			if(tobject && tobject->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(tobject);
				if(lobbygame){
					overlay->text = "Map: ";
					overlay->text += lobbygame->mapname;
				}else{
					overlay->text = "";
				}
			}
			tobject = iface->GetObjectWithUid(world, GSEL_OVL_INFO);
			if(tobject && tobject->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(tobject);
				if(lobbygame){
					const char * passwordlock = (strlen(lobbygame->password) > 0) ? "*PASSWORD LOCK*" : "";
					std::string security = "No";
					switch(lobbygame->securitylevel){
						case LobbyGame::SECLOW:    security = "Low"; break;
						case LobbyGame::SECMEDIUM: security = "Medium"; break;
						case LobbyGame::SECHIGH:   security = "High"; break;
					}
					overlay->text = security + " Security";
					while(overlay->text.length() < 21){
						overlay->text += " ";
					}
					overlay->text += passwordlock;
				}else{
					overlay->text = "";
				}
			}
			tobject = iface->GetObjectWithUid(world, GSEL_OVL_CREATOR);
			if(tobject && tobject->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(tobject);
				if(lobbygame){
					overlay->text = "Creator: ";
					overlay->text += world.lobby.GetUserInfo(lobbygame->accountid)->name;
				}else{
					overlay->text = "";
				}
			}
			// LobbyGame::state — 0 = pre-match lobby, 1 = INGAME (heartbeat-driven).
			bool ingame = lobbygame && lobbygame->state == 1;
			tobject = iface->GetObjectWithUid(world, GSEL_OVL_LIMITS);
			if(tobject && tobject->type == ObjectTypes::OVERLAY){
				Overlay * overlay = static_cast<Overlay *>(tobject);
				if(lobbygame && !ingame){
					overlay->text = "MinLv:" + std::to_string(lobbygame->minlevel)
					              + " MaxLv:" + std::to_string(lobbygame->maxlevel)
					              + " MaxPl:" + std::to_string(lobbygame->maxplayers)
					              + " MaxTm:" + std::to_string(lobbygame->maxteams);
				}else{
					// Hidden for INGAME rows — Spectate button at y=408 shares that row.
					overlay->text = "";
				}
			}
			// Dual-button visibility: Join when the user is permitted (pre-match
			// row with capacity, or INGAME row with a parked-peer slot for our
			// accountid); Spectate when the game is INGAME and spectatable.
			bool joinvisible = false;
			bool spectatevisible = false;
			if(lobbygame){
				if(!ingame && lobbygame->players < lobbygame->maxplayers){
					joinvisible = true;
				}else if(ingame && lobbygame->canrejoin){
					joinvisible = true;
				}
				if(ingame && lobbygame->spectatable){
					spectatevisible = true;
				}
			}
			Object * joinobj = iface->GetObjectWithUid(world, GSEL_BTN_JOIN);
			if(joinobj && joinobj->type == ObjectTypes::BUTTON){
				static_cast<Button *>(joinobj)->draw = joinvisible;
			}
			Object * spectateobj = iface->GetObjectWithUid(world, GSEL_BTN_SPECTATE);
			if(spectateobj && spectateobj->type == ObjectTypes::BUTTON){
				static_cast<Button *>(spectateobj)->draw = spectatevisible;
			}
			// Enter activates Join when visible; otherwise Spectate when only it
			// is visible. Fall back to Join (no-op) when neither is visible.
			if(joinvisible){
				iface->buttonenter = joinobj ? joinobj->id : 0;
			}else if(spectatevisible){
				iface->buttonenter = spectateobj ? spectateobj->id : 0;
			}else{
				iface->buttonenter = joinobj ? joinobj->id : 0;
			}
		}else if(object->type == ObjectTypes::BUTTON){
			Button * button = static_cast<Button *>(object);
			if(!button->clicked || button->type == Button::BCHECKBOX) continue;
			switch(button->uid){
				case GSEL_BTN_JOIN:{
					button->clicked = false;
					SelectBox * selectbox = nullptr;
					for(std::vector<Uint16>::iterator it2 = iface->objects.begin(); it2 != iface->objects.end(); it2++){
						Object * obj2 = world.GetObjectFromId(*it2);
						if(obj2 && obj2->type == ObjectTypes::SELECTBOX){
							selectbox = static_cast<SelectBox *>(obj2);
							break;
						}
					}
					if(!selectbox) break;
					if(selectbox->selecteditem == -1){
						ctx.ShowMessage("No game selected");
						break;
					}
					Uint32 gameid = selectbox->IndexToId(selectbox->selecteditem);
					if(!gameid) break;
					LobbyGame * lobbygame = world.lobby.GetGameById(gameid);
					if(!lobbygame) break;
					if(!world.IsIdle()) break;
					User * user = world.lobby.GetUserInfo(world.lobby.accountid);
					bool canjoin = true;
					if(user){
						if(lobbygame->minlevel > user->agency[Config::GetInstance().defaultagency].level){
							canjoin = false;
							ctx.ShowMessage("Your player level is too low");
						}else if(lobbygame->maxlevel < user->agency[Config::GetInstance().defaultagency].level){
							canjoin = false;
							ctx.ShowMessage("Your player level is too high");
						}
					}
					if(!canjoin) break;
					ctx.game.currentlobbygameid = lobbygame->id;
					if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
						Uint32 gameId = lobbygame->id;
						ctx.PushScreen(std::make_unique<PasswordModal>(
							[&ctx, gameId](const char * password){
								LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
								if(lg){
									char buf[64];
									std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
									buf[sizeof(buf) - 1] = '\0';
									ctx.game.JoinGame(*lg, buf);
								}
							}));
					}else{
						ctx.game.JoinGame(*lobbygame);
					}
				}break;
				case GSEL_BTN_SPECTATE:{
					button->clicked = false;
					SelectBox * selectbox = nullptr;
					for(std::vector<Uint16>::iterator it2 = iface->objects.begin(); it2 != iface->objects.end(); it2++){
						Object * obj2 = world.GetObjectFromId(*it2);
						if(obj2 && obj2->type == ObjectTypes::SELECTBOX){
							selectbox = static_cast<SelectBox *>(obj2);
							break;
						}
					}
					if(!selectbox) break;
					if(selectbox->selecteditem == -1){
						ctx.ShowMessage("No game selected");
						break;
					}
					Uint32 gameid = selectbox->IndexToId(selectbox->selecteditem);
					if(!gameid) break;
					LobbyGame * lobbygame = world.lobby.GetGameById(gameid);
					if(!lobbygame) break;
					if(!world.IsIdle()) break;
					ctx.game.currentlobbygameid = lobbygame->id;
					if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
						Uint32 gameId = lobbygame->id;
						ctx.PushScreen(std::make_unique<PasswordModal>(
							[&ctx, gameId](const char * password){
								LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
								if(lg){
									char buf[64];
									std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
									buf[sizeof(buf) - 1] = '\0';
									ctx.game.SpectateGame(*lg, buf);
								}
							}));
					}else{
						ctx.game.SpectateGame(*lobbygame);
					}
				}break;
				case GSEL_BTN_CREATE:{
					button->clicked = false;
					owner.ShowGameCreate(ctx);
					return;
				}
			}
		}
	}
}

void GameSelectPanel::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
