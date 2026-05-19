#include "world.h"
#include "serializer.h"
#include "player.h"
#include "civilian.h"
#include "robot.h"
#include "fixedcannon.h"
#include "walldefense.h"
#include "techstation.h"
#include "surveillancemonitor.h"
#include "team.h"
#include "objecttypes.h"
#include "terminal.h"
#include "basedoor.h"
#include "bodypart.h"
#include "gasloader.h"
#include "gamestateobject.h"
#include "text_wrap.h"
#include <algorithm>

#define DELTAENABLED 1

Peer * World::AddPeer(char * address, unsigned short port, Uint8 agency, Uint32 accountid, bool observer){
	Uint8 newpeerid = 0;
	sockaddr_in addr;
	addr.sin_addr.s_addr = inet_addr(address);
	addr.sin_port = htons(port);
	Peer * peer = FindPeer(addr);
	if(!peer){
		bool peeradded = false;
		for(unsigned int i = 1; i < maxpeers; i++){
			if(!peerlist[i]){
				newpeerid = i;
				peeradded = true;
				break;
			}
		}
		Peer * newpeer = new Peer();
		newpeer->id = newpeerid;
		newpeer->ip = ntohl(inet_addr(address));
		newpeer->port = port;
		newpeer->accountid = accountid;
		if(peeradded){
			if(!observer){
				if(!FindTeamForPeer(*newpeer, agency)){
					//printf("could not find team for new peer\n");
					delete newpeer;
					return 0;
				}
			}
			peerlist[newpeerid] = newpeer;
			peercount++;
			//printf("new peer added, peer id: %d (%s:%d) peercount = %d\n", newpeerid, address, port, peercount);
			return newpeer;
		}else{
			delete newpeer;
			return 0;
		}
	}else{
		//printf("existing peer added, peer id: %d\n", peer->id);
		return 0;
		//return peer;
	}
	return 0;
}

Peer * World::AddBot(Uint8 agency){
	Uint8 newpeerid = 0;
	bool peeradded = false;
	for(unsigned int i = 1; i < maxpeers; i++){
		if(!peerlist[i]){
			newpeerid = i;
			peeradded = true;
			break;
		}
	}
	Peer * newpeer = new Peer();
	newpeer->id = newpeerid;
	if(peeradded){
		if(!FindTeamForPeer(*newpeer, agency)){
			//printf("could not find team for new peer\n");
			delete newpeer;
			return 0;
		}
		newpeer->isbot = true;
		peerlist[newpeerid] = newpeer;
		peercount++;
		//printf("added bot, peer id: %d peercount = %d\n", newpeerid, peercount);
		return newpeer;
	}else{
		delete newpeer;
		return 0;
	}
	return 0;
}

Peer * World::FindPeer(sockaddr_in & sockaddr){
	Peer * peer = 0;
	for(int i = 0; i < maxpeers; i++){
		if(peerlist[i]){
			if(peerlist[i]->ip == ntohl(sockaddr.sin_addr.s_addr)){
				if(peerlist[i]->port == ntohs(sockaddr.sin_port)){
					peer = peerlist[i];
				}
			}
		}
	}
	return peer;
}

void World::ReadPeerList(Serializer & data){
	for(unsigned int i = 0; i < maxpeers; i++){
		if(i == authoritypeer){
			continue;
		}
		Peer * peer = peerlist[i];
		if(peer){
			delete peer;
			peerlist[i] = 0;
			peercount--;
		}
	}
	while(data.MoreBytesToRead()){
		Peer * peer = new Peer();
		peer->Serialize(Serializer::READ, data);
		if(peer->id == authoritypeer && peerlist[peer->id]){
			peerlist[peer->id]->accountid = peer->accountid;
			peerlist[peer->id]->controlledlist = peer->controlledlist;
			peerlist[peer->id]->gameinfoloaded = peer->gameinfoloaded;
			peerlist[peer->id]->isready = peer->isready;
			peerlist[peer->id]->ishost = peer->ishost;
			delete peer;
			continue;
		}
		peerlist[peer->id] = peer;
		//printf("peerlist[%d]\n", peer->id);
		peercount++;
	}
}

