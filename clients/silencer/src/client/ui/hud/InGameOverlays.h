#pragma once

class Renderer;
class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;

void BuildInGameOverlaysUi(Renderer& renderer,
                           const Resources& resources,
                           const HudView& view,
                           Surface* surface,
                           bool drawLegacyTextOverlays);

}  // namespace client_ui
}  // namespace silencer
