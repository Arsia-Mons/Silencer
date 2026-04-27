#include "modal.h"

#include <array>

#include "../font.h"
#include "button.h"
#include "gameselect.h"

namespace silencer {

namespace {

void DrawLabel(Framebuffer &fb, const SpriteSet &sprites,
               const Palette &palette, int active_sub, int x, int y,
               const std::string &text) {
  DrawText(fb, x, y, text, /*bank=*/134, /*advance=*/8, sprites, palette,
           active_sub, /*brightness=*/128);
}

void DrawSmall(Framebuffer &fb, const SpriteSet &sprites,
               const Palette &palette, int active_sub, int x, int y,
               const std::string &text) {
  DrawText(fb, x, y, text, /*bank=*/133, /*advance=*/6, sprites, palette,
           active_sub, /*brightness=*/128);
}

}  // namespace

void RenderGameCreateModal(Framebuffer &fb, const SpriteSet &sprites,
                           const Palette &palette, int active_sub,
                           const GameCreateView &view) {
  RenderGameSelectChrome(fb, sprites);

  // Form labels + values.
  DrawLabel(fb, sprites, palette, active_sub, 272,  70, "Game Options");
  DrawLabel(fb, sprites, palette, active_sub, 245,  93, "Security:");
  DrawLabel(fb, sprites, palette, active_sub, 323,  93, "Medium");
  DrawLabel(fb, sprites, palette, active_sub, 245, 111, "Min Level:");
  DrawLabel(fb, sprites, palette, active_sub, 350, 111, "0");
  DrawLabel(fb, sprites, palette, active_sub, 245, 129, "Max Level:");
  DrawLabel(fb, sprites, palette, active_sub, 350, 129, "99");
  DrawLabel(fb, sprites, palette, active_sub, 245, 147, "Max Players:");
  DrawLabel(fb, sprites, palette, active_sub, 350, 147, "24");
  DrawLabel(fb, sprites, palette, active_sub, 245, 165, "Max Teams:");
  DrawLabel(fb, sprites, palette, active_sub, 350, 165, "6");

  // Select Maps.
  DrawLabel(fb, sprites, palette, active_sub, 510,  88, "Select Maps:");
  constexpr int kMapX = 510;
  constexpr int kMapY0 = 105;
  constexpr int kMapDy = 14;
  for (size_t i = 0; i < view.maps.size(); ++i) {
    DrawSmall(fb, sprites, palette, active_sub, kMapX,
              kMapY0 + static_cast<int>(i) * kMapDy, view.maps[i]);
  }

  // Game Name + Password fields.
  DrawLabel(fb, sprites, palette, active_sub, 405, 360, "Game Name:");
  DrawSmall(fb, sprites, palette, active_sub, 410, 375, view.game_name);
  DrawLabel(fb, sprites, palette, active_sub, 405, 390,
            "Password (optional):");

  // Create button.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Create", .x = 436, .y = 430,
                .variant = ButtonVariant::B156x21});
}

void RenderGameJoinModal(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub) {
  // The "Create Game" anchor (242, 68) from LOBBY is replaced by Choose
  // Tech; Change Team and Ready stack below. Join Game stays at (436, 430).
  struct Btn {
    const char *label;
    int x, y;
  };
  const std::array<Btn, 4> buttons = {{
      {"Choose Tech", 242,  68},
      {"Change Team", 242, 100},
      {"Ready",       242, 160},
      {"Join Game",   436, 430},
  }};
  for (const auto &b : buttons) {
    RenderButton(fb, sprites, palette, active_sub,
                 {.label = b.label, .x = b.x, .y = b.y,
                  .variant = ButtonVariant::B156x21});
  }
}

void RenderGameTechModal(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub) {
  // The "Create Game" anchor is replaced by Back To Teams; Join Game
  // stays at (436, 430).
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Back To Teams", .x = 242, .y =  68,
                .variant = ButtonVariant::B156x21});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Join Game",     .x = 436, .y = 430,
                .variant = ButtonVariant::B156x21});

  // Tech checkbox grid scaffold: BCHECKBOX = bank 7 idx 19 (13×13). Per
  // spec, columns 0..2 have draw=false (skipped); column 3 (x=452) is the
  // active selection column. 6 rows with placeholder labels at x=467
  // (bank 133 advance 6).
  constexpr int kCheckboxBank = 7;
  constexpr int kCheckboxIdx = 19;
  constexpr int kColX = 452;
  constexpr int kRowY0 = 125;
  constexpr int kRowDy = 13;
  constexpr int kRows = 6;
  constexpr int kLabelX = 467;
  if (sprites.Has(kCheckboxBank, kCheckboxIdx)) {
    const Sprite &cb = sprites.Get(kCheckboxBank, kCheckboxIdx);
    for (int i = 0; i < kRows; ++i) {
      BlitSprite(fb, cb, kColX, kRowY0 + i * kRowDy, nullptr);
    }
  }
  const std::array<const char *, kRows> tech_names = {{
      "Tech 1 (1)",
      "Tech 2 (1)",
      "Tech 3 (2)",
      "Tech 4 (1)",
      "Tech 5 (2)",
      "Tech 6 (1)",
  }};
  for (int i = 0; i < kRows; ++i) {
    DrawText(fb, kLabelX, kRowY0 + 2 + i * kRowDy, tech_names[i],
             /*bank=*/133, /*advance=*/6, sprites, palette, active_sub,
             /*brightness=*/128);
  }
}

}  // namespace silencer
