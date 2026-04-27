#include "../screens.h"

#include <array>
#include <cstring>
#include <string>

#include "../components/button.h"
#include "../components/panel.h"
#include "../font.h"

namespace silencer {

void ComposeOptionsControls(Framebuffer &fb, const SpriteSet &sprites,
                            const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 6, .idx = 0});
  // Frame panel (bank 7 idx 7) — bordered green-bevel overlay. The scrollbar
  // track + both chevron caps are baked into this sprite; only the moveable
  // thumb (idx 10) would be drawn separately, and at scrollposition=0 with
  // 6 keynames scrollmax==0 so the thumb is hidden.
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 7, .idx = 7});

  // Title.
  {
    const std::string title = "Configure Controls";
    constexpr int kAdvance = 12;
    int title_x = 320 - static_cast<int>(title.size()) * kAdvance / 2;
    DrawText(fb, title_x, 14, title, /*bank=*/135, kAdvance, sprites, palette,
             active_sub, /*brightness=*/128);
  }

  struct Row {
    const char *label;
    const char *key1;
    const char *connector;
    const char *key2;
  };
  const std::array<Row, 6> rows = {{
      {"Move Up:",      "Up",    "OR",  ""},
      {"Move Down:",    "Down",  "OR",  ""},
      {"Move Left:",    "Left",  "OR",  ""},
      {"Move Right:",   "Right", "OR",  ""},
      {"Aim Up/Left:",  "Up",    "AND", "Left"},
      {"Aim Up/Right:", "Up",    "AND", "Right"},
  }};

  constexpr int kRowStride = 53;
  constexpr int kLabelX = 80;
  constexpr int kLabelYBase = 95;
  constexpr int kKey1X = -30;
  constexpr int kKey2X = 120;
  constexpr int kConnectorX = 383;
  constexpr int kConnectorW = 40;
  constexpr int kConnectorAdvance = 9;

  for (size_t i = 0; i < rows.size(); ++i) {
    int yi = static_cast<int>(i) * kRowStride;
    int by = yi;
    int ty = kLabelYBase + yi;

    // Row label.
    DrawText(fb, kLabelX, ty, rows[i].label, /*bank=*/134, /*advance=*/10,
             sprites, palette, active_sub, /*brightness=*/128);

    // Key1 button (always blits chrome; label may be empty).
    RenderButton(fb, sprites, palette, active_sub,
                 {.label = rows[i].key1, .x = kKey1X, .y = by,
                  .variant = ButtonVariant::B112x33});

    // BNONE connector text, centered within (kConnectorX, ty, w=40).
    {
      int conn_len = static_cast<int>(std::strlen(rows[i].connector));
      int conn_xoff = (kConnectorW - conn_len * kConnectorAdvance) / 2;
      DrawText(fb, kConnectorX + conn_xoff, ty, rows[i].connector,
               /*bank=*/134, kConnectorAdvance, sprites, palette, active_sub,
               /*brightness=*/128);
    }

    // Key2 button.
    RenderButton(fb, sprites, palette, active_sub,
                 {.label = rows[i].key2, .x = kKey2X, .y = by,
                  .variant = ButtonVariant::B112x33});
  }

  // Save / Cancel.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Save",   .x = -200, .y = 117,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Cancel", .x =   20, .y = 117,
                .variant = ButtonVariant::B196x33});
}

}  // namespace silencer
