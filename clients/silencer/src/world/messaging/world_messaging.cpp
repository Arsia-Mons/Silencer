#include "world_messaging.h"
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

WorldMessaging::WorldMessaging(World & world) : world(world){
	message[0] = 0;
	message_i = 0;
	messagetype = 0;
	messagetime = 0;
	memset(topmessage, 0, sizeof(topmessage));
	topmessage_i = 0;
	showchat_i = 0;
}

void WorldMessaging::DisplayChatMessage(Uint32 accountid, const char * msg){
	std::string chatmsg(world.lobby.GetUserInfo(accountid)->DisplayName());
	std::replace(chatmsg.begin(), chatmsg.end(), ' ', '\xA0');
	// replace spaces with nbsp so usernames dont wordwrap
	chatmsg.append(":\xA0");
	chatmsg.append(msg);
	
	char * wrapped = silencer::ui::WordWrapText(chatmsg.c_str(), 36, "\n ");
	char * line = strtok(wrapped, "\n");
	while(line){
		chatlines.push_back(line);
		line = strtok(NULL, "\n");
	}
	delete[] wrapped;
	
	showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
	while((int)chatlines.size() > GASLoader::Get().gameengine.chatMaxLines){
		chatlines.pop_front();
	}
}

void WorldMessaging::ShowMessage(const char * message, Uint8 time, Uint8 type, bool networked, Peer * peer){
	if(networked && world.IsAuthority()){
		int msgsize = 1 + strlen(message) + 1 + 1 + 1;
		char * msg = new char[msgsize];
		msg[0] = World::MSG_MESSAGE;
		memcpy(&msg[1], message, strlen(message) + 1);
		msg[1 + strlen(message) + 1] = time;
		msg[1 + strlen(message) + 1 + 1] = type;
		if(!peer){
			for(unsigned int i = 0; i < World::maxpeers; i++){
				Peer * peer = world.peers.peerlist[i];
				if(peer && i != world.peers.localpeerid){
					world.SendPacket(peer, msg, msgsize);
				}
			}
		}else{
			world.SendPacket(peer, msg, msgsize);
		}
		delete[] msg;
	}
	if(!networked || (world.IsAuthority() && !peer) || (world.IsAuthority() && peer && peer->id == world.peers.localpeerid)){
		if(messagetype >= 10){ // Skip any messages after end of game messages, so it is not replaced
			return;
		}
		strncpy(this->message, message, sizeof(this->message) - 1);
		this->message[sizeof(this->message) - 1] = 0;
		message_i = 1;
		messagetime = time;
		messagetype = type;
	}
}

void WorldMessaging::ShowStatus(const char * status, Uint8 color, bool networked, Peer * peer){
	char * newstatus = CreateStatusString(status, color, 100);
	if(networked && world.IsAuthority()){
		int msgsize = 1 + strlen(status) + 1 + 1 + 1;
		char * msg = new char[msgsize];
		msg[0] = World::MSG_STATUS;
		memcpy(&msg[1], newstatus, strlen(status) + 1 + 1 + 1);
		if(!peer){
			for(unsigned int i = 0; i < World::maxpeers; i++){
				Peer * peer = world.peers.peerlist[i];
				if(peer && i != world.peers.localpeerid){
					world.SendPacket(peer, msg, msgsize);
				}
			}
		}else{
			world.SendPacket(peer, msg, msgsize);
		}
		delete[] msg;
	}
	if(!networked || (world.IsAuthority() && !peer) || (world.IsAuthority() && peer && peer->id == world.peers.localpeerid)){
		PushStatusString(newstatus);
	}
}

void WorldMessaging::ShowTopMessage(const char * message){
	if(world.gameplaystate == World::INGAME){
		memset(topmessage, 0, sizeof(topmessage));
		strncpy(topmessage, message, sizeof(topmessage) - 1);
		topmessage_i = 1;
	}
}

void WorldMessaging::SendChat(bool toteam, char * message){
	char msg[2 + 100 + 1];
	memset(msg, 0, sizeof(msg));
	msg[0] = World::MSG_CHAT;
	msg[1] = toteam ? 1 : 0;
	strncpy(&msg[2], message, 100);
	msg[2 + 100] = 0;
	world.SendPacket(world.GetAuthorityPeer(), msg, sizeof(msg));
}

void WorldMessaging::SendSound(const char * name, Peer * peer, Uint8 volume){
	if(world.IsAuthority()){
		if(!peer || (peer && peer->id == world.peers.localpeerid)){
			auto _r = ResolveSound(name, world.resources);
			if(_r.chunk) Audio::GetInstance().Play(_r.chunk, static_cast<int>(volume * _r.volume));
		}
		char msg[2 + 255];
		msg[0] = World::MSG_SOUND;
		msg[1] = volume;
		strcpy(&msg[2], name);
		msg[2 + strlen(name)] = 0;
		if(!peer){
			for(unsigned int i = 0; i < World::maxpeers; i++){
				Peer * peer = world.peers.peerlist[i];
				if(peer && i != world.peers.localpeerid){
					world.SendPacket(peer, msg, 2 + strlen(name) + 1);
				}
			}
		}else{
			if(peer->id != world.peers.localpeerid){
				world.SendPacket(peer, msg, 2 + strlen(name) + 1);
			}
		}
	}
}

void WorldMessaging::BroadcastTriggerState() {
    if(!world.IsAuthority()) return;
    Serializer payload;
    world.triggerGraph.SerializeState(payload);
    int msgsize = 1 + payload.offset;
    char * msg = new char[msgsize];
    msg[0] = World::MSG_TRIGGER_STATE;
    memcpy(&msg[1], payload.data, payload.offset);
    for(unsigned int i = 0; i < World::maxpeers; i++) {
        if(world.peers.peerlist[i] && i != world.peers.localpeerid) {
            world.SendPacket(world.peers.peerlist[i], msg, msgsize);
        }
    }
    delete[] msg;
}

char * WorldMessaging::CreateStatusString(const char * status, Uint8 color, Uint8 duration){
	char * newstatus = new char[strlen(status) + 1 + 1 + 1];
	strcpy(newstatus, status);
	newstatus[strlen(status) + 1] = duration;
	newstatus[strlen(status) + 2] = color;
	return newstatus;
}

void WorldMessaging::PushStatusString(char * statusstring){
	while(statusmessages.size() >= maxstatusmessages){
		delete[] statusmessages.back();
		statusmessages.pop_back();
	}
	statusmessages.push_front(statusstring);
}
