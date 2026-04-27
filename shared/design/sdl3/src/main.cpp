#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "font.h"
#include "palette.h"
#include "sprite.h"

using namespace silencer;

// ---------------------------------------------------------------------------
// Logo overlay tick simulation. We just need to reach res_index = 60 (the
// steady-state hold frame) per tick.md / widget-overlay.md.
//
// Per spec:
//   state_i < 60   -> res_index = state_i / 2 + 29  (fade in 29..60)
//   60 <= state_i < 120 -> res_index = 60           (hold)
//
// We drive enough ticks to land in the "hold" branch.
// ---------------------------------------------------------------------------
static int LogoTickToHold() {
  // Run 90 ticks to be comfortably inside the hold window.
  int state_i = 0;
  int res_index = 29;
  while (state_i < 90) {
    if (state_i < 60) {
      res_index = state_i / 2 + 29;
    } else if (state_i < 120) {
      res_index = 60;
    } else {
      // shouldn't happen at 90
      res_index = 60;
    }
    if (res_index > 60) res_index = 60;
    if (res_index < 29) res_index = 29;
    ++state_i;
  }
  return res_index;
}

// ---------------------------------------------------------------------------
// Resolve indexed framebuffer to RGB via the active sub-palette and write
// a binary P6 PPM.
// ---------------------------------------------------------------------------
static bool WritePPM(const std::string &path, const Framebuffer &fb,
                     const Palette &palette, int active_sub) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "ppm: cannot open %s for writing\n", path.c_str());
    return false;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", Framebuffer::W, Framebuffer::H);
  std::vector<uint8_t> rgb(Framebuffer::W * Framebuffer::H * 3);
  const auto &pal = palette.palettes[active_sub];
  for (int i = 0; i < Framebuffer::W * Framebuffer::H; ++i) {
    uint8_t idx = fb.px[i];
    rgb[i * 3 + 0] = pal[idx][0];
    rgb[i * 3 + 1] = pal[idx][1];
    rgb[i * 3 + 2] = pal[idx][2];
  }
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
  return true;
}

