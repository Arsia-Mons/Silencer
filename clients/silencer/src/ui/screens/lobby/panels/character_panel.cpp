#include "character_panel.h"

#include "screen_context.h"
#include "world.h"
#include "lobby.h"
#include "config.h"
#include "team.h"
#include "user.h"
#include "objecttypes.h"
#include "interface.h"
#include "overlay.h"
#include "toggle.h"

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum CharacterPanelToggle : Uint8 {
	CHR_TGL_NOXIS     = 1,
	CHR_TGL_LAZARUS   = 2,
	CHR_TGL_CALIBER   = 3,
	CHR_TGL_STATIC    = 4,
	CHR_TGL_BLACKROSE = 5,
};
enum CharacterPanelOverlay : Uint8 {
	CHR_OVL_LEVEL  = 2,
	CHR_OVL_WINS   = 3,
	CHR_OVL_LOSSES = 4,
	CHR_OVL_XP     = 5,
};
}

void CharacterPanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	Interface * characterinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	characterinterface->x = 10;
	characterinterface->y = 64;
	characterinterface->width = 217;
	characterinterface->height = 120;
	Overlay * usertext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	usertext->text = world.lobby.GetLocalUsername();
	usertext->textbank = 134;
	usertext->textwidth = 8;
	usertext->effectcolor = 200;
	usertext->x = 20;
	usertext->y = 71;
	Overlay * leveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	leveltext->uid = CHR_OVL_LEVEL;
	leveltext->textbank = 133;
	leveltext->textwidth = 7;
	leveltext->effectcolor = 129;
	leveltext->effectbrightness = 128 + 32;
	leveltext->textcolorramp = true;
	leveltext->x = 17;
	leveltext->y = 130;
	Overlay * winstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	winstext->uid = CHR_OVL_WINS;
	winstext->textbank = 133;
	winstext->textwidth = 7;
	winstext->effectcolor = 129;
	winstext->effectbrightness = 128 + 32;
	winstext->textcolorramp = true;
	winstext->x = 17;
	winstext->y = 143;
	Overlay * lossestext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	lossestext->uid = CHR_OVL_LOSSES;
	lossestext->textbank = 133;
	lossestext->textwidth = 7;
	lossestext->effectcolor = 129;
	lossestext->effectbrightness = 128 + 32;
	lossestext->textcolorramp = true;
	lossestext->x = 17;
	lossestext->y = 156;
	Overlay * etctext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	etctext->uid = CHR_OVL_XP;
	etctext->textbank = 133;
	etctext->textwidth = 7;
	etctext->effectcolor = 129;
	etctext->effectbrightness = 128 + 32;
	etctext->textcolorramp = true;
	etctext->x = 17;
	etctext->y = 169;
	int xmargin = 42;
	Toggle * noxisbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	noxisbutton->y = 90;
	noxisbutton->x = 20 + (0 * xmargin);
	noxisbutton->res_bank = 181;
	noxisbutton->res_index = 0;
	noxisbutton->uid = CHR_TGL_NOXIS;
	noxisbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::NOXIS){
		noxisbutton->selected = true;
	}
	Toggle * lazarusbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	lazarusbutton->y = 90;
	lazarusbutton->x = 20 + (1 * xmargin);
	lazarusbutton->res_bank = 181;
	lazarusbutton->res_index = 1;
	lazarusbutton->uid = CHR_TGL_LAZARUS;
	lazarusbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::LAZARUS){
		lazarusbutton->selected = true;
	}
	Toggle * caliberbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	caliberbutton->y = 90;
	caliberbutton->x = 20 + (2 * xmargin);
	caliberbutton->res_bank = 181;
	caliberbutton->res_index = 2;
	caliberbutton->uid = CHR_TGL_CALIBER;
	caliberbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::CALIBER){
		caliberbutton->selected = true;
	}
	Toggle * staticbutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	staticbutton->y = 90;
	staticbutton->x = 20 + (3 * xmargin);
	staticbutton->res_bank = 181;
	staticbutton->res_index = 3;
	staticbutton->uid = CHR_TGL_STATIC;
	staticbutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::STATIC){
		staticbutton->selected = true;
	}
	Toggle * blackrosebutton = (Toggle *)world.CreateObject(ObjectTypes::TOGGLE);
	blackrosebutton->y = 90;
	blackrosebutton->x = 20 + (4 * xmargin);
	blackrosebutton->res_bank = 181;
	blackrosebutton->res_index = 4;
	blackrosebutton->uid = CHR_TGL_BLACKROSE;
	blackrosebutton->set = 1;
	if(Config::GetInstance().defaultagency == Team::BLACKROSE){
		blackrosebutton->selected = true;
	}
	characterinterface->AddObject(usertext->id);
	characterinterface->AddObject(leveltext->id);
	characterinterface->AddObject(winstext->id);
	characterinterface->AddObject(lossestext->id);
	characterinterface->AddObject(etctext->id);
	characterinterface->AddObject(noxisbutton->id);
	characterinterface->AddObject(lazarusbutton->id);
	characterinterface->AddObject(caliberbutton->id);
	characterinterface->AddObject(staticbutton->id);
	characterinterface->AddObject(blackrosebutton->id);

	interfaceId = characterinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
	// Mirror onto Game so legacy code (GetSelectedAgency, lobby pump joining
	// flow) keeps reading the character iface id directly. Removed in stage H.
	ctx.SetCharacterInterfaceId(interfaceId);
}

void CharacterPanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

	Uint8 selectedagency = ctx.GetSelectedAgency();
	if((int)selectedagency != oldselectedagency){
		Config::GetInstance().defaultagency = selectedagency;
		Config::GetInstance().Save();
		oldselectedagency = selectedagency;
		agencychanged = true;
		ctx.NotifyAgencyChanged(selectedagency);
	}

	if(!agencychanged) return;

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user || user->retrieving) return;

	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object || object->type != ObjectTypes::OVERLAY) continue;
		Overlay * overlay = static_cast<Overlay *>(object);
		switch(overlay->uid){
			case CHR_OVL_LEVEL:
				overlay->text = "LEVEL: " + std::to_string(user->agency[selectedagency].level);
			break;
			case CHR_OVL_WINS:
				overlay->text = "WINS: " + std::to_string(user->agency[selectedagency].wins);
			break;
			case CHR_OVL_LOSSES:
				overlay->text = "LOSSES: " + std::to_string(user->agency[selectedagency].losses);
			break;
			case CHR_OVL_XP:{
				// xptonextlevel is accumulated XP toward current level;
				// threshold is 100*(level+1). Display the remaining amount.
				int lvl = user->agency[selectedagency].level;
				int remaining = 100 * (lvl + 1) - (int)user->agency[selectedagency].xptonextlevel;
				overlay->text = "XP TO NEXT LEVEL: " + std::to_string(remaining);
				agencychanged = false;
			}break;
		}
	}
}

void CharacterPanel::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
