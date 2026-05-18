#include "data_retrieval_mode.h"
#include "gamestateobject.h"
#include "gasloader.h"
#include "lobby.h"
#include "objecttypes.h"
#include "player.h"
#include "team.h"
#include "terminal.h"
#include "world.h"

// Re-fire OnSecretDelivered each tick for any team that has reached the
// secret threshold but whose win hasn't been registered yet — guards against
// lost network packets on the delivery event.
void DataRetrievalMode::Tick(World& world) {
	if(world.winningteamid) return;
	for(Object* obj : world.objectlist){
		if(obj->type != ObjectTypes::TEAM) continue;
		Team* team = static_cast<Team*>(obj);
		if(team->secrets >= GASLoader::Get().player.secretsNeededToWin){
			OnSecretDelivered(world, *team);
			return;
		}
	}
}

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

void DataRetrievalMode::OnSecretBeamReady(World& world, Team& team) {
	if(!world.IsAuthority()) return;

	std::vector<Terminal*> terminals;
	for(auto* obj : world.objectlist){
		if(obj->type != ObjectTypes::TERMINAL) continue;
		Terminal* terminal = static_cast<Terminal*>(obj);
		if(terminal->isbig &&
		   terminal->state != Terminal::SECRETBEAMING &&
		   terminal->state != Terminal::SECRETREADY){
			terminals.push_back(terminal);
		}
	}
	if(terminals.empty()) return;

	Terminal* terminal = terminals[world.Random() % terminals.size()];
	terminal->state = Terminal::SECRETBEAMING;
	terminal->state_i = 0;
	team.beamingterminalid = terminal->id;
	const TerminalDef* td = GASLoader::Get().GetTerminalDef("big");
	terminal->beamingtime = td ? td->beaconTimeSecs : 65;

	char teamtext[256];
	char enemytext[256];
	sprintf(teamtext, "TOP SECRET LOCATION DETERMINED\n\nApproximate time : %d seconds", terminal->beamingtime);
	sprintf(enemytext, "ENEMY BEAMING DETECTED\n\nTracking location on radar");
	for(int i = 0; i < world.maxpeers; i++){
		Peer* peer = world.peerlist[i];
		if(!peer) continue;
		if(world.GetPeerTeam(peer->id) == &team){
			world.ShowMessage(teamtext, 128, 0, true, peer);
		} else {
			world.ShowMessage(enemytext, 128, 0, true, peer);
		}
	}
	world.SendSound(GASLoader::Get().player.soundTeamHack.c_str());
}

void DataRetrievalMode::OnSecretDelivered(World& world, Team& team) {
	// Award credits and show delivery message. team.secretdelivered holds the
	// delivering player's object ID (cleared by Team::Tick after this call).
	Player* deliverer = static_cast<Player*>(world.GetObjectFromId(team.secretdelivered));
	if(deliverer){
		deliverer->EmitSound(world, world.resources.soundbank[GASLoader::Get().player.soundTeamHeal], 48);
		Peer* peer = deliverer->GetPeer(world);
		if(peer){
			User* user = world.lobby.GetUserInfo(peer->accountid);
			bool stolen = deliverer->secretteamid != team.id;
			int remaining = GASLoader::Get().player.secretsNeededToWin - team.secrets;
			char text[128];
			sprintf(text, "%s returned a %s\n( %d remaining )\n\nTeam awarded %d credits",
			        user->name, stolen ? "stolen secret" : "secret", remaining,
			        GASLoader::Get().player.secretDeliveryCredits);
			if(!world.intutorialmode){
				world.ShowMessage(text, 128, 0, true);
			}
		}
	}
	for(int i = 0; i < team.numpeers; i++){
		Player* p = world.GetPeerPlayer(team.peers[i]);
		if(p) p->AddCredits(GASLoader::Get().player.secretDeliveryCredits);
	}
	world.Illuminate();

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

