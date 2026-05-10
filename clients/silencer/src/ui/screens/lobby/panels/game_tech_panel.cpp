#include "game_tech_panel.h"

#include "screen_context.h"
#include "lobby_screen.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "config.h"
#include "user.h"
#include "team.h"
#include "peer.h"
#include "buyableitem.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "overlay.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum GameTechButton : Uint8 {
	GTECH_BTN_BACK = 28,
};
enum GameTechOverlay : Uint8 {
	GTECH_OVL_TECHNAME    = 60,
	// uid 61..68 are the 8 description lines (61 + i).
	GTECH_OVL_SLOTS       = 70,
	// uid 80..82 are per-peer name overlays (80 + i).
	// uid 90..92 are per-peer separator-line overlays (90 + i).
	// uid 110..220 are per-peer-per-item checkboxes (110 + 30*x + i).
	// uid 230..N are local-row tech name overlays (230 + i, x==3).
};
}

GameTechPanel::GameTechPanel(LobbyScreen & owner_) : owner(owner_) {}

void GameTechPanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	Interface * gametechinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gametechinterface->x = 403;
	gametechinterface->y = 87;
	gametechinterface->width = 222;
	gametechinterface->height = 390;
	Button * teamsbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	teamsbutton->x = 242;
	teamsbutton->y = 68;
	teamsbutton->SetType(Button::B156x21);
	teamsbutton->uid = GTECH_BTN_BACK;
	strcpy(teamsbutton->text, "Back To Teams");
	Overlay * techslotsoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	if(techslotsoverlay){
		techslotsoverlay->x = 455;
		techslotsoverlay->y = 100;
		techslotsoverlay->textbank = 133;
		techslotsoverlay->textwidth = 6;
		techslotsoverlay->effectcolor = 129;
		techslotsoverlay->effectbrightness = 128 + 16;
		techslotsoverlay->textcolorramp = true;
		techslotsoverlay->uid = GTECH_OVL_SLOTS;
		gametechinterface->AddObject(techslotsoverlay->id);
	}
	for(int i = 0; i < 3; i++){
		int j = 2 - i;
		Overlay * techoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techoverlay){
			techoverlay->text = "Player ";
			techoverlay->text += std::to_string(i + 1);
			techoverlay->textbank = 133;
			techoverlay->textwidth = 6;
			techoverlay->uid = 80 + i;
			techoverlay->x = 375 - (techoverlay->text.length() * techoverlay->textwidth);
			techoverlay->y = 112 + (j * 16);
			techoverlay->draw = false;
			gametechinterface->AddObject(techoverlay->id);
		}
		Overlay * techlineoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techlineoverlay){
			techlineoverlay->x = 0;
			techlineoverlay->y = 0;
			techlineoverlay->uid = 90 + i;
			techlineoverlay->res_bank = 7;
			techlineoverlay->res_index = 20 + j;
			techlineoverlay->draw = false;
			gametechinterface->AddObject(techlineoverlay->id);
		}
	}
	Team * team = world.GetPeerTeam(world.localpeerid);
	if(team){
		for(int x = 0; x < 4; x++){
			int i = 0;
			int ipos = 0;
			for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
				BuyableItem * buyableitem = *it;
				if(buyableitem->techslots){
					if(buyableitem->agencyspecific == -1 || (buyableitem->agencyspecific == team->agency)){
						Button * button = (Button *)world.CreateObject(ObjectTypes::BUTTON);
						if(button){
							button->x = 410 + (x * 14);
							button->y = 125 + (ipos * 13);
							button->uid = 110 + (30 * x) + i;
							button->SetType(Button::BCHECKBOX);
							if(x < 3){
								button->effectbrightness = 64;
								button->draw = false;
							}
							gametechinterface->AddObject(button->id);
							if(x == 3){
								Overlay * technameoverlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
								if(technameoverlay){
									technameoverlay->text = buyableitem->name;
									technameoverlay->text += " (" + std::to_string(buyableitem->techslots) + ")";
									technameoverlay->x = 425 + (x * 14);
									technameoverlay->y = 127 + (ipos * 13);
									technameoverlay->textbank = 133;
									technameoverlay->textwidth = 6;
									technameoverlay->uid = 230 + i;
									gametechinterface->AddObject(technameoverlay->id);
								}
							}
						}
						ipos++;
					}
					i++;
				}
			}
		}
	}
	Overlay * techname = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	if(techname){
		techname->textbank = 134;
		techname->textwidth = 8;
		techname->uid = GTECH_OVL_TECHNAME;
		techname->x = 401 + (116 - ((techname->text.length() * techname->textwidth) / 2));
		techname->y = 350;
		gametechinterface->AddObject(techname->id);
	}
	for(int i = 0; i < 8; i++){
		Overlay * techdesc = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
		if(techdesc){
			techdesc->textbank = 133;
			techdesc->textwidth = 6;
			techdesc->effectcolor = 129;
			techdesc->effectbrightness = 128 + 16;
			techdesc->textcolorramp = true;
			techdesc->uid = 61 + i;
			techdesc->x = 405;
			techdesc->y = 370 + (i * 10);
			gametechinterface->AddObject(techdesc->id);
		}
	}
	gametechinterface->AddObject(teamsbutton->id);
	gametechinterface->buttonescape = teamsbutton->id;

	interfaceId = gametechinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
}

void GameTechPanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * gametechiface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!gametechiface) return;

	// Slots-left counter (uid 70). Recover from missing peerlist packet by
	// re-requesting on a 12-tick cadence when localpeer is unknown.
	int techslotsleft = 0;
	{
		Overlay * overlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, GTECH_OVL_SLOTS));
		if(overlay){
			Peer * peer = world.peerlist[world.localpeerid];
			if(peer){
				Team * team = world.GetPeerTeam(world.localpeerid);
				User * user = world.lobby.GetUserInfo(peer->accountid);
				if(user && team){
					techslotsleft = user->agency[team->agency].techslots - world.TechSlotsUsed(*peer);
					overlay->text = "Tech slots left: " + std::to_string(techslotsleft);
				}
			}else{
				if(world.tickcount % 12 == 0){
					world.RequestPeerList();
				}
			}
		}
	}

	Team * team = world.GetPeerTeam(world.localpeerid);
	if(!team) return;

	int peerindex = 0;
	for(int i = 0; i < 4; i++){
		Peer * peer = world.peerlist[team->peers[i]];
		User * user = peer ? world.lobby.GetUserInfo(peer->accountid) : nullptr;
		bool draw = (i < team->numpeers);
		int b = 0;
		int bpos = 0;
		for(std::vector<BuyableItem *>::iterator it = world.buyableitems.begin(); it != world.buyableitems.end(); it++){
			BuyableItem * buyableitem = *it;
			if(buyableitem->techslots){
				if(buyableitem->agencyspecific == -1 || buyableitem->agencyspecific == team->agency){
					bool usable = true;
					Uint8 uid = 110 + (30 * peerindex) + b;
					if(team->peers[i] == world.localpeerid){
						uid = 110 + (30 * 3) + b;
						if(buyableitem->techslots <= techslotsleft || (peer && peer->techchoices & buyableitem->techchoice)){
							usable = false;
						}
					}
					Button * button = static_cast<Button *>(gametechiface->GetObjectWithUid(world, uid));
					if(button){
						if(peer && peer->techchoices & buyableitem->techchoice){
							button->res_index = 18; // on
						}else{
							button->res_index = 19; // off
						}
						if(team->peers[i] == world.localpeerid){
							if(!usable){
								button->effectbrightness = 128;
							}else{
								button->effectbrightness = 64;
							}
						}
						if(button->type == Button::BCHECKBOX){
							if(button->clicked){
								if(button->uid >= 200 && button->effectbrightness == 128){
									Peer * lp = world.peerlist[world.localpeerid];
									if(lp){
										world.SetTech(lp->techchoices ^ buyableitem->techchoice);
										Team * lteam = world.GetPeerTeam(world.localpeerid);
										if(lteam){
											Config::GetInstance().defaulttechchoices[lteam->agency] = lp->techchoices ^ buyableitem->techchoice;
											Config::GetInstance().Save();
										}
									}
								}
								button->clicked = false;
							}
						}
						button->draw = draw;
					}
					Overlay * descoverlay_anchor = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 230 + b));
					if(descoverlay_anchor){
						if(descoverlay_anchor->clicked){
							descoverlay_anchor->clicked = false;
							Overlay * nameoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, GTECH_OVL_TECHNAME));
							if(nameoverlay){
								nameoverlay->text = "-" + std::string(buyableitem->name) + "-";
								nameoverlay->x = 401 + (116 - ((nameoverlay->text.length() * nameoverlay->textwidth) / 2));
							}
							char desc[1024];
							strcpy(desc, buyableitem->description);
							int linenum = 0;
							char * descline = strtok(desc, "\n");
							while(descline){
								Overlay * descoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 61 + linenum));
								if(descoverlay){
									descoverlay->text = descline;
								}
								linenum++;
								descline = strtok(NULL, "\n");
							}
							for(int j = linenum; j < 9; j++){
								Overlay * descoverlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 61 + j));
								if(descoverlay){
									descoverlay->text = "";
								}
							}
						}
						if(team->peers[i] == world.localpeerid){
							if(!usable){
								descoverlay_anchor->effectbrightness = 128;
							}else{
								descoverlay_anchor->effectbrightness = 64;
							}
						}
					}
					bpos++;
				}
				b++;
			}
		}
		if(team->peers[i] != world.localpeerid){
			Overlay * overlay = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 80 + peerindex));
			Overlay * overlayline = static_cast<Overlay *>(gametechiface->GetObjectWithUid(world, 90 + peerindex));
			if(overlay && overlayline){
				overlay->draw = draw;
				if(user){
					overlay->text = user->name;
					overlay->x = 375 - (overlay->text.length() * overlay->textwidth);
				}
				overlayline->draw = draw;
			}
			peerindex++;
		}
	}

	// "Back To Teams" button: returns to the GameJoinPanel.
	Button * back = static_cast<Button *>(gametechiface->GetObjectWithUid(world, GTECH_BTN_BACK));
	if(back && back->clicked && back->type != Button::BCHECKBOX){
		back->clicked = false;
		owner.ShowGameJoin(ctx);
		return;
	}
}

void GameTechPanel::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
