#include "replay.h"
#include "world.h"
#include "user.h"
#include "player.h"

Replay::Replay(){
	file = 0;
	isrecording = false;
	isplaying = false;
	uniqueport = 0;
	inputsize = 0;
	gamestarted = false;
	showallnames = false;
	x = 0;
	y = 0;
	oldx = 0;
	oldy = 0;
	xv = 0;
	yv = 0;
	speed = 1;
	ffmpeg = 0;
	ffmpegvideo = true;
	tick = 0;
}

Replay::~Replay(){
	EndRecording();
	EndPlaying();
}

void Replay::BeginRecording(const char * filename){
	CDDataDir();
	file = SDL_IOFromFile(filename, "wb");
	if(file){
		isrecording = true;
	}
}

void Replay::EndRecording(void){
	if(file){
		SDL_CloseIO(file);
	}
	file = 0;
	isrecording = false;
}

void Replay::BeginPlaying(const char * filename, const char * outfilename, bool video){
	if(!filename){
		return;
	}
	CDDataDir();
	file = SDL_IOFromFile(filename, "rb");
	if(file){
		isplaying = true;
	}
#ifdef POSIX
	if(outfilename){
		ffmpegvideo = video;
		if(video){
			char cmd[256];
			sprintf(cmd, "ffmpeg -v debug -y -f rawvideo -vcodec rawvideo -s 640x480 -pix_fmt rgb24 -r 50 -i pipe: -an -vcodec libx264 -preset veryslow -qp 0 %s", outfilename);
			ffmpeg = popen(cmd, "w");
			tick = 0;
		}else{
			char cmd[256];
			sprintf(cmd, "ffmpeg -v debug -y -f s16le -ar 44100 -ac 1 -acodec pcm_s16le -i pipe: -vn -acodec libvo_aacenc %s", outfilename);
			ffmpeg = popen(cmd, "w");
		}
	}
#endif
	gamestarted = false;
}

void Replay::EndPlaying(void){
	if(file){
		SDL_CloseIO(file);
	}
	file = 0;
#ifdef POSIX
	if(ffmpeg){
		pclose(ffmpeg);
	}
	ffmpeg = 0;
#endif
	isplaying = false;
}

void Replay::WriteHeader(World & world){
	Serializer data;
	for(int i = 0; i < strlen(world.version) + 1; i++){
		SDL_WriteIO(file, &world.version[i], 1);
	}
	data.Put(world.randomseed);
	data.Put(world.tickcount);
	world.gameinfo.Serialize(Serializer::WRITE, data);
	Uint32 datasize = data.BitsToBytes(data.offset);
	SDL_WriteIO(file, &datasize, sizeof(datasize));
	SDL_WriteIO(file, data.data, data.BitsToBytes(data.offset));
}

bool Replay::ReadHeader(World & world){
	Uint8 byte;
	int i = 0;
	int read;
	char versionstring[16 + 1];
	memset(versionstring, 0, sizeof(versionstring));
	do{
		read = SDL_ReadIO(file, &byte, 1);
		versionstring[i++] = byte;
	}while(read && byte && i < 16);
	if(strcmp(versionstring, world.version) != 0){
		return false;
	}
	Serializer data;
	Uint32 datasize = 0;
	SDL_ReadIO(file, &datasize, sizeof(datasize));
	if(datasize > data.size){
		return false;
	}
	read = SDL_ReadIO(file, data.data, datasize);
	if(read == 0){
		return false;
	}
	data.offset = datasize * 8;
	data.Get(world.randomseed);
	data.Get(world.tickcount);
	world.gameinfo.Serialize(Serializer::READ, data);
	return true;
}

