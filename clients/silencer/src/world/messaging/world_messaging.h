#ifndef WORLD_MESSAGING_H
#define WORLD_MESSAGING_H

#include "shared.h"
#include <deque>
#include <string>

class World;
class Peer;

class WorldMessaging
{
	friend class World;

public:
	static const int maxstatusmessages = 4;

	explicit WorldMessaging(World & world);

	void ShowMessage(const char * message, Uint8 time = 255, Uint8 type = 0, bool networked = false, Peer * peer = 0);
	void ShowStatus(const char * status, Uint8 color = 0, bool networked = false, Peer * peer = 0);
	void ShowTopMessage(const char * message);
	void SendChat(bool toteam, char * message);
	void SendSound(const char * name, Peer * peer = 0, Uint8 volume = 128);
	void BroadcastTriggerState();
	void DisplayChatMessage(Uint32 accountid, const char * msg);
	char * CreateStatusString(const char * status, Uint8 color = 0, Uint8 duration = 100);
	void PushStatusString(char * statusstring);

	const char * GetMessageText() const { return message; }
	Uint8 GetMessageProgress() const { return message_i; }
	Uint8 GetMessageType() const { return messagetype; }
	Uint8 GetMessageTime() const { return messagetime; }
	const char * GetTopMessageText() const { return topmessage; }
	Uint8 GetTopMessageProgress() const { return topmessage_i; }

private:
	World & world;
	char message[256];
	Uint8 message_i;
	Uint8 messagetype;
	Uint8 messagetime;
	char topmessage[100];
	Uint8 topmessage_i;
	std::deque<std::string> chatlines;
	int showchat_i;
	std::deque<char *> statusmessages;
};

#endif
