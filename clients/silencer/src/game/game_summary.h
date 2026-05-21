#ifndef GAME_SUMMARY_H
#define GAME_SUMMARY_H

#include <string>
#include <vector>

struct WorldPeerSummary {
	int id = 0;
	unsigned int accountId = 0;
	bool observer = false;
	bool disconnected = false;
	std::vector<int> controlledList;
};

struct WorldPlayerSummary {
	int id = 0;
	int hp = 0;
	int x = 0;
	int y = 0;
};

struct WorldSummary {
	std::string map;
	int peers = 0;
	int localPeerId = 0;
	int viewedPeerId = 0;
	int authorityPeer = 0;
	unsigned int lobbyAccountId = 0;
	bool isLocalObserver = false;
	bool spectatorInitialized = false;
	bool spectatorFreecam = false;
	std::vector<WorldPeerSummary> peerList;
	std::vector<WorldPlayerSummary> players;
	int objectsCount = 0;
	std::string messageText;
	int messageProgress = 0;
	int messageType = 0;
	int messageTime = 0;
	std::string topMessageText;
	int topMessageProgress = 0;
};

#endif
