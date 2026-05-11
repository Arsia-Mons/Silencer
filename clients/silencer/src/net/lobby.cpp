#include "lobby.h"
#include "sha1.h"
#include "world.h"

#ifdef __EMSCRIPTEN__
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#endif

// MSG_VERSION request now carries a trailing platform byte so the lobby
// can pick the right download URL when it rejects us. Values mirror
// server/protocol.go::Platform.
#if defined(__APPLE__)
static const uint8_t kUpdaterPlatform = 1; // macos_arm64
#elif defined(_WIN32)
static const uint8_t kUpdaterPlatform = 2; // windows_x64
#else
static const uint8_t kUpdaterPlatform = 0; // unknown — lobby will send bare reject
#endif

Lobby::Lobby(World * world){
	Lobby::world = world;
	msgsize = 0;
	he = 0;
	resolvehost[0] = 0;
	mutex = SDL_CreateMutex();
	resolvethread = nullptr;
	resolvethreadrunning = false;
	state = WAITING;
	accountid = 0;
	sendbuffersize = 0;
	creategamestatus = 0;
	connectgamestatus = 0;
	sendbufferoffset = 0;
	gamesprocessed = false;
	presencechanged = false;
	channelchanged = false;
	channel[0] = 0;
	lastchannel[0] = 0;
	serverip[0] = 0;
	sockethandle = -1;
	statupgraded = false;
	updateavailable = false;
	updateurl.clear();
	memset(updatesha256, 0, sizeof(updatesha256));
	localusername[0] = '\0';
}

void Lobby::SetLocalUsername(const char * name){
	if(!name) return;
	strncpy(localusername, name, sizeof(localusername) - 1);
	localusername[sizeof(localusername) - 1] = '\0';
}

Lobby::~Lobby(){
	LockMutex();
	Disconnect();
	ClearGames();
	userinfos.clear();
	UnlockMutex();
	if(resolvethread){
		SDL_WaitThread(resolvethread, nullptr);
		resolvethread = nullptr;
	}
	SDL_DestroyMutex(mutex);
}

