#pragma once

#include <string>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// Left-panel CharacterInterface composition (LOBBY family). Bank 181 idx
// 0..4 supply the five agency toggle widgets at y=90, x=20+i*42.
// All values are steady-state — no input/state machine.
struct CharacterView {
  std::string username;   // "demo"
  int level;              // 8
  int wins;               // 47
  int losses;             // 12
  int xp_to_next;         // 220
};

void RenderCharacter(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub,
                     const CharacterView &view);

}  // namespace silencer
