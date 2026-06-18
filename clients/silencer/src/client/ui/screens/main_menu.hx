#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// MainMenu phase view, wrapped as a keyed component. AppRoot's
// make_phase_element returns this for SessionPhase::MainMenu. The screen is a
// pure view: it composes the use_session intents + use_app().quit through
// AppButtons in a Menu ScreenLayout and owns no navigation of its own.
::ui::UiElement MainMenu(const char *key);

} // namespace client::ui
