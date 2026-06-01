#pragma once

#include <SDL3/SDL_stdinc.h>

class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;
struct TeamHudView;

// The vertical team strip: per-team peer-state sprites, secret-slot sprites,
// and the agency emblem. Returns the number of teams rendered (used by the
// caller to position the secret-progress overlay).
int BuildHudTeams(const HudView& view, Surface* surface,
                  const Resources& resources, Uint8 phase);

}  // namespace client_ui
}  // namespace silencer
