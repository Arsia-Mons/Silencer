#pragma once

class Renderer;
class Surface;
class World;

namespace silencer {
namespace client_ui {

void DrawInGameOverlays(Renderer& renderer, World& world, Surface* surface);

}  // namespace client_ui
}  // namespace silencer
