#pragma once

#include "ui/runtime/draw_command.h"

class Renderer;
class Resources;
class Surface;

namespace silencer {
namespace retained_bridge {

void Render(Resources& resources,
            Renderer& renderer,
            Surface * dst,
            const ::ui::DrawCommandList& commands,
            int virtualWidth,
            int virtualHeight,
            float scale);

}  // namespace retained_bridge
}  // namespace silencer
