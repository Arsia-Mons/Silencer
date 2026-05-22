#include "world_network.h"
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
#include "audio/soundcue.h"
#include <algorithm>

#define DELTAENABLED 1

WorldNetwork::WorldNetwork(World & world) : world(world), lagsimulator(&sockethandle){
	sockethandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	unsigned long iomode = 1;
	ioctl(sockethandle, FIONBIO, &iomode);
	boundport = 0;
	state = World::IDLE;
	totalbytesread = 0;
	totalbytessent = 0;
	lastpingsent = 0;
	lastpingid = 0;
	pingtime = 0;
	for(int i = 0; i < static_cast<int>(sizeof(pinghistory) / sizeof(int)); i++){
		pinghistory[i] = 0;
	}
}

Peer * WorldNetwork::FindPeer(sockaddr_in & sockaddr){
	return world.FindPeer(sockaddr);
}

void WorldNetwork::DoNetwork(void){
	if(world.mode == World::AUTHORITY){
		DoNetwork_Authority();
	}else{
		DoNetwork_Replica();
	}
	world.lobby.DoNetwork();
	lagsimulator.Process(world);
}

void WorldNetwork::DoNetwork_Authority(void){
	Serializer data(10000);
	sockaddr_in senderaddr;
	socklen_t senderaddrsize = sizeof(senderaddr);
	int received;
	while((received = recvfrom(sockethandle, data.data, data.size, 0, (sockaddr *)&senderaddr, &senderaddrsize)) > 0){
		data.offset = received * 8;
		data.readoffset = 0;
		totalbytesread += received;
		Peer * peer = FindPeer(senderaddr);
		if(peer){
			peer->lastpacket = SDL_GetTicks();
		}
		char code;
		data.Get(code);
		switch(code){
			case World::MSG_CONNECT:{
				//printf("World::MSG_CONNECT received\n");
				Serializer response;
				Uint8 code = World::MSG_CONNECT;
				response.Put(code);
				if(world.gameplaystate == World::INLOBBY || world.gameplaystate == World::INGAME){
						char * host = inet_ntoa(senderaddr.sin_addr);
						unsigned short port = ntohs(senderaddr.sin_port);
						Uint8 agency;
						data.Get(agency);
						Uint32 accountid;
						data.Get(accountid);
						Uint32 selectedcharid;
						data.Get(selectedcharid);
						Uint8 passwordsize;
						data.Get(passwordsize);
					char temp[256];
					memset(temp, 0, sizeof(temp));
					for(int i = 0; i < passwordsize; i++){
						data.Get(temp[i]);
					}
					bool observerRequest = data.GetBit();
					bool canjoin = true;
					if(strcmp(world.gameinfo.password, temp) != 0){
						if(!(world.dedicatedserver.active && accountid == world.dedicatedserver.accountid)){
							//printf("bad password\n");
							canjoin = false;
						}
					}
					// Find a parked peer to rebind, if any. Done before the
					// maxplayers gate so a rejoiner reclaiming their existing
					// slot isn't rejected for a full world.lobby.
					Peer * rejoinpeer = 0;
					if(accountid != 0){
						for(unsigned int i = 1; i < World::maxpeers; i++){
							if(world.peers.peerlist[i] && world.peers.peerlist[i]->disconnected && world.peers.peerlist[i]->accountid == accountid){
								rejoinpeer = world.peers.peerlist[i];
								break;
							}
						}
					}
					if(world.dedicatedserver.active){
						if(world.dedicatedserver.IsBanned(accountid)){
							canjoin = false;
						}
						if(!rejoinpeer && !observerRequest && world.peers.peercount >= world.gameinfo.maxplayers){
							canjoin = false;
						}
					}
					if(canjoin && observerRequest && !world.gameinfo.spectatable){
						// defense-in-depth: button gating already prevents this in normal flow
						canjoin = false;
					}
					if(canjoin && world.gameplaystate == World::INGAME && !rejoinpeer && !observerRequest){
						// mid-game connects are only for rejoiners or observers
						canjoin = false;
					}
					if(canjoin && rejoinpeer){
						rejoinpeer->ip = ntohl(inet_addr(host));
						rejoinpeer->port = port;
						rejoinpeer->lastpacket = SDL_GetTicks();
						rejoinpeer->disconnected = false;
						for(std::list<Uint16>::iterator it = rejoinpeer->controlledlist.begin(); it != rejoinpeer->controlledlist.end(); it++){
							Object * obj = world.GetObjectFromId(*it);
							if(obj && obj->type == ObjectTypes::PLAYER){
								Player * p = static_cast<Player *>(obj);
								world.map.RandomPlayerStartLocation(world, p->x, p->y);
								p->oldx = p->x;
								p->oldy = p->y;
								p->health = p->maxhealth;
								p->shield = p->maxshield;
								p->state = Player::DEPLOYING;
								p->state_i = GASLoader::Get().player.deployWaitTicks;
								p->draw = false;
								p->collidable = false;
							}
						}
						response.PutBit(true);
						response.Put(rejoinpeer->id);
						world.SendGameInfo(rejoinpeer->id);
						world.SendPeerList();
					}else if(canjoin && observerRequest){
						Peer * newpeer = world.AddPeer(host, port, agency, accountid, selectedcharid, true);
						if(newpeer){
							newpeer->observer = true;
							response.PutBit(true);
							response.Put(newpeer->id);
							world.SendGameInfo(newpeer->id);
							world.SendPeerList();
						}else{
							response.PutBit(false);
						}
					}else if(canjoin){
						Peer * newpeer = world.AddPeer(host, port, agency, accountid, selectedcharid);
						if(newpeer){
							if(world.dedicatedserver.active){
								world.lobby.GetUserInfo(newpeer->accountid);
								if(newpeer->accountid == world.dedicatedserver.accountid){
									//printf("host connected\n");
									newpeer->ishost = true;
									newpeer->gameinfoloaded = true;
									newpeer->mapdownloaded = true;
								}
							}
							response.PutBit(true);
							response.Put(newpeer->id);
							if(!newpeer->ishost){
								world.SendGameInfo(newpeer->id);
							}
							if(world.replay.IsRecording()){
								world.replay.WriteNewPeer(agency, accountid, selectedcharid);
							}
						}else{
							//printf("couldnt add peer\n");
							response.PutBit(false);
						}
					}else{
						response.PutBit(false);
					}
				}else{
					response.PutBit(false);
				}
				Peer temppeer;
				temppeer.ip = ntohl(senderaddr.sin_addr.s_addr);
				temppeer.port = ntohs(senderaddr.sin_port);
				SendPacket(&temppeer, response.data, response.BitsToBytes(response.offset));
			}break;
			case World::MSG_INPUT:{ // client sending input
				if(peer && !peer->observer && world.gameplaystate == World::INGAME){
					world.replication.totalinputpackets++;
					peer->totalinputs++;
					Serializer * inputcopy = new Serializer;
					inputcopy->Copy(data);
					world.replication.inputqueue[peer->id].push_back(inputcopy);
					if(!peer->firstinputtime){
						peer->firstinputtime = world.tickcount;
					}
					if(world.replay.IsRecording()){
						world.replay.WriteInputCommand(world, peer->id, data);
					}
				}
			}break;
			case World::MSG_PEERLIST:{ // world.peers.peerlist requested
				if(peer){
					world.SendPeerList(peer->id);
				}
			}break;
			case World::MSG_DISCONNECT:{ // disconnect
				if(peer){
					world.HandleDisconnect(peer->id);
				}
			}break;
			case World::MSG_PING:{ // ping
				//printf("received ping from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
				if(peer){
					Uint32 pingid;
					data.Get(pingid);
					Serializer response;
					Uint8 code = World::MSG_PONG;
					response.Put(code);
					response.Put(pingid);
					SendPacket(peer, response.data, response.BitsToBytes(response.offset));
				}
			}break;
			case World::MSG_PONG:{ // pong

			}break;
			case World::MSG_GAMEINFO:{
				//printf("Received World::MSG_GAMEINFO\n");
				if(peer){
					if(peer->ishost){
						//printf("loading game info from host\n");
						world.gameinfo.Serialize(Serializer::READ, data);
						if(world.replay.IsRecording()){
							world.replay.WriteGameInfo(world.gameinfo);
						}
						Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
						if(localpeer){
							localpeer->gameinfoloaded = true;
						}
						for(int i = 0; i < World::maxpeers; i++){
							Peer * peer = world.peers.peerlist[i];
							if(peer && peer->id != world.peers.localpeerid && !peer->ishost){
								world.SendGameInfo(peer->id);
							}
						}
					}else{
						peer->gameinfoloaded = true;
						//world.SendGameInfo(peer->id);
					}
				}
			}break;
			case World::MSG_READY:{
				if(peer){
					if(peer->isready){
						peer->isready = false;
					}else{
						peer->isready = true;
					}
					world.SendPeerList();
					if(!peer->gameinfoloaded){
						world.SendGameInfo(peer->id);
					}
				}
			}break;
			case World::MSG_CHAT:{
				if(peer){
					Player * player = world.GetPeerPlayer(peer->id);
					Uint8 to;
					data.Get(to);
					if(peer->observer && to == 1){
						to = 0;
					}
					Serializer response;
					Uint8 code = World::MSG_CHAT;
					response.Put(code);
					response.Put(peer->accountid);
					data.data[256] = 0;
					char * msg = &data.data[data.BitsToBytes(data.readoffset)];
					for(int i = 0; i < strlen(msg); i++){
						response.Put(msg[i]);
					}
					msg[strlen(msg)] = 0;
					if(world.replay.IsRecording()){
						world.replay.WriteChat(peer->id, to, msg);
					}
					char nullend = 0;
					response.Put(nullend);
					if(to == 1){ // send to team
						if(player){
							Team * team = player->GetTeam(world);
							if(team){
								for(int i = 0; i < team->numpeers; i++){
									if(world.peers.peerlist[team->peers[i]] && team->peers[i] != world.peers.localpeerid){
										SendPacket(world.peers.peerlist[team->peers[i]], response.data, response.BitsToBytes(response.offset));
									}
								}
							}
						}
					}else{
						for(int i = 0; i < World::maxpeers; i++){
							if(world.peers.peerlist[i] && i != world.peers.localpeerid){
								SendPacket(world.peers.peerlist[i], response.data, response.BitsToBytes(response.offset));
							}
						}
					}
				}
			}break;
			case World::MSG_STATION:{
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					Uint8 id;
					data.Get(id);
					switch(subcode){
						case World::STA_BUY:{
							data.Get(id);
							Player * player = world.GetPeerPlayer(peer->id);
							if(player){
								player->BuyItem(world, id);
							}
							if(world.replay.IsRecording()){
								world.replay.WriteStation(peer->id, Replay::STA_BUY, id);
							}
						}break;
						case World::STA_REPAIR:{
							Player * player = world.GetPeerPlayer(peer->id);
							if(player){
								player->RepairItem(world, id);
							}
							if(world.replay.IsRecording()){
								world.replay.WriteStation(peer->id, Replay::STA_REPAIR, id);
							}
						}break;
						case World::STA_VIRUS:{
							Player * player = world.GetPeerPlayer(peer->id);
							if(player){
								player->VirusItem(world, id);
							}
							if(world.replay.IsRecording()){
								world.replay.WriteStation(peer->id, Replay::STA_VIRUS, id);
							}
						}break;
					}
					/*Uint8 id;
					data.Get(id);
					Player * player = world.GetPeerPlayer(peer->id);
					if(player){
						player->BuyItem(world, id);
					}
					if(world.replay.IsRecording()){
						world.replay.WriteStation(peer->id, Replay::STA_BUY, id);
					}*/
				}
			}break;
			/*case World::MSG_REPAIR:{
				if(peer){
					Uint8 id;
					data.Get(id);
					Player * player = world.GetPeerPlayer(peer->id);
					if(player){
						player->RepairItem(world, id);
					}
					if(world.replay.IsRecording()){
						world.replay.WriteStation(peer->id, Replay::STA_REPAIR, id);
					}
				}
			}break;
			case World::MSG_VIRUS:{
				if(peer){
					Uint8 id;
					data.Get(id);
					Player * player = world.GetPeerPlayer(peer->id);
					if(player){
						player->VirusItem(world, id);
					}
					if(world.replay.IsRecording()){
						world.replay.WriteStation(peer->id, Replay::STA_VIRUS, id);
					}
				}
			}break;*/
			case World::MSG_CHANGETEAM:{
				if(peer && world.gameplaystate == World::INLOBBY){
					world.ChangeTeam(peer->id);
					if(world.replay.IsRecording()){
						world.replay.WriteChangeTeam(peer->id);
					}
				}
			}break;
			case World::MSG_SETAGENCY:{
				if(peer && world.gameplaystate == World::INLOBBY){
					Uint8 newagency;
					data.Get(newagency);
					Team * peerteam = world.GetPeerTeam(peer->id);
					if(peerteam && peerteam->agency != newagency){
						peerteam->RemovePeer(peer->id);
						peer->isready = false;
						world.FindTeamForPeer(*peer, newagency);
						world.SendPeerList();
					}
				}
			}break;
			case World::MSG_TECH:{
				if(peer && world.gameplaystate == World::INLOBBY){
					Uint32 techchoices;
					data.Get(techchoices);
					world.SetTech(peer->id, techchoices);
					if(world.replay.IsRecording()){
						world.replay.WriteSetTech(peer->id, techchoices);
					}
				}
			}break;
			case World::MSG_EXISTS:{
				if(peer){
					Serializer response;
					Uint8 code = World::MSG_REMOVE;
					response.Put(code);
					while(data.MoreBytesToRead()){
						Uint16 objectid;
						data.Get(objectid);
						if(!world.GetObjectFromId(objectid)){
							response.Put(objectid);
						}
					}
					SendPacket(peer, response.data, response.BitsToBytes(response.offset));
				}
			}break;
			case World::MSG_MAP:{
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					switch(subcode){
						case World::MAP_DOWNLOADED:{
							peer->mapdownloaded = true;
							world.SendPeerList();
							world.BroadcastTriggerState();
						}break;
						case World::MAP_GETCHUNK:{
							Uint32 offset;
							data.Get(offset);
							world.PutMapChunk(offset, *peer);
						}break;
						case World::MAP_PUTCHUNK:{
							if(peer->ishost){
								Uint32 offset;
								data.Get(offset);
								Uint32 size;
								data.Get(size);
								world.StoreMapChunk((unsigned char *)&data.data[data.BitsToBytes(data.readoffset)], offset, size);
							}
						}break;
					}
				}
			}break;
			case World::MSG_KICK:{
				if(world.dedicatedserver.active && senderaddr.sin_addr.s_addr == inet_addr(world.dedicatedserver.lobbyaddress)){
					Uint32 accountid;
					data.Get(accountid);
					for(int i = 0; i < World::maxpeers; i++){
						Peer * p = world.peers.peerlist[i];
						if(p && p->accountid == accountid){
							if(world.gameplaystate == World::INGAME){
								world.KillByGovt(*p);
							}
							world.HandleDisconnect(p->id, true);
							break;
						}
					}
				}
			}break;
		}
	}
	if(!world.replay.IsPlaying()){
		Uint32 tickcheck = SDL_GetTicks();
		for(int i = 0; i < World::maxpeers; i++){
			if(world.peers.peerlist[i]){
				if(i != world.peers.localpeerid && !world.peers.peerlist[i]->isbot && !world.peers.peerlist[i]->disconnected && world.peers.peerlist[i]->lastpacket < tickcheck && tickcheck - world.peers.peerlist[i]->lastpacket >= World::peertimeout){
					world.HandleDisconnect(i);
				}
			}
		}
	}
}

