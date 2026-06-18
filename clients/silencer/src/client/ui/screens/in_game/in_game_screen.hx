#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// In-match HUD view, wrapped as a keyed component. AppRoot's make_phase_element
// returns this for SessionPhase::InMatch and ::SinglePlayer. The core read HUD:
// a transparent ScreenLayout(Hud) floating a match readout (mode /
// phase / time / score) + a center status message + an agent readout (vitals /
// armament / credits) over the live world, composed from use_match +
// use_player_status. Overlays (buy/tech/scoreboard/chat) + PauseScreen land next.
::ui::UiElement InGameScreen(const char *key);

} // namespace client::ui
