#pragma once

class Surface;

namespace silencer {
namespace client_ui {

struct BuyTechOverlayView;

// Modal-style center panel listing buyable items (or tech, depending on the
// origin station). The viewed player's row selection drives focus.
void BuildBuyTechOverlay(Surface* surface, const BuyTechOverlayView& view);

}  // namespace client_ui
}  // namespace silencer
