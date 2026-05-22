#ifndef WORLD_NETWORK_H
#define WORLD_NETWORK_H

#include "lagsimulator.h"
#include "shared.h"

class World;
class Peer;

class WorldNetwork
{
	friend class World;

public:
	explicit WorldNetwork(World & world);

	void DoNetwork();
	void DoNetwork_Authority();
	void DoNetwork_Replica();
	Peer * FindPeer(sockaddr_in & sockaddr);
	void SendPacket(Peer * peer, char * data, unsigned int size);
	void SwitchToMode(bool newmode);
	bool Listen(unsigned short port = 0);
	unsigned short Bind(unsigned short port = 0);
	void Connect(Uint8 agency, Uint32 accountid, Uint32 selectedcharid = 0,
	             const char * password = 0, bool observer = false);
	void Disconnect();
	void SwitchToLocalAuthorityMode();
	bool IsAuthority();
	bool IsLocalObserver();
	bool IsConnected() const;
	bool IsIdle() const;
	void SendPing();
	int GetPingTime();
	int AveragePingJitter();

private:
	World & world;

public:
	SOCKET sockethandle;
	unsigned short boundport;
	int state;
	LagSimulator lagsimulator;
	int pinghistory[10];
	Uint32 lastpingsent;
	Uint32 lastpingid;
	int pingtime;
	unsigned int totalbytesread;
	unsigned int totalbytessent;
};

#endif
