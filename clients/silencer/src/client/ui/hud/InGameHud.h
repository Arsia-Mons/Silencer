#pragma once

class Renderer;
class Surface;
class World;

namespace silencer {
namespace client_ui {

void DrawInGameHud(Renderer& renderer, World& world, Surface* surface, float frametime);

}  // namespace client_ui
}  // namespace silencer