bool Replay::ReadToNextTick(World & world){
	Uint8 code;
	do{
		int read = SDL_ReadIO(file, &code, 1);
		if(!read){
			return false;
		}
		switch(code){
			case RPL_GAMEINFO:{
				printf("RPL_GAMEINFO\n");
				Uint32 gameinfosize;
				SDL_ReadIO(file, &gameinfosize, sizeof(gameinfosize));
				Serializer data;
				SDL_ReadIO(file, data.data, gameinfosize);
				data.offset = gameinfosize * 8;
				world.gameinfo.Serialize(Serializer::READ, data);
			}break;
			case RPL_NEWPEER:{
				printf("RPL_NEWPEER\n");
				Uint8 agency;
				Uint32 accountid;
				SDL_ReadIO(file, &agency, 1);
				SDL_ReadIO(file, &accountid, sizeof(accountid));
				world.AddPeer((char *)"local", uniqueport++, agency, accountid);
			}break;
			case RPL_START:{
				printf("RPL_START\n");
				gamestarted = true;
			}break;
			case RPL_USERINFO:{
				printf("RPL_USERINFO\n");
				Uint32 size;
				SDL_ReadIO(file, &size, sizeof(size));
				Serializer data;
				SDL_ReadIO(file, data.data, data.BitsToBytes(size));
				data.offset = size;
				User user;
				user.Serialize(Serializer::READ, data);
				User * userp = world.lobby.GetUserInfo(user.accountid);
				data.readoffset = 0;
				userp->Serialize(Serializer::READ, data);
				userp->retrieving = false;
			}break;
			case RPL_CHANGETEAM:{
				printf("RPL_CHANGETEAM\n");
				Uint8 peerid;
				SDL_ReadIO(file, &peerid, 1);
				world.ChangeTeam(peerid);
			}break;
			case RPL_TECH:{
				printf("RPL_TECH\n");
				Uint8 peerid;
				Uint32 techchoices;
				SDL_ReadIO(file, &peerid, 1);
				SDL_ReadIO(file, &techchoices, sizeof(techchoices));
				world.SetTech(peerid, techchoices);
			}break;
			case RPL_CHAT:{
				printf("RPL_CHAT\n");
				Uint8 peerid;
				Uint8 to;
				Uint8 msgsize;
				char msg[256];
				SDL_ReadIO(file, &peerid, 1);
				SDL_ReadIO(file, &to, 1);
				SDL_ReadIO(file, &msgsize, 1);
				SDL_ReadIO(file, msg, msgsize);
				msg[msgsize] = 0;
				Peer * peer = world.peerlist[peerid];
				if(peer){
					if(to == 0 || world.GetPeerTeam(peer->id) == world.GetPeerTeam(world.localpeerid)){
						world.DisplayChatMessage(peer->accountid, msg);
					}
				}
			}break;
			case RPL_STATION:{
				printf("RPL_STATION\n");
				Uint8 peerid;
				Uint8 action;
				Uint8 itemid;
				SDL_ReadIO(file, &peerid, 1);
				SDL_ReadIO(file, &action, 1);
				SDL_ReadIO(file, &itemid, 1);
				Peer * peer = world.peerlist[peerid];
				if(peer){
					Player * player = world.GetPeerPlayer(peer->id);
					if(player){
						switch(action){
							case STA_BUY: player->BuyItem(world, itemid); break;
							case STA_REPAIR: player->RepairItem(world, itemid); break;
							case STA_VIRUS: player->VirusItem(world, itemid); break;
						}
					}
				}
			}break;
			case RPL_INPUT:{
				//printf("RPL_INPUT\n");
				Uint8 peerid;
				SDL_ReadIO(file, &peerid, 1);
				Serializer * data = new Serializer;
				SDL_ReadIO(file, data->data, GetInputSize());
				data->offset = GetInputSize() * 8;
				world.inputqueue[peerid].push_back(data);
			}break;
			case RPL_DISCONNECT:{
				printf("RPL_DISCONNECT\n");
				Uint8 peerid;
				SDL_ReadIO(file, &peerid, 1);
				world.HandleDisconnect(peerid);
			}break;
			case RPL_TICK:{
				//printf("RPL_TICK\n");
			}break;
			default:{
				printf("Unknown replay code: %d\n", code);
				return false;
			}break;
		}
	}while(code != RPL_TICK);
	return true;
}

void Replay::WriteGameInfo(LobbyGame & gameinfo){
	Uint8 code = RPL_GAMEINFO;
	SDL_WriteIO(file, &code, 1);
	Serializer data;
	gameinfo.Serialize(Serializer::WRITE, data);
	Uint32 gameinfosize = data.BitsToBytes(data.offset);
	SDL_WriteIO(file, &gameinfosize, sizeof(gameinfosize));
	SDL_WriteIO(file, data.data, data.BitsToBytes(data.offset));
}

void Replay::WriteNewPeer(Uint8 agency, Uint32 accountid){
	Uint8 code = RPL_NEWPEER;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &agency, 1);
	SDL_WriteIO(file, &accountid, sizeof(accountid));
}

void Replay::WriteStart(void){
	Uint8 code = RPL_START;
	SDL_WriteIO(file, &code, 1);
}

void Replay::WriteUserInfo(User & user){
	Uint8 code = RPL_USERINFO;
	SDL_WriteIO(file, &code, 1);
	Serializer data;
	user.Serialize(Serializer::WRITE, data);
	Uint32 size = data.offset;
	SDL_WriteIO(file, &size, sizeof(size));
	SDL_WriteIO(file, data.data, data.BitsToBytes(data.offset));
}

void Replay::WriteChangeTeam(Uint8 peerid){
	Uint8 code = RPL_CHANGETEAM;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
}

void Replay::WriteSetTech(Uint8 peerid, Uint32 techchoices){
	Uint8 code = RPL_TECH;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
	SDL_WriteIO(file, &techchoices, sizeof(techchoices));
}

void Replay::WriteChat(Uint8 peerid, Uint8 to, char * msg){
	Uint8 code = RPL_CHAT;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
	SDL_WriteIO(file, &to, 1);
	Uint8 msgsize = strlen(msg);
	SDL_WriteIO(file, &msgsize, 1);
	SDL_WriteIO(file, msg, msgsize);
}

void Replay::WriteStation(Uint8 peerid, Uint8 action, Uint8 itemid){
	Uint8 code = RPL_STATION;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
	SDL_WriteIO(file, &action, 1);
	SDL_WriteIO(file, &itemid, 1);
}

void Replay::WriteInputCommand(World & world, Uint8 peerid, Serializer & data){
	Uint8 code = RPL_INPUT;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
	int dataoffset = data.BitsToBytes(data.readoffset);
	SDL_WriteIO(file, &data.data[dataoffset], data.BitsToBytes(data.offset) - dataoffset);
}

void Replay::WriteDisconnect(Uint8 peerid){
	Uint8 code = RPL_DISCONNECT;
	SDL_WriteIO(file, &code, 1);
	SDL_WriteIO(file, &peerid, 1);
}

void Replay::WriteTick(void){
	Uint8 code = RPL_TICK;
	SDL_WriteIO(file, &code, 1);
}

bool Replay::IsRecording(void){
	return isrecording;
}

bool Replay::IsPlaying(void){
	return isplaying;
}

bool Replay::GameStarted(void){
	return gamestarted;
}

bool Replay::ShowAllNames(void){
	return showallnames;
}

int Replay::GetInputSize(void){
	if(!inputsize){
		Input input;
		input.mousex = 0xFFFF;
		input.mousey = 0xFFFF;
		Serializer data;
		input.Serialize(Serializer::WRITE, data);
		inputsize = sizeof(Uint32)/*tickcount*/ + sizeof(Uint32)/*lasttick*/ + data.BitsToBytes(data.offset);
	}
	return inputsize;
}