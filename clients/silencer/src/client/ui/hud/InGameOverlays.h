#pragma once

class Renderer;
class Surface;
class World;

namespace silencer {
namespace client_ui {

void BuildInGameOverlaysUi(Renderer& renderer, World& world, Surface* surface);

}  // namespace client_ui
}  // namespace silencer
