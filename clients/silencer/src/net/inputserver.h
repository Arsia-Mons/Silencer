#ifndef INPUTSERVER_H
#define INPUTSERVER_H

#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <thread>
#include "input.h"

// One-way binary input channel.
//
// Three layered message types — all latest-wins, all honored simultaneously:
//
//   type 0x01 INPUT_SNAPSHOT     action-level state ("keymoveup is held")
//     12-byte payload, see inputserver.cpp.
//     For programmatic / CLI / agent control. Bypasses the user's keymap
//     profile — the sender directly addresses Input fields. Use when you
//     want to "press fire" without knowing which key is bound to fire.
//
//   type 0x02 SCANCODE_SNAPSHOT  scancode-level state ("SDL_SCANCODE_W is held")
//     64-byte payload — bitmask of all SDL_SCANCODE_COUNT scancodes.
//     For the TUI client, which acts as a dumb keyboard proxy. The engine
//     decodes this into Game::keystate and runs the same UpdateInputState
//     pipeline as native SDL — so the user's keymap profile (default.json
//     or custom) is honored automatically.
//
//   type 0x03 MOUSE_SNAPSHOT     mouse position + button state
//     5-byte payload: [u16 LE x][u16 LE y][u8 buttons]. buttons bit 0 = left.
//     Coordinates are engine-pixel-space (same units as
//     SDL_GetMouseState produces in native mode). The TUI sends this
//     independently of the action/scancode messages so mouse motion
//     doesn't trample held-key state.
//
// Combination: each tick the engine writes the scancode bitmask into
// keystate, derives action-level Input via keymap.IsPressed, ORs the
// action snapshot on top, then overwrites mouse fields with the latest
// mouse snapshot. A client may send any subset of the three types.
//
// Wire envelope (little-endian, framed):
//   [u8 type][u16 len][payload]
//
// Connection handshake: client sends 1 byte = protocol version (currently
// 0x01). Server rejects unknown versions.
class InputServer {
public:
	InputServer();
	~InputServer();

	bool Start(int port);
	void Stop();

	// Most recent action-level snapshot. Returns true if any has ever
	// arrived; subsequent calls keep returning true with the cached value
	// (last-write-wins, stale-tolerant).
	bool LatestAction(Input& out);

	// Most recent scancode-level snapshot, decoded into a 512-byte
	// keystate-style array (kb[scancode] = 0 or 1). `out` must have
	// SDL_SCANCODE_COUNT bytes. Returns true if any scancode snapshot has
	// arrived; on false `out` is untouched.
	bool LatestScancodes(Uint8* out);

	// Most recent mouse snapshot (engine-pixel coords + left-button state).
	// Returns true if any mouse snapshot has arrived; on false outputs
	// are untouched.
	bool LatestMouse(Uint16& x, Uint16& y, bool& down);

private:
	void AcceptLoop();
	void HandleConnection(int fd);

	int listenfd;
	std::atomic<bool> running;
	std::thread acceptthread;

	std::mutex conn_mu;
	std::thread connthread;
	int connfd;

	std::mutex action_mu;
	Input action_snap;
	bool have_action;

	std::mutex sc_mu;
	Uint8 sc_state[SDL_SCANCODE_COUNT];
	bool have_sc;

	std::mutex mouse_mu;
	Uint16 mouse_x;
	Uint16 mouse_y;
	bool mouse_down;
	bool have_mouse;
};

#endif
