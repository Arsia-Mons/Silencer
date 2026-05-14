#pragma once

#include <SDL3/SDL_stdinc.h>

class Resources;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;
struct TeamHudView;
struct PlayerHudView;

// Highlight sprites overlayed on the secret/minimap during the hacking phase.
void BuildHudSecretSprites(const Resources& resources, Surface* surface,
                           const HudView& view, const TeamHudView& team,
                           int yoffset, Uint8 phase);

// The nine-line "Guv Net / OS / Protocol / ..." secret-hack progress display.
void BuildHudSecretProgress(Surface* surface, const PlayerHudView& player,
                            int yoffset, int secretprogress, Uint8 phase);

}  // namespace client_ui
}  // namespace silencer
