#pragma once

#include "ui/runtime/draw_command.h"

class Renderer;
class Surface;

namespace silencer {
namespace client_ui {

void RenderRetainedDrawCommands(Renderer& renderer,
                                Surface& dst,
                                const ::ui::DrawCommandList& commands);

}  // namespace client_ui
}  // namespace silencer
