#pragma once

#include "client/ui/hooks/use_session.h" // client::ui::SessionPhase
#include "client/ui/providers/world_session_provider.h"

class Game;

namespace silencer::game_ui {

// Capture the in-match read-state the HUD needs into a POD snapshot, once per
// tick on the game thread, *before* the build phase (doc §5) — so the
// WorldSessionProvider never reaches into World/Player at build time. Reads the
// viewed agent (Player) + the replicated GameStateObject (so it works for both
// authority and replicas). Returns a default (invalid) snapshot outside the
// in-match phases (InMatch / SinglePlayer). `phase` is the projected session phase.
client::ui::WorldSessionSnapshot
CaptureWorldSessionSnapshot(Game &game, client::ui::SessionPhase phase);

} // namespace silencer::game_ui
