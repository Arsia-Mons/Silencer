#pragma once

// The three CharacterCreate wizard step pages. Their presentational
// subcomponents (agency/roster rows, the agent detail panel, the pane header)
// stay file-local in wizard_components.cppx; only the steps are public so the
// screen view can pick one by wizard step.

#include "client/ui/hooks/use_character_create.h" // CharacterWizard
#include "client/ui/hooks/use_characters.h"       // Characters
#include "ui/runtime/element.h"                    // UiElement
#include "ui/runtime/tree.h"                       // ActivationEvent

#include <functional>

namespace client::ui {

// step 1 — "SILENCER ALIAS": SELECT AGENT frame + a floating alias field.
::ui::UiElement AliasStep(
    const CharacterWizard &w,
    std::function<void(const ::ui::ActivationEvent &)> on_alias_enter);

// step 2 — "SELECT AGENCY": the five agency ovals + advantages/description.
::ui::UiElement AgencyStep(const CharacterWizard &w,
                           std::function<void(int)> on_pick);

// step 0 — "SELECT AGENT": the roster + a preview of the focused agent.
::ui::UiElement RosterStep(const CharacterWizard &w, const Characters &chars,
                           std::function<void(int)> on_select);

} // namespace client::ui