// ---------------------------------------------------------------------------
// Compose the main menu and write the PPM.
// ---------------------------------------------------------------------------
static int RunDump(const std::string &assets_dir,
                   const std::string &dump_dir) {
  // Init SDL just enough to satisfy "init SDL". We don't open a window in
  // dump mode; the PPM is the deliverable.
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Banks the main menu touches (per sprite-banks.md "Banks the main menu
  // touches" table). Bank 132 / 134 / 136 are not needed by the menu but
  // can be co-loaded — for menu-only we just need 6, 133, 135, 208.
  SpriteSet sprites;
  std::vector<int> banks = {6, 133, 135, 208};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  // MAINMENU sub-palette is 1 (palette.md).
  constexpr int kSubMenu = 1;

  Framebuffer fb;
  fb.Clear();

  // Camera at (320, 240) on a 640x480 surface => GetXOffset = GetYOffset = 0.
  // (See widget-interface.md.) Object coords map straight to screen coords.

  // 1) Background plate: bank 6 idx 0, position (0, 0), no effects.
  if (sprites.Has(6, 0)) {
    BlitSprite(fb, sprites.Get(6, 0), 0, 0, nullptr);
  }

  // 2) Logo: bank 208, animated. Tick to the hold frame (idx 60).
  int logo_idx = LogoTickToHold();
  if (sprites.Has(208, logo_idx)) {
    BlitSprite(fb, sprites.Get(208, logo_idx), 0, 0, nullptr);
  }

  // 3) Buttons: B196x33, bank 6, base idx 7. INACTIVE state initially
  //    (activeobject = 0 sentinel "nothing focused"). All buttons render
  //    at res_index = 7 with effectbrightness = 128.
  struct ButtonSpec {
    const char *text;
    int x;
    int y;
  };
  const std::array<ButtonSpec, 4> buttons = {{
      {"Tutorial", 40, -134},
      {"Connect To Lobby", 80, -67},
      {"Options", 40, 0},
      {"Exit", 0, 67},
  }};

  for (const auto &b : buttons) {
    // Sprite chrome.
    const Sprite &chrome = sprites.Get(6, 7);
    BlitSprite(fb, chrome, b.x, b.y, nullptr);

    // Text label, centered. Per widget-button.md / font.md:
    //   xoff = (196 - strlen(text) * 11) / 2
    //   yoff = 8
    //   textX = b.x - chrome.offset_x + xoff = (b.x + 310) + xoff
    //   textY = b.y - chrome.offset_y + 8    = (b.y + 288) + 8
    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (196 - len * 11) / 2;
    int textX = b.x - chrome.offset_x + xoff;
    int textY = b.y - chrome.offset_y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  // 4) Version overlay: text mode, bank 133, advance 11 at (10, 463).
  //    Default version 00028 per screen-main-menu.md.
  std::string version_text = "Silencer v00028";
  DrawText(fb, 10, 463, version_text, /*bank=*/133, /*advance=*/11, sprites,
           palette, kSubMenu, /*brightness=*/128);

  // Resolve & write.
  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (logo idx=%d)\n", out.c_str(), logo_idx);

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the OPTIONS hub and write the PPM. Per docs/design/screen-options.md:
//   - sub-palette 1 (same menu palette)
//   - bank-6 idx-0 fullscreen background
//   - four B196x33 buttons at anchor x=-89, y in {-142, -90, -38, 15}
//     with labels Controls / Display / Audio / Go Back
//   - NO bank-208 logo overlay
//   - NO version overlay
//   - all buttons INACTIVE (res_index=7, effectbrightness=128)
// ---------------------------------------------------------------------------
static int RunDumpOptions(const std::string &assets_dir,
                          const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Banks the OPTIONS hub touches: 6 (bg plate + B196x33 button chrome) and
  // 135 (button-label font). Bank 133 (version font) and 208 (logo) are
  // unused on this screen — see "What's NOT on this screen" in
  // docs/design/screen-options.md.
  SpriteSet sprites;
  std::vector<int> banks = {6, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubMenu = 1;

  Framebuffer fb;
  fb.Clear();

  // 1) Background plate.
  if (sprites.Has(6, 0)) {
    BlitSprite(fb, sprites.Get(6, 0), 0, 0, nullptr);
  }

  // 2) Four buttons. Anchors per docs/design/screen-options.md object
  //    inventory. INACTIVE state -> res_index=7, effectbrightness=128.
  struct ButtonSpec {
    const char *text;
    int x;
    int y;
  };
  const std::array<ButtonSpec, 4> buttons = {{
      {"Controls", -89, -142},
      {"Display", -89, -90},
      {"Audio", -89, -38},
      {"Go Back", -89, 15},
  }};

  for (const auto &b : buttons) {
    const Sprite &chrome = sprites.Get(6, 7);
    BlitSprite(fb, chrome, b.x, b.y, nullptr);

    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (196 - len * 11) / 2;
    int textX = b.x - chrome.offset_x + xoff;
    int textY = b.y - chrome.offset_y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (options)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the OPTIONSAUDIO screen and write the PPM. Per
// docs/design/screen-options-audio.md:
//   - sub-palette 1 (same menu palette)
//   - bank-6 idx-0 fullscreen background plate
//   - title "Audio Options" at y=14, x = 320 - len*12/2, bank 135 advance 12
//   - one B220x33 button (bank 6 idx 23, "Music") at anchor (100, 50)
//   - off pill (bank 6 idx 12) at literal screen (420, 137)
//   - on  pill (bank 6 idx 14) at literal screen (450, 137)
//   - Save B196x33 (bank 6 idx 7, "Save")   at anchor (-200, 117)
//   - Cancel B196x33 (bank 6 idx 7, "Cancel") at anchor ( 20, 117)
//   - all buttons INACTIVE: chrome res_index = base, brightness = 128
//   - NO bank-208 logo overlay, NO version-text overlay
// ---------------------------------------------------------------------------
static int RunDumpOptionsAudio(const std::string &assets_dir,
                               const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Banks the OPTIONSAUDIO screen touches: 6 (bg plate, B196x33 idx 7,
  // B220x33 idx 23, off/on half-pills idx 12/14) and 135 (title + label
  // font). No 133, no 208.
  SpriteSet sprites;
  std::vector<int> banks = {6, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubMenu = 1;

  Framebuffer fb;
  fb.Clear();

  // 1) Background plate.
  if (sprites.Has(6, 0)) {
    BlitSprite(fb, sprites.Get(6, 0), 0, 0, nullptr);
  }

  // 2) Title overlay: "Audio Options", bank 135 advance 12, centered,
  //    y = 14. (widget-overlay text-mode: literal x,y, no anchor offset.)
  {
    const std::string title = "Audio Options";
    constexpr int kTitleAdvance = 12;
    int title_x = 320 - static_cast<int>(title.size()) * kTitleAdvance / 2;
    int title_y = 14;
    DrawText(fb, title_x, title_y, title, /*bank=*/135,
             /*advance=*/kTitleAdvance, sprites, palette, kSubMenu,
             /*brightness=*/128);
  }

  // 3) Music button: B220x33 at anchor (100, 50). Bank 6 base idx 23,
  //    INACTIVE -> res_index = 23, brightness = 128.
  {
    constexpr int kMusicX = 100;
    constexpr int kMusicY = 50;
    constexpr int kB220Base = 23;
    constexpr int kB220Width = 220;
    const Sprite &chrome = sprites.Get(6, kB220Base);
    BlitSprite(fb, chrome, kMusicX, kMusicY, nullptr);

    const char *text = "Music";
    int len = static_cast<int>(std::strlen(text));
    int xoff = (kB220Width - len * 11) / 2;
    int textX = kMusicX - chrome.offset_x + xoff;
    int textY = kMusicY - chrome.offset_y + 8;
    DrawText(fb, textX, textY, text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  // 4) Off / On half-pill indicators. Sprite overlays at literal screen
  //    coords (no anchor offset on overlays-with-explicit-position).
  //    Bank 6 indices 12..15 are a 2x2 grid: 12=off-left-half (dim
  //    outline), 13=on-left-half (filled), 14=off-right-half (dim
  //    outline), 15=on-right-half (filled). The screen-options-audio
  //    spec lists the "on" pill as idx 14 — that is wrong (idx 14 is
  //    the unlit right half); the canonical reference dump uses
  //    idx 15 (lit right half) for the on indicator at x=450.
  if (sprites.Has(6, 12)) {
    BlitSprite(fb, sprites.Get(6, 12), 420, 137, nullptr);
  }
  if (sprites.Has(6, 15)) {
    BlitSprite(fb, sprites.Get(6, 15), 450, 137, nullptr);
  }

  // 5) Save / Cancel: two B196x33 buttons at the bottom.
  struct ButtonSpec {
    const char *text;
    int x;
    int y;
  };
  const std::array<ButtonSpec, 2> buttons = {{
      {"Save", -200, 117},
      {"Cancel", 20, 117},
  }};
  for (const auto &b : buttons) {
    const Sprite &chrome = sprites.Get(6, 7);
    BlitSprite(fb, chrome, b.x, b.y, nullptr);

    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (196 - len * 11) / 2;
    int textX = b.x - chrome.offset_x + xoff;
    int textY = b.y - chrome.offset_y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (options_audio)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the OPTIONSDISPLAY screen and write the PPM. Per
// docs/design/screen-options-display.md:
//   - sub-palette 1
//   - bank-6 idx-0 fullscreen background plate
//   - title "Display Options" at y=14, bank 135 advance 12, centered
//   - two B220x33 toggle buttons at anchors (100, 50) and (100, 103)
//     (row stride +53), labels "Fullscreen" and "Smooth Scaling"
//   - per row: off pill (bank 6 idx 12) @ (420, 137+i*53), on pill
//     (bank 6 idx 15) @ (450, 137+i*53)
//   - Save B196x33 at anchor (-200, 117), Cancel at (20, 117)
//   - all buttons INACTIVE (chrome res_index = base, brightness = 128)
//   - NO logo, NO version text
// ---------------------------------------------------------------------------
static int RunDumpOptionsDisplay(const std::string &assets_dir,
                                 const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Banks the OPTIONSDISPLAY screen touches: 6 (bg, B196x33 idx 7,
  // B220x33 idx 23, off/on pills idx 12/15) and 135 (title + label font).
  SpriteSet sprites;
  std::vector<int> banks = {6, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubMenu = 1;

  Framebuffer fb;
  fb.Clear();

  // 1) Background plate.
  if (sprites.Has(6, 0)) {
    BlitSprite(fb, sprites.Get(6, 0), 0, 0, nullptr);
  }

  // 2) Title overlay: "Display Options", bank 135 advance 12, centered, y=14.
  {
    const std::string title = "Display Options";
    constexpr int kTitleAdvance = 12;
    int title_x = 320 - static_cast<int>(title.size()) * kTitleAdvance / 2;
    int title_y = 14;
    DrawText(fb, title_x, title_y, title, /*bank=*/135,
             /*advance=*/kTitleAdvance, sprites, palette, kSubMenu,
             /*brightness=*/128);
  }

  // 3) Two B220x33 toggle buttons: Fullscreen at (100, 50), Smooth Scaling
  //    at (100, 103). Row stride +53.
  struct ToggleRow {
    const char *text;
  };
  const std::array<ToggleRow, 2> rows = {{
      {"Fullscreen"},
      {"Smooth Scaling"},
  }};
  constexpr int kToggleX = 100;
  constexpr int kToggleY0 = 50;
  constexpr int kRowStride = 53;
  constexpr int kB220Base = 23;
  constexpr int kB220Width = 220;
  for (size_t i = 0; i < rows.size(); ++i) {
    int by = kToggleY0 + static_cast<int>(i) * kRowStride;
    const Sprite &chrome = sprites.Get(6, kB220Base);
    BlitSprite(fb, chrome, kToggleX, by, nullptr);

    int len = static_cast<int>(std::strlen(rows[i].text));
    int xoff = (kB220Width - len * 11) / 2;
    int textX = kToggleX - chrome.offset_x + xoff;
    int textY = by - chrome.offset_y + 8;
    DrawText(fb, textX, textY, rows[i].text, /*bank=*/135, /*advance=*/11,
             sprites, palette, kSubMenu, /*brightness=*/128);
  }

  // 4) Off / on half-pill indicators per row at literal screen coords.
  //    Bank 6 idx 12 = off-left (dim outline), idx 15 = on-right (bright
  //    filled). Indicator y = 137 + i*53 per spec.
  constexpr int kPillY0 = 137;
  for (size_t i = 0; i < rows.size(); ++i) {
    int py = kPillY0 + static_cast<int>(i) * kRowStride;
    if (sprites.Has(6, 12)) {
      BlitSprite(fb, sprites.Get(6, 12), 420, py, nullptr);
    }
    if (sprites.Has(6, 15)) {
      BlitSprite(fb, sprites.Get(6, 15), 450, py, nullptr);
    }
  }

  // 5) Save / Cancel: two B196x33 buttons at the bottom (y=117).
  struct ButtonSpec {
    const char *text;
    int x;
    int y;
  };
  const std::array<ButtonSpec, 2> buttons = {{
      {"Save", -200, 117},
      {"Cancel", 20, 117},
  }};
  for (const auto &b : buttons) {
    const Sprite &chrome = sprites.Get(6, 7);
    BlitSprite(fb, chrome, b.x, b.y, nullptr);

    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (196 - len * 11) / 2;
    int textX = b.x - chrome.offset_x + xoff;
    int textY = b.y - chrome.offset_y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (options_display)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the OPTIONSCONTROLS screen and write the PPM. Per
// docs/design/screen-options-controls.md:
//   - sub-palette 1
//   - bank-6 idx-0 fullscreen background plate
//   - bank-7 idx-7 frame panel (bordered green-bevel overlay)
//   - title "Configure Controls" at y=14, bank 135 advance 12, centered
//   - 6 form rows: keyname label (bank 134, w=10) at (80, 95+i*53),
//     B112x33 key1 button (bank 6 idx 28) at anchor (-30, 0+i*53),
//     OR/AND BNONE connector text (bank 134, w=9) at (383, 95+i*53),
//     B112x33 key2 button (bank 6 idx 28) at anchor (120, 0+i*53)
//   - vertical scrollbar (bank 7, res_index 9 track + idx 10 thumb)
//     at right edge, scrollposition=0
//   - Save B196x33 at anchor (-200, 117), Cancel at (20, 117)
//   - all buttons INACTIVE, brightness=128
//
// Bank dependencies: 6 (bg + B196x33 idx 7 + B112x33 idx 28),
// 7 (frame panel idx 7 + scrollbar idx 9/10), 134 (small body font),
// 135 (title font).
// ---------------------------------------------------------------------------
static int RunDumpOptionsControls(const std::string &assets_dir,
                                  const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  SpriteSet sprites;
  std::vector<int> banks = {6, 7, 134, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubMenu = 1;

  Framebuffer fb;
  fb.Clear();

  // 1) Background plate.
  if (sprites.Has(6, 0)) {
    BlitSprite(fb, sprites.Get(6, 0), 0, 0, nullptr);
  }

  // 2) Frame panel: bank 7 idx 7, fullscreen bordered overlay at (0, 0).
  //    The sprite's own offset (likely 0, 0 since the frame is screen-aligned)
  //    plus the anchor convention top_left = anchor - sprite.offset places it
  //    correctly. May decode via tile-mode RLE — the codec already handles it.
  if (sprites.Has(7, 7)) {
    BlitSprite(fb, sprites.Get(7, 7), 0, 0, nullptr);
  }

  // 3) Title overlay: "Configure Controls", bank 135 advance 12, centered, y=14.
  {
    const std::string title = "Configure Controls";
    constexpr int kTitleAdvance = 12;
    int title_x = 320 - static_cast<int>(title.size()) * kTitleAdvance / 2;
    DrawText(fb, title_x, 14, title, /*bank=*/135, /*advance=*/kTitleAdvance,
             sprites, palette, kSubMenu, /*brightness=*/128);
  }

  // 4) Six-row form. Each row i ∈ 0..5:
  //    - row label   "Move Up:" etc.    at (80, 95+i*53), font 134 advance 10
  //    - key1 button B112x33 chrome     at anchor (-30, 0+i*53)
  //    - BNONE connector "OR" or "AND"  at (383, 95+i*53), w=40 h=30, font 134
  //                                     advance 9, centered horizontally
  //    - key2 button B112x33 chrome     at anchor (120, 0+i*53)
  //    Per docs/design/screen-options-controls.md "Object inventory" table.
  //    Per-row content (button labels Up/Down/.../Left/Right) is C3's gate.
  struct ControlsRow {
    const char *label;
    const char *key1;
    const char *connector;
    const char *key2;
  };
  const std::array<ControlsRow, 6> rows = {{
      {"Move Up:", "Up", "OR", ""},
      {"Move Down:", "Down", "OR", ""},
      {"Move Left:", "Left", "OR", ""},
      {"Move Right:", "Right", "OR", ""},
      {"Aim Up/Left:", "Up", "AND", "Left"},
      {"Aim Up/Right:", "Up", "AND", "Right"},
  }};

  constexpr int kRowStride = 53;
  constexpr int kLabelX = 80;
  constexpr int kLabelYBase = 95;
  constexpr int kKey1X = -30;
  constexpr int kKey2X = 120;
  constexpr int kButtonYBase = 0;
  constexpr int kConnectorX = 383;
  constexpr int kConnectorW = 40;
  constexpr int kB112Base = 28;
  constexpr int kB112Width = 112;
  constexpr int kButtonAdvance = 11;
  constexpr int kButtonYoff = 8;
  constexpr int kSmallAdvance = 10;
  constexpr int kConnectorAdvance = 9;

  for (size_t i = 0; i < rows.size(); ++i) {
    int yi = static_cast<int>(i) * kRowStride;
    int by = kButtonYBase + yi;
    int ty = kLabelYBase + yi;

    // Row label.
    DrawText(fb, kLabelX, ty, rows[i].label, /*bank=*/134,
             /*advance=*/kSmallAdvance, sprites, palette, kSubMenu,
             /*brightness=*/128);

    // Key1 button chrome + label.
    if (sprites.Has(6, kB112Base)) {
      const Sprite &chrome = sprites.Get(6, kB112Base);
      BlitSprite(fb, chrome, kKey1X, by, nullptr);
      const char *text = rows[i].key1;
      int klen = static_cast<int>(std::strlen(text));
      if (klen > 0) {
        int xoff = (kB112Width - klen * kButtonAdvance) / 2;
        int textX = kKey1X - chrome.offset_x + xoff;
        int textY = by - chrome.offset_y + kButtonYoff;
        DrawText(fb, textX, textY, text, /*bank=*/135,
                 /*advance=*/kButtonAdvance, sprites, palette, kSubMenu,
                 /*brightness=*/128);
      }
    }

    // BNONE connector text, centered within (kConnectorX, ty, w=40, h=30).
    {
      int conn_len = static_cast<int>(std::strlen(rows[i].connector));
      int conn_xoff = (kConnectorW - conn_len * kConnectorAdvance) / 2;
      DrawText(fb, kConnectorX + conn_xoff, ty, rows[i].connector,
               /*bank=*/134, /*advance=*/kConnectorAdvance, sprites, palette,
               kSubMenu, /*brightness=*/128);
    }

    // Key2 button chrome + label (key2 may be empty for OR rows).
    if (sprites.Has(6, kB112Base)) {
      const Sprite &chrome = sprites.Get(6, kB112Base);
      BlitSprite(fb, chrome, kKey2X, by, nullptr);
      const char *text = rows[i].key2;
      int klen = static_cast<int>(std::strlen(text));
      if (klen > 0) {
        int xoff = (kB112Width - klen * kButtonAdvance) / 2;
        int textX = kKey2X - chrome.offset_x + xoff;
        int textY = by - chrome.offset_y + kButtonYoff;
        DrawText(fb, textX, textY, text, /*bank=*/135,
                 /*advance=*/kButtonAdvance, sprites, palette, kSubMenu,
                 /*brightness=*/128);
      }
    }
  }

  // 5) Scrollbar widget (bank 7, track idx 9, thumb idx 10).
  //    The scrollbar track + both chevron caps are baked into the bank 7 idx 7
  //    frame panel sprite (see C1's discovery). Only the moveable thumb (idx 10)
  //    is in principle rendered as a separate widget. At scrollposition=0 with
  //    this dump's six-entry keynames array, scrollmax = numkeys - 6 = 0, so
  //    `ScrollBar.draw = false` (per docs/design-system.md.archive:1093) and the
  //    thumb is not drawn. Pixel-level comparison confirms this: the candidate
  //    matches the reference exactly in the scrollbar zone (x>=540) with no
  //    thumb blit needed. Probe sprite dims for the orchestrator's spec audit.
  if (sprites.Has(7, 10)) {
    const Sprite &thumb = sprites.Get(7, 10);
    std::fprintf(stderr,
                 "scrollbar thumb sprite (7,10): w=%d h=%d offset=(%d,%d)\n",
                 thumb.w, thumb.h, thumb.offset_x, thumb.offset_y);
  }

  // 6) Save / Cancel: two B196x33 buttons at the bottom (y=117). Same chrome
  //    sprite (bank 6 idx 7) and font (bank 135 advance 11, brightness 128) as
  //    OPTIONS / OPTIONSAUDIO / OPTIONSDISPLAY.
  struct ButtonSpec {
    const char *text;
    int x;
    int y;
  };
  const std::array<ButtonSpec, 2> buttons = {{
      {"Save", -200, 117},
      {"Cancel", 20, 117},
  }};
  for (const auto &b : buttons) {
    const Sprite &chrome = sprites.Get(6, 7);
    BlitSprite(fb, chrome, b.x, b.y, nullptr);

    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (196 - len * 11) / 2;
    int textX = b.x - chrome.offset_x + xoff;
    int textY = b.y - chrome.offset_y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/135, /*advance=*/11, sprites,
             palette, kSubMenu, /*brightness=*/128);
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (options_controls)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the LOBBYCONNECT screen and write the PPM. Per
// docs/design/screen-lobby-connect.md:
//   - sub-palette 2 (the LOBBY palette — distinct from menu sub-palette 1
//     used by main_menu / options-family screens). First non-menu palette
//     this hydration uses.
//   - NO bank-6 idx-0 starfield. Panel exterior is Clear(0) (black).
//   - NO logo, NO version, NO title overlay (panel sprite includes its own
//     chrome).
//   - Bank 7 idx 2 panel sprite at (0, 0) — different idx from CONTROLS' 7.
//   - Multi-line TextBox at (185, 101) 250x170, font bank 133.
//   - "Username" / "Password" labels at (190, 291/318), font bank 134 w=9.
//   - Two TextInput fields at (275, 293/320) 180x14, font bank 133.
//   - Login B52x21 at (264, 339), Cancel B52x21 at (321, 339). Spec gap:
//     widget-button.md doesn't list B52x21 yet.
//
// L0's gate is build/run + sub-palette 2 resolution only — rendering of
// the panel sprite (L2), composition (L3), and final visual equivalence
// (L4) are downstream. Bank set planned ahead: {7, 133, 134, 135}.
// ---------------------------------------------------------------------------
static int RunDumpLobbyConnect(const std::string &assets_dir,
                               const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Bank set planned ahead: 7 (panel idx 2 + B52x21 chrome — TBD),
  // 133 (textbox/textinput font), 134 (Username/Password label font),
  // 135 (button-label font, by analogy with other B-series buttons).
  // L0 doesn't render any of these yet; they're loaded so subsequent
  // iterations can add rendering without churn to the bank list.
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  // LOBBYCONNECT sub-palette is 2 per palette.md / screen-lobby-connect.md
  // §Sub-palette. This is the first screen in the hydration that uses a
  // non-menu sub-palette.
  constexpr int kSubLobby = 2;

  Framebuffer fb;
  fb.Clear();

  // L2: Bank 7 idx 2 panel sprite at (0, 0). Different idx from CONTROLS'
  // idx 7. Same blit convention (top_left = anchor - sprite.offset; the
  // sprite is screen-aligned so its offset is (0,0)). Codec branch is
  // already validated for bank-7 sprites (linear/tile RLE) — no new
  // codec risk. Includes outer border, inner textbox border + scrollbar
  // lane, horizontal divider, input-field wells, button-row well.
  if (sprites.Has(7, 2)) {
    BlitSprite(fb, sprites.Get(7, 2), 0, 0, nullptr);
  }

  // L3: Form composition.
  // Username / Password labels — text-mode overlays at literal screen coords,
  // font bank 134 advance 9 (per docs/design/screen-lobby-connect.md object
  // inventory).
  DrawText(fb, 190, 291, "Username", /*bank=*/134, /*advance=*/9, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 190, 318, "Password", /*bank=*/134, /*advance=*/9, sprites,
           palette, kSubLobby, /*brightness=*/128);

  // Login / Cancel buttons — B52x21 variant. Per docs/design-system.md.archive
  // (Button table) B52x21 is **text-only with no sprite chrome** (sprite bank
  // is "—"); label uses font bank 133 advance 7, yoff=8, and centering xoff
  // adds +1 px after the standard center. Sprite-offset is (0,0) since there
  // is no sprite, so textX = anchor.x + xoff, textY = anchor.y + 8.
  struct B52Spec {
    const char *text;
    int x;
    int y;
  };
  const std::array<B52Spec, 2> b52_buttons = {{
      {"Login", 264, 339},
      {"Cancel", 321, 339},
  }};
  constexpr int kB52Width = 52;
  constexpr int kB52Advance = 7;
  for (const auto &b : b52_buttons) {
    int len = static_cast<int>(std::strlen(b.text));
    int xoff = (kB52Width - len * kB52Advance) / 2 + 1;
    int textX = b.x + xoff;
    int textY = b.y + 8;
    DrawText(fb, textX, textY, b.text, /*bank=*/133,
             /*advance=*/kB52Advance, sprites, palette, kSubLobby,
             /*brightness=*/128);
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubLobby);
  std::fprintf(stderr, "wrote %s (lobby_connect)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Compose the LOBBY screen and write the PPM. Per
// docs/design/screen-lobby.md:
//   - sub-palette 2 (lobby palette — same as LOBBYCONNECT).
//   - Bank 7 idx 1 fullscreen panel chrome at (0, 0). Distinct from
//     LOBBYCONNECT's idx 2 and CONTROLS' idx 7.
//   - Header bar: "Silencer" (font 135 w=11 color=152) at (15, 32),
//     "v.<world.version>" (font 133 w=6 color=189) at (115, 39),
//     map name (font 135 w=11 color=129 brightness 128+32) at (180, 32)
//     (empty on canonical dump), B156x21 "Go Back" button at (473, 29).
//   - NO bank-6 starfield, NO logo, NO version-style overlay (the
//     header bar carries the version).
//   - Sub-interfaces (CharacterInterface, GameSelectInterface,
//     ChatInterface) are runtime-driven and out of scope; their bounding
//     regions show through the panel-7-idx-1 chrome as empty.
//
// Y0's gate is build/run + sub-palette 2 resolution only — rendering of
// the panel sprite (Y1), header composition (Y2), and final visual
// equivalence (Y3) are downstream. Bank set planned ahead per spec:
// {7, 133, 135}. (No bank 134: spec doesn't list any w=9 small label
// font on this screen — only the header text and the Go Back button
// label, which both use bank 135 advance 11 / bank 133 advance 6.)
// ---------------------------------------------------------------------------
static int RunDumpLobby(const std::string &assets_dir,
                        const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Bank set: 7 (panel idx 1 + B156x21 chrome at idx 24),
  // 133 ("v.<version>" header text, advance 6; CharacterInterface stat
  //      overlays Level/Wins/Losses/Etc, advance 7),
  // 134 (B156x21 "Go Back" button label, advance 8 per widget-button.md
  //      archive — different from prior screens which used 135/advance 11
  //      for B196x33/B112x33/B220x33; CharacterInterface username overlay
  //      at advance 8),
  // 135 ("Silencer" header text, advance 11),
  // 181 (CharacterInterface five agency toggle widgets, idx 0..4 =
  //      NOXIS/LAZARUS/CALIBER/STATIC/BLACKROSE per
  //      docs/design/screen-lobby-character.md).
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135, 181};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  // LOBBY sub-palette is 2 (same as LOBBYCONNECT) per palette.md /
  // screen-lobby.md §Sub-palette.
  constexpr int kSubLobby = 2;

  Framebuffer fb;
  fb.Clear();

  // Y1: Bank 7 idx 1 fullscreen lobby panel chrome at (0, 0). Distinct
  // from LOBBYCONNECT's bank-7 idx-2 and CONTROLS' bank-7 idx-7. The
  // sprite contains the corner LED ornaments, the framing borders for
  // all three sub-interfaces (CharacterInterface left, GameSelectInterface
  // right, ChatInterface bottom-left), and the header-bar separator.
  // Same anchor convention as every other panel sprite (top_left =
  // anchor - sprite.offset; the sprite is screen-aligned with offset
  // (0,0)). Codec branch already validated for bank-7 frames.
  if (sprites.Has(7, 1)) {
    BlitSprite(fb, sprites.Get(7, 1), 0, 0, nullptr);
  }

  // Y2: Header bar overlays (drawn over the panel chrome).
  //
  //   "Silencer" overlay at literal (15, 32) — text-mode, bank 135 advance 11.
  //   "v.<world.version>" overlay at literal (115, 39) — bank 133 advance 6.
  //   Map name overlay at (180, 32) is empty on the canonical dump (no map
  //     selected pre-Join Game) — skip.
  //   B156x21 "Go Back" button at anchor (473, 29) — sprite chrome at
  //     bank 7 idx 24, label bank 134 advance 8, text yoff=4. INACTIVE:
  //     res_index=24, brightness=128 (per docs/design-system.md.archive).
  //
  // The spec lists `effectcolor` values (152 / 189 / 129) for the header
  // overlays, but DrawText() in this hydration doesn't yet model
  // effectcolor — every prior screen has rendered with brightness=128 only
  // and matched the reference. The font glyphs in banks 133/134/135 carry
  // their canonical palette indices baked in, and the lobby reference dump
  // is gated structurally — if a residual color drift shows up at Y3, the
  // fix is to extend DrawText, not to patch Y2.
  DrawText(fb, 15, 32, "Silencer", /*bank=*/135, /*advance=*/11, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 115, 39, "v.00028", /*bank=*/133, /*advance=*/6, sprites,
           palette, kSubLobby, /*brightness=*/128);

  // B156x21 Go Back button. Bank 7 idx 24 chrome + bank 134 advance 8 label.
  {
    constexpr int kGoBackX = 473;
    constexpr int kGoBackY = 29;
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kGoBackX, kGoBackY, nullptr);
      const char *text = "Go Back";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kGoBackX - chrome.offset_x + xoff;
      int textY = kGoBackY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134, /*advance=*/kB156Advance,
               sprites, palette, kSubLobby, /*brightness=*/128);
    }
  }

  // I0/E0: CharacterInterface composition (left panel). Per
  // docs/design/screen-lobby-character.md — bbox (10, 64, 217, 120).
  //   - Username overlay "demo" at literal (20, 71), font 134 advance 8.
  //   - Four stat overlays at literal x=17, y in {130, 143, 156, 169},
  //     font 133 advance 7. Demo data per RALPH.md: NOXIS agency
  //     stats — Level: 8, Wins: 47, Losses: 12, XP To Next Level: 220.
  //   - Five Toggle widgets at y=90, x = 20 + i*42 for i ∈ 0..4
  //     (NOXIS / LAZARUS / CALIBER / STATIC / BLACKROSE), bank 181
  //     idx 0..4. Selected-state highlight is non-gated — render the
  //     base sprite for each.
  DrawText(fb, 20, 71, "demo", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  for (int i = 0; i < 5; ++i) {
    int tx = 20 + i * 42;
    int ty = 90;
    if (sprites.Has(181, i)) {
      BlitSprite(fb, sprites.Get(181, i), tx, ty, nullptr);
    }
  }
  DrawText(fb, 17, 130, "LEVEL: 8", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 143, "WINS: 47", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 156, "LOSSES: 12", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 169, "XP TO NEXT LEVEL: 220", /*bank=*/133, /*advance=*/7,
           sprites, palette, kSubLobby, /*brightness=*/128);

  // I1: GameSelectInterface composition (right panel). Per
  // docs/design/screen-lobby-gameselect.md — bbox (403, 87, 222, 267).
  //   - Bank 7 idx 8 right-border chrome at literal (0, 0) — additional
  //     chrome supplying the game-list area's inner edges/divider on top
  //     of the LOBBY panel idx 1. Same anchor convention as the panel
  //     (top_left = anchor − sprite.offset; sprite is screen-aligned).
  //   - "Active Games" label at literal (405, 70), font 134 advance 8.
  //   - SelectBox bbox (407, 89, 214, 265), lineheight 14 — empty without
  //     a running Go lobby; outline already supplied by idx 8 chrome.
  //   - Scrollbar (bank 7 idx 9): engine-positioned widget; with empty
  //     data scrollmax=0 ⇒ ScrollBar.draw=false (per options-controls
  //     precedent at idx 9/10), so the thumb is NOT drawn. We don't
  //     emit the track sprite here either: in this hydration's
  //     other scrollbar instance the track was baked into the panel
  //     chrome, and structural scrollbar pixels in the empty-data
  //     reference dump live entirely in bank 7 idx 1's borders.
  //   - Five selected-game info overlays at (405, 358/370/382/394/406),
  //     font 133 advance 6 — empty without a selection. Skip the
  //     DrawText calls (empty string is a no-op anyway); recorded
  //     here as the spec's structural slots.
  //   - Create Game B156x21 at anchor (242, 68) — note this is OUTSIDE
  //     the GameSelectInterface bbox, sitting above the SelectBox.
  //   - Join Game B156x21 at anchor (436, 430).
  if (sprites.Has(7, 8)) {
    BlitSprite(fb, sprites.Get(7, 8), 0, 0, nullptr);
  }
  DrawText(fb, 405, 70, "Active Games", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);

  // E0: SelectBox content (4 game rows). Demo data per RALPH.md, in
  // reference-dump display order:
  //   Veterans Only / Tutorial / Capture the Tag / Casual Match #1.
  // SelectBox bbox is (407, 89, 214, 265), lineheight=14. Rows render
  // top-to-bottom, font 133 advance 6 (selectbox row font matches the
  // game-info textbox font). x=407 with a small left padding so the row
  // text doesn't sit on the chrome edge; y starts at 89 + half the
  // line spacing offset to vertically center each row in its slot.
  {
    constexpr int kRowX = 410;
    constexpr int kRowY0 = 92;
    constexpr int kRowDy = 14;
    const std::array<const char *, 4> games = {{
        "Veterans Only",
        "Tutorial",
        "Capture the Tag",
        "Casual Match #1",
    }};
    for (size_t i = 0; i < games.size(); ++i) {
      DrawText(fb, kRowX, kRowY0 + static_cast<int>(i) * kRowDy, games[i],
               /*bank=*/133, /*advance=*/6, sprites, palette, kSubLobby,
               /*brightness=*/128);
    }
  }

  // Two B156x21 action buttons. Same chrome (bank 7 idx 24) + label
  // (bank 134 advance 8, yoff=4) as the Go Back button above. Width=156,
  // centering xoff = (156 − len*8) / 2.
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    struct B156Spec {
      const char *text;
      int x;
      int y;
    };
    const std::array<B156Spec, 2> b156_buttons = {{
        {"Create Game", 242, 68},
        {"Join Game", 436, 430},
    }};
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      for (const auto &b : b156_buttons) {
        BlitSprite(fb, chrome, b.x, b.y, nullptr);
        int len = static_cast<int>(std::strlen(b.text));
        int xoff = (kB156Width - len * kB156Advance) / 2;
        int textX = b.x - chrome.offset_x + xoff;
        int textY = b.y - chrome.offset_y + kB156Yoff;
        DrawText(fb, textX, textY, b.text, /*bank=*/134,
                 /*advance=*/kB156Advance, sprites, palette, kSubLobby,
                 /*brightness=*/128);
      }
    }
  }

  // I2: ChatInterface composition (bottom-left panel). Per
  // docs/design/screen-lobby-chat.md — bbox (15, 216, 368, 234).
  //   - Bank 7 idx 11 chat-area chrome at literal (0, 0). Same anchor
  //     convention as bank 7 idx 1 / idx 8 — sprite is screen-aligned with
  //     offset (0, 0); BlitSprite applies the offset normalization.
  //   - Bank 7 idx 14 chat-input-row chrome at literal (0, 0). Distinct
  //     index from idx 11; supplies the input-row borders below the
  //     scrollback area.
  //   - Channel name overlay at literal (15, 200), font 134 advance 8 —
  //     uid=1, runtime content (e.g., "#general"). Empty without a
  //     running Go lobby; DrawText with empty string is a no-op.
  //     Documented here as the structural slot.
  //   - Chat TextBox bbox (19, 220, 242, 207), font 133 lineheight=11,
  //     fontwidth=6, bottom-to-top. Empty without server messages; the
  //     bordered region is supplied by the idx 11 chrome above.
  //   - Presence TextBox bbox (267, 220, 110, 207), uid=9, top-to-bottom.
  //     Empty without server presence list; bordered region likewise
  //     supplied by idx 11.
  //   - Chat TextInput bbox (18, 437, 360, 14), uid=1, font 133
  //     fontwidth=6, maxchars=200. Empty until the user types; the
  //     bordered input region is supplied by idx 14.
  //   - Chat Scrollbar bank 7 idx 12 (track) + idx 13 (thumb),
  //     scrollpixels=11, scrollposition=0. Engine-positioned widget;
  //     with empty messages scrollmax=0 ⇒ ScrollBar.draw=false (same
  //     precedent as options-controls and the GameSelectInterface
  //     scrollbar at I1). Omit the render entirely; the scrollbar
  //     channel pixels in the empty-data reference dump are supplied
  //     by bank 7 idx 11's baked-in borders.
  if (sprites.Has(7, 11)) {
    BlitSprite(fb, sprites.Get(7, 11), 0, 0, nullptr);
  }
  if (sprites.Has(7, 14)) {
    BlitSprite(fb, sprites.Get(7, 14), 0, 0, nullptr);
  }

  // E0: ChatInterface populated content per RALPH.md demo data.
  //   Channel name "Lobby" at literal (15, 200), font 134 advance 8.
  DrawText(fb, 15, 200, "Lobby", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);

  // Chat textbox bottom-to-top, bbox (19, 220, 242, 207), lineheight=11,
  // font 133 advance 6. With 5 messages, the newest sits at the bottom
  // of the bbox and older messages stack upward. Bottom y = 220+207-11 =
  // 416 for the newest line; oldest at 416 - 4*11 = 372.
  {
    constexpr int kChatX = 19;
    constexpr int kChatYBottom = 416;
    constexpr int kChatDy = 11;
    const std::array<const char *, 5> chat = {{
        // oldest first (top)
        "Vector: anyone up for a round?",
        "Solace: still waiting on Krieg's match to finish",
        "Ember: we got 4 in casual #1",
        "Vector: joining",
        // newest last (bottom)
        "Halcyon: gg everyone",
    }};
    for (size_t i = 0; i < chat.size(); ++i) {
      int y = kChatYBottom -
              static_cast<int>(chat.size() - 1 - i) * kChatDy;
      DrawText(fb, kChatX, y, chat[i], /*bank=*/133, /*advance=*/6, sprites,
               palette, kSubLobby, /*brightness=*/128);
    }
  }

  // Presence textbox top-to-bottom, bbox (267, 220, 110, 207),
  // lineheight=11, font 133 advance 6. Three sections:
  //   In Lobby: Halcyon, Ember, Solace, Vector, demo
  //   Pregame: Quill -Capture the Tag-
  //   Playing: Krieg -Casual Match #1-
  {
    constexpr int kPresX = 267;
    constexpr int kPresY0 = 220;
    constexpr int kPresDy = 11;
    // Section order and member order match the reference dump.
    const std::array<const char *, 10> presence = {{
        "In Lobby",
        "Ember",
        "Halcyon",
        "Solace",
        "Vector",
        "demo",
        "Pregame",
        "Quill -Capture the Tag-",
        "Playing",
        "Krieg -Casual Match #1-",
    }};
    for (size_t i = 0; i < presence.size(); ++i) {
      DrawText(fb, kPresX, kPresY0 + static_cast<int>(i) * kPresDy,
               presence[i], /*bank=*/133, /*advance=*/6, sprites, palette,
               kSubLobby, /*brightness=*/128);
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubLobby);
  std::fprintf(stderr, "wrote %s (lobby)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// E1: lobby-game-create modal. Per docs/design/screen-lobby-game-create.md.
// Reuses LOBBY chrome + populated CharacterInterface + populated ChatInterface
// (E0 work). Replaces the GameSelectInterface region with the GameCreate
// form: Game Options header + 5 label/value rows + Select Maps section + Game
// Name + Password inputs + Create button.
// ---------------------------------------------------------------------------
static int RunDumpLobbyGameCreate(const std::string &assets_dir,
                                  const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Same banks as RunDumpLobby — no new sprite banks introduced for the
  // GameCreate modal. Form labels are font 134 advance 8; map rows / Game
  // Name / Password use font 133 advance 6.
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135, 181};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubLobby = 2;

  Framebuffer fb;
  fb.Clear();

  // Y1: Lobby panel chrome.
  if (sprites.Has(7, 1)) {
    BlitSprite(fb, sprites.Get(7, 1), 0, 0, nullptr);
  }

  // Y2: Header overlays + Go Back B156x21 (identical to LOBBY).
  DrawText(fb, 15, 32, "Silencer", /*bank=*/135, /*advance=*/11, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 115, 39, "v.00028", /*bank=*/133, /*advance=*/6, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kGoBackX = 473;
    constexpr int kGoBackY = 29;
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kGoBackX, kGoBackY, nullptr);
      const char *text = "Go Back";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kGoBackX - chrome.offset_x + xoff;
      int textY = kGoBackY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134, /*advance=*/kB156Advance,
               sprites, palette, kSubLobby, /*brightness=*/128);
    }
  }

  // I0/E0: CharacterInterface populated (identical to LOBBY).
  DrawText(fb, 20, 71, "demo", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  for (int i = 0; i < 5; ++i) {
    int tx = 20 + i * 42;
    int ty = 90;
    if (sprites.Has(181, i)) {
      BlitSprite(fb, sprites.Get(181, i), tx, ty, nullptr);
    }
  }
  DrawText(fb, 17, 130, "LEVEL: 8", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 143, "WINS: 47", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 156, "LOSSES: 12", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 169, "XP TO NEXT LEVEL: 220", /*bank=*/133, /*advance=*/7,
           sprites, palette, kSubLobby, /*brightness=*/128);

  // I1-replaced: GameCreate form. Right-border chrome (bank 7 idx 8) reused
  // — supplies the same outer borders that GameSelectInterface uses, since
  // the GameCreate modal occupies the same right-side region (extended down
  // for the Game Name + Password rows).
  if (sprites.Has(7, 8)) {
    BlitSprite(fb, sprites.Get(7, 8), 0, 0, nullptr);
  }

  // E1: Form labels + values (font 134 advance 8 for labels; values use the
  // same font as the input would render, font 134 advance 8 for the
  // Security button label and Min/Max/Players/Teams TextInputs).
  // Anchors per docs/design/screen-lobby-game-create.md "Object inventory":
  //   "Game Options" — (272, 70)
  //   "Security:"    — (245, 93)   value "Medium" — Security button BNONE at (323, 93)
  //   "Min Level:"   — (245, 111)  value "0"      — TextInput at (350, 111)
  //   "Max Level:"   — (245, 129)  value "99"     — TextInput at (350, 129)
  //   "Max Players:" — (245, 147)  value "24"     — TextInput at (350, 147)
  //   "Max Teams:"   — (245, 165)  value "6"      — TextInput at (350, 165)
  DrawText(fb, 272, 70, "Game Options", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 245, 93, "Security:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 323, 93, "Medium", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 245, 111, "Min Level:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 350, 111, "0", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  DrawText(fb, 245, 129, "Max Level:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 350, 129, "99", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  DrawText(fb, 245, 147, "Max Players:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 350, 147, "24", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  DrawText(fb, 245, 165, "Max Teams:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 350, 165, "6", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);

  // "Select Maps:" label sits at the top of the right column above the
  // map SelectBox. Spec places it "(right column) (~190)"; reference dump
  // shows the label above the map list near y=88 in the right column at
  // x=510. Map list rows render top-to-bottom, font 133 advance 6,
  // lineheight 14 — same SelectBox machinery as GameSelectInterface E0.
  DrawText(fb, 510, 88, "Select Maps:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kMapX = 510;
    constexpr int kMapY0 = 105;
    constexpr int kMapDy = 14;
    const std::array<const char *, 4> maps = {{
        "ALLY10c",
        "CRAN01h",
        "EASY05c",
        "PIT16d",
    }};
    for (size_t i = 0; i < maps.size(); ++i) {
      DrawText(fb, kMapX, kMapY0 + static_cast<int>(i) * kMapDy, maps[i],
               /*bank=*/133, /*advance=*/6, sprites, palette, kSubLobby,
               /*brightness=*/128);
    }
  }

  // "Game Name:" + Game Name TextInput; "Password (optional):" + Password
  // TextInput. Labels font 134 advance 8 per spec; input default text font
  // 133 advance 6. Game Name default = Config::defaultgamename runtime
  // value; render as a placeholder string for visual parity.
  DrawText(fb, 405, 360, "Game Name:", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 410, 375, "demo's game", /*bank=*/133, /*advance=*/6, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 405, 390, "Password (optional):", /*bank=*/134, /*advance=*/8,
           sprites, palette, kSubLobby, /*brightness=*/128);

  // Create B156x21 at (436, 430) — bottom-right action button. Same chrome
  // (bank 7 idx 24) + label (bank 134 advance 8, yoff=4) machinery as the
  // Go Back / Create Game / Join Game buttons.
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    constexpr int kCreateX = 436;
    constexpr int kCreateY = 430;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kCreateX, kCreateY, nullptr);
      const char *text = "Create";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kCreateX - chrome.offset_x + xoff;
      int textY = kCreateY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134, /*advance=*/kB156Advance,
               sprites, palette, kSubLobby, /*brightness=*/128);
    }
  }

  // I2: ChatInterface chrome + populated content (identical to LOBBY E0).
  if (sprites.Has(7, 11)) {
    BlitSprite(fb, sprites.Get(7, 11), 0, 0, nullptr);
  }
  if (sprites.Has(7, 14)) {
    BlitSprite(fb, sprites.Get(7, 14), 0, 0, nullptr);
  }
  DrawText(fb, 15, 200, "Lobby", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  {
    constexpr int kChatX = 19;
    constexpr int kChatYBottom = 416;
    constexpr int kChatDy = 11;
    const std::array<const char *, 5> chat = {{
        "Vector: anyone up for a round?",
        "Solace: still waiting on Krieg's match to finish",
        "Ember: we got 4 in casual #1",
        "Vector: joining",
        "Halcyon: gg everyone",
    }};
    for (size_t i = 0; i < chat.size(); ++i) {
      int y = kChatYBottom -
              static_cast<int>(chat.size() - 1 - i) * kChatDy;
      DrawText(fb, kChatX, y, chat[i], /*bank=*/133, /*advance=*/6, sprites,
               palette, kSubLobby, /*brightness=*/128);
    }
  }
  {
    constexpr int kPresX = 267;
    constexpr int kPresY0 = 220;
    constexpr int kPresDy = 11;
    const std::array<const char *, 10> presence = {{
        "In Lobby",
        "Ember",
        "Halcyon",
        "Solace",
        "Vector",
        "demo",
        "Pregame",
        "Quill -Capture the Tag-",
        "Playing",
        "Krieg -Casual Match #1-",
    }};
    for (size_t i = 0; i < presence.size(); ++i) {
      DrawText(fb, kPresX, kPresY0 + static_cast<int>(i) * kPresDy,
               presence[i], /*bank=*/133, /*advance=*/6, sprites, palette,
               kSubLobby, /*brightness=*/128);
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubLobby);
  std::fprintf(stderr, "wrote %s (lobby_gamecreate)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// E2: lobby-game-join modal. Per docs/design/screen-lobby-game-join.md.
// LOBBY-derivative-modal pattern (single-button substitution variant): copy
// RunDumpLobby verbatim and substitute the I1 "Create Game" B156x21 button
// (anchor 242,68) with three stacked B156x21 buttons in the same column —
// Choose Tech (242,68), Change Team (242,100), Ready (242,160). Everything
// else (panel chrome, CharacterInterface populated, full GameSelectInterface
// content, ChatInterface populated, Join Game) renders unchanged. The
// "Disconnected from game" modal overlay (CreateModalDialog renderpass=3)
// is non-structural per the spec carve-out and is intentionally omitted.
// ---------------------------------------------------------------------------
static int RunDumpLobbyGameJoin(const std::string &assets_dir,
                                const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Same banks as RunDumpLobby — the GameJoin substitution introduces no
  // new sprites or fonts (3 B156x21 buttons reuse bank 7 idx 24 + label
  // font bank 134 advance 8).
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135, 181};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubLobby = 2;

  Framebuffer fb;
  fb.Clear();

  // Y1: Lobby panel chrome.
  if (sprites.Has(7, 1)) {
    BlitSprite(fb, sprites.Get(7, 1), 0, 0, nullptr);
  }

  // Y2: Header overlays + Go Back B156x21 (identical to LOBBY).
  DrawText(fb, 15, 32, "Silencer", /*bank=*/135, /*advance=*/11, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 115, 39, "v.00028", /*bank=*/133, /*advance=*/6, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kGoBackX = 473;
    constexpr int kGoBackY = 29;
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kGoBackX, kGoBackY, nullptr);
      const char *text = "Go Back";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kGoBackX - chrome.offset_x + xoff;
      int textY = kGoBackY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134, /*advance=*/kB156Advance,
               sprites, palette, kSubLobby, /*brightness=*/128);
    }
  }

  // I0/E0: CharacterInterface populated (identical to LOBBY).
  DrawText(fb, 20, 71, "demo", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  for (int i = 0; i < 5; ++i) {
    int tx = 20 + i * 42;
    int ty = 90;
    if (sprites.Has(181, i)) {
      BlitSprite(fb, sprites.Get(181, i), tx, ty, nullptr);
    }
  }
  DrawText(fb, 17, 130, "LEVEL: 8", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 143, "WINS: 47", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 156, "LOSSES: 12", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 169, "XP TO NEXT LEVEL: 220", /*bank=*/133, /*advance=*/7,
           sprites, palette, kSubLobby, /*brightness=*/128);

  // I1: GameSelectInterface populated (identical to LOBBY E0). The reference
  // dump shows the Active Games panel fully rendered behind the GameJoin
  // buttons — the spec's "replaces GameSelectInterface region" wording is
  // about the Create Game button slot only; the rest of the panel remains.
  if (sprites.Has(7, 8)) {
    BlitSprite(fb, sprites.Get(7, 8), 0, 0, nullptr);
  }
  DrawText(fb, 405, 70, "Active Games", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kRowX = 410;
    constexpr int kRowY0 = 92;
    constexpr int kRowDy = 14;
    const std::array<const char *, 4> games = {{
        "Veterans Only",
        "Tutorial",
        "Capture the Tag",
        "Casual Match #1",
    }};
    for (size_t i = 0; i < games.size(); ++i) {
      DrawText(fb, kRowX, kRowY0 + static_cast<int>(i) * kRowDy, games[i],
               /*bank=*/133, /*advance=*/6, sprites, palette, kSubLobby,
               /*brightness=*/128);
    }
  }

  // E2: GameJoin action buttons. The "Create Game" anchor (242,68) from
  // RunDumpLobby is now occupied by Choose Tech; Change Team and Ready
  // stack below at y=100 and y=160. Join Game stays at (436,430). All
  // four buttons share the bank 7 idx 24 chrome + bank 134 advance 8
  // label machinery (yoff=4, centered xoff = (156 − len*8) / 2).
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    struct B156Spec {
      const char *text;
      int x;
      int y;
    };
    const std::array<B156Spec, 4> b156_buttons = {{
        {"Choose Tech", 242, 68},
        {"Change Team", 242, 100},
        {"Ready", 242, 160},
        {"Join Game", 436, 430},
    }};
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      for (const auto &b : b156_buttons) {
        BlitSprite(fb, chrome, b.x, b.y, nullptr);
        int len = static_cast<int>(std::strlen(b.text));
        int xoff = (kB156Width - len * kB156Advance) / 2;
        int textX = b.x - chrome.offset_x + xoff;
        int textY = b.y - chrome.offset_y + kB156Yoff;
        DrawText(fb, textX, textY, b.text, /*bank=*/134,
                 /*advance=*/kB156Advance, sprites, palette, kSubLobby,
                 /*brightness=*/128);
      }
    }
  }

  // I2: ChatInterface chrome + populated content (identical to LOBBY E0).
  if (sprites.Has(7, 11)) {
    BlitSprite(fb, sprites.Get(7, 11), 0, 0, nullptr);
  }
  if (sprites.Has(7, 14)) {
    BlitSprite(fb, sprites.Get(7, 14), 0, 0, nullptr);
  }
  DrawText(fb, 15, 200, "Lobby", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  {
    constexpr int kChatX = 19;
    constexpr int kChatYBottom = 416;
    constexpr int kChatDy = 11;
    const std::array<const char *, 5> chat = {{
        "Vector: anyone up for a round?",
        "Solace: still waiting on Krieg's match to finish",
        "Ember: we got 4 in casual #1",
        "Vector: joining",
        "Halcyon: gg everyone",
    }};
    for (size_t i = 0; i < chat.size(); ++i) {
      int y = kChatYBottom -
              static_cast<int>(chat.size() - 1 - i) * kChatDy;
      DrawText(fb, kChatX, y, chat[i], /*bank=*/133, /*advance=*/6, sprites,
               palette, kSubLobby, /*brightness=*/128);
    }
  }
  {
    constexpr int kPresX = 267;
    constexpr int kPresY0 = 220;
    constexpr int kPresDy = 11;
    const std::array<const char *, 10> presence = {{
        "In Lobby",
        "Ember",
        "Halcyon",
        "Solace",
        "Vector",
        "demo",
        "Pregame",
        "Quill -Capture the Tag-",
        "Playing",
        "Krieg -Casual Match #1-",
    }};
    for (size_t i = 0; i < presence.size(); ++i) {
      DrawText(fb, kPresX, kPresY0 + static_cast<int>(i) * kPresDy,
               presence[i], /*bank=*/133, /*advance=*/6, sprites, palette,
               kSubLobby, /*brightness=*/128);
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubLobby);
  std::fprintf(stderr, "wrote %s (lobby_gamejoin)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Lobby GameTech modal: per docs/design/screen-lobby-game-tech.md, this is
// the LOBBY-derivative-modal *single-button-substitution* variant (same
// shape as GameJoin) — copy RunDumpLobby verbatim and substitute the I1
// "Create Game" B156x21 button (anchor 242,68) with a single Back To Teams
// B156x21 button at the same anchor. Adds a tech checkbox grid scaffold
// (BCHECKBOX = bank 7 idx 19, 13×13) at column-3 anchor x=452,
// y=125+i*13 with placeholder tech-name labels at x=467 (bank 133
// advance 6). The "Disconnected from game" modal overlay is non-structural
// per the spec carve-out and is intentionally omitted.
// ---------------------------------------------------------------------------
static int RunDumpLobbyGameTech(const std::string &assets_dir,
                                const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Same banks as RunDumpLobby — BCHECKBOX (bank 7 idx 19) is already in
  // bank 7 which is loaded for B156x21 chrome and panel sprites.
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135, 181};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  constexpr int kSubLobby = 2;

  Framebuffer fb;
  fb.Clear();

  // Y1: Lobby panel chrome.
  if (sprites.Has(7, 1)) {
    BlitSprite(fb, sprites.Get(7, 1), 0, 0, nullptr);
  }

  // Y2: Header overlays + Go Back B156x21 (identical to LOBBY).
  DrawText(fb, 15, 32, "Silencer", /*bank=*/135, /*advance=*/11, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 115, 39, "v.00028", /*bank=*/133, /*advance=*/6, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kGoBackX = 473;
    constexpr int kGoBackY = 29;
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kGoBackX, kGoBackY, nullptr);
      const char *text = "Go Back";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kGoBackX - chrome.offset_x + xoff;
      int textY = kGoBackY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134, /*advance=*/kB156Advance,
               sprites, palette, kSubLobby, /*brightness=*/128);
    }
  }

  // I0/E0: CharacterInterface populated (identical to LOBBY).
  DrawText(fb, 20, 71, "demo", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  for (int i = 0; i < 5; ++i) {
    int tx = 20 + i * 42;
    int ty = 90;
    if (sprites.Has(181, i)) {
      BlitSprite(fb, sprites.Get(181, i), tx, ty, nullptr);
    }
  }
  DrawText(fb, 17, 130, "LEVEL: 8", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 143, "WINS: 47", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 156, "LOSSES: 12", /*bank=*/133, /*advance=*/7, sprites,
           palette, kSubLobby, /*brightness=*/128);
  DrawText(fb, 17, 169, "XP TO NEXT LEVEL: 220", /*bank=*/133, /*advance=*/7,
           sprites, palette, kSubLobby, /*brightness=*/128);

  // I1: GameSelectInterface populated (identical to LOBBY E0). The reference
  // dump shows the Active Games panel fully rendered behind the GameTech
  // overlay — same as GameJoin.
  if (sprites.Has(7, 8)) {
    BlitSprite(fb, sprites.Get(7, 8), 0, 0, nullptr);
  }
  DrawText(fb, 405, 70, "Active Games", /*bank=*/134, /*advance=*/8, sprites,
           palette, kSubLobby, /*brightness=*/128);
  {
    constexpr int kRowX = 410;
    constexpr int kRowY0 = 92;
    constexpr int kRowDy = 14;
    const std::array<const char *, 4> games = {{
        "Veterans Only",
        "Tutorial",
        "Capture the Tag",
        "Casual Match #1",
    }};
    for (size_t i = 0; i < games.size(); ++i) {
      DrawText(fb, kRowX, kRowY0 + static_cast<int>(i) * kRowDy, games[i],
               /*bank=*/133, /*advance=*/6, sprites, palette, kSubLobby,
               /*brightness=*/128);
    }
  }

  // E3: GameTech action buttons + tech grid scaffold. The "Create Game"
  // anchor (242,68) from RunDumpLobby is now occupied by Back To Teams.
  // Join Game stays at (436,430).
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    struct B156Spec {
      const char *text;
      int x;
      int y;
    };
    const std::array<B156Spec, 2> b156_buttons = {{
        {"Back To Teams", 242, 68},
        {"Join Game", 436, 430},
    }};
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      for (const auto &b : b156_buttons) {
        BlitSprite(fb, chrome, b.x, b.y, nullptr);
        int len = static_cast<int>(std::strlen(b.text));
        int xoff = (kB156Width - len * kB156Advance) / 2;
        int textX = b.x - chrome.offset_x + xoff;
        int textY = b.y - chrome.offset_y + kB156Yoff;
        DrawText(fb, textX, textY, b.text, /*bank=*/134,
                 /*advance=*/kB156Advance, sprites, palette, kSubLobby,
                 /*brightness=*/128);
      }
    }
  }

  // Tech checkbox grid scaffold: BCHECKBOX = bank 7 idx 19 (13×13). Per
  // spec, columns 0..2 have draw=false (skipped); column 3 (x=452) is
  // the active selection column. Render a 6-row scaffold at column 3
  // with placeholder tech-name labels at x=467 (bank 133 advance 6).
  {
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
               /*bank=*/133, /*advance=*/6, sprites, palette, kSubLobby,
               /*brightness=*/128);
    }
  }

  // Selected tech name + tech description: per spec these are
  // (font 133/134, centered at y~350+) overlay text. In the reference
  // they are entirely covered by the "Disconnected from game" modal +
  // ChatInterface chrome (chat occupies y=200-420). Rendering them in
  // the candidate would overlap the chat region and add visual noise
  // without structural gain — exact text is non-structural per spec
  // ("the candidate just needs to render the checkbox-grid scaffold
  // with placeholder overlays — exact tech-name text content is
  // non-structural"). Skipped.

  // I2: ChatInterface chrome + populated content (identical to LOBBY E0).
  if (sprites.Has(7, 11)) {
    BlitSprite(fb, sprites.Get(7, 11), 0, 0, nullptr);
  }
  if (sprites.Has(7, 14)) {
    BlitSprite(fb, sprites.Get(7, 14), 0, 0, nullptr);
  }
  DrawText(fb, 15, 200, "Lobby", /*bank=*/134, /*advance=*/8, sprites, palette,
           kSubLobby, /*brightness=*/128);
  {
    constexpr int kChatX = 19;
    constexpr int kChatYBottom = 416;
    constexpr int kChatDy = 11;
    const std::array<const char *, 5> chat = {{
        "Vector: anyone up for a round?",
        "Solace: still waiting on Krieg's match to finish",
        "Ember: we got 4 in casual #1",
        "Vector: joining",
        "Halcyon: gg everyone",
    }};
    for (size_t i = 0; i < chat.size(); ++i) {
      int y = kChatYBottom -
              static_cast<int>(chat.size() - 1 - i) * kChatDy;
      DrawText(fb, kChatX, y, chat[i], /*bank=*/133, /*advance=*/6, sprites,
               palette, kSubLobby, /*brightness=*/128);
    }
  }
  {
    constexpr int kPresX = 267;
    constexpr int kPresY0 = 220;
    constexpr int kPresDy = 11;
    const std::array<const char *, 10> presence = {{
        "In Lobby",
        "Ember",
        "Halcyon",
        "Solace",
        "Vector",
        "demo",
        "Pregame",
        "Quill -Capture the Tag-",
        "Playing",
        "Krieg -Casual Match #1-",
    }};
    for (size_t i = 0; i < presence.size(); ++i) {
      DrawText(fb, kPresX, kPresY0 + static_cast<int>(i) * kPresDy,
               presence[i], /*bank=*/133, /*advance=*/6, sprites, palette,
               kSubLobby, /*brightness=*/128);
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubLobby);
  std::fprintf(stderr, "wrote %s (lobby_gametech)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// LOBBY-derivative — GameSummary modal (post-match Mission Summary +
// Agency Upgrade panel, env SILENCER_DUMP_SCREEN=lobby_gamesummary). Per
// docs/design/screen-lobby-game-summary.md the modal sits over the LOBBY
// chrome — left panel area shows Mission Summary, right panel area shows
// Agency Upgrade (XP overlay + 6 upgrade-row buttons + Done). Numeric
// values are runtime-zero in the canonical dump.
//
// Spec gaps: precise panel-chrome sprite indices for Mission Summary +
// Agency Upgrade panels are unconfirmed; upgrade-row buttons are
// "B156x33-style" but the bank index isn't documented. We reuse the
// loaded LOBBY chrome (panel idx 1, right border idx 8) as the visual
// scaffold and B156x21 (idx 24) as the upgrade-row + Done button chrome.
// Spec gates only on title + label list + 6 row labels + Done text.
// ---------------------------------------------------------------------------
static int RunDumpLobbyGameSummary(const std::string &assets_dir,
                                   const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // GameSummary covers most of the screen — no LOBBY chrome bleed-through
  // in the post-harness-fix reference. Banks: bank 7 for B156x21 (Done
  // button chrome), banks 133/134/135 for the three text fonts. Bank 181
  // dropped — no character-portrait blits needed.
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134, 135};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  // Sub-palette 0 (in-game) per the spec. Reference shows a black/star
  // background, not the LOBBY sub=2 panel-exterior teal.
  constexpr int kSubGameSummary = 0;

  Framebuffer fb;
  fb.Clear();

  // Mission Summary title (left panel top, bank 135 logo font).
  DrawText(fb, 102, 47, "Mission Summary", /*bank=*/135, /*advance=*/11,
           sprites, palette, kSubGameSummary, /*brightness=*/128);

  // Mission Summary stat label list (24 rows). Anchors derived from
  // burst-scanning the reference dump: labels at x=89 (subordinate
  // x=95), values at x=257, y-rows in three sections separated by
  // ~22 px gaps. "Picked up:" was added under Secrets per the
  // post-harness-fix reference (was missing from prior 23-row list).
  {
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
               palette, kSubGameSummary, /*brightness=*/128);
      // Skip value column for the Secrets / Files section headers.
      if (std::strchr(r.label, ':') != nullptr) {
        DrawText(fb, kValueX, r.y, "0", /*bank=*/133, /*advance=*/6, sprites,
                 palette, kSubGameSummary, /*brightness=*/128);
      }
    }
  }

  // Agency Upgrade panel header: "+ N XP" in big-font (bank 135) at the
  // top of the right panel. Value zero — runtime in the engine.
  DrawText(fb, 388, 47, "+ 0 XP", /*bank=*/135, /*advance=*/11, sprites,
           palette, kSubGameSummary, /*brightness=*/128);

  // 6 "Current X Level: N" labels down the right panel. The post-harness-
  // fix reference no longer shows the +1 upgrade-row buttons or the
  // *NEW UPGRADE AVAILABLE* banner — those are shown only when an
  // upgrade is actually available; the canonical capture is from the
  // no-upgrade-available state. The 6 level labels are still required
  // structural anchors.
  {
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
               palette, kSubGameSummary, /*brightness=*/128);
    }
  }

  // Done button at the bottom of the right panel. Reference shows a
  // wider rounded button chrome (~228 px) we don't have a sprite for;
  // use B156x21 as the spec-compatible structural slot ("B156x21 or
  // similar" per spec). Centered around (468, 410).
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    constexpr int kDoneX = 390;
    constexpr int kDoneY = 410;
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      BlitSprite(fb, chrome, kDoneX, kDoneY, nullptr);
      const char *text = "Done";
      int len = static_cast<int>(std::strlen(text));
      int xoff = (kB156Width - len * kB156Advance) / 2;
      int textX = kDoneX - chrome.offset_x + xoff;
      int textY = kDoneY - chrome.offset_y + kB156Yoff;
      DrawText(fb, textX, textY, text, /*bank=*/134,
               /*advance=*/kB156Advance, sprites, palette, kSubGameSummary,
               /*brightness=*/128);
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubGameSummary);
  std::fprintf(stderr, "wrote %s (lobby_gamesummary)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// UPDATING state (env SILENCER_DUMP_SCREEN=updating). Per
// docs/design/screen-updating.md the captured reference is intentionally
// minimal — black background + bordered box centered around y=215 + Cancel
// B156x21 button right-aligned in the box. The reference also shows the
// Update B156x21 button at (161,230) sharing the same row (per the spec
// object inventory: Update uid=250, Cancel uid=251 both at y=230, 156 wide
// each — they sit side-by-side filling the box interior horizontally).
//
// Spec gaps: precise sprite index for the box border is unknown (likely
// CreateModalDialog bank 40 idx 4 per the spec table, but bank 40 isn't in
// this candidate's loaded set). We draw the bordered box manually by poking
// palette indices into the framebuffer — simple rectangle with a bright-
// green palette index that matches the reference border color under the
// menu sub-palette. Spec gates only on: dark background, bordered box,
// Cancel button inside.
// ---------------------------------------------------------------------------
static int RunDumpUpdating(const std::string &assets_dir,
                           const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  // Minimal bank set: bank 7 for B156x21 button chrome (idx 24), 133/134
  // for the small/medium fonts (status text + button labels). No sprite
  // for the box border — drawn manually below.
  SpriteSet sprites;
  std::vector<int> banks = {7, 133, 134};
  if (!sprites.Load(assets_dir, banks)) {
    SDL_Quit();
    return 1;
  }

  // UPDATING is entered from LOBBYCONNECT — same menu sub-palette. The
  // observed reference greens (24,125,20) etc. match sub-palette 1's idx
  // 212..222 range exactly.
  constexpr int kSubUpdating = 1;

  Framebuffer fb;
  fb.Clear();

  // Bordered box at (159..478) x (193..258). The border is bright green
  // (palette idx 220 in sub=1 -> RGB (24,125,20)). Top border thick (3
  // rows), bottom border 2 rows, vertical sides 6 cols thick.
  constexpr uint8_t kBorderIdx = 220;
  constexpr int kBoxLeft = 159;
  constexpr int kBoxRight = 478;
  constexpr int kBoxTop = 193;
  constexpr int kBoxBottom = 258;
  auto plot = [&](int x, int y, uint8_t idx) {
    if (x < 0 || x >= Framebuffer::W || y < 0 || y >= Framebuffer::H) return;
    fb.px[y * Framebuffer::W + x] = idx;
  };
  // Top border (y=193..195).
  for (int y = kBoxTop; y <= kBoxTop + 2; ++y) {
    for (int x = kBoxLeft; x <= kBoxRight; ++x) plot(x, y, kBorderIdx);
  }
  // Bottom border (y=257..258).
  for (int y = kBoxBottom - 1; y <= kBoxBottom; ++y) {
    for (int x = kBoxLeft; x <= kBoxRight; ++x) plot(x, y, kBorderIdx);
  }
  // Left vertical (x=159..164).
  for (int y = kBoxTop; y <= kBoxBottom; ++y) {
    for (int x = kBoxLeft; x <= kBoxLeft + 5; ++x) plot(x, y, kBorderIdx);
  }
  // Right vertical (x=473..478).
  for (int y = kBoxTop; y <= kBoxBottom; ++y) {
    for (int x = kBoxRight - 5; x <= kBoxRight; ++x) plot(x, y, kBorderIdx);
  }

  // Status text at (centered ~320, y=200): "An update is required to play
  // online." per spec. Center horizontally on the box — bank 134 advance 8.
  {
    const char *text = "An update is required to play online.";
    int len = static_cast<int>(std::strlen(text));
    int textX = (kBoxLeft + kBoxRight) / 2 - (len * 8) / 2;
    DrawText(fb, textX, 200, text, /*bank=*/134, /*advance=*/8, sprites,
             palette, kSubUpdating, /*brightness=*/128);
  }

  // Update + Cancel B156x21 buttons at (161,230) and (322,230). The
  // reference shows both filling the box interior side-by-side. Per spec
  // PROMPTING state shows Update; Cancel is the dismiss action.
  {
    constexpr int kB156Base = 24;
    constexpr int kB156Width = 156;
    constexpr int kB156Advance = 8;
    constexpr int kB156Yoff = 4;
    struct Btn {
      const char *label;
      int x;
      int y;
    };
    const std::array<Btn, 2> buttons = {{
        {"Update", 161, 230},
        {"Cancel", 322, 230},
    }};
    if (sprites.Has(7, kB156Base)) {
      const Sprite &chrome = sprites.Get(7, kB156Base);
      for (const auto &b : buttons) {
        BlitSprite(fb, chrome, b.x, b.y, nullptr);
        int len = static_cast<int>(std::strlen(b.label));
        int xoff = (kB156Width - len * kB156Advance) / 2;
        int textX = b.x - chrome.offset_x + xoff;
        int textY = b.y - chrome.offset_y + kB156Yoff;
        DrawText(fb, textX, textY, b.label, /*bank=*/134,
                 /*advance=*/kB156Advance, sprites, palette, kSubUpdating,
                 /*brightness=*/128);
      }
    }
  }

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubUpdating);
  std::fprintf(stderr, "wrote %s (updating)\n", out.c_str());

  SDL_Quit();
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  std::string assets_dir;
  if (argc >= 2) {
    assets_dir = argv[1];
  } else {
    // Default: ../../../assets relative to the binary.
    const char *base = SDL_GetBasePath();
    if (base) {
      assets_dir = std::string(base) + "../../../assets";
    } else {
      assets_dir = "../../../assets";
    }
  }

  const char *dump = std::getenv("SILENCER_DUMP_DIR");
  if (dump && *dump) {
    const char *screen = std::getenv("SILENCER_DUMP_SCREEN");
    std::string screen_str = (screen && *screen) ? screen : "main_menu";
    if (screen_str == "options") {
      return RunDumpOptions(assets_dir, dump);
    }
    if (screen_str == "options_audio") {
      return RunDumpOptionsAudio(assets_dir, dump);
    }
    if (screen_str == "options_display") {
      return RunDumpOptionsDisplay(assets_dir, dump);
    }
    if (screen_str == "options_controls") {
      return RunDumpOptionsControls(assets_dir, dump);
    }
    if (screen_str == "lobby_connect") {
      return RunDumpLobbyConnect(assets_dir, dump);
    }
    if (screen_str == "lobby") {
      return RunDumpLobby(assets_dir, dump);
    }
    if (screen_str == "lobby_gamecreate") {
      return RunDumpLobbyGameCreate(assets_dir, dump);
    }
    if (screen_str == "lobby_gamejoin") {
      return RunDumpLobbyGameJoin(assets_dir, dump);
    }
    if (screen_str == "lobby_gametech") {
      return RunDumpLobbyGameTech(assets_dir, dump);
    }
    if (screen_str == "lobby_gamesummary") {
      return RunDumpLobbyGameSummary(assets_dir, dump);
    }
    if (screen_str == "updating") {
      return RunDumpUpdating(assets_dir, dump);
    }
    return RunDump(assets_dir, dump);
  }

  // No dump dir — open a window and render once, then poll for quit.
  // (Not strictly required, but keeps the binary useful interactively.)
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *win =
      SDL_CreateWindow("Silencer Design", 640, 480, SDL_WINDOW_RESIZABLE);
  if (!win) {
    std::fprintf(stderr, "no window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  // Just clear the window and exit immediately; the meaningful path is
  // dump mode.
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
