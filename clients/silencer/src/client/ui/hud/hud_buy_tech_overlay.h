#pragma once

class Resources;
class Surface;

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
namespace client_ui {

struct BuyTechOverlayView;

// Modal-style center panel listing buyable items (or tech, depending on the
// origin station). The viewed player's row selection drives focus.
void BuildBuyTechOverlay(const BuyTechOverlayView& view,
                         const Resources& resources,
                         Surface* surface,
                         silencer::ui::UiInteractionRegistry& interactions);

}  // namespace client_ui
}  // namespace silencer