void Lobby::Connect(const char * host, unsigned short port){
#ifdef __EMSCRIPTEN__
	// Browser builds talk to the lobby's spectator facade (Stage 3) over
	// WebSocket instead of the binary TCP protocol. The facade exposes
	// the read-only game list + a `spectate` command and is anonymous —
	// no opVersion handshake, no opAuth, no channel join. We fast-forward
	// the state machine all the way to AUTHENTICATED and let the lobby
	// browser UI render straight from the JSON pushes we'll receive in
	// WasmDoNetwork.
	//
	// The port we got from `host`/`port` is the native TCP port (517 / 15170)
	// — the facade listens on its own port (default :15173). We assume
	// the operator co-locates them on the same host; in practice the
	// /spectate page passes the right hostname and we pick :15173 here.
	motdreceived = true;
	versionchecked = true;
	versionok = true;
	updateavailable = false;
	motd[0] = 0;
	failmessage[0] = 0;
	char url[512];
	const unsigned short facadePort = 15173;
	snprintf(url, sizeof(url), "ws://%s:%u/spectate", host, (unsigned)facadePort);
	(void)port;
	fprintf(stderr, "[lobby] (wasm) connecting to facade %s\n", url);
	facadeWS.Close();
	if(!facadeWS.Open(url)){
		fprintf(stderr, "[lobby] (wasm) facade open failed\n");
		state = CONNECTIONFAILED;
		return;
	}
	// Pretend we authed. The lobby browser UI just needs `games`
	// populated and `state == AUTHENTICATED` to render rows.
	state = AUTHENTICATED;
	accountid = 1; // bot-range placeholder; spectator has no real identity
	gamesprocessed = false;
	return;
#else
	LockMutex();
	motdreceived = false;
	versionchecked = false;
	motd[0] = 0;
	if(sockethandle == -1){
		sockethandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	unsigned long iomode = 1;
    // set to nonblocking
    ioctl(sockethandle, FIONBIO, &iomode);
	failmessage[0] = 0;
	if(strcmp(resolvehost, host) != 0){
		he = 0;
	}
	lasttime = SDL_GetTicks();
	if(strlen(serverip) > 0){
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(serverip);//*(in_addr *)(he->h_addr_list[0]);
		connect(sockethandle, (sockaddr *)&addr, sizeof(addr));
		//printf("%d\n", errno);
		state = CONNECTING;
	}else{
		state = RESOLVING;
		ResolveHostname(host);
	}
	UnlockMutex();
#endif
}

void Lobby::Disconnect(void){
	if(state == CONNECTING || state == RESOLVED || state == CONNECTIONFAILED){
		state = CONNECTIONFAILED;
	}else{
		state = DISCONNECTED;
	}
	sendbuffersize = 0;
	sendbufferoffset = 0;
	presence.clear();
	presencechanged = true;
#ifdef __EMSCRIPTEN__
	facadeWS.Close();
#else
    shutdown(sockethandle, SHUT_RDWR);
    closesocket(sockethandle);
	sockethandle = -1;
#endif
}

void Lobby::DoNetwork(void){
#ifdef __EMSCRIPTEN__
	WasmDoNetwork();
	return;
#endif
	if(state == IDLE || state == WAITING || sockethandle == -1){
		return;
	}
	LockMutex();
	fd_set readset;
	fd_set writeset;
	int result;
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if(state == CONNECTING){
		Uint32 newtick = SDL_GetTicks();
		if(newtick > lasttime && newtick - lasttime > 5000){
			state = CONNECTIONFAILED;
		}
	}
	if(state == CONNECTED || state == AUTHENTICATING || state == AUTHENTICATED){
		Uint32 newtick = SDL_GetTicks();
		if(newtick > lasttime && newtick - lasttime > 20000){
			Disconnect();
		}
	}
	do{
		result = 0;
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		if(sockethandle != -1 && (state != CONNECTING && state != RESOLVED && state != RESOLVING)){
			FD_SET(sockethandle, &readset);
		}
		if(state == CONNECTING || sendbufferoffset > 0){
			FD_SET(sockethandle, &writeset);
		}
		if(state != DISCONNECTED && state != RESOLVEFAILED){
			result = select(sockethandle + 1, &readset, &writeset, NULL, &timeout);
			if(FD_ISSET(sockethandle, &writeset)){
				if(sendbuffersize > 0){
					int ret = send(sockethandle, sendbuffer, sendbuffersize, 0);
					if(ret > 0){
						sendbuffersize -= ret;
						memcpy(sendbuffer, &sendbuffer[ret], sendbuffersize);
						sendbufferoffset -= ret;
					}
				}else{
					if(state == CONNECTING){
						state = CONNECTED;
						//unsigned short port = world->Bind();
						//printf("Bound to port %d\n", port);
					}
				}
			}
			if(FD_ISSET(sockethandle, &readset)){
				lasttime = SDL_GetTicks();
				if(!msgsize){
					int ret = recv(sockethandle, (char *)&msgsize, sizeof(msgsize), 0);
					if(ret > 0){
						msgoffset = 0;
					}else
					if(ret == 0){
						Disconnect();
					}else
					if(ret == -1){
						//printf("errno: %d\n", errno);
						if(errno == ECONNREFUSED){
							state = CONNECTIONFAILED;
							Disconnect();
						}else{
							Disconnect();
						}
					}
				}else{
					int ret = recv(sockethandle, &msg[msgoffset], msgsize - msgoffset, 0);
					if(ret > 0){
						msgoffset += ret;
						if(msgoffset == msgsize){
							Serializer data(msg, msgsize);
							Uint8 code;
							data.Get(code);
							switch(code){
								case MSG_AUTH:{
									Uint8 status;
									data.Get(status);
									if(status){
										state = AUTHENTICATED;
										data.Get(accountid);
										ForgetUserInfo(accountid);
										GetUserInfo(accountid);
									}else{
										state = AUTHFAILED;
										strcpy(failmessage, (char *)&msg[2]);
									}
								}break;
								case MSG_MOTD:{
									Uint8 status;
									data.Get(status);
									if(status){
										strcat(motd, (char *)&msg[1]);
									}else{
										motdreceived = true;
									}
								}break;
								case MSG_CHAT:{
									char * channel = (char *)&msg[1];
									char * message = (char *)&msg[1 + strlen(channel) + 1];
									std::vector<char> newmessage(strlen(message) + 1 + 2);
									newmessage[0] = 0;
									memcpy(newmessage.data(), message, strlen(message) + 1 + 2);
									chatmessages.push_back(newmessage);
								}break;
								case MSG_NEWGAME:{
									Uint8 creategamestatus;
									data.Get(creategamestatus);
									//printf("MSG_NEWGAME received %d %d\n", accountid, creategamestatus);
									LobbyGame * lobbygame = new LobbyGame;
									lobbygame->createdtime = SDL_GetTicks();
									lobbygame->Serialize(Serializer::READ, data);
									// Trailing can-rejoin byte the lobby derives per recipient
									// (set when our accountid matches one of the dedicated server's
									// parked peers — signals that "Join" on an INGAME row will rebind
									// our preserved slot via PR #152's rejoin path).
									{
										unsigned int consumed_bits = data.readoffset;
										unsigned int total_bits = data.offset;
										unsigned int remaining_bytes = (total_bits > consumed_bits)
											? (total_bits - consumed_bits) / 8
											: 0;
										if(remaining_bytes >= 1){
											Uint8 canrejoinflag;
											data.Get(canrejoinflag);
											lobbygame->canrejoin = (canrejoinflag != 0);
										}
									}
									//printf("password: %s\n", lobbygame->password);
									for(std::list<LobbyGame *>::iterator dit = games.begin(); dit != games.end(); ){
										if((*dit)->id == lobbygame->id){
											delete *dit;
											dit = games.erase(dit);
										}else{
											++dit;
										}
									}
									games.push_back(lobbygame);
									gamesprocessed = false;
									presencechanged = true;
									if(lobbygame->accountid == Lobby::accountid){ // CreateGame response
										Lobby::creategamestatus = creategamestatus;
										if(creategamestatus == 1){ // success
											createdgameid = lobbygame->id;
											//char * info = (char *)&msg[1 + sizeof(accountid) + sizeof(creategamestatus)];
											//lobbygame->accountid = accountid;
											//lobbygame->LoadInfo(info);
										}else{ // failure
											games.pop_back();
											delete lobbygame;
										}
									}
								}break;
								case MSG_DELGAME:{
									Uint32 id;// = *(Uint32 *)((char *)&msg[1]);
									data.Get(id);
									for(std::list<LobbyGame *>::iterator it = games.begin(); it != games.end(); it++){
										LobbyGame * lobbygame = *it;
										if(lobbygame->id == id/* && (SDL_GetTicks() - lobbygame->createdtime > 3000)*/){
											delete lobbygame;
											games.erase(it);
											gamesprocessed = false;
											presencechanged = true;
											break;
										}
									}
								}break;
								case MSG_CHANNEL:{
									char * name = (char *)&msg[1];
									strcpy(channel, name);
									channelchanged = true;
								}break;
								case MSG_CONNECT:{
									/*data.Get(connectgamestatus);
									switch(connectgamestatus){
										case 1:{ // game is open
											Uint8 hostsize;
											data.Get(hostsize);
											char hostname[255];
											memset(hostname, 0, sizeof(hostname));
											for(int i = 0; i < hostsize; i++){
												data.Get(hostname[i]);
											}
											char * host = strtok(hostname, ",");
											char * portstr = strtok(NULL, ",");
											unsigned short port = atoi(portstr);
											world->Connect(selectedagency, accountid);
										}break;
										case 2:{ // could not join game
											
										}break;
									}*/
									
									/*Uint8 hostsize;
									data.Get(hostsize);
									char hostname[255];
									memset(hostname, 0, sizeof(hostname));
									for(int i = 0; i < hostsize; i++){
										data.Get(hostname[i]);
									}
									char * host = strtok(hostname, ",");
									char * portstr = strtok(NULL, ",");
									char * publicportstr = strtok(NULL, ",");
									unsigned short port = atoi(portstr);
									unsigned short publicport = atoi(publicportstr);
									Uint32 accountid;
									data.Get(accountid);
									Uint8 agency;
									data.Get(agency);
									printf("received lobby server connect request (%s:%d,%d) accountid: %d, agency: %d\n", host, port, publicport, accountid, agency);
									Peer * peeradded = 0;
									if(world->gameplaystate == World::INLOBBY){
										peeradded = world->AddPeer(hostname, port, publicport, agency, accountid);
									}
									Serializer response;
									Uint8 code = World::MSG_CONNECT;
									response.Put(code);
									if(peeradded){
										response.PutBit(true);
										response.Put(peeradded->id);
									}else{
										response.PutBit(false);
									}
									//int oldoffset = response.offset;
									//response.Put(port);
									sockaddr_in addr;
									addr.sin_family = AF_INET;
									addr.sin_port = htons(port);
									addr.sin_addr.s_addr = inet_addr(host);
									int ret = sendto(world->sockethandle, response.data, response.BitsToBytes(response.offset), 0, (sockaddr *)&addr, sizeof(addr));
									addr.sin_family = AF_INET;
									addr.sin_port = htons(publicport);
									addr.sin_addr.s_addr = inet_addr(host);
									//response.offset = oldoffset;
									//response.Put(publicport);
									int ret2 = sendto(world->sockethandle, response.data, response.BitsToBytes(response.offset), 0, (sockaddr *)&addr, sizeof(addr));
									printf("sent MSG_CONNECTs to client (%d:%d, %d:%d)\n", port, ret, publicport, ret2);*/
								}break;
								case MSG_VERSION:{
									Uint8 success;
									data.Get(success);
									versionchecked = true;
									versionok = (success != 0);
									updateavailable = false;
									updateurl.clear();
									memset(updatesha256, 0, sizeof(updatesha256));

									if(!success){
										// New wire format carries optional url + sha256 after the success byte.
										// Old lobbies send just the success byte — detect by checking remaining bytes.
										unsigned int consumed_bits = data.readoffset;
										unsigned int total_bits = data.offset;
										unsigned int remaining_bytes = (total_bits > consumed_bits)
											? (total_bits - consumed_bits) / 8
											: 0;

										if(remaining_bytes >= 2 + 32){
											Uint16 urllen;
											data.Get(urllen);
											if(urllen > 0 && remaining_bytes >= 2u + urllen + 32u && urllen < 512){
												char urlbuf[512] = {0};
												for(Uint16 i = 0; i < urllen; i++){
													Uint8 ch;
													data.Get(ch);
													urlbuf[i] = (char)ch;
												}
												for(int i = 0; i < 32; i++){
													Uint8 ch;
													data.Get(ch);
													updatesha256[i] = ch;
												}
												updateurl.assign(urlbuf, urllen);
												updateavailable = true;
												fprintf(stderr, "[updater] MSG_VERSION reject + update: url=%s\n", urlbuf);
											} else {
												fprintf(stderr, "[updater] MSG_VERSION reject with malformed update payload (urllen=%u, remaining=%u)\n",
													(unsigned)urllen, remaining_bytes);
											}
										} else {
											fprintf(stderr, "[updater] MSG_VERSION reject, no update info (old lobby or unknown platform)\n");
										}
									}
								}break;
								case MSG_USERINFO:{
									//printf("MSG_USERINFO received\n");
									unsigned int oldreadoffset = data.readoffset;
									Uint32 accountid;
									data.Get(accountid);
									data.readoffset = oldreadoffset;
									if(!userinfos[accountid]){
										userinfos[accountid] = std::make_shared<User>();
									}
									User * user = userinfos[accountid].get();
									user->retrieving = false;
									user->Serialize(Serializer::READ, data);
									//printf("account id = %d\n", user->accountid);
									for(int i = 0; i < world->maxpeers; i++){
										Peer * peer = world->peerlist[i];
										if(peer && peer->accountid == accountid){
											world->UserInfoReceived(*peer);
										}
									}
								}break;
								case MSG_PING:{
									//printf("received MSG_PING\n");
									char msg[2];
									msg[0] = MSG_PING;
									msg[1] = 1;
									SendMessage(msg, sizeof(msg));
								}break;
								case MSG_UPGRADESTAT:{
									Uint8 oldstatsagency = GetUserInfo(accountid)->statsagency;
									ForgetUserInfo(accountid);
									GetUserInfo(accountid)->statsagency = oldstatsagency;
									statupgraded = true;
								}break;
								case MSG_PRESENCE:{
									Uint8 action;
									data.Get(action);
									Uint32 acct;
									data.Get(acct);
									Uint32 gid;
									data.Get(gid);
									Uint8 status;
									data.Get(status);
									Uint8 namelen;
									data.Get(namelen);
									if(namelen > 16){
										namelen = 16;
									}
									char name[17];
									for(Uint8 i = 0; i < namelen; i++){
										data.Get(name[i]);
									}
									name[namelen] = 0;
									if(action == 0){
										PresenceEntry & e = presence[acct];
										e.accountid = acct;
										e.gameid = gid;
										e.status = status;
										strcpy(e.name, name);
									}else{
										presence.erase(acct);
									}
									presencechanged = true;
								}break;
							}
							msgsize = 0;
						}
					}else
					if(ret == 0){
						Disconnect();
					}else
					if(ret == -1){
						Disconnect();
					}
				}
			}
		}
	}while(result == 1);
	UnlockMutex();
}

void Lobby::ResolveHostname(const char * host){
	LockMutex();
	resolvethreadrunning = false;
	strcpy(resolvehost, host);
	resolvethread = SDL_CreateThread(ResolveThreadFunc, "resolvehost", (void *)this);
	while(!resolvethreadrunning){
		// wait for thread to start and get mutex pointer;
		SDL_Delay(1);
	}
	UnlockMutex();
}

void Lobby::LockMutex(void){
	SDL_LockMutex(mutex);
}

void Lobby::UnlockMutex(void){
	SDL_UnlockMutex(mutex);
}

void Lobby::SendMessage(const char msg[256], Uint8 size){
	Send((char *)&size, sizeof(size));
	Send(msg, size);
}

void Lobby::SendVersion(void){
	memset(msg, 0, sizeof(msg));
	msg[0] = MSG_VERSION;
	strcpy((char *)&msg[1], world->version);
	size_t version_len = strlen(world->version);
	size_t platform_off = 1 + version_len + 1; // opcode + cstring + null
	msg[platform_off] = kUpdaterPlatform;
	Uint8 size = sizeof(msg[0]) + version_len + 1 + 1;
	fprintf(stderr, "[updater] sending MSG_VERSION version=%s platform=%u\n",
		world->version, (unsigned)kUpdaterPlatform);
	SendMessage(msg, size);
}

void Lobby::SendCredentials(const char * username, const char * password){
	if(strlen(username) > maxusername){
		return;
		//username[maxusername] = 0;
	}
	char msg[256];
	memset(msg, 0, sizeof(msg));
	msg[0] = MSG_AUTH;
	strcpy((char *)&msg[1], username);
	unsigned char hash[20];
	sha1::calc(password, strlen(password), hash);
	memcpy((char *)&msg[1 + strlen(username) + 1], hash, 20);
	Uint8 size = sizeof(msg[0]) + strlen(username) + 1 + sizeof(hash);
	SendMessage(msg, size);
}

void Lobby::SendChat(const char * channel, const char * message){
	if(strlen(channel) > 32){
		return;
		//channel[32] = 0;
	}
	if(strlen(message) > 256 - 1 - strlen(channel)){
		return;
		//message[256 - 1 - strlen(channel)] = 0;
	}
	char msg[256];
	memset(msg, 0, sizeof(msg));
	msg[0] = MSG_CHAT;
	strcpy((char *)&msg[1], channel);
	strcpy((char *)&msg[1 + strlen(channel) + 1], message);
	Uint8 size = sizeof(msg[0]) + strlen(channel) + 1 + strlen(message) + 1;
	SendMessage(msg, size);
}

void Lobby::JoinChannel(const char * channel){
	char msg[256];
	memset(msg, 0, sizeof(msg));
	msg[0] = MSG_CHAT;
	const char * joinstr = "/join ";
	strcpy((char *)&msg[1], Lobby::channel);
	strcpy((char *)&msg[1 + strlen(Lobby::channel) + 1], joinstr);
	strcpy((char *)&msg[1 + strlen(Lobby::channel) + 1 + strlen(joinstr)], channel);
	Uint8 size = 1 + strlen(Lobby::channel) + 1 + strlen(joinstr) + strlen(channel) + 1;
	SendMessage(msg, size);
}

void Lobby::CreateGame(const char * name, const char * map, const unsigned char maphash[20], const char * password, Uint8 securitylevel, Uint8 minlevel, Uint8 maxlevel, Uint8 maxplayers, Uint8 maxteams, bool spectatable){
	Serializer data;
	Uint8 code = MSG_NEWGAME;
	data.Put(code);
	LobbyGame lobbygame;
	strncpy(lobbygame.name, name, sizeof(lobbygame.name));
	strncpy(lobbygame.mapname, map, sizeof(lobbygame.mapname));
	if(password){
		strncpy(lobbygame.password, password, sizeof(lobbygame.password));
	}
	lobbygame.securitylevel = securitylevel;
	lobbygame.minlevel = minlevel;
	lobbygame.maxlevel = maxlevel;
	lobbygame.maxplayers = maxplayers;
	lobbygame.maxteams = maxteams;
	lobbygame.spectatable = spectatable;
	//lobbygame.CalculateMapHash();
	memcpy(lobbygame.maphash, maphash, sizeof(lobbygame.maphash));
	lobbygame.Serialize(Serializer::WRITE, data);
	SendMessage(data.data, data.BitsToBytes(data.offset));
	creategamestatus = 100;
	//printf("sent creategame %s %s %s\n", name, map, password ? password : "");
}

/*void Lobby::ConnectToGame(LobbyGame & lobbygame, Uint8 agency){
	selectedagency = agency;
	Serializer data;
	Uint8 code = MSG_CONNECT;
	data.Put(code);
	data.Put(lobbygame.accountid);
	data.Put(port);
	data.Put(publicport);
	data.Put(agency);
	world->GetAuthorityPeer()->port = lobbygame.port;
	world->GetAuthorityPeer()->publicport = lobbygame.publicport;
	world->state = World::CONNECTING;
	SendMessage(data.data, data.BitsToBytes(data.offset));
	printf("requested thru lobby server to connect to game id %d, (ports %d,%d) agency %d\n", lobbygame.accountid, port, publicport, agency);
}*/

void Lobby::SendSetGame(Uint32 gameid, Uint8 status){
	Serializer data;
	Uint8 code = MSG_SETGAME;
	data.Put(code);
	data.Put(gameid);
	data.Put(status);
	SendMessage(data.data, data.BitsToBytes(data.offset));
}

void Lobby::ClearGames(void){
	for(std::list<LobbyGame *>::iterator it = games.begin(); it != games.end(); it++){
		delete (*it);
	}
	games.clear();
}

LobbyGame * Lobby::GetGameById(Uint32 id){
	for(std::list<LobbyGame *>::iterator it = games.begin(); it != games.end(); it++){
		LobbyGame * lobbygame = (*it);
		if(lobbygame->id == id){
			return lobbygame;
		}
	}
	return 0;
}

User * Lobby::GetUserInfo(Uint32 accountid){
	User * userinfo = 0;
	if(!userinfos[accountid]){
		userinfos[accountid] = std::make_shared<User>();
		userinfo = userinfos[accountid].get();
		if(state != AUTHENTICATED){
			strcpy(userinfo->name, "Player");
		}
		userinfo->accountid = accountid;
		Serializer data;
		Uint8 code = MSG_USERINFO;
		data.Put(code);
		data.Put(accountid);
		SendMessage(data.data, data.BitsToBytes(data.offset));
		//printf("requested user info for account id %u\n", accountid);
		userinfo->retrieving = true;
		if(accountid >= 0xFFFFFFFF - 24){
			// Bot
			static const char * botnames[] = {"Sweet pea", "Jimison", "Gamenut56k", "Damien", "Quicknades", "Giblet",
				"travis", "MileyFan3", "J0hnny", "Young Watson", "melissa", "Wi11i4m",
				"iPad", "0b4m4", "stevenson", "digitalcourtney", "juan valdez", "Rebdomine",
				"willis", "spamloaf", "barnacle", "nodule", "samantha", "kyle"};
			Uint32 index = 0xFFFFFFFF - accountid;
			strcpy(userinfo->name, botnames[index]);
		}
	}else{
		userinfo = userinfos[accountid].get();
	}
	return userinfo;
}

void Lobby::ForgetUserInfo(Uint32 accountid){
	if(userinfos[accountid]){
		userinfos.erase(accountid);
	}
}

void Lobby::ForgetAllUserInfo(void){
	userinfos.clear();
}

void Lobby::UpgradeStat(Uint8 agency, Uint8 stat){
	char msg[3];
	msg[0] = MSG_UPGRADESTAT;
	msg[1] = agency;
	msg[2] = stat;
	SendMessage(msg, sizeof(msg));
}

void Lobby::RegisterStats(User & user, Uint8 won, Uint32 gameid){
	Serializer msg;
	Uint8 code = MSG_REGISTERSTATS;
	msg.Put(code);
	msg.Put(gameid);
	msg.Put(user.teamnumber);
	msg.Put(user.accountid);
	msg.Put(user.statsagency);
	msg.Put(won);
	Uint32 xp = user.statscopy.CalculateExperience();
	msg.Put(xp);
	user.statscopy.Serialize(Serializer::WRITE, msg);
	SendMessage(msg.data, msg.BitsToBytes(msg.offset));
}

bool Lobby::Send(const char * data, unsigned int size){
	int ret = send(sockethandle, data, size, 0);
	if(ret == -1){
		return false;
	}else{
		if(ret < size){
			unsigned int bytesremaining = size - ret;
			if(sendbuffersize + bytesremaining > sizeof(sendbuffer)){
				Disconnect();
				return false;
			}
			memcpy(&sendbuffer[sendbufferoffset], data, bytesremaining);
			sendbuffersize += bytesremaining;
		}
		return true;
	}
}

int Lobby::ResolveThreadFunc(void * param){
	SDL_Mutex * mutex = ((Lobby *)param)->mutex;
	char host[256];
	strcpy(host, ((Lobby *)param)->resolvehost);
	((Lobby *)param)->resolvethreadrunning = true;
	hostent * he = gethostbyname(host);
	SDL_LockMutex(mutex);
	{
		((Lobby *)param)->he = he;
		if(he && he->h_addr){
			((Lobby *)param)->state = RESOLVED;
			strcpy(((Lobby *)param)->serverip, inet_ntoa(*(in_addr *)(he->h_addr)));
		}else{
			//printf("RESOLVE FAILED: %d\n", errno);
			((Lobby *)param)->state = RESOLVEFAILED;
		}
		((Lobby *)param)->resolvethreadrunning = false;
		SDL_UnlockMutex(mutex);
	}
	return 0;
}

#ifdef __EMSCRIPTEN__

void Lobby::RequestSpectate(Uint32 gameid){
	if(!facadeWS.IsOpen()) return;
	pendingSpectateGameID = gameid;
	pendingSpectateURL.clear();
	nlohmann::json j = {{"type","spectate"},{"gameid", gameid}};
	facadeWS.SendText(j.dump());
}

void Lobby::WasmDoNetwork(){
	if(facadeWS.IsDead() && state == AUTHENTICATED){
		state = DISCONNECTED;
		return;
	}
	std::string frame;
	while(facadeWS.PopText(frame)){
		WasmHandleEnvelope(frame);
	}
}

void Lobby::WasmHandleEnvelope(const std::string &payload){
	using nlohmann::json;
	json j;
	try {
		j = json::parse(payload);
	} catch (const json::exception &e) {
		fprintf(stderr, "[lobby] (wasm) bad envelope: %s\n", e.what());
		return;
	}
	std::string type = j.value("type", std::string());

	auto decodeGame = [](const json &g, LobbyGame *out){
		out->id           = g.value("id", 0u);
		out->accountid    = g.value("account_id", 0u);
		auto cp = [&](const char *src, char *dst, size_t cap){
			std::string s = g.value(src, std::string());
			if(s.size() >= cap) s.resize(cap - 1);
			memset(dst, 0, cap);
			memcpy(dst, s.data(), s.size());
		};
		cp("name",     out->name,     sizeof(out->name));
		cp("hostname", out->hostname, sizeof(out->hostname));
		cp("map_name", out->mapname,  sizeof(out->mapname));
		out->players       = (Uint8)g.value("players", 0);
		out->state         = (Uint8)g.value("state", 0);
		out->securitylevel = (Uint8)g.value("security_level", 0);
		out->minlevel      = (Uint8)g.value("min_level", 0);
		out->maxlevel      = (Uint8)g.value("max_level", 99);
		out->maxplayers    = (Uint8)g.value("max_players", 24);
		out->maxteams      = (Uint8)g.value("max_teams", 6);
		out->extra         = (Uint8)g.value("extra", 0);
		out->spectatable   = g.value("spectatable", 0) != 0;
		out->port          = (unsigned short)g.value("port", 0);
		out->password[0]   = 0; // facade strips passwords
		out->canrejoin     = false;
		out->loaded        = true;
		// map_hash arrives hex-encoded; lobby browser UI ignores it, so we leave the bytes zeroed.
		memset(out->maphash, 0, sizeof(out->maphash));
		out->createdtime   = SDL_GetTicks();
	};

	if(type == "hello"){
		// Initial snapshot of currently-live games.
		ClearGames();
		if(j.contains("games") && j["games"].is_array()){
			for(const auto &g : j["games"]){
				LobbyGame * lg = new LobbyGame;
				decodeGame(g, lg);
				games.push_back(lg);
			}
		}
		gamesprocessed = false;
		std::string motdtext = j.value("motd", std::string());
		if(motdtext.size() >= sizeof(motd)) motdtext.resize(sizeof(motd) - 1);
		memcpy(motd, motdtext.data(), motdtext.size());
		motd[motdtext.size()] = 0;
		motdreceived = true;
	} else if(type == "newgame"){
		if(j.contains("game")){
			LobbyGame * lg = new LobbyGame;
			decodeGame(j["game"], lg);
			// Replace by id if it's already there.
			for(std::list<LobbyGame *>::iterator it = games.begin(); it != games.end(); ){
				if((*it)->id == lg->id){
					delete *it;
					it = games.erase(it);
				}else{
					++it;
				}
			}
			games.push_back(lg);
			gamesprocessed = false;
		}
	} else if(type == "delgame"){
		Uint32 id = j.value("id", 0u);
		for(std::list<LobbyGame *>::iterator it = games.begin(); it != games.end(); ++it){
			if((*it)->id == id){
				delete *it;
				games.erase(it);
				gamesprocessed = false;
				break;
			}
		}
	} else if(type == "spectate_url"){
		pendingSpectateGameID = j.value("gameid", 0u);
		pendingSpectateURL    = j.value("url", std::string());
		fprintf(stderr, "[lobby] (wasm) spectate_url game=%u url=%s\n",
		        (unsigned)pendingSpectateGameID, pendingSpectateURL.c_str());
		if(world && !pendingSpectateURL.empty()){
			world->OpenRelayWS(pendingSpectateURL.c_str());
		}
	} else if(type == "error"){
		std::string code    = j.value("code", std::string());
		std::string message = j.value("message", std::string());
		fprintf(stderr, "[lobby] (wasm) facade error: code=%s msg=%s\n",
		        code.c_str(), message.c_str());
		pendingSpectateGameID = 0;
		pendingSpectateURL.clear();
	}
}

#endif // __EMSCRIPTEN__