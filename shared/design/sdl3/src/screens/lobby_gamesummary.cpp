#include "../screens.h"

#include <array>
#include <cstring>

#include "../components/button.h"
#include "../font.h"

namespace silencer {

void ComposeLobbyGameSummary(Framebuffer &fb, const SpriteSet &sprites,
                             const Palette &palette, int active_sub) {
  // Mission Summary title (left panel top, bank 135 logo font).
  DrawText(fb, 102, 47, "Mission Summary", /*bank=*/135, /*advance=*/11,
           sprites, palette, active_sub, /*brightness=*/128);

  // Mission Summary stat label list (24 rows). Anchors derived from
  // burst-scanning the reference dump: labels at x=89 (subordinate x=95),
  // values at x=257, y-rows in three sections separated by ~22 px gaps.
  constexpr int kLabelXMain = 89;
  constexpr int kLabelXSub = 95;
  constexpr int kValueX = 257;
  struct Row {
    const char *label;
    int x;
    int y;
  };
  const std::array<Row, 24> rows = {{
      {"Kills:",                    kLabelXMain,  92},
      {"Deaths:",                   kLabelXMain, 104},
      {"Suicides:",                 kLabelXMain, 114},
      {"Secrets",                   kLabelXMain, 138},
      {"Returned:",                 kLabelXSub,  148},
      {"Stolen:",                   kLabelXSub,  158},
      {"Picked up:",                kLabelXSub,  170},
      {"Fumbled:",                  kLabelXSub,  180},
      {"Civilians killed:",         kLabelXMain, 202},
      {"Guards killed:",            kLabelXMain, 214},
      {"Robots killed:",            kLabelXMain, 224},
      {"Defenses destroyed:",       kLabelXMain, 236},
      {"Fixed Cannons destroyed:",  kLabelXMain, 246},
      {"Files",                     kLabelXMain, 268},
      {"Hacked:",                   kLabelXSub,  280},
      {"Returned:",                 kLabelXSub,  290},
      {"Powerups picked up:",       kLabelXMain, 312},
      {"Health packs used:",        kLabelXMain, 324},
      {"Cameras placed:",           kLabelXMain, 334},
      {"Detonators planted:",       kLabelXMain, 346},
      {"Fixed Cannons placed:",     kLabelXMain, 356},
      {"Viruses used:",             kLabelXMain, 368},
      {"Poisons:",                  kLabelXMain, 378},
      {"Lazarus Tracts planted:",   kLabelXMain, 390},
  }};
  for (const auto &r : rows) {
    DrawText(fb, r.x, r.y, r.label, /*bank=*/133, /*advance=*/6, sprites,
             palette, active_sub, /*brightness=*/128);
    if (std::strchr(r.label, ':') != nullptr) {
      DrawText(fb, kValueX, r.y, "0", /*bank=*/133, /*advance=*/6, sprites,
               palette, active_sub, /*brightness=*/128);
    }
  }

  // Agency Upgrade panel header at top of the right panel.
  DrawText(fb, 388, 47, "+ 0 XP", /*bank=*/135, /*advance=*/11, sprites,
           palette, active_sub, /*brightness=*/128);

  // 6 "Current X Level: N" labels down the right panel.
  struct Upgrade {
    const char *label;
    int y;
  };
  const std::array<Upgrade, 6> upgrades = {{
      {"Current Endurance Level: 0", 100},
      {"Current Shield Level: 0",    146},
      {"Current Jetpack Level: 0",   192},
      {"Current Tech Slot Level: 0", 238},
      {"Current Hacking Level: 0",   284},
      {"Current Contacts Level: 0",  330},
  }};
  for (const auto &u : upgrades) {
    DrawText(fb, 336, u.y, u.label, /*bank=*/133, /*advance=*/6, sprites,
             palette, active_sub, /*brightness=*/128);
  }

  // Done button (B156x21 — spec-compatible structural slot).
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Done", .x = 390, .y = 410,
                .variant = ButtonVariant::B156x21});
}

}  // namespace silencer
