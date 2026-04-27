#pragma once

#include <string>
#include <vector>

#include "../components/character.h"

namespace silencer::demo {

// Demo data shared across LOBBY + LOBBY GameCreate/Join/Tech screens.
// Snapshot of `services/lobby/silencer-lobby -demo` output captured for
// the Ralph runs — the canonical PPM dumps were rendered against these
// exact values, so they must be preserved verbatim.

CharacterView Character();
const char *ChannelName();
const std::vector<std::string> &ChatLines();
const std::vector<std::string> &GameRows();
const std::vector<std::string> &MapRows();

}  // namespace silencer::demo
