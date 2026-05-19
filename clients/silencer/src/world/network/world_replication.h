#ifndef WORLD_REPLICATION_H
#define WORLD_REPLICATION_H

#include "input.h"
#include "shared.h"
#include <list>

class World;
class Peer;
class Serializer;

class WorldReplication
{
	friend class World;

public:
	static const unsigned int maxoldsnapshots = 36;
	static const unsigned int maxlocalinputhistory = maxoldsnapshots + 1;

	explicit WorldReplication(World & world);

	bool ProcessInputQueue(Peer & peer);
	void ProcessSnapshotQueue();
	void ClientSidePredict(Uint32 ourtick);
	void CheckExists();
	void ClearSnapshotQueue();
	void DeleteOldSnapshots(Uint8 peerid);
	static bool CompareSnapshot(Serializer * snapshot1, Serializer * snapshot2);
	void SendInput();
	void SendSnapshots();
	void SendGameInfo(Uint8 peerid);
	void SendGameInfoLoaded();
	void SendReady();
	bool AllPeersReady(Uint8 except);
	bool AllPeersLoadedGameInfo();
	bool AllPeersDownloadedMap();
	void SaveSnapshot(Serializer & data, Uint8 peerid);
	void LoadSnapshot(Serializer & data, bool create = true, Serializer * delta = 0, Uint16 objectid = 0);

private:
	World & world;

public:
	Serializer * oldsnapshots[25][maxoldsnapshots];
	Input localinputhistory[maxlocalinputhistory];
	Uint32 localtoremoteticks[maxoldsnapshots];
	std::list<Serializer *> snapshotqueue;
	int snapshotqueueminsize;
	int snapshotqueuemaxsize;
	Uint32 lastsnapshotqueueadjust;
	std::list<Serializer *> inputqueue[25];
	unsigned int totalsnapshots;
	unsigned int totalinputpackets;
};

#endif
