#pragma once

#include "client/ui/hooks/use_session.h" // client::ui::SessionPhase
#include "client/ui/providers/lobby_provider.h"

class Game;

namespace silencer::game_ui {

// Capture the lobby read-state the cppx UI needs into a POD snapshot, once per
// tick under Lobby::LockMutex, *before* the build phase (doc §5) — so the
// LobbyProvider never locks at build time and no raw Lobby*/mutex leaks into the
// app shell. Returns a default (empty) snapshot outside lobby phases (no lock
// taken there). `phase` is the composition root's projected session phase.
client::ui::LobbySnapshot CaptureLobbySnapshot(Game &game,
                                               client::ui::SessionPhase phase);

} // namespace silencer::game_ui
