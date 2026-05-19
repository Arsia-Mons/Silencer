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

void World::DisplayChatMessage(Uint32 accountid, const char * msg){
	std::string chatmsg(lobby.GetUserInfo(accountid)->name);
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

void World::ShowMessage(const char * message, Uint8 time, Uint8 type, bool networked, Peer * peer){
	if(networked && IsAuthority()){
		int msgsize = 1 + strlen(message) + 1 + 1 + 1;
		char * msg = new char[msgsize];
		msg[0] = MSG_MESSAGE;
		memcpy(&msg[1], message, strlen(message) + 1);
		msg[1 + strlen(message) + 1] = time;
		msg[1 + strlen(message) + 1 + 1] = type;
		if(!peer){
			for(unsigned int i = 0; i < maxpeers; i++){
				Peer * peer = peerlist[i];
				if(peer && i != localpeerid){
					SendPacket(peer, msg, msgsize);
				}
			}
		}else{
			SendPacket(peer, msg, msgsize);
		}
		delete[] msg;
	}
	if(!networked || (IsAuthority() && !peer) || (IsAuthority() && peer && peer->id == localpeerid)){
		if(messagetype >= 10){ // Skip any messages after end of game messages, so it is not replaced
			return;
		}
		strncpy(World::message, message, sizeof(World::message) - 1);
		World::message[sizeof(World::message) - 1] = 0;
		message_i = 1;
		messagetime = time;
		messagetype = type;
	}
}

void World::ShowStatus(const char * status, Uint8 color, bool networked, Peer * peer){
	char * newstatus = CreateStatusString(status, color, 100);
	if(networked && IsAuthority()){
		int msgsize = 1 + strlen(status) + 1 + 1 + 1;
		char * msg = new char[msgsize];
		msg[0] = MSG_STATUS;
		memcpy(&msg[1], newstatus, strlen(status) + 1 + 1 + 1);
		if(!peer){
			for(unsigned int i = 0; i < maxpeers; i++){
				Peer * peer = peerlist[i];
				if(peer && i != localpeerid){
					SendPacket(peer, msg, msgsize);
				}
			}
		}else{
			SendPacket(peer, msg, msgsize);
		}
		delete[] msg;
	}
	if(!networked || (IsAuthority() && !peer) || (IsAuthority() && peer && peer->id == localpeerid)){
		PushStatusString(newstatus);
	}
}

void World::ShowTopMessage(const char * message){
	if(gameplaystate == INGAME){
		memset(topmessage, 0, sizeof(topmessage));
		strncpy(topmessage, message, sizeof(topmessage) - 1);
		topmessage_i = 1;
	}
}

void World::SendChat(bool toteam, char * message){
	char msg[2 + 100 + 1];
	memset(msg, 0, sizeof(msg));
	msg[0] = MSG_CHAT;
	msg[1] = toteam ? 1 : 0;
	strncpy(&msg[2], message, 100);
	msg[2 + 100] = 0;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

void World::SendSound(const char * name, Peer * peer, Uint8 volume){
	if(IsAuthority()){
		if(!peer || (peer && peer->id == localpeerid)){
			Audio::GetInstance().Play(resources.soundbank[name], volume);
		}
		char msg[2 + 255];
		msg[0] = MSG_SOUND;
		msg[1] = volume;
		strcpy(&msg[2], name);
		msg[2 + strlen(name)] = 0;
		if(!peer){
			for(unsigned int i = 0; i < maxpeers; i++){
				Peer * peer = peerlist[i];
				if(peer && i != localpeerid){
					SendPacket(peer, msg, 2 + strlen(name) + 1);
				}
			}
		}else{
			if(peer->id != localpeerid){
				SendPacket(peer, msg, 2 + strlen(name) + 1);
			}
		}
	}
}

void World::BroadcastTriggerState() {
    if (!IsAuthority()) return;
    Serializer payload;
    triggerGraph.SerializeState(payload);
    int msgsize = 1 + payload.offset;
    char * msg = new char[msgsize];
    msg[0] = MSG_TRIGGER_STATE;
    memcpy(&msg[1], payload.data, payload.offset);
    for (unsigned int i = 0; i < maxpeers; i++) {
        if (peerlist[i] && i != localpeerid) {
            SendPacket(peerlist[i], msg, msgsize);
        }
    }
    delete[] msg;
}

char * World::CreateStatusString(const char * status, Uint8 color, Uint8 duration){
	char * newstatus = new char[strlen(status) + 1 + 1 + 1];
	strcpy(newstatus, status);
	newstatus[strlen(status) + 1] = duration;
	newstatus[strlen(status) + 2] = color;
	return newstatus;
}

void World::PushStatusString(char * statusstring){
	while(statusmessages.size() >= maxstatusmessages){
		delete[] statusmessages.back();
		statusmessages.pop_back();
	}
	statusmessages.push_front(statusstring);
}