void WorldNetwork::DoNetwork_Replica(void){
	if(!world.peers.peerlist[world.peers.authoritypeer]){
		return;
	}
	if(SDL_GetTicks() - lastpingsent >= (Uint32)GASLoader::Get().gameengine.pingIntervalMs){
		SendPing();
	}
	Serializer data(10000); // hopefully snapshots dont get larger than this
	sockaddr_in senderaddr;
	socklen_t senderaddrsize = sizeof(senderaddr);
	Peer * peer = 0;
	int received;
	while((received = recvfrom(sockethandle, data.data, data.size, 0, (sockaddr *)&senderaddr, &senderaddrsize)) > 0){
		//printf("received data from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
		if(world.peers.peerlist[world.peers.authoritypeer]->ip == ntohl(senderaddr.sin_addr.s_addr) && world.peers.peerlist[world.peers.authoritypeer]->port == ntohs(senderaddr.sin_port)){
			peer = world.peers.peerlist[world.peers.authoritypeer];
			peer->lastpacket = SDL_GetTicks();
		}
		data.offset = received * 8;
		data.readoffset = 0;
		totalbytesread += received;
		char code;
		data.Get(code);
		switch(code){
			case World::MSG_CONNECT:{ // connect response
				//printf("World::MSG_CONNECT response received from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
				if(peer){
					if(data.GetBit()){
						data.Get(world.peers.localpeerid);
						world.viewedpeerid = world.peers.localpeerid;
						//printf("we are connected, our peer id is %d\n", world.peers.localpeerid);
						world.RequestPeerList();
					}else{
						//printf("failed to connect to game\n");
						// host not accepting connection, password is wrong, or teams are full, etc
						Disconnect();
					}
				}
			}break;
			case World::MSG_SNAPSHOT:{ // snapshot data
				if(peer){
					if(state == World::CONNECTED){
						world.replication.totalsnapshots++;
						Serializer * snapshotcopy = new Serializer;
						snapshotcopy->Copy(data);
						world.replication.snapshotqueue.push_back(snapshotcopy);
					}
				}
			}break;
			case World::MSG_PEERLIST:{ // world.peers.peerlist update
				if(peer){
					world.ReadPeerList(data);
					// The World::MSG_CONNECT response (3 bytes) is often dropped by
					// carrier-grade NAT. Fall back to self-identifying by
					// accountid: our peer in the authority's world.peers.peerlist has
					// accountid == world.lobby.accountid and id != world.peers.authoritypeer.
					if(world.peers.localpeerid == world.peers.authoritypeer){
						for(unsigned int i = 0; i < World::maxpeers; i++){
							if(i == world.peers.authoritypeer) continue;
							if(world.peers.peerlist[i] && world.peers.peerlist[i]->accountid == world.lobby.accountid){
								world.peers.localpeerid = i;
								world.viewedpeerid = i;
								break;
							}
						}
					}
					Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
					if(localpeer && localpeer->ishost && !world.GetAuthorityPeer()->gameinfoloaded && world.gameinfo.loaded){
						world.SendGameInfo(world.GetAuthorityPeer()->id);
						//printf("We are host, sending game info\n");
					}
					if(state == World::CONNECTING && localpeer){
						state = World::CONNECTED;
					}
				}
			}break;
			case World::MSG_DISCONNECT:{ // disconnect
				if(peer){
					world.HandleDisconnect(world.peers.authoritypeer);
				}
			}break;
			case World::MSG_PING:{ // ping
				//printf("received ping from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
			}break;
			case World::MSG_PONG:{ // pong
				//printf("received World::MSG_PONG\n");
				Uint32 pingid;
				data.Get(pingid);
				if(pingid == lastpingid){
					pingtime = SDL_GetTicks() - lastpingsent;
					pinghistory[lastpingid % (sizeof(pinghistory) / sizeof(int))] = pingtime;
				}
			}break;
			case World::MSG_GAMEINFO:{
				if(peer){
					//printf("Received World::MSG_GAMEINFO\n");
					world.gameinfo.Serialize(Serializer::READ, data);
					//printf("password %s\n", world.gameinfo.password);
					world.SendGameInfoLoaded();
					/*Serializer response;
					Uint8 code = World::MSG_GAMEINFO;
					response.Put(code);
					SendPacket(world.GetAuthorityPeer(), response.data, response.BitsToBytes(response.offset));*/
				}
			}break;
			case World::MSG_CHAT:{
				Uint32 accountid;
				data.Get(accountid);
				world.DisplayChatMessage(accountid, &data.data[1 + 4]);
				/*std::string chatmsg(world.lobby.GetUserInfo(accountid)->name);
				chatmsg.append(":\xA0");
				chatmsg.append(&data.data[1 + 4]);
				
				char * wrapped = silencer::ui::WordWrapText(chatmsg.c_str(), 36);
				char * line = strtok(wrapped, "\n");
				while(line){
					chatlines.push_back(line);
					line = strtok(NULL, "\n");
				}
				delete[] wrapped;
				
				showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
				while(chatlines.size() > 5){
					chatlines.pop_front();
				}*/
			}break;
			case World::MSG_STATUS:{
				int size = data.BitsToBytes(data.offset - data.readoffset);
				char * newstatus = new char[size];
				memcpy(newstatus, &data.data[data.BitsToBytes(data.readoffset)], size);
				world.PushStatusString(newstatus);
			}break;
			case World::MSG_MESSAGE:{
				char message[1024];
				strcpy(message, &data.data[data.BitsToBytes(data.readoffset)]);
				Uint8 time = data.data[data.BitsToBytes(data.readoffset) + strlen(message) + 1];
				Uint8 type = data.data[data.BitsToBytes(data.readoffset) + strlen(message) + 1 + 1];
				//printf("GOT World::MSG_MESSAGE %s %d %d\n", message, time, type);
				world.ShowMessage(message, time, type);
			}break;
			case World::MSG_STATS:{
				if(peer){
					Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
					if(localpeer){
						localpeer->stats.Serialize(Serializer::READ, data);
						//printf("World::MSG_STATS received\n");
						Team * team = world.GetPeerTeam(localpeer->id);
						if(team){
							User * user = world.lobby.GetUserInfo(localpeer->accountid);
							char namecopy[64];
							strcpy(namecopy, user->name);
							world.lobby.ForgetUserInfo(localpeer->accountid);
							user = world.lobby.GetUserInfo(localpeer->accountid);
							strcpy(user->name, namecopy);
							user->statscopy = localpeer->stats;
							user->selectedcharid = localpeer->selectedcharid;
							user->statsagency = team->agency;
							user->teamnumber = team->number;
						}

					}
				}
			}break;
			case World::MSG_GOVTKILL:{
				if(peer){
					Uint8 peerid = data.data[data.BitsToBytes(data.readoffset)];
					Player * player = world.GetPeerPlayer(peerid);
					if(player){
						player->KillByGovt(world);
					}
				}
			}break;
			case World::MSG_SOUND:{
				if(peer){
					Uint8 volume = data.data[data.BitsToBytes(data.readoffset)];
					char * name = &data.data[data.BitsToBytes(data.readoffset) + 1];
					auto _r = ResolveSound(name, world.resources);
					if(_r.chunk) Audio::GetInstance().Play(_r.chunk, static_cast<int>(volume * _r.volume));
					//printf("World::MSG_SOUND %d %s\n", volume, name);
				}
			}break;
			case World::MSG_REMOVE:{
				if(peer){
					while(data.MoreBytesToRead()){
						Uint16 objectid;
						data.Get(objectid);
						Object * object = world.GetObjectFromId(objectid);
						if(object){
							world.MarkDestroyObject(objectid);
							//printf("DELETED %d\n", objectid);
						}
					}
				}
			}break;
			case World::MSG_MAP:{
				//printf("World::MSG_MAP received\n");
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					//printf("%d\n", subcode);
					switch(subcode){
						case World::MAP_GETCHUNK:{
							Uint32 offset;
							data.Get(offset);
							world.PutMapChunk(offset, *peer);
						}break;
						case World::MAP_PUTCHUNK:{
							Uint32 offset;
							data.Get(offset);
							Uint32 size;
							data.Get(size);
							world.StoreMapChunk((unsigned char *)&data.data[data.BitsToBytes(data.readoffset)], offset, size);
						}break;
					}
				}
			}break;
			case World::MSG_TRIGGER_STATE:{
				if(peer){
					world.triggerGraph.ApplySerializedState(data);
				}
			}break;
			case World::MSG_CAMERA:{
				Sint16 cx, cy;
				memcpy(&cx, &data.data[data.BitsToBytes(data.readoffset)],     sizeof(cx));
				memcpy(&cy, &data.data[data.BitsToBytes(data.readoffset) + 2], sizeof(cy));
				if(cx != 0 || cy != 0){
					world.pancameraactive = true;
					world.pancamerareturn = false;
					world.pancamerax = cx;
					world.pancameray = cy;
				} else {
					world.pancameraactive = false;
					world.pancamerareturn = true;
					world.pancamerareturncount = GASLoader::Get().gameengine.ticksPerSecond * 3; // 3s return window
				}
			}break;
		}
	}
	Uint32 tickcheck = SDL_GetTicks();
	if(world.peers.peerlist[world.peers.authoritypeer]){
		if(world.peers.peerlist[world.peers.authoritypeer]->lastpacket < tickcheck && tickcheck - world.peers.peerlist[world.peers.authoritypeer]->lastpacket >= World::peertimeout){
			world.HandleDisconnect(world.peers.authoritypeer);
		}
	}
}

