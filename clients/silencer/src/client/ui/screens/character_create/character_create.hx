#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// CharacterCreate phase view, wrapped as a keyed component. AppRoot's
// make_phase_element returns this for SessionPhase::CharacterCreate (reached
// when a freshly authenticated account has no agents). Pure view over
// use_characters(): pick an alias + agency and create the first agent; the
// CREATECHARACTER tick routes to the lobby once the new agent round-trips. The
// existing-agent roster step + agency emblems join when the lobby's
// "create new character" entry lands.
::ui::UiElement CharacterCreate(const char *key);

} // namespace client::ui
