#pragma once

// The CharacterCreate wizard render helpers. Their presentational
// subcomponents (agency/roster rows, the agent detail panel, the pane header)
// stay file-local in wizard_components.cppx; only these three are public so the
// roster phase screen + the alias/agency overlays can compose them. They are
// pure views over plain state/callbacks — the wizard cursor itself is now the
// ScreenStack (roster base screen → AliasModalScreen → AgencyModalScreen).

#include "client/ui/hooks/use_characters.h" // Characters
#include "ui/runtime/element.h"             // UiElement
#include "ui/runtime/tree.h"                // ActivationEvent

#include <functional>
#include <string>

namespace client::ui {

// "SILENCER ALIAS": the floating alias capsule over a transparent, focus-trapped
// CenteredOverlay so the roster behind it stays visible. Enter confirms.
::ui::UiElement AliasFloat(
    const char *alias_value,
    std::function<void(const std::string &)> on_change,
    std::function<void(const ::ui::ActivationEvent &)> on_enter);

// "SELECT AGENCY": the five agency ovals + advantages/description. `preview` is
// the focused/hovered agency; `set_preview` updates it; `on_pick` commits.
::ui::UiElement AgencyStep(int preview, std::function<void(int)> on_pick,
                           std::function<void(int)> set_preview);

// "SELECT AGENT": the roster + a preview of the focused agent + the Create New
// Character button. `preview` is the focused agent; `on_select` selects an
// existing agent; `on_create` opens the alias overlay.
::ui::UiElement RosterStep(const Characters &chars, int preview,
                           std::function<void(int)> on_select,
                           std::function<void(int)> set_preview,
                           std::function<void()> on_create);

} // namespace client::ui
