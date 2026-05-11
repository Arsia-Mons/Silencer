#ifndef WASMWS_H
#define WASMWS_H

// Emscripten-only WebSocket helpers used by lobby.cpp and world.cpp when
// compiled for the browser. Native builds do not see this file.
//
// Stage 4 of the WASM browser spectator project
// (docs/plans/2026-05-10-wasm-spectator.md). Native builds keep raw
// TCP/UDP; Emscripten builds route those calls through WebSocket here.
//
// Two endpoints:
//   - Lobby facade  (text/JSON frames): the WS endpoint we connect to is
//     services/lobby/wsfacade.go (default `ws://<lobbyhost>:15173/spectate`).
//     We consume {hello,newgame,delgame,spectate_url,error} envelopes and
//     emit `{"type":"spectate","gameid":N}` to request a relay URL.
//   - Relay        (binary frames):   the WS endpoint a `silencer --relay`
//     process exposes. We consume snapshot bytes verbatim and feed them
//     to World::DoNetwork_Replica's existing dispatch.
//
// All operations are non-blocking and main-thread-only — Emscripten
// dispatches WebSocket callbacks from the JS event loop, which is the
// same thread our SDL main loop runs on, so we never need locks.

#ifdef __EMSCRIPTEN__

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class WasmWS {
public:
	enum State { STATE_CONNECTING, STATE_OPEN, STATE_CLOSED, STATE_FAILED };

	WasmWS();
	~WasmWS();

	// Initiate connection. Returns false if the URL is invalid or
	// Emscripten couldn't start the socket. Does not block — Status()
	// transitions from CONNECTING to OPEN (or CLOSED/FAILED) as the
	// browser completes the handshake on later main-loop ticks.
	bool Open(const char *url);

	// Close the socket; safe to call multiple times.
	void Close();

	State Status() const { return state; }
	bool IsOpen() const  { return state == STATE_OPEN; }
	bool IsDead() const  { return state == STATE_CLOSED || state == STATE_FAILED; }

	// Pop the next received binary frame into `out`. Returns false if no
	// frame is ready. `out` is replaced with the frame contents on hit.
	bool PopBinary(std::vector<unsigned char> &out);

	// Pop the next received text frame into `out`. Returns false if no
	// frame is ready.
	bool PopText(std::string &out);

	// Send a UTF-8 text frame. Returns false on send failure (caller
	// should treat the socket as dead).
	bool SendText(const std::string &payload);

	// Send a binary frame. Returns false on send failure.
	bool SendBinary(const unsigned char *bytes, size_t len);

private:
	// Emscripten socket id (EMSCRIPTEN_WEBSOCKET_T = int). -1 = none.
	int sockId = -1;
	State state = STATE_CLOSED;

	// Received-frame queues. Text and binary kept separate because the
	// lobby uses text and the relay uses binary; mixing them at the same
	// endpoint never happens.
	std::vector<std::vector<unsigned char>> binaryQueue;
	std::vector<std::string> textQueue;

	// Trampolines installed via Emscripten's *_callback_on_thread APIs.
	static int OnOpen(int eventType, const void *e, void *userData);
	static int OnMessage(int eventType, const void *e, void *userData);
	static int OnClose(int eventType, const void *e, void *userData);
	static int OnError(int eventType, const void *e, void *userData);
};

#endif // __EMSCRIPTEN__
#endif // WASMWS_H
