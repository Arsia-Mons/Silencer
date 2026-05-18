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
void BuildHudSecretSprites(const HudView& view, Surface* surface,
                           const Resources& resources, const TeamHudView& team,
                           int yoffset, Uint8 phase);

// The nine-line "Guv Net / OS / Protocol / ..." secret-hack progress display.
void BuildHudSecretProgress(const PlayerHudView& player, Surface* surface,
                            int yoffset, int secretprogress, Uint8 phase);

}  // namespace client_ui
}  // namespace silencer
