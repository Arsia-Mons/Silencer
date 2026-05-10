#include "mission_summary_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "user.h"
#include "stats.h"
#include "interface.h"
#include "overlay.h"
#include "textbox.h"
#include "button.h"
#include "scrollbar.h"
#include "objecttypes.h"
#include "renderer.h"

#include <cstdio>
#include <cstring>
#include <string>

void MissionSummaryScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return;
	Stats & stats = user->statscopy;
	Uint8 agency = user->statsagency;
	(void)agency; // present for parity with the legacy signature; not branched on here.

	Overlay * background = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background->res_bank = 6;
	background->res_index = 0;
	Overlay * background2 = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background2->res_bank = 7;
	background2->res_index = 5;
	Overlay * title = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	title->text = "Mission Summary";
	title->textbank = 135;
	title->textwidth = 12;
	title->x = 192 - ((int)(title->text.length() * title->textwidth) / 2);
	title->y = 44;
	Interface * iface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	TextBox * textbox = static_cast<TextBox *>(world.CreateObject(ObjectTypes::TEXTBOX));
	textbox->x = 89;
	textbox->y = 92;
	textbox->width = 180;
	textbox->height = 300;
	textbox->res_bank = 133;
	textbox->lineheight = 11;
	textbox->fontwidth = 6;

	AddSummaryLine(*textbox, "Kills:", stats.kills);
	AddSummaryLine(*textbox, "Deaths:", stats.deaths);
	AddSummaryLine(*textbox, "Suicides", stats.suicides);
	textbox->AddLine("");
	textbox->AddLine("Secrets");
	AddSummaryLine(*textbox, "  Returned:", stats.secretsreturned);
	AddSummaryLine(*textbox, "  Stolen:", stats.secretsstolen);
	AddSummaryLine(*textbox, "  Picked up:", stats.secretspickedup);
	AddSummaryLine(*textbox, "  Fumbled:", stats.secretsdropped);
	textbox->AddLine("");
	AddSummaryLine(*textbox, "Civilians killed:", stats.civilianskilled);
	AddSummaryLine(*textbox, "Guards killed:", stats.guardskilled);
	AddSummaryLine(*textbox, "Robots killed:", stats.robotskilled);
	AddSummaryLine(*textbox, "Defenses destroyed:", stats.defensekilled);
	AddSummaryLine(*textbox, "Fixed Cannons destroyed:", stats.fixedcannonsdestroyed);
	textbox->AddLine("");
	textbox->AddLine("Files");
	AddSummaryLine(*textbox, "  Hacked:", stats.fileshacked);
	AddSummaryLine(*textbox, "  Returned:", stats.filesreturned);
	textbox->AddLine("");
	AddSummaryLine(*textbox, "Powerups picked up:", stats.powerupspickedup);
	AddSummaryLine(*textbox, "Health packs used:", stats.healthpacksused);
	AddSummaryLine(*textbox, "Cameras placed:", stats.camerasplanted);
	AddSummaryLine(*textbox, "Detonators planted:", stats.detsplanted);
	AddSummaryLine(*textbox, "Fixed Cannons placed:", stats.fixedcannonsplaced);
	AddSummaryLine(*textbox, "Viruses used:", stats.virusesused);
	AddSummaryLine(*textbox, "Poisons:", stats.poisons);
	AddSummaryLine(*textbox, "Lazarus Tracts planted:", stats.tractsplanted);
	textbox->AddLine("");
	textbox->AddLine("Grenades thrown");
	AddSummaryLine(*textbox, "  E.M.P:", stats.empsthrown);
	AddSummaryLine(*textbox, "  Plasma:", stats.plasmasthrown);
	AddSummaryLine(*textbox, "  Shaped:", stats.shapedthrown);
	AddSummaryLine(*textbox, "  Flare:", stats.flaresthrown);
	AddSummaryLine(*textbox, "  Poison Flare:", stats.poisonflaresthrown);
	AddSummaryLine(*textbox, "  Neutron:", stats.neutronsthrown);
	for(int i = 0; i < 4; i++){
		textbox->AddLine("");
		switch(i){
			case 0: textbox->AddLine("Blaster"); break;
			case 1: textbox->AddLine("Laser"); break;
			case 2: textbox->AddLine("Rocket"); break;
			case 3: textbox->AddLine("Flamer"); break;
		}
		AddSummaryLine(*textbox, "  Shots fired:", stats.weaponfires[i]);
		AddSummaryLine(*textbox, "  Hits:", stats.weaponhits[i]);
		AddSummaryLine(*textbox, "  Accuracy:", (Uint32)((float(stats.weaponhits[i]) / stats.weaponfires[i]) * 100), true);
		AddSummaryLine(*textbox, "  Player kills:", stats.playerkillsweapon[i]);
	}
	ScrollBar * scrollbar = static_cast<ScrollBar *>(world.CreateObject(ObjectTypes::SCROLLBAR));
	scrollbar->res_index = 9;
	scrollbar->scrollposition = 0;
	scrollbar->scrollmax = textbox->text.size() - (textbox->height / textbox->lineheight);
	scrollbar->scrollpixels = textbox->lineheight;
	scrollbar->scrollposition = 0;

	Overlay * xptext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	xptext->text = "+ ";
	xptext->text += std::to_string(stats.CalculateExperience()) + " XP";
	xptext->textbank = 136;
	xptext->textwidth = 15;
	xptext->x = 467 - ((int)(xptext->text.length() * xptext->textwidth) / 2);
	xptext->y = 45;

	Overlay * line1text = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	line1text->text = "*NEW UPGRADE AVAILABLE*";
	line1text->textbank = 133;
	line1text->effectcolor = 129;
	line1text->effectbrightness = 128 + 32;
	line1text->textcolorramp = true;
	line1text->textwidth = 6;
	line1text->uid = 1;
	line1text->x = 467 - ((int)(line1text->text.length() * line1text->textwidth) / 2);
	line1text->y = 77;
	line1text->draw = false;
	iface->AddObject(line1text->id);

	for(int i = 0; i < 6; i++){
		Overlay * text = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
		Overlay * textlevel = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
		switch(i){
			default:
			case 0: text->text = "Current Endurance Level:"; break;
			case 1: text->text = "Current Shield Level:"; break;
			case 2: text->text = "Current Jetpack Level:"; break;
			case 3: text->text = "Current Tech Slot Level:"; break;
			case 4: text->text = "Current Hacking Level:"; break;
			case 5: text->text = "Current Contacts Level:"; break;
		}
		text->textbank = 133;
		textlevel->textbank = 133;
		text->textwidth = 6;
		textlevel->textwidth = 6;
		textlevel->uid = 20 + i;
		text->x = 390;
		textlevel->x = 556 - ((int)(textlevel->text.length() * textlevel->textwidth));
		text->y = 97 + (i * 46);
		textlevel->y = text->y;
		iface->AddObject(text->id);
		iface->AddObject(textlevel->id);
	}

	Button * okbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	okbutton->y = 100;
	okbutton->x = 62;
	okbutton->uid = 0;
	strcpy(okbutton->text, "Done");
	iface->AddObject(background->id);
	iface->AddObject(background2->id);
	iface->AddObject(title->id);
	iface->AddObject(textbox->id);
	iface->AddObject(scrollbar->id);
	iface->AddObject(okbutton->id);
	iface->buttonenter = okbutton->id;
	iface->buttonescape = okbutton->id;
	iface->scrollbar = scrollbar->id;
	interfaceId = iface->id;
	Refresh(ctx);
}

void MissionSummaryScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(interfaceId));
	if(!iface) return;
	if(world.lobby.statupgraded || !infoLoaded){
		User * user = world.lobby.GetUserInfo(world.lobby.accountid);
		if(user && !user->retrieving){
			Refresh(ctx);
			world.lobby.statupgraded = false;
		}
	}
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object) continue;
		switch(object->type){
			case ObjectTypes::TEXTBOX:{
				TextBox * textbox = static_cast<TextBox *>(object);
				if(textbox){
					Object * sbo = world.GetObjectFromId(iface->scrollbar);
					ScrollBar * scrollbar = static_cast<ScrollBar *>(sbo);
					if(scrollbar) textbox->scrolled = scrollbar->scrollposition;
				}
			}break;
			case ObjectTypes::BUTTON:{
				Button * button = static_cast<Button *>(object);
				if(!button || !button->clicked) break;
				button->clicked = false;
				User * user = world.lobby.GetUserInfo(world.lobby.accountid);
				switch(button->uid){
					case 0:
						if(world.lobby.state == Lobby::AUTHENTICATED){
							ctx.GoToState(GameState::LOBBY);
							world.lobby.JoinChannel(world.lobby.lastchannel);
						}else{
							ctx.GoToState(GameState::MAINMENU);
						}
					break;
					case 10: if(user) world.lobby.UpgradeStat(user->statsagency, 0); break;
					case 11: if(user) world.lobby.UpgradeStat(user->statsagency, 1); break;
					case 12: if(user) world.lobby.UpgradeStat(user->statsagency, 2); break;
					case 13: if(user) world.lobby.UpgradeStat(user->statsagency, 3); break;
					case 14: if(user) world.lobby.UpgradeStat(user->statsagency, 4); break;
					case 15: if(user) world.lobby.UpgradeStat(user->statsagency, 5); break;
				}
			}break;
		}
	}
}

