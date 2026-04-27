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

	// Multi-frame wait management. EnqueueWait is called from DrainControlQueue
	// (before the sim loop). TickWaits MUST be called AFTER the sim while-loop
	// in Game::Loop so wait_frames --n 1 / step --frames 1 see ≥1 sim tick before
	// resolving.
	void EnqueueWait(Game& game, ControlCommand cmd);
	void TickWaits(Game& game);
}

#endif
