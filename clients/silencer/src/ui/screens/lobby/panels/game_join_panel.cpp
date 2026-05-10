#include "game_join_panel.h"

#include "screen_context.h"
#include "lobby_screen.h"
#include "game.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "peer.h"

#include <cstring>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum GameJoinButton : Uint8 {
	GJN_BTN_READY  = 25,
	GJN_BTN_TEAM   = 26,
	GJN_BTN_TECH   = 27,
};
}

GameJoinPanel::GameJoinPanel(LobbyScreen & owner_) : owner(owner_) {}

void GameJoinPanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	Interface * gamejoininterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gamejoininterface->x = 403;
	gamejoininterface->y = 87;
	gamejoininterface->width = 222;
	gamejoininterface->height = 267;
	Button * gamestartbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamestartbutton->x = 242;
	gamestartbutton->y = 160;
	gamestartbutton->SetType(Button::B156x21);
	gamestartbutton->uid = GJN_BTN_READY;
	strcpy(gamestartbutton->text, "Ready");
	Button * techbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	techbutton->x = 242;
	techbutton->y = 68;
	techbutton->SetType(Button::B156x21);
	techbutton->uid = GJN_BTN_TECH;
	strcpy(techbutton->text, "Choose Tech");
	Button * changeteambutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	changeteambutton->x = 242;
	changeteambutton->y = 100;
	changeteambutton->SetType(Button::B156x21);
	changeteambutton->uid = GJN_BTN_TEAM;
	strcpy(changeteambutton->text, "Change Team");
	gamejoininterface->AddObject(techbutton->id);
	gamejoininterface->AddObject(changeteambutton->id);
	gamejoininterface->AddObject(gamestartbutton->id);
	gamejoininterface->buttonenter = gamestartbutton->id;

	interfaceId = gamejoininterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
	// Mirror onto Game so the legacy TickLobbyBody / GoBack paths can still
	// query and tear down via gamejoininterface. Removed in stage H.
	ctx.game.gamejoininterface = interfaceId;
}

void GameJoinPanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

	// Per-frame Ready button label: while the host is waiting for peers to
	// finish downloading the map, the button reads "Waiting..." instead of
	// "Ready". Used to live in case INGAME's INLOBBY branch in Game::Tick.
	if(world.gameplaystate == World::INLOBBY){
		Button * readybtn = static_cast<Button *>(iface->GetObjectWithUid(world, GJN_BTN_READY));
		if(readybtn){
			Peer * localpeer = world.peerlist[world.localpeerid];
			bool blocked = localpeer && localpeer->ishost && !world.AllPeersDownloadedMap();
			strcpy(readybtn->text, blocked ? "Waiting..." : "Ready");
		}
	}

	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object || object->type != ObjectTypes::BUTTON) continue;
		Button * button = static_cast<Button *>(object);
		if(!button->clicked || button->type == Button::BCHECKBOX) continue;
		switch(button->uid){
			case GJN_BTN_READY:{
				button->clicked = false;
				Peer * localpeer = world.peerlist[world.localpeerid];
				bool ishost = localpeer && localpeer->ishost;
				if(!ishost || world.AllPeersDownloadedMap()){
					world.SendReady();
				}
			}break;
			case GJN_BTN_TEAM:{
				button->clicked = false;
				world.ChangeTeam();
			}break;
			case GJN_BTN_TECH:{
				button->clicked = false;
				owner.ShowGameTech(ctx);
				return;
			}
		}
	}
}

void GameJoinPanel::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