void MissionSummaryScreen::Destroy(ScreenContext & ctx)
{
	if(!interfaceId) return;
	Interface * iface = static_cast<Interface *>(ctx.world.GetObjectFromId(interfaceId));
	if(iface) iface->DestroyInterface(ctx.world);
	interfaceId = 0;
}

void MissionSummaryScreen::Refresh(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(interfaceId));
	if(!iface) return;
	bool upgradeavailable = false;
	bool upgradesavailable[6] = {};
	int totalbonusupgrades = 0;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && !user->retrieving){
		infoLoaded = true;
		auto & ag = user->agency[user->statsagency];
		totalbonusupgrades += ag.endurance + ag.shield + ag.jetpack + ag.techslots + ag.hacking + ag.contacts;
		upgradesavailable[0] = ag.endurance < ag.maxendurance;
		upgradesavailable[1] = ag.shield    < ag.maxshield;
		upgradesavailable[2] = ag.jetpack   < ag.maxjetpack;
		upgradesavailable[3] = ag.techslots < ag.maxtechslots;
		upgradesavailable[4] = ag.hacking   < ag.maxhacking;
		upgradesavailable[5] = ag.contacts  < ag.maxcontacts;
		int maxupgrades = ag.level;
		if(maxupgrades > user->TotalUpgradePointsPossible(user->statsagency)){
			maxupgrades = user->TotalUpgradePointsPossible(user->statsagency);
		}
		if(totalbonusupgrades - ag.defaultbonuses < maxupgrades){
			upgradeavailable = true;
		}
	}
	for(int i = 0; i < 6; i++){
		if(upgradesavailable[i] && upgradeavailable){
			if(!iface->GetObjectWithUid(world, 10 + i)){
				Button * button = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
				button->y = -180 + (i * 46);
				button->x = 62;
				button->uid = 10 + i;
				switch(i){
					case 0: sprintf(button->text, "+1 Endurance"); break;
					case 1: sprintf(button->text, "+1 Shield   "); break;
					case 2: sprintf(button->text, "+1 Jetpack  "); break;
					case 3: sprintf(button->text, "+1 Tech Slot"); break;
					case 4: sprintf(button->text, "+1 Hacking  "); break;
					case 5: sprintf(button->text, "+1 Contacts "); break;
				}
				iface->AddObject(button->id);
			}
		}else{
			Button * button = static_cast<Button *>(iface->GetObjectWithUid(world, 10 + i));
			if(button){
				iface->RemoveObject(button->id);
				world.MarkDestroyObject(button->id);
			}
		}
		Overlay * overlay = static_cast<Overlay *>(iface->GetObjectWithUid(world, 20 + i));
		if(overlay && user){
			auto & ag = user->agency[user->statsagency];
			switch(i){
				case 0: overlay->text = std::to_string(ag.endurance); break;
				case 1: overlay->text = std::to_string(ag.shield); break;
				case 2: overlay->text = std::to_string(ag.jetpack); break;
				case 3: overlay->text = std::to_string(ag.techslots); break;
				case 4: overlay->text = std::to_string(ag.hacking); break;
				case 5: overlay->text = std::to_string(ag.contacts); break;
			}
			overlay->x = 556 - ((int)(overlay->text.length() * overlay->textwidth));
		}
	}
	Overlay * banner = static_cast<Overlay *>(iface->GetObjectWithUid(world, 1));
	if(banner) banner->draw = upgradeavailable;
}

void MissionSummaryScreen::AddSummaryLine(TextBox & textbox, const char * name, Uint32 value, bool percentage)
{
	char string[256];
	char valuetext[64];
	sprintf(valuetext, "%d%s", value, percentage ? "%" : " ");
	int maxchars = textbox.width / textbox.fontwidth;
	int used = strlen(name) + strlen(valuetext);
	strcpy(string, name);
	for(int i = 0; i < maxchars - used; i++){
		strcat(string, " ");
	}
	strcat(string, valuetext);
	textbox.AddLine(string);
}
