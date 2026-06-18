#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// MissionSummary phase view, wrapped as a keyed component. AppRoot's
// make_phase_element returns this for SessionPhase::PostMatch. Pure view over
// use_progression(): renders the post-match stats + the six per-stat upgrade
// rows and exposes Continue (rejoin the lobby if still authenticated, else the
// menu). All mutation routes through the progression intents.
::ui::UiElement MissionSummary(const char *key);

} // namespace client::ui
