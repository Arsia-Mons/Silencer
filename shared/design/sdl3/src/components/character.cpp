#include "character.h"

#include <cstdio>

#include "../font.h"

namespace silencer {

namespace {

void DrawStat(Framebuffer &fb, const SpriteSet &sprites,
              const Palette &palette, int active_sub, int y,
              const char *label, int value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s: %d", label, value);
  DrawText(fb, 17, y, buf, /*bank=*/133, /*advance=*/7, sprites, palette,
           active_sub, /*brightness=*/128);
}

}  // namespace

void RenderCharacter(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub,
                     const CharacterView &view) {
  // Username overlay at literal (20, 71), font 134 advance 8.
  DrawText(fb, 20, 71, view.username, /*bank=*/134, /*advance=*/8, sprites,
           palette, active_sub, /*brightness=*/128);

  // Five agency toggle widgets at y=90, x = 20 + i*42, bank 181 idx 0..4.
  // Selected-state highlight is non-gated — render base sprite for each.
  for (int i = 0; i < 5; ++i) {
    if (sprites.Has(181, i)) {
      BlitSprite(fb, sprites.Get(181, i), 20 + i * 42, 90, nullptr);
    }
  }

  // Four stat overlays at literal x=17, y in {130, 143, 156, 169}, font 133
  // advance 7.
  DrawStat(fb, sprites, palette, active_sub, 130, "LEVEL", view.level);
  DrawStat(fb, sprites, palette, active_sub, 143, "WINS", view.wins);
  DrawStat(fb, sprites, palette, active_sub, 156, "LOSSES", view.losses);
  DrawStat(fb, sprites, palette, active_sub, 169, "XP TO NEXT LEVEL",
           view.xp_to_next);
}

}  // namespace silencer
