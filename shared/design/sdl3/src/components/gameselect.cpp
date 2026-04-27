#include "gameselect.h"

#include "../font.h"

namespace silencer {

void RenderGameSelectChrome(Framebuffer &fb, const SpriteSet &sprites) {
  if (sprites.Has(7, 8)) {
    BlitSprite(fb, sprites.Get(7, 8), 0, 0, nullptr);
  }
}

void RenderGameSelect(Framebuffer &fb, const SpriteSet &sprites,
                      const Palette &palette, int active_sub,
                      const GameSelectView &view) {
  RenderGameSelectChrome(fb, sprites);

  DrawText(fb, 405, 70, "Active Games", /*bank=*/134, /*advance=*/8, sprites,
           palette, active_sub, /*brightness=*/128);

  constexpr int kRowX = 410;
  constexpr int kRowY0 = 92;
  constexpr int kRowDy = 14;
  for (size_t i = 0; i < view.games.size(); ++i) {
    DrawText(fb, kRowX, kRowY0 + static_cast<int>(i) * kRowDy, view.games[i],
             /*bank=*/133, /*advance=*/6, sprites, palette, active_sub,
             /*brightness=*/128);
  }
}

}  // namespace silencer
