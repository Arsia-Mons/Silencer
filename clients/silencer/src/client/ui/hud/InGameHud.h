#pragma once

class Renderer;
class Resources;
class Surface;

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
namespace client_ui {

struct HudView;

void BuildInGameHudUi(Renderer& renderer,
                      const Resources& resources,
                      const HudView& view,
                      Surface* surface,
                      silencer::ui::UiInteractionRegistry& interactions,
                      bool drawLegacyRetainedHudParts);

}  // namespace client_ui
}  // namespace silencer
