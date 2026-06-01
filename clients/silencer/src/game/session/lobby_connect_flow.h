#ifndef LOBBY_CONNECT_FLOW_H
#define LOBBY_CONNECT_FLOW_H

#include <string>
#include <vector>

class Game;
class Updater;

// Drives the lobby connect → version-check → authenticate → route sequence on
// the game tick, alongside `Lobby::DoNetwork` (doc §7a keeps lobby protocol
// orchestration on the tick; it polls `versionchecked`/`charactersreceived`, so
// it is naturally per-tick rather than an event). The cppx LobbyConnect screen
// is a pure read projection of this flow's `log` plus the credential-submit /
// cancel intents (see client::ui::use_lobby_session). Terminal states route by
// flipping the game state with `Game::GoToState`; the session-phase reconciler
// then mounts the destination screen — the flow names no screen.
class LobbyConnectFlow {
public:
	// Reset the connection log. Call when entering LOBBYCONNECT (the game-loop
	// handler already resets the lobby itself to WAITING).
	void Reset();
	// Advance one step of the connect FSM. Call each tick while the lobby is
	// settling (after the menu fade completes).
	void Advance(Game & game, Updater & updater);
	const std::vector<std::string> & Log() const { return log_; }

private:
	void AddLine(const char * text);
	void AddMotdLines(const char * motd);

	std::vector<std::string> log_;
	bool motdPrinted_ = false;
};

#endif
