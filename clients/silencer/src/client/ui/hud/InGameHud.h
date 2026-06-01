#pragma once

class Renderer;
class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;

void BuildInGameHudUi(Renderer& renderer,
                      const Resources& resources,
                      const HudView& view,
                      Surface* surface);

}  // namespace client_ui
}  // namespace silencer
