#pragma once

#include <string>
#include <vector>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// LOBBY modal right-pane variants. Each takes a steady-state view and the
// shared sprite/palette/sub-palette. Callers paint these *over* the LOBBY
// chrome + character + (optionally) gameselect already laid down.

// GameCreate: Game Options + 5 form rows + Select Maps + Game Name +
// Password fields + Create button. Includes its own bank-7-idx-8 chrome
// blit (no populated game list behind the form).
struct GameCreateView {
  std::string game_name;            // "demo's game"
  std::vector<std::string> maps;    // top-to-bottom map list
};
void RenderGameCreateModal(Framebuffer &fb, const SpriteSet &sprites,
                           const Palette &palette, int active_sub,
                           const GameCreateView &view);

// GameJoin: 4 B156x21 buttons over the populated GameSelect.
void RenderGameJoinModal(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub);

// GameTech: Back-to-Teams + Join Game buttons + 6-row tech checkbox grid.
void RenderGameTechModal(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub);

}  // namespace silencer
