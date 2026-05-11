#ifndef MAP_DOWNLOADER_H
#define MAP_DOWNLOADER_H

#include "shared.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class World;

// Map storage I/O (find/save/hash on disk) and the three async fetches the
// client runs against the community map server: per-row "[DL]" downloads
// from the create-game UI, the pre-game map fetch on the join path, and the
// upload performed during create-game.
class MapDownloader
{
public:
	explicit MapDownloader(World & world);
	~MapDownloader();

	MapDownloader(const MapDownloader &) = delete;
	MapDownloader & operator=(const MapDownloader &) = delete;

	std::vector<std::string> ListFiles(const char * directory);
	std::string FindMap(const char * name, unsigned char (*hash)[20] = 0, const char * directory = 0);
	std::string SaveMap(const char * name, unsigned char * data, int size);
	bool CalculateMapHash(const char * filename, unsigned char (*hash)[20]);
	std::string StringFromHash(unsigned char (*hash)[20]);
	void LoadMapData(const char * filename);
	void ProcessMapDownload(void);

	// Joins all background threads. Game::~Game() calls this before tearing
	// down SDL so threads don't reference freed audio/video resources; the
	// destructor calls it again as a safety net (idempotent).
	void JoinAndShutdown();

	// "[DL] NAME.SIL" → sha1hex; populated when CreateGameCreateInterface
	// queries FetchServerMapList.
	std::map<std::string, std::string> servermaps;

	int    selectedmap         = -1;
	Uint32 lastmapchunkrequest = 0;
	bool   mapexistchecked     = false;

	std::atomic<int> dlprogress{0};
	std::atomic<int> dlresult{0};
	std::string      dlitemname;
	std::thread      dlthread;

	// Pre-game map fetch (join path): async server download before P2P
	// fallback. State: 0=idle 1=in-flight 2=downloaded 3=not-on-server.
	std::atomic<int>      mapjoinstate{0};
	std::atomic<uint32_t> mapjoingeneration{0};
	std::string           mapjoinpath;
	std::mutex            mapjoinmutex;
	std::thread           mapjointhread;

	// Map upload for create-game flow.
	// State: 0=idle 1=uploading 2=ok 3=fail.
	std::atomic<int>      mapUploadState{0};
	std::atomic<uint32_t> mapUploadGeneration{0};
	std::thread           mapUploadThread;
	struct {
		std::string gamename, mapname, password;
		unsigned char maphash[20];
		Uint8 securitylevel, minlevel, maxlevel, maxplayers, maxteams;
	} pendingCreate;

private:
	World & world;
};

#endif
