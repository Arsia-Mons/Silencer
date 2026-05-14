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
int BuildHudTeams(const Resources& resources, Surface* surface,
                  const HudView& view, Uint8 phase);

// Linear lookup by team id. Used by the composition function to find the team
// that owns the viewed player.
const TeamHudView* FindTeamById(const HudView& view, Uint16 teamId);

}  // namespace client_ui
}  // namespace silencer
