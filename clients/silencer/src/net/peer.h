#ifndef PEER_H
#define PEER_H

#include "shared.h"
#include <list>
#include <deque>
#include "input.h"
#include "serializer.h"
#include "stats.h"

class Peer
{
public:
	Peer();
	void Serialize(bool write, Serializer & data);

	Uint8 id;
	bool ishost;
	bool gameinfoloaded;
	bool mapdownloaded;
	bool isready;
	Uint32 techchoices;
	Uint32 accountid;
	bool disconnected;
	bool observer;
	std::list<Uint16> controlledlist;

	// Local-user chat modal state. Only the local peer's value is ever set;
	// stored here (not on Player) so observers — who have no Player — can
	// also open chat. Not serialized.
	Uint16 chatinterfaceid;
	bool chatwithteam;

	// local only
	unsigned short port;
	unsigned long ip;
	Input input;
	std::deque<Input> inputqueue;
	Uint32 lasttick;
	Uint32 theirlasttick;
	Uint32 lastpacket;
	Stats stats;
	Uint32 wantedtechchoices;
	bool isbot;
	Uint32 firstinputtime;
	int totalinputs;
};

#endif