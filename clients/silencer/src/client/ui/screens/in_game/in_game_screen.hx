#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// In-match HUD view. AppRoot's make_phase_element returns this for
// SessionPhase::InMatch and ::SinglePlayer.
::ui::UiElement InGameScreen(const char *key);

} // namespace client::ui
