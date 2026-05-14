#pragma once

#include <SDL3/SDL_stdinc.h>

class Surface;

namespace silencer {
namespace client_ui {

struct PlayerHudView;

// HUD text overlays: ammo readouts, credits, health/shield numbers, inventory
// counts. `currentammo` is the value picked by BuildHudStatusSprites for the
// currently-selected weapon.
void BuildHudReadouts(const PlayerHudView& player, Uint8 currentammo,
                      Surface* surface);

// "Government Trace Time: NNN" debug-style line shown when a trace is active.
void BuildHudTraceTime(Uint8 tracetime, Surface* surface);

}  // namespace client_ui
}  // namespace silencer
