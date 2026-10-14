#include "dedicatedserver.h"
#include "world.h"
#include "team.h"
#include "peer.h"
#include "../gas/gasloader.h"
#include <algorithm>
#include <vector>

DedicatedServer::DedicatedServer(){
	active = false;
	state_i = GASLoader::Get().gameengine.heartbeatIntervalTicks;
	tickcount = 0;
	sockethandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	unsigned long iomode = 1;
    ioctl(sockethandle, FIONBIO, &iomode);
	nopeerstime = 0;
}

DedicatedServer::~DedicatedServer(){
	shutdown(sockethandle, SHUT_RDWR);
    closesocket(sockethandle);
}

void DedicatedServer::Start(char * lobbyaddress, unsigned short lobbyport, Uint32 gameid, Uint32 accountid){
	strcpy(DedicatedServer::lobbyaddress, lobbyaddress);
	DedicatedServer::lobbyport = lobbyport;
	DedicatedServer::gameid = gameid;
	DedicatedServer::accountid = accountid;
	active = true;
}

void DedicatedServer::Tick(World & world){
	if(world.gameplaystate == World::INLOBBY){
		for(int i = 0; i < world.maxpeers; i++){
			Peer * peer = world.peers.peerlist[i];
			if(peer){
				User * user = world.lobby.GetUserInfo(peer->accountid);
				if(!user->retrieving){
					Team * team = world.GetPeerTeam(peer->id);
					if(team){
						if(user->agency[team->agency].level < world.gameinfo.minlevel || user->agency[team->agency].level > world.gameinfo.maxlevel){
							world.HandleDisconnect(peer->id);
						}
					}
				}
			}
		}
	}
	if(world.peers.peercount < 1){
		nopeerstime++;
	}else{
		nopeerstime = 0;
	}
	if(state_i == GASLoader::Get().gameengine.heartbeatIntervalTicks){
		Uint8 state = 0;
		if(world.gameplaystate == World::INGAME){
			state = 1;
		}
		SendHeartBeat(world, state);
		state_i = 0;
	}
	state_i++;
	tickcount++;
}

void DedicatedServer::SendHeartBeat(World & world, Uint8 state){
	//printf("dedicated server sent heart beat update to %s:%d\n", lobbyaddress, lobbyport);
	Serializer data;
	char code = 0;
	data.Put(code);
	data.Put(gameid);
	data.Put(world.network.boundport);
	data.Put(state);
	// Parked-peer accountids — lobby derives a per-recipient can-rejoin bit
	// for the game-list payload so clients can offer "Join" on INGAME rows
	// they previously disconnected from.
	std::vector<Uint32> parked;
	for(int i = 1; i < world.maxpeers; i++){
		Peer * p = world.peers.peerlist[i];
		if(p && p->disconnected && p->accountid != 0 && !p->isbot){
			parked.push_back(p->accountid);
		}
	}
	Uint8 parkedcount = (Uint8)parked.size();
	data.Put(parkedcount);
	for(size_t i = 0; i < parked.size(); i++){
		Uint32 acct = parked[i];
		data.Put(acct);
	}
	// Extended fields for live game monitor (issue #23).
	// Lobby reads these if bytes are present; older lobbies ignore them.
	data.Put(tickcount);
	Uint32 aliveMask = 0;
	for(int i = 0; i < world.maxpeers && i < 32; i++){
		Peer * p = world.peers.peerlist[i];
		if(p && !p->disconnected){
			aliveMask |= (1u << i);
		}
	}
	data.Put(aliveMask);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(lobbyport);
	addr.sin_addr.s_addr = inet_addr(lobbyaddress);
	sendto(sockethandle, data.data, data.BitsToBytes(data.offset), 0, (sockaddr *)&addr, sizeof(addr));
}

void DedicatedServer::SendKillEvent(Uint32 killerAccountId, Uint32 victimAccountId, Uint8 weapon, Uint8 agencyIdx, Sint32 x, Sint32 y){
	if(!active) return;
	Serializer data;
	Uint8 code = 1;
	data.Put(code);
	data.Put(gameid);
	data.Put(killerAccountId);
	data.Put(victimAccountId);
	data.Put(weapon);
	data.Put(agencyIdx);
	data.Put(x);
	data.Put(y);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(lobbyport);
	addr.sin_addr.s_addr = inet_addr(lobbyaddress);
	sendto(sockethandle, data.data, data.BitsToBytes(data.offset), 0, (sockaddr *)&addr, sizeof(addr));
}

void DedicatedServer::Ban(Uint32 accountid){
	if(!IsBanned(accountid)){
		banlist.push_back(accountid);
	}
}

bool DedicatedServer::IsBanned(Uint32 accountid){
	if(std::find(banlist.begin(), banlist.end(), accountid) != banlist.end()){
		return true;
	}
	return false;
}