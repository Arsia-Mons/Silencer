#pragma once

#include <SDL3/SDL_stdinc.h>

class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;

// Draws the system-camera inset frame around an in-game system-camera view.
// The world content shown inside the frame is drawn by Game::DrawInGameWorldInsets.
void BuildHudSystemCameraFrame(const Resources& resources, Surface* surface,
                               Uint8 bank, Uint16 index, Uint8 offsetBank,
                               int logicalY);

}  // namespace client_ui
}  // namespace silencer
