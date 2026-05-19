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

void World::DoNetwork(void){
	if(mode == AUTHORITY){
		DoNetwork_Authority();
	}else{
		DoNetwork_Replica();
	}
	lobby.DoNetwork();
	lagsimulator.Process(*this);
}

void World::DoNetwork_Authority(void){
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
			case MSG_CONNECT:{
				//printf("MSG_CONNECT received\n");
				Serializer response;
				Uint8 code = MSG_CONNECT;
				response.Put(code);
				if(gameplaystate == INLOBBY || gameplaystate == INGAME){
					char * host = inet_ntoa(senderaddr.sin_addr);
					unsigned short port = ntohs(senderaddr.sin_port);
					Uint8 agency;
					data.Get(agency);
					Uint32 accountid;
					data.Get(accountid);
					Uint8 passwordsize;
					data.Get(passwordsize);
					char temp[256];
					memset(temp, 0, sizeof(temp));
					for(int i = 0; i < passwordsize; i++){
						data.Get(temp[i]);
					}
					bool observerRequest = data.GetBit();
					bool canjoin = true;
					if(strcmp(gameinfo.password, temp) != 0){
						if(!(dedicatedserver.active && accountid == dedicatedserver.accountid)){
							//printf("bad password\n");
							canjoin = false;
						}
					}
					// Find a parked peer to rebind, if any. Done before the
					// maxplayers gate so a rejoiner reclaiming their existing
					// slot isn't rejected for a full lobby.
					Peer * rejoinpeer = 0;
					if(accountid != 0){
						for(unsigned int i = 1; i < maxpeers; i++){
							if(peerlist[i] && peerlist[i]->disconnected && peerlist[i]->accountid == accountid){
								rejoinpeer = peerlist[i];
								break;
							}
						}
					}
					if(dedicatedserver.active){
						if(dedicatedserver.IsBanned(accountid)){
							canjoin = false;
						}
						if(!rejoinpeer && !observerRequest && peercount >= gameinfo.maxplayers){
							canjoin = false;
						}
					}
					if(canjoin && observerRequest && !gameinfo.spectatable){
						// defense-in-depth: button gating already prevents this in normal flow
						canjoin = false;
					}
					if(canjoin && gameplaystate == INGAME && !rejoinpeer && !observerRequest){
						// mid-game connects are only for rejoiners or observers
						canjoin = false;
					}
					if(canjoin && rejoinpeer){
						rejoinpeer->ip = ntohl(inet_addr(host));
						rejoinpeer->port = port;
						rejoinpeer->lastpacket = SDL_GetTicks();
						rejoinpeer->disconnected = false;
						for(std::list<Uint16>::iterator it = rejoinpeer->controlledlist.begin(); it != rejoinpeer->controlledlist.end(); it++){
							Object * obj = GetObjectFromId(*it);
							if(obj && obj->type == ObjectTypes::PLAYER){
								Player * p = static_cast<Player *>(obj);
								map.RandomPlayerStartLocation(*this, p->x, p->y);
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
						SendGameInfo(rejoinpeer->id);
						SendPeerList();
					}else if(canjoin && observerRequest){
						Peer * newpeer = AddPeer(host, port, agency, accountid, true);
						if(newpeer){
							newpeer->observer = true;
							response.PutBit(true);
							response.Put(newpeer->id);
							SendGameInfo(newpeer->id);
							SendPeerList();
						}else{
							response.PutBit(false);
						}
					}else if(canjoin){
						Peer * newpeer = AddPeer(host, port, agency, accountid);
						if(newpeer){
							if(dedicatedserver.active){
								lobby.GetUserInfo(newpeer->accountid);
								if(newpeer->accountid == dedicatedserver.accountid){
									//printf("host connected\n");
									newpeer->ishost = true;
									newpeer->gameinfoloaded = true;
									newpeer->mapdownloaded = true;
								}
							}
							response.PutBit(true);
							response.Put(newpeer->id);
							if(!newpeer->ishost){
								SendGameInfo(newpeer->id);
							}
							if(replay.IsRecording()){
								replay.WriteNewPeer(agency, accountid);
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
			case MSG_INPUT:{ // client sending input
				if(peer && !peer->observer && gameplaystate == INGAME){
					totalinputpackets++;
					peer->totalinputs++;
					Serializer * inputcopy = new Serializer;
					inputcopy->Copy(data);
					inputqueue[peer->id].push_back(inputcopy);
					if(!peer->firstinputtime){
						peer->firstinputtime = tickcount;
					}
					if(replay.IsRecording()){
						replay.WriteInputCommand(*this, peer->id, data);
					}
				}
			}break;
			case MSG_PEERLIST:{ // peerlist requested
				if(peer){
					SendPeerList(peer->id);
				}
			}break;
			case MSG_DISCONNECT:{ // disconnect
				if(peer){
					HandleDisconnect(peer->id);
				}
			}break;
			case MSG_PING:{ // ping
				//printf("received ping from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
				if(peer){
					Uint32 pingid;
					data.Get(pingid);
					Serializer response;
					Uint8 code = MSG_PONG;
					response.Put(code);
					response.Put(pingid);
					SendPacket(peer, response.data, response.BitsToBytes(response.offset));
				}
			}break;
			case MSG_PONG:{ // pong

			}break;
			case MSG_GAMEINFO:{
				//printf("Received MSG_GAMEINFO\n");
				if(peer){
					if(peer->ishost){
						//printf("loading game info from host\n");
						gameinfo.Serialize(Serializer::READ, data);
						if(replay.IsRecording()){
							replay.WriteGameInfo(gameinfo);
						}
						Peer * localpeer = peerlist[localpeerid];
						if(localpeer){
							localpeer->gameinfoloaded = true;
						}
						for(int i = 0; i < maxpeers; i++){
							Peer * peer = peerlist[i];
							if(peer && peer->id != localpeerid && !peer->ishost){
								SendGameInfo(peer->id);
							}
						}
					}else{
						peer->gameinfoloaded = true;
						//SendGameInfo(peer->id);
					}
				}
			}break;
			case MSG_READY:{
				if(peer){
					if(peer->isready){
						peer->isready = false;
					}else{
						peer->isready = true;
					}
					SendPeerList();
					if(!peer->gameinfoloaded){
						SendGameInfo(peer->id);
					}
				}
			}break;
			case MSG_CHAT:{
				if(peer){
					Player * player = GetPeerPlayer(peer->id);
					Uint8 to;
					data.Get(to);
					if(peer->observer && to == 1){
						to = 0;
					}
					Serializer response;
					Uint8 code = MSG_CHAT;
					response.Put(code);
					response.Put(peer->accountid);
					data.data[256] = 0;
					char * msg = &data.data[data.BitsToBytes(data.readoffset)];
					for(int i = 0; i < strlen(msg); i++){
						response.Put(msg[i]);
					}
					msg[strlen(msg)] = 0;
					if(replay.IsRecording()){
						replay.WriteChat(peer->id, to, msg);
					}
					char nullend = 0;
					response.Put(nullend);
					if(to == 1){ // send to team
						if(player){
							Team * team = player->GetTeam(*this);
							if(team){
								for(int i = 0; i < team->numpeers; i++){
									if(peerlist[team->peers[i]] && team->peers[i] != localpeerid){
										SendPacket(peerlist[team->peers[i]], response.data, response.BitsToBytes(response.offset));
									}
								}
							}
						}
					}else{
						for(int i = 0; i < maxpeers; i++){
							if(peerlist[i] && i != localpeerid){
								SendPacket(peerlist[i], response.data, response.BitsToBytes(response.offset));
							}
						}
					}
				}
			}break;
			case MSG_STATION:{
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					Uint8 id;
					data.Get(id);
					switch(subcode){
						case STA_BUY:{
							data.Get(id);
							Player * player = GetPeerPlayer(peer->id);
							if(player){
								player->BuyItem(*this, id);
							}
							if(replay.IsRecording()){
								replay.WriteStation(peer->id, Replay::STA_BUY, id);
							}
						}break;
						case STA_REPAIR:{
							Player * player = GetPeerPlayer(peer->id);
							if(player){
								player->RepairItem(*this, id);
							}
							if(replay.IsRecording()){
								replay.WriteStation(peer->id, Replay::STA_REPAIR, id);
							}
						}break;
						case STA_VIRUS:{
							Player * player = GetPeerPlayer(peer->id);
							if(player){
								player->VirusItem(*this, id);
							}
							if(replay.IsRecording()){
								replay.WriteStation(peer->id, Replay::STA_VIRUS, id);
							}
						}break;
					}
					/*Uint8 id;
					data.Get(id);
					Player * player = GetPeerPlayer(peer->id);
					if(player){
						player->BuyItem(*this, id);
					}
					if(replay.IsRecording()){
						replay.WriteStation(peer->id, Replay::STA_BUY, id);
					}*/
				}
			}break;
			/*case MSG_REPAIR:{
				if(peer){
					Uint8 id;
					data.Get(id);
					Player * player = GetPeerPlayer(peer->id);
					if(player){
						player->RepairItem(*this, id);
					}
					if(replay.IsRecording()){
						replay.WriteStation(peer->id, Replay::STA_REPAIR, id);
					}
				}
			}break;
			case MSG_VIRUS:{
				if(peer){
					Uint8 id;
					data.Get(id);
					Player * player = GetPeerPlayer(peer->id);
					if(player){
						player->VirusItem(*this, id);
					}
					if(replay.IsRecording()){
						replay.WriteStation(peer->id, Replay::STA_VIRUS, id);
					}
				}
			}break;*/
			case MSG_CHANGETEAM:{
				if(peer && gameplaystate == INLOBBY){
					ChangeTeam(peer->id);
					if(replay.IsRecording()){
						replay.WriteChangeTeam(peer->id);
					}
				}
			}break;
			case MSG_SETAGENCY:{
				if(peer && gameplaystate == INLOBBY){
					Uint8 newagency;
					data.Get(newagency);
					Team * peerteam = GetPeerTeam(peer->id);
					if(peerteam && peerteam->agency != newagency){
						peerteam->RemovePeer(peer->id);
						peer->isready = false;
						FindTeamForPeer(*peer, newagency);
						SendPeerList();
					}
				}
			}break;
			case MSG_TECH:{
				if(peer && gameplaystate == INLOBBY){
					Uint32 techchoices;
					data.Get(techchoices);
					SetTech(peer->id, techchoices);
					if(replay.IsRecording()){
						replay.WriteSetTech(peer->id, techchoices);
					}
				}
			}break;
			case MSG_EXISTS:{
				if(peer){
					Serializer response;
					Uint8 code = MSG_REMOVE;
					response.Put(code);
					while(data.MoreBytesToRead()){
						Uint16 objectid;
						data.Get(objectid);
						if(!GetObjectFromId(objectid)){
							response.Put(objectid);
						}
					}
					SendPacket(peer, response.data, response.BitsToBytes(response.offset));
				}
			}break;
			case MSG_MAP:{
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					switch(subcode){
						case MAP_DOWNLOADED:{
							peer->mapdownloaded = true;
							SendPeerList();
							BroadcastTriggerState();
						}break;
						case MAP_GETCHUNK:{
							Uint32 offset;
							data.Get(offset);
							PutMapChunk(offset, *peer);
						}break;
						case MAP_PUTCHUNK:{
							if(peer->ishost){
								Uint32 offset;
								data.Get(offset);
								Uint32 size;
								data.Get(size);
								StoreMapChunk((unsigned char *)&data.data[data.BitsToBytes(data.readoffset)], offset, size);
							}
						}break;
					}
				}
			}break;
			case MSG_KICK:{
				if(dedicatedserver.active && senderaddr.sin_addr.s_addr == inet_addr(dedicatedserver.lobbyaddress)){
					Uint32 accountid;
					data.Get(accountid);
					for(int i = 0; i < maxpeers; i++){
						Peer * p = peerlist[i];
						if(p && p->accountid == accountid){
							if(gameplaystate == World::INGAME){
								KillByGovt(*p);
							}
							HandleDisconnect(p->id, true);
							break;
						}
					}
				}
			}break;
		}
	}
	if(!replay.IsPlaying()){
		Uint32 tickcheck = SDL_GetTicks();
		for(int i = 0; i < maxpeers; i++){
			if(peerlist[i]){
				if(i != localpeerid && !peerlist[i]->isbot && !peerlist[i]->disconnected && peerlist[i]->lastpacket < tickcheck && tickcheck - peerlist[i]->lastpacket >= peertimeout){
					HandleDisconnect(i);
				}
			}
		}
	}
}

void World::DoNetwork_Replica(void){
	if(!peerlist[authoritypeer]){
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
		if(peerlist[authoritypeer]->ip == ntohl(senderaddr.sin_addr.s_addr) && peerlist[authoritypeer]->port == ntohs(senderaddr.sin_port)){
			peer = peerlist[authoritypeer];
			peer->lastpacket = SDL_GetTicks();
		}
		data.offset = received * 8;
		data.readoffset = 0;
		totalbytesread += received;
		char code;
		data.Get(code);
		switch(code){
			case MSG_CONNECT:{ // connect response
				//printf("MSG_CONNECT response received from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
				if(peer){
					if(data.GetBit()){
						data.Get(localpeerid);
						viewedpeerid = localpeerid;
						//printf("we are connected, our peer id is %d\n", localpeerid);
						RequestPeerList();
					}else{
						//printf("failed to connect to game\n");
						// host not accepting connection, password is wrong, or teams are full, etc
						Disconnect();
					}
				}
			}break;
			case MSG_SNAPSHOT:{ // snapshot data
				if(peer){
					if(state == CONNECTED){
						totalsnapshots++;
						Serializer * snapshotcopy = new Serializer;
						snapshotcopy->Copy(data);
						snapshotqueue.push_back(snapshotcopy);
					}
				}
			}break;
			case MSG_PEERLIST:{ // peerlist update
				if(peer){
					ReadPeerList(data);
					// The MSG_CONNECT response (3 bytes) is often dropped by
					// carrier-grade NAT. Fall back to self-identifying by
					// accountid: our peer in the authority's peerlist has
					// accountid == lobby.accountid and id != authoritypeer.
					if(localpeerid == authoritypeer){
						for(unsigned int i = 0; i < maxpeers; i++){
							if(i == authoritypeer) continue;
							if(peerlist[i] && peerlist[i]->accountid == lobby.accountid){
								localpeerid = i;
								viewedpeerid = i;
								break;
							}
						}
					}
					Peer * localpeer = peerlist[localpeerid];
					if(localpeer && localpeer->ishost && !GetAuthorityPeer()->gameinfoloaded && gameinfo.loaded){
						SendGameInfo(GetAuthorityPeer()->id);
						//printf("We are host, sending game info\n");
					}
					if(state == CONNECTING && localpeer){
						state = CONNECTED;
					}
				}
			}break;
			case MSG_DISCONNECT:{ // disconnect
				if(peer){
					HandleDisconnect(authoritypeer);
				}
			}break;
			case MSG_PING:{ // ping
				//printf("received ping from %s:%d\n", inet_ntoa(senderaddr.sin_addr), ntohs(senderaddr.sin_port));
			}break;
			case MSG_PONG:{ // pong
				//printf("received MSG_PONG\n");
				Uint32 pingid;
				data.Get(pingid);
				if(pingid == lastpingid){
					pingtime = SDL_GetTicks() - lastpingsent;
					pinghistory[lastpingid % (sizeof(pinghistory) / sizeof(int))] = pingtime;
				}
			}break;
			case MSG_GAMEINFO:{
				if(peer){
					//printf("Received MSG_GAMEINFO\n");
					gameinfo.Serialize(Serializer::READ, data);
					//printf("password %s\n", gameinfo.password);
					SendGameInfoLoaded();
					/*Serializer response;
					Uint8 code = MSG_GAMEINFO;
					response.Put(code);
					SendPacket(GetAuthorityPeer(), response.data, response.BitsToBytes(response.offset));*/
				}
			}break;
			case MSG_CHAT:{
				Uint32 accountid;
				data.Get(accountid);
				DisplayChatMessage(accountid, &data.data[1 + 4]);
				/*std::string chatmsg(lobby.GetUserInfo(accountid)->name);
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
			case MSG_STATUS:{
				int size = data.BitsToBytes(data.offset - data.readoffset);
				char * newstatus = new char[size];
				memcpy(newstatus, &data.data[data.BitsToBytes(data.readoffset)], size);
				PushStatusString(newstatus);
			}break;
			case MSG_MESSAGE:{
				char message[1024];
				strcpy(message, &data.data[data.BitsToBytes(data.readoffset)]);
				Uint8 time = data.data[data.BitsToBytes(data.readoffset) + strlen(message) + 1];
				Uint8 type = data.data[data.BitsToBytes(data.readoffset) + strlen(message) + 1 + 1];
				//printf("GOT MSG_MESSAGE %s %d %d\n", message, time, type);
				ShowMessage(message, time, type);
			}break;
			case MSG_STATS:{
				if(peer){
					Peer * localpeer = peerlist[localpeerid];
					if(localpeer){
						localpeer->stats.Serialize(Serializer::READ, data);
						//printf("MSG_STATS received\n");
						Team * team = GetPeerTeam(localpeer->id);
						if(team){
							User * user = lobby.GetUserInfo(localpeer->accountid);
							char namecopy[64];
							strcpy(namecopy, user->name);
							lobby.ForgetUserInfo(localpeer->accountid);
							user = lobby.GetUserInfo(localpeer->accountid);
							strcpy(user->name, namecopy);
							user->statscopy = localpeer->stats;
							user->statsagency = team->agency;
							user->teamnumber = team->number;
						}

					}
				}
			}break;
			case MSG_GOVTKILL:{
				if(peer){
					Uint8 peerid = data.data[data.BitsToBytes(data.readoffset)];
					Player * player = GetPeerPlayer(peerid);
					if(player){
						player->KillByGovt(*this);
					}
				}
			}break;
			case MSG_SOUND:{
				if(peer){
					Uint8 volume = data.data[data.BitsToBytes(data.readoffset)];
					char * name = &data.data[data.BitsToBytes(data.readoffset) + 1];
					Audio::GetInstance().Play(resources.soundbank[name], volume);
					//printf("MSG_SOUND %d %s\n", volume, name);
				}
			}break;
			case MSG_REMOVE:{
				if(peer){
					while(data.MoreBytesToRead()){
						Uint16 objectid;
						data.Get(objectid);
						Object * object = GetObjectFromId(objectid);
						if(object){
							MarkDestroyObject(objectid);
							//printf("DELETED %d\n", objectid);
						}
					}
				}
			}break;
			case MSG_MAP:{
				//printf("MSG_MAP received\n");
				if(peer){
					Uint8 subcode;
					data.Get(subcode);
					//printf("%d\n", subcode);
					switch(subcode){
						case MAP_GETCHUNK:{
							Uint32 offset;
							data.Get(offset);
							PutMapChunk(offset, *peer);
						}break;
						case MAP_PUTCHUNK:{
							Uint32 offset;
							data.Get(offset);
							Uint32 size;
							data.Get(size);
							StoreMapChunk((unsigned char *)&data.data[data.BitsToBytes(data.readoffset)], offset, size);
						}break;
					}
				}
			}break;
			case MSG_TRIGGER_STATE:{
				if(peer){
					triggerGraph.ApplySerializedState(data);
				}
			}break;
			case MSG_CAMERA:{
				Sint16 cx, cy;
				memcpy(&cx, &data.data[data.BitsToBytes(data.readoffset)],     sizeof(cx));
				memcpy(&cy, &data.data[data.BitsToBytes(data.readoffset) + 2], sizeof(cy));
				if(cx != 0 || cy != 0){
					pancameraactive = true;
					pancamerareturn = false;
					pancamerax = cx;
					pancameray = cy;
				} else {
					pancameraactive = false;
					pancamerareturn = true;
					pancamerareturncount = GASLoader::Get().gameengine.ticksPerSecond * 3; // 3s return window
				}
			}break;
		}
	}
	Uint32 tickcheck = SDL_GetTicks();
	if(peerlist[authoritypeer]){
		if(peerlist[authoritypeer]->lastpacket < tickcheck && tickcheck - peerlist[authoritypeer]->lastpacket >= peertimeout){
			HandleDisconnect(authoritypeer);
		}
	}
}

void World::SendPacket(Peer * peer, char * data, unsigned int size){
	if(replay.IsPlaying()){
		return;
	}
	if(peer){
		if(lagsimulator.Active() && gameplaystate == INGAME){
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

void World::SwitchToMode(bool newmode){
	if(newmode == REPLICA && mode == AUTHORITY){
		mode = REPLICA;
		for(unsigned int i = 0; i < maxpeers; i++){
			if(peerlist[i]){
				authoritypeer = i;
				break;
			}
		}
	}else
	if(newmode == AUTHORITY && mode == REPLICA){
		mode = AUTHORITY;
		authoritypeer = localpeerid;
		for(unsigned int i = 0; i < maxpeers; i++){
			if(peerlist[i]){
				peerlist[i]->lastpacket = SDL_GetTicks();
			}
		}
		Listen(GetAuthorityPeer()->port);
		SendPeerList();
	}
}

bool World::Listen(unsigned short port){
	if(!boundport){
		if(!Bind(port)){
			return false;
		}
	}
	AllocateMapData(65535);
	SwitchToMode(AUTHORITY);
	state = LISTENING;
	Peer * authoritypeerptr = GetAuthorityPeer();
	authoritypeerptr->ip = INADDR_ANY;
	authoritypeerptr->port = boundport;
	//printf("Listening on port %d\n", boundport);
	return true;
}

unsigned short World::Bind(unsigned short port){
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

void World::Connect(Uint8 agency, Uint32 accountid, const char * password, bool observer){
	AllocateMapData(65535);
	sockaddr_in addr;
	addr.sin_addr.s_addr = htonl(GetAuthorityPeer()->ip);
	//printf("sending connect request with agency %d, accountid %d to %s:%d\n", agency, accountid, inet_ntoa(addr.sin_addr), GetAuthorityPeer()->port);
	SwitchToMode(REPLICA);
	state = CONNECTING;
	messagetype = 0;
	authoritypeer = 0;
	GetAuthorityPeer()->lastpacket = SDL_GetTicks();
	Serializer data;
	Uint8 code = MSG_CONNECT;
	data.Put(code);
	data.Put(agency);
	data.Put(accountid);
	Uint8 passwordsize = password ? strlen(password) : 0;
	data.Put(passwordsize);
	for(int i = 0; i < passwordsize; i++){
		data.Put(password[i]);
	}
	data.PutBit(observer);
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

void World::Disconnect(void){
	ClearSnapshotQueue();
	ClearMapData();
	state = IDLE;
	viewedpeerid = 0;
	spectator.freecam = false;
	spectator.camx = 0;
	spectator.camy = 0;
	spectator.camvx = 0;
	spectator.camvy = 0;
	spectator.holdshowallnames = false;
	spectator.initialized = false;
	char data[1];
	data[0] = MSG_DISCONNECT;
	if(mode == AUTHORITY){
		for(int i = 0; i < maxpeers; i++){
			Peer * peer = peerlist[i];
			if(peer){
				SendPacket(peer, data, sizeof(data));
			}
		}
	}else
	if(mode == REPLICA){
		SendPacket(GetAuthorityPeer(), data, sizeof(data));
		HandleDisconnect(GetAuthorityPeer()->id);
	}
}

void World::SwitchToLocalAuthorityMode(void){
	mode = AUTHORITY;
	for(int i = 0; i < maxpeers; i++){
		if(peerlist[i]){
			delete peerlist[i];
			peerlist[i] = 0;
		}
	}
	peercount = 0;
	authoritypeer = GetAuthorityPeer()->id;
	localpeerid = authoritypeer;
	viewedpeerid = localpeerid;
}

bool World::IsAuthority(void){
	return mode == AUTHORITY;
}

bool World::IsLocalObserver(void){
	Peer * lp = peerlist[localpeerid];
	return lp && lp->observer;
}

bool World::IsConnected() const {
	return state == CONNECTED;
}

bool World::IsIdle() const {
	return state == IDLE;
}

void World::SendPing(void){
	Serializer data;
	Uint8 code = MSG_PING;
	data.Put(code);
	lastpingid++;
	data.Put(lastpingid);
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
	lastpingsent = SDL_GetTicks();
}

int World::GetPingTime(void){
	if(state == CONNECTED){
		return pingtime;
	}else{
		return 0;
	}
}

int World::AveragePingJitter(void){
	int average = 0;
	for(int i = 0; i < sizeof(pinghistory) / sizeof(int); i++){
		int jitter = abs(pinghistory[i] - pinghistory[(i - 1) % sizeof(pinghistory) / sizeof(int)]);
		average += jitter;
	}
	average /= sizeof(pinghistory) / sizeof(int);
	return average;
}

