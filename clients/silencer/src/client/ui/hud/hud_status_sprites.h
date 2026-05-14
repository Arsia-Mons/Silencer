#pragma once

#include <SDL3/SDL_stdinc.h>

class Renderer;
class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct PlayerHudView;

// Bottom-of-screen status row: minimap frame, fuel/health/shield/files bars,
// weapon face/glow/selector, and the four inventory icons + letters. Returns
// the ammo count of the currently-selected weapon, which the readout overlay
// then renders as the large `currentAmmo` number.
Uint8 BuildHudStatusSprites(Renderer& renderer, const Resources& resources,
                            Surface* surface, const PlayerHudView& player,
                            Uint8 phase);

}  // namespace client_ui
}  // namespace silencer
