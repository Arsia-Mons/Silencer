#ifndef WASMWS_H
#define WASMWS_H

// Emscripten-only WebSocket wrapper. Browsers can't open UDP and can't
// speak the lobby's binary TCP protocol, so when compiled for the
// browser the C++ networking code in lobby.cpp and world.cpp routes
// through here instead.
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

	// Initiate connection. Returns false if Emscripten couldn't start
	// the socket. Does not block — Status() transitions from CONNECTING
	// to OPEN (or CLOSED/FAILED) as the browser completes the handshake.
	bool Open(const char *url);

	void Close();

	State Status() const { return state; }
	bool IsOpen() const  { return state == STATE_OPEN; }
	bool IsDead() const  { return state == STATE_CLOSED || state == STATE_FAILED; }

	// Pop the next received binary frame into `out`. Returns false if
	// no frame is ready.
	bool PopBinary(std::vector<unsigned char> &out);

	// Pop the next received text frame into `out`. Returns false if no
	// frame is ready.
	bool PopText(std::string &out);

	// Send a UTF-8 text frame. Returns false on send failure.
	bool SendText(const std::string &payload);

	// Send a binary frame. Returns false on send failure.
	bool SendBinary(const unsigned char *bytes, size_t len);

private:
	int sockId = -1;
	State state = STATE_CLOSED;

	// Text and binary queues stay separate because a given endpoint
	// only uses one or the other — the lobby facade is text/JSON, the
	// relay is binary snapshot bytes.
	std::vector<std::vector<unsigned char>> binaryQueue;
	std::vector<std::string> textQueue;

	static int OnOpen(int eventType, const void *e, void *userData);
	static int OnMessage(int eventType, const void *e, void *userData);
	static int OnClose(int eventType, const void *e, void *userData);
	static int OnError(int eventType, const void *e, void *userData);
};

#endif // __EMSCRIPTEN__
#endif // WASMWS_H