void World::HandleDisconnect(Uint8 peerid, bool permanent){
	//printf("peer %d disconnected\n", peerid);
	if(replay.IsRecording()){
		replay.WriteDisconnect(peerid);
	}
	bool park = (!permanent && mode == AUTHORITY && gameplaystate == INGAME && peerlist[peerid] && peerlist[peerid]->accountid != 0 && !peerlist[peerid]->isbot && !peerlist[peerid]->observer);
	for(std::list<Uint16>::iterator i = peerlist[peerid]->controlledlist.begin(); i != peerlist[peerid]->controlledlist.end(); i++){
		Object * object = GetObjectFromId((*i));
		if(object){
			object->HandleDisconnect(*this, peerid);
		}
	}
	if(park){
		peerlist[peerid]->disconnected = true;
		peerlist[peerid]->isready = false;
		SendPeerList();
		return;
	}
	ClearSnapshotQueue();
	// Capture team before RemovePeer strips the peer from all teams.
	// GetPeerTeam searches team membership lists, so it must run first.
	Team * leavingTeam = (mode == AUTHORITY) ? GetPeerTeam(peerid) : 0;
	for(std::list<Object *>::iterator it = objectlist.begin(); it != objectlist.end(); it++){
		Object * object = *it;
		if(object->type == ObjectTypes::TEAM){
			Team * team = static_cast<Team *>(object);
			if(team){
				team->RemovePeer(peerid);
			}
		}
	}
	if(mode == REPLICA){
		delete peerlist[peerid];
		peerlist[peerid] = 0;
		peercount--;
		if(peerid == authoritypeer){
			ShowMessage("CONNECTION LOST", 128, 20);
			SwitchToLocalAuthorityMode();
			state = IDLE;
		}
	}else
	if(mode == AUTHORITY){
		// If a player disconnects mid-game, record their partial stats immediately
		// so their progress isn't lost (won=0 since they left before the game ended).
		if(gameplaystate == INGAME && peerlist[peerid]->accountid != 0){
			Peer * leavingPeer = peerlist[peerid];
			User * user = lobby.GetUserInfo(leavingPeer->accountid);
			if(user && leavingTeam){
				user->statscopy = leavingPeer->stats;
				user->statsagency = leavingTeam->agency;
				user->teamnumber = leavingTeam->number;
				lobby.RegisterStats(*user, 0, gameinfo.id);
			}
		}
		delete peerlist[peerid];
		peerlist[peerid] = 0;
		peercount--;
		for(int i = 0; i < maxoldsnapshots; i++){
			if(oldsnapshots[peerid][i]){
				delete oldsnapshots[peerid][i];
				oldsnapshots[peerid][i] = 0;
			}
		}
		SendPeerList();
	}
}

void World::SendStats(Peer & peer){
	Serializer msg;
	Uint8 code = MSG_STATS;
	msg.Put(code);
	peer.stats.Serialize(Serializer::WRITE, msg);
	SendPacket(&peer, msg.data, msg.BitsToBytes(msg.offset));
}

void World::UserInfoReceived(Peer & peer){
	if(replay.IsRecording()){
		replay.WriteUserInfo(*lobby.GetUserInfo(peer.accountid));
	}
	ApplyWantedTech(peer);
}

bool World::CompareTeamByNumber(Team * team1, Team * team2){
	return(team1->number < team2->number);
}

Peer * World::GetAuthorityPeer(void){
	if(!peerlist[authoritypeer]){
		peerlist[authoritypeer] = new Peer();
		peerlist[authoritypeer]->ip = INADDR_ANY;
		peerlist[authoritypeer]->id = authoritypeer;
	}
	return peerlist[authoritypeer];
}

