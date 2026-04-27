#ifndef CONTROLDISPATCH_H
#define CONTROLDISPATCH_H

#include "controlserver.h"

class Game;

namespace ControlDispatch {
	// Determine which queue an op belongs in. Called from the accept thread,
	// so this must be pure (no Game/World access).
	ControlCommand::Phase PhaseFor(const std::string& op);

	// Game-thread: handle one command, fulfill its reply promise.
	void HandleImmediate(Game& game, ControlCommand& cmd);
	void HandlePostRender(Game& game, ControlCommand& cmd);

	// Multi-frame wait management (called from DrainControlQueue).
	void EnqueueWait(Game& game, ControlCommand cmd);
	void TickWaits(Game& game);
}

#endif
