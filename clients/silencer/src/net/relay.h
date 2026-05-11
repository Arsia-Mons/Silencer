#ifndef RELAY_H
#define RELAY_H

#include "shared.h"
#include <string>
#include <vector>
#include <mutex>

// `silencer --relay <peer_host> <peer_port> <gameid> [--ws-port=N]`
//
// One process per spectatable INGAME match. Joins the dedicated server
// as a passive spectator peer (MSG_CONNECT observer bit set) and fans
// every UDP datagram received from the AUTHORITY out over WebSocket to
// N browser clients as a single binary frame, verbatim.
//
// Backpressure: if a WS client falls behind on writes (kernel send
// buffer fills, socket non-blocking returns EWOULDBLOCK), the relay
// drops that client's pending frames. One slow spectator does not stall
// the others.

class Relay {
public:
	Relay();
	~Relay();

	// Blocks until shutdown or fatal error. Returns the process exit
	// code (0 = clean stop, non-zero = error).
	int Run(const char *peerHost, unsigned short peerPort,
	        Uint32 gameId, unsigned short wsPort);

private:
	struct WSClient {
		int sock;        // accepted socket, non-blocking after upgrade
		std::vector<unsigned char> sendBuf;
		bool dead;
	};

	bool AcceptOne(int listenSock);
	bool DoHandshake(int sock);
	void Broadcast(const unsigned char *bytes, size_t len);
	void DrainOutbox(WSClient &c);
	void CloseClient(WSClient &c);

	void SendConnectRequest();
	void SendPing();
	void SendPeerlistRequest();

	int listenSock = -1;
	std::mutex clientsMu;
	std::vector<WSClient *> clients;

	int udpSock = -1;
	sockaddr_in peerAddr{};   // AUTHORITY's UDP host:port
	Uint32 gameId = 0;        // logging only
	bool admitted = false;    // set on first datagram back from AUTHORITY

	// Bootstrap messages from AUTHORITY (MSG_CONNECT response, MSG_GAMEINFO,
	// MSG_PEERLIST). AUTHORITY only sends these on observer admit, well
	// before a browser WS attaches to us. We keep the most recent copy of
	// each and replay them whenever a fresh WS client connects so it can
	// bootstrap its World state without waiting for the next admit.
	std::vector<unsigned char> lastConnect;
	std::vector<unsigned char> lastGameinfo;
	std::vector<unsigned char> lastPeerlist;
	void SendFrameTo(WSClient &c, const unsigned char *bytes, size_t len);
};

#endif
