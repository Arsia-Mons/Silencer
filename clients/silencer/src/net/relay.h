#ifndef RELAY_H
#define RELAY_H

#include "shared.h"
#include <string>
#include <vector>
#include <mutex>

// Server-side relay mode (Stage 2 of the WASM browser spectator project —
// docs/plans/2026-05-10-wasm-spectator.md).
//
// One process per spectatable INGAME match. The Go lobby spawns it
// alongside the normal `silencer -s` dedicated server, the same way it
// already spawns dedicated servers per MSG_NEWGAME (proc.go).
//
//   silencer --relay <peer_host> <peer_port> <gameid> [--ws-port=N]
//
// <peer_host>:<peer_port> is the AUTHORITY's (dedicated server's) UDP
// address. Stage 7 wires lobby spawn-on-demand, at which point the lobby
// looks up the address (it already tracks it via UDP heartbeat in
// hub.OnHeartbeat) and passes it on argv.
//
// On startup:
//   1. Open a UDP socket and join the dedicated game as a passive
//      spectator peer: build a World::MSG_CONNECT with the observer
//      bit set (uses the existing Serializer wire format) and sendto
//      the AUTHORITY's host:port. AUTHORITY admits via the
//      spectator-peer path that landed in PR #156 (world.cpp:364 —
//      AddPeer(..., observer=true) + SendGameInfo + SendPeerList).
//   2. Listen on a TCP port for HTTP→WebSocket upgrades. Each accepted
//      WS becomes a fan-out target.
//   3. For every UDP datagram received from the AUTHORITY, broadcast
//      the bytes verbatim to all connected WS clients as a single
//      binary frame. WASM clients run the same World::DoNetwork_Replica
//      dispatch so verbatim is what they expect.
//   4. Periodic MSG_PING to AUTHORITY so its peer-prune loop
//      (world.cpp:697, peertimeout) doesn't drop us.
//
// Multiple spectators share one outgoing snapshot stream — per-
// spectator state (camera, follow target) lives in the browser
// (Stage 6).
//
// Backpressure: if a WS client falls behind on writes (kernel send
// buffer fills, socket nonblocking would return EWOULDBLOCK), the relay
// drops that client's pending frames. One slow spectator does not stall
// the others.

class Relay {
public:
	Relay();
	~Relay();

	// Bring up the relay loop. Blocks until shutdown or fatal error.
	// Returns the process exit code (0 = clean stop, non-zero = error).
	int Run(const char *peerHost, unsigned short peerPort,
	        Uint32 gameId, unsigned short wsPort);

private:
	struct WSClient {
		int sock;        // Accepted socket, set non-blocking after upgrade
		std::vector<unsigned char> sendBuf;
		bool dead;
	};

	// WS server helpers.
	bool AcceptOne(int listenSock);
	bool DoHandshake(int sock);
	void Broadcast(const unsigned char *bytes, size_t len);
	void DrainOutbox(WSClient &c);
	void CloseClient(WSClient &c);

	// Spectator-peer (UDP) helpers.
	void SendConnectRequest();
	void SendPing();

	int listenSock = -1;
	std::mutex clientsMu;
	std::vector<WSClient *> clients;

	// Game-side connection state.
	int udpSock = -1;
	sockaddr_in peerAddr{};      // AUTHORITY's UDP host:port (filled in Run)
	Uint32 gameId = 0;           // logging only
	bool admitted = false;       // set on first datagram back from AUTHORITY
};

#endif