void WorldNetwork::SendPacket(Peer * peer, char * data, unsigned int size){
	if(world.replay.IsPlaying()){
		return;
	}
	if(peer){
		if(lagsimulator.Active() && world.gameplaystate == World::INGAME){
			lagsimulator.QueuePacket(peer, data, size);
		}else{
			sockaddr_in recvaddr;
			recvaddr.sin_family = AF_INET;
			recvaddr.sin_port = htons(peer->port);
			recvaddr.sin_addr.s_addr = htonl(peer->ip);
			int ret = sendto(sockethandle, data, size, 0, (sockaddr *)&recvaddr, sizeof(recvaddr));
			if(ret > 0){
				totalbytessent += ret;
			}
		}
	}
}

void WorldNetwork::SwitchToMode(bool newmode){
	if(newmode == World::REPLICA && world.mode == World::AUTHORITY){
		world.mode = World::REPLICA;
		for(unsigned int i = 0; i < World::maxpeers; i++){
			if(world.peers.peerlist[i]){
				world.peers.authoritypeer = i;
				break;
			}
		}
	}else
	if(newmode == World::AUTHORITY && world.mode == World::REPLICA){
		world.mode = World::AUTHORITY;
		world.peers.authoritypeer = world.peers.localpeerid;
		for(unsigned int i = 0; i < World::maxpeers; i++){
			if(world.peers.peerlist[i]){
				world.peers.peerlist[i]->lastpacket = SDL_GetTicks();
			}
		}
		Listen(world.GetAuthorityPeer()->port);
		world.SendPeerList();
	}
}

