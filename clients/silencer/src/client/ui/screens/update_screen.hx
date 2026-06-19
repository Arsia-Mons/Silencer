#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// UpdateScreen phase view, wrapped as a keyed component. AppRoot's
// make_phase_element returns this for SessionPhase::Updating. Pure view over
// use_updater(): renders the self-updater phase/progress snapshot and exposes
// consent/retry/cancel as buttons gated by the phase.
::ui::UiElement UpdateScreen(const char *key);

} // namespace client::ui
