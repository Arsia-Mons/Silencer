#pragma once

class Surface;

namespace silencer {
namespace client_ui {

struct HudView;

// Bottom-right chat overlay: history + an in-progress chat input line when the
// viewed player has chat focus.
void BuildChatOverlay(const HudView& view, Surface* surface);

}  // namespace client_ui
}  // namespace silencer
