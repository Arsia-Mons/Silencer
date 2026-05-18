#include "data_retrieval_mode.h"
#include "gamestateobject.h"
#include "gasloader.h"
#include "lobby.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "world.h"

bool DataRetrievalMode::IsMatchOver(const World& w) const {
	return w.GetWinningTeamId() != 0;
}

Uint16 DataRetrievalMode::WinningTeamId(const World& w) const {
	return w.GetWinningTeamId();
}

void DataRetrievalMode::UpdateScores(GameStateObject& gso, const World& w) const {
	for(Object* obj : w.objectlist){
		if(obj->type != ObjectTypes::TEAM) continue;
		const Team* team = static_cast<const Team*>(obj);
		if(team->number < 6){
			gso.score[team->number] = team->secrets;
		}
	}
}

void DataRetrievalMode::OnSecretDelivered(World& world, Team& team) {
	if(team.secrets < GASLoader::Get().player.secretsNeededToWin) return;
	if(world.winningteamid) return;

	world.winningteamid = team.id;

	// Un-deploy winners; terminate everyone else.
	for(auto* obj : world.objectlist){
		if(obj->type != ObjectTypes::PLAYER) continue;
		Player* player = static_cast<Player*>(obj);
		Team* pt = player->GetTeam(world);
		if(pt && pt->id == world.winningteamid){
			player->UnDeploy();
		} else {
			Peer* peer = player->GetPeer(world);
			if(peer) world.KillByGovt(*peer);
		}
	}

	// Broadcast result to all peers.
	for(int i = 0; i < world.maxpeers; i++){
		Peer* peer = world.peerlist[i];
		if(!peer) continue;

		bool isourteam = false;
		for(int j = 0; j < team.numpeers; j++){
			Peer* tp = world.peerlist[team.peers[j]];
			if(tp && tp->id == peer->id){ isourteam = true; break; }
		}

		char fs[64];
		Uint8 type;
		if(isourteam){
			strcpy(fs, "MISSION SUCCESS\n");
			type = 10;
		} else {
			strcpy(fs, "MISSION FAILED\n");
			type = 11;
		}

		char message[256];
		sprintf(message, "%sAll secrets retrieved\nby %s agents:\n\n", fs, team.GetAgencyName());
		for(int j = 0; j < team.numpeers; j++){
			Peer* tp = world.peerlist[team.peers[j]];
			if(tp){
				User* user = world.lobby.GetUserInfo(tp->accountid);
				strcat(message, user->name);
				strcat(message, "\n");
			}
		}
		world.ShowMessage(message, 255, type, true, peer);
	}
}

