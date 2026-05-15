#pragma once

class Surface;

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
namespace client_ui {

struct HudView;

// Bottom-right chat overlay: history + an in-progress chat input line when the
// viewed player has chat focus.
void BuildChatOverlay(const HudView& view,
                      Surface* surface,
                      silencer::ui::UiInteractionRegistry& interactions);

}  // namespace client_ui
}  // namespace silencer