Peer * World::GetPeer(Uint8 peerid){
	if(peerid >= maxpeers) return 0;
	return peerlist[peerid];
}

Player * World::GetPeerPlayer(Uint8 peerid){
	Object * object = 0;
	Player * player = 0;
	if(peerlist[peerid]){
		if(peerlist[peerid]->controlledlist.size() > 0){
			object = GetObjectFromId((*peerlist[peerid]->controlledlist.begin()));
			if(object && object->type == ObjectTypes::PLAYER){
				player = static_cast<Player *>(object);
				if(player){
					return player;
				}
			}
		}
	}
	return 0;
}

Team * World::GetPeerTeam(Uint8 peerid){
	if(peerlist[peerid]){
		for(std::vector<Uint16>::iterator it = objectsbytype[ObjectTypes::TEAM].begin(); it != objectsbytype[ObjectTypes::TEAM].end(); it++){
			Team * team = static_cast<Team *>(GetObjectFromId((*it)));
			for(int i = 0; i < team->numpeers; i++){
				if(team->peers[i] == peerid){
					return team;
				}
			}
		}
	}
	return 0;
}

bool World::FindTeamForPeer(Peer & peer, Uint8 agency, int start){
	int maxteams2 = maxteams;
	if(dedicatedserver.active){
		maxteams2 = gameinfo.maxteams;
		if(maxteams2 > maxteams){
			maxteams2 = maxteams;
		}
	}
	if(start >= maxteams2){
		start = 0;
	}
	int teamnumber = start;
	bool teamfound = false;
	bool slotfound = true;
	std::vector<Team *> teamlist;
	for(std::list<Object *>::iterator it = objectlist.begin(); it != objectlist.end(); it++){
		Object * object = (*it);
		if(object->type == ObjectTypes::TEAM){
			Team * team = static_cast<Team *>(object);
			if(team){
				teamlist.push_back(team);
			}
		}
	}
	std::sort(teamlist.begin(), teamlist.end(), CompareTeamByNumber);
	std::vector<Team *>::iterator it = teamlist.begin();
	while(it != teamlist.end()){
		Team * team = *it;
		if(team->number == start){
			slotfound = false;
		}
		if(team->number == teamnumber){
			teamnumber = team->number + 1;
			it = teamlist.begin();
		}else{
			it++;
		}
	}
	if(!slotfound){
		for(std::vector<Team *>::iterator it = teamlist.begin(); it != teamlist.end(); it++){
			Team * team = *it;
			if(team->number >= start){
				if(team->agency == agency){
					if(team->AddPeer(peer.id)){
						teamfound = true;
						break;
					}
				}
			}
		}
	}
	if(!teamfound){
		if(teamlist.size() >= maxteams2){
			return false;
		}
		Team * newteam = (Team *)CreateObject(ObjectTypes::TEAM);
		newteam->agency = agency;
		newteam->number = teamnumber;
		newteam->AddPeer(peer.id);
	}
	return true;
}

void World::RequestPeerList(void){
	if(authoritypeer != localpeerid){
		Serializer data;
		Uint8 code = MSG_PEERLIST;
		data.Put(code);
		SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
	}
}

void World::SendPeerList(Uint8 peerid){
	if(mode == AUTHORITY){
		Serializer data;
		Uint8 code = MSG_PEERLIST;
		data.Put(code);
		for(unsigned int i = 0; i < maxpeers; i++){
			Peer * peer = peerlist[i];
			if(peer){
				peer->Serialize(Serializer::WRITE, data);
			}
		}
		for(unsigned int i = 0; i < maxpeers; i++){
			Peer * peer = peerlist[i];
			if(peer && i != localpeerid && (!peerid || peerid == peer->id) && !peer->disconnected){
				SendPacket(peer, data.data, data.BitsToBytes(data.offset));
			}
		}
	}
}

