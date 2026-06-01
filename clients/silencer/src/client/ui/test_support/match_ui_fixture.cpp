#include "client/ui/test_support/match_ui_fixture.h"

#include "basedoor.h"
#include "buyableitem.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "world.h"

#include <cstring>
#include <vector>

namespace silencer {
namespace client_ui {

MatchUiFixtureResult ConfigureMatchUiFixture(
		World& world,
		int viewed_peer_id,
		MatchUiFixtureMode mode) {
	MatchUiFixtureResult result;
	result.mode = mode;

	Player * player = world.GetPeerPlayer(viewed_peer_id);
	if(!player){
		result.error = "no viewed player";
		return result;
	}

	auto populate = [&]() {
		std::vector<BuyableItem *> buyItems;
		std::vector<BuyableItem *> techItems;
		player->CollectBuyMenuItems(world, false, buyItems);
		player->CollectBuyMenuItems(world, true, techItems);
		result.available = true;
		result.chatActive = player->chatActive;
		result.chatDraft = player->chatText;
		result.buyActive = player->isbuying;
		result.techActive = player->techstationactive;
		result.showChatTicks = world.messaging.showchat_i;
		result.showPlayerList = world.IsShowingPlayerList();
		result.quitState = world.quitstate;
		result.topMessageProgress = world.messaging.topmessage_i;
		result.messageProgress = world.messaging.message_i;
		result.statusMessageCount =
			static_cast<int>(world.messaging.statusmessages.size());
		result.buyItemCount = static_cast<int>(buyItems.size());
		result.techItemCount = static_cast<int>(techItems.size());
		result.buySelectedIndex = player->buyifacelastitem;
		result.techSelectedIndex = player->techifacelastitem;
	};

	auto clear = [&]() {
		player->chatActive = false;
		player->chatText[0] = '\0';
		player->isbuying = false;
		player->techstationactive = false;
		world.messaging.showchat_i = 0;
		world.SetShowingPlayerList(false);
		world.quitstate = 0;
		world.messaging.topmessage_i = 0;
		world.messaging.topmessage[0] = '\0';
		world.messaging.message_i = 0;
		world.messaging.message[0] = '\0';
		for(char * status : world.messaging.statusmessages){
			delete[] status;
		}
		world.messaging.statusmessages.clear();
	};

	if(mode == MatchUiFixtureMode::Clear){
		clear();
		populate();
		return result;
	}

	if(mode != MatchUiFixtureMode::Status) clear();
	if(mode == MatchUiFixtureMode::Chat || mode == MatchUiFixtureMode::All){
		player->chatActive = true;
		player->chatwithteam = false;
		std::strncpy(player->chatText, "clay chat smoke", sizeof(player->chatText) - 1);
		player->chatText[sizeof(player->chatText) - 1] = '\0';
		world.messaging.showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
	}
	if(mode == MatchUiFixtureMode::Buy || mode == MatchUiFixtureMode::All){
		player->isbuying = true;
		player->buyifacelastitem = 0;
		player->buyifacelastscrolled = 0;
	}
	if(mode == MatchUiFixtureMode::Tech || mode == MatchUiFixtureMode::All){
		Team * team = player->GetTeam(world);
		const std::vector<Uint16> & teams = world.GetObjectsByType(ObjectTypes::TEAM);
		if(!team && !teams.empty()){
			team = static_cast<Team *>(world.GetObjectFromId(teams[0]));
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
			player->SetTeamId(team->id);
			team->AddPeer(world.GetLocalPeerId());
		}
		BaseDoor * door = nullptr;
		if(team){
			team->disabledtech = 0xffffffff;
			door = static_cast<BaseDoor *>(world.GetObjectFromId(team->basedoorid));
			for(Uint16 objectid : world.GetObjectsByType(ObjectTypes::BASEDOOR)){
				auto * candidate = static_cast<BaseDoor *>(world.GetObjectFromId(objectid));
				if(candidate && candidate->teamid == team->id){
					door = candidate;
					break;
				}
			}
		}
		if(door){
			player->SetBaseDoorEntering(door->id);
		}
		if(team){
			int baseY = ((world.map.height + 10) * 64)
				+ (team->number * 26 * 64) + 64;
			player->y = static_cast<Sint16>(baseY);
		}
		player->techstationactive = true;
		player->techifacelastitem = 0;
		player->techifacelastscrolled = 0;
	}
	if(mode == MatchUiFixtureMode::PlayerList || mode == MatchUiFixtureMode::All){
		world.SetShowingPlayerList(true);
	}
	if(mode == MatchUiFixtureMode::QuitPrompt || mode == MatchUiFixtureMode::All){
		world.quitstate = 1;
	}
	if(mode == MatchUiFixtureMode::TopMessage || mode == MatchUiFixtureMode::All){
		world.ShowTopMessage("        RETAINED TOP MESSAGE");
	}
	if(mode == MatchUiFixtureMode::Message || mode == MatchUiFixtureMode::All){
		world.ShowMessage("RETAINED CENTER MESSAGE", 128, 0);
	}
	if(mode == MatchUiFixtureMode::StatusLine || mode == MatchUiFixtureMode::All){
		world.ShowStatus("RETAINED STATUS MESSAGE", 153);
	}

	populate();
	return result;
}

}  // namespace client_ui
}  // namespace silencer
