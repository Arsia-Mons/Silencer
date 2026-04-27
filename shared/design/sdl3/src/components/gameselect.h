#pragma once

#include <string>
#include <vector>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// Right-panel GameSelectInterface composition (LOBBY family). Renders the
// bank 7 idx 8 right-border chrome, the "Active Games" heading, and the
// game-row labels. Action buttons (Create Game, Join Game, etc.) are NOT
// drawn here — callers add them on top via RenderButton, which lets each
// LOBBY-modal vary the button set independently.
struct GameSelectView {
  std::vector<std::string> games;  // top-to-bottom, lineheight 14
};

void RenderGameSelect(Framebuffer &fb, const SpriteSet &sprites,
                      const Palette &palette, int active_sub,
                      const GameSelectView &view);

// Helper: just the bank 7 idx 8 chrome (no heading, no rows). Used by
// LOBBY GameCreate where the form replaces the populated game list.
void RenderGameSelectChrome(Framebuffer &fb, const SpriteSet &sprites);

}  // namespace silencer
