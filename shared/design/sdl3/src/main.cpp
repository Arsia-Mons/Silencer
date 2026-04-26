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
    const char *connector;
  };
  const std::array<ControlsRow, 6> rows = {{
      {"Move Up:", "OR"},
      {"Move Down:", "OR"},
      {"Move Left:", "OR"},
      {"Move Right:", "OR"},
      {"Aim Up/Left:", "AND"},
      {"Aim Up/Right:", "AND"},
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

    // Key1 button chrome (no text — that's C3).
    if (sprites.Has(6, kB112Base)) {
      BlitSprite(fb, sprites.Get(6, kB112Base), kKey1X, by, nullptr);
    }

    // BNONE connector text, centered within (kConnectorX, ty, w=40, h=30).
    {
      int conn_len = static_cast<int>(std::strlen(rows[i].connector));
      int conn_xoff = (kConnectorW - conn_len * kConnectorAdvance) / 2;
      DrawText(fb, kConnectorX + conn_xoff, ty, rows[i].connector,
               /*bank=*/134, /*advance=*/kConnectorAdvance, sprites, palette,
               kSubMenu, /*brightness=*/128);
    }

    // Key2 button chrome (no text — that's C3).
    if (sprites.Has(6, kB112Base)) {
      BlitSprite(fb, sprites.Get(6, kB112Base), kKey2X, by, nullptr);
    }
  }

  // C2 gate stops here: bg + frame panel + title + 6-row form structure.
  // C3 will populate key1/key2 button text; C4 the scrollbar thumb;
  // C5 the Save/Cancel buttons.

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, kSubMenu);
  std::fprintf(stderr, "wrote %s (options_controls)\n", out.c_str());

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
