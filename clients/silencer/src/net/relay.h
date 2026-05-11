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
//   silencer --relay <lobbyaddr> <lobbyport> <gameid> [--ws-port=N]
//
// On startup:
//   1. Connect to the dedicated game as a passive spectator peer using
//      the existing world.cpp / peer.cpp UDP code (gated on PR #148
//      Phase 3 — the spectator-peer handshake. Until that lands, this
//      relay accepts WS clients and forwards a placeholder keepalive
//      so the wire end-to-end can be smoke-tested).
//   2. Listen on a TCP port for HTTP→WebSocket upgrades. Each accepted
//      WS becomes a fan-out target.
//   3. For every UDP snapshot bytes received from the game, broadcast
//      to all connected WS clients as a single binary frame.
//
// Multiple spectators share one outgoing snapshot stream — per-
// spectator state (camera, follow target) lives in the browser
// (Stage 6).
//
// Backpressure: if a WS client falls behind on writes (kernel send
// buffer fills, socket nonblocking would return EAGAIN), the relay
// drops that client's pending frames and sends the next keyframe.
// One slow spectator does not stall the others.

class Relay {
public:
	Relay();
	~Relay();

	// Bring up the relay loop. Blocks until shutdown or fatal error.
	// Returns the process exit code (0 = clean stop, non-zero = error).
	int Run(const char *lobbyAddr, unsigned short lobbyPort,
	        Uint32 gameId, unsigned short wsPort);

private:
	struct WSClient {
		int sock;        // Accepted socket, set non-blocking after upgrade
		std::vector<unsigned char> sendBuf;
		bool dead;
	};

	// Accept loop helpers — implemented in relay.cpp.
	bool AcceptOne(int listenSock);
	bool DoHandshake(int sock);
	void Broadcast(const unsigned char *bytes, size_t len);
	void DrainOutbox(WSClient &c);
	void CloseClient(WSClient &c);

	int listenSock = -1;
	std::mutex clientsMu;
	std::vector<WSClient *> clients;

	// Game-side connection state. Stub for Stage 2 — Phase 3 fills in
	// the spectator peer UDP path. For now we emit a synthetic
	// heartbeat frame every 250ms so connected browsers can verify the
	// WS path works end-to-end before real snapshots flow.
	bool gameConnected = false;
};

#endif
