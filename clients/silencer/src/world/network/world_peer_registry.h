#ifndef WORLD_PEER_REGISTRY_H
#define WORLD_PEER_REGISTRY_H

#include "shared.h"

class World;
class Peer;
class Player;
class Team;
class Serializer;

class WorldPeerRegistry
{
	friend class World;

public:
	static const unsigned int maxpeers = 25;
	static const unsigned int peertimeout = 10000;
	static const unsigned int maxteams = 6;

	explicit WorldPeerRegistry(World & world);

	Peer * AddPeer(char * address, unsigned short port, Uint8 agency, Uint32 accountid, bool observer = false);
	Peer * AddBot(Uint8 agency);
	Peer * FindPeer(sockaddr_in & sockaddr);
	void ReadPeerList(Serializer & data);
	void HandleDisconnect(Uint8 peerid, bool permanent = false);
	void SendStats(Peer & peer);
	void UserInfoReceived(Peer & peer);
	static bool CompareTeamByNumber(Team * team1, Team * team2);
	Peer * GetAuthorityPeer();
	Peer * GetPeer(Uint8 peerid);
	Player * GetPeerPlayer(Uint8 peerid);
	Team * GetPeerTeam(Uint8 peerid);
	bool FindTeamForPeer(Peer & peer, Uint8 agency, int start = 0);
	void RequestPeerList();
	void SendPeerList(Uint8 peerid = 0);

private:
	World & world;

public:
	Peer * peerlist[maxpeers];
	unsigned int authoritypeer;
	unsigned int peercount;
	Uint8 localpeerid;
	unsigned short localpublicport;
};

#endif