bool WorldNetwork::Listen(unsigned short port){
	if(!boundport){
		if(!Bind(port)){
			return false;
		}
	}
	world.AllocateMapData(65535);
	SwitchToMode(World::AUTHORITY);
	state = World::LISTENING;
	Peer * authoritypeerptr = world.GetAuthorityPeer();
	authoritypeerptr->ip = INADDR_ANY;
	authoritypeerptr->port = boundport;
	//printf("Listening on port %d\n", boundport);
	return true;
}

unsigned short WorldNetwork::Bind(unsigned short port){
	sockaddr_in recvaddr;
	recvaddr.sin_family = AF_INET;
	recvaddr.sin_port = htons(port);
	recvaddr.sin_addr.s_addr = INADDR_ANY;
	int ret = bind(sockethandle, (sockaddr *)&recvaddr, sizeof(recvaddr));
	if(ret == 0){
		sockaddr_in boundaddr;
		socklen_t boundaddrlen = sizeof(boundaddr);
		getsockname(sockethandle, (sockaddr *)&boundaddr, &boundaddrlen);
		boundport = ntohs(boundaddr.sin_port);
		return boundport;
	}
	return false;
}

void WorldNetwork::Connect(Uint8 agency, Uint32 accountid, Uint32 selectedcharid, const char * password, bool observer){
	world.AllocateMapData(65535);
	sockaddr_in addr;
	addr.sin_addr.s_addr = htonl(world.GetAuthorityPeer()->ip);
	//printf("sending connect request with agency %d, accountid %d to %s:%d\n", agency, accountid, inet_ntoa(addr.sin_addr), world.GetAuthorityPeer()->port);
	SwitchToMode(World::REPLICA);
	state = World::CONNECTING;
	world.messaging.messagetype = 0;
	world.peers.authoritypeer = 0;
	world.GetAuthorityPeer()->lastpacket = SDL_GetTicks();
	Serializer data;
	Uint8 code = World::MSG_CONNECT;
	data.Put(code);
	data.Put(agency);
	data.Put(accountid);
	data.Put(selectedcharid);
	Uint8 passwordsize = password ? strlen(password) : 0;
	data.Put(passwordsize);
	for(int i = 0; i < passwordsize; i++){
		data.Put(password[i]);
	}
	data.PutBit(observer);
	SendPacket(world.GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

void WorldNetwork::Disconnect(void){
	world.ClearSnapshotQueue();
	world.ClearMapData();
	state = World::IDLE;
	world.viewedpeerid = 0;
	world.spectator.freecam = false;
	world.spectator.camx = 0;
	world.spectator.camy = 0;
	world.spectator.camvx = 0;
	world.spectator.camvy = 0;
	world.spectator.holdshowallnames = false;
	world.spectator.initialized = false;
	char data[1];
	data[0] = World::MSG_DISCONNECT;
	if(world.mode == World::AUTHORITY){
		for(int i = 0; i < World::maxpeers; i++){
			Peer * peer = world.peers.peerlist[i];
			if(peer){
				SendPacket(peer, data, sizeof(data));
			}
		}
	}else
	if(world.mode == World::REPLICA){
		SendPacket(world.GetAuthorityPeer(), data, sizeof(data));
		world.HandleDisconnect(world.GetAuthorityPeer()->id);
	}
}

void WorldNetwork::SwitchToLocalAuthorityMode(void){
	world.mode = World::AUTHORITY;
	for(int i = 0; i < World::maxpeers; i++){
		if(world.peers.peerlist[i]){
			delete world.peers.peerlist[i];
			world.peers.peerlist[i] = 0;
		}
	}
	world.peers.peercount = 0;
	world.peers.authoritypeer = world.GetAuthorityPeer()->id;
	world.peers.localpeerid = world.peers.authoritypeer;
	world.viewedpeerid = world.peers.localpeerid;
}

bool WorldNetwork::IsAuthority(void){
	return world.mode == World::AUTHORITY;
}

bool WorldNetwork::IsLocalObserver(void){
	Peer * lp = world.peers.peerlist[world.peers.localpeerid];
	return lp && lp->observer;
}

bool WorldNetwork::IsConnected() const {
	return state == World::CONNECTED;
}

bool WorldNetwork::IsIdle() const {
	return state == World::IDLE;
}

void WorldNetwork::SendPing(void){
	Serializer data;
	Uint8 code = World::MSG_PING;
	data.Put(code);
	lastpingid++;
	data.Put(lastpingid);
	SendPacket(world.GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
	lastpingsent = SDL_GetTicks();
}

int WorldNetwork::GetPingTime(void){
	if(state == World::CONNECTED){
		return pingtime;
	}else{
		return 0;
	}
}

int WorldNetwork::AveragePingJitter(void){
	int average = 0;
	for(int i = 0; i < sizeof(pinghistory) / sizeof(int); i++){
		int jitter = abs(pinghistory[i] - pinghistory[(i - 1) % sizeof(pinghistory) / sizeof(int)]);
		average += jitter;
	}
	average /= sizeof(pinghistory) / sizeof(int);
	return average;
}
