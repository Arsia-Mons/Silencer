#pragma once

// Shared client-UI design tokens: the single source of app paint (one palette +
// the visual builders surfaces/text resolve through). Each component owns its
// variant->token switch locally. Colors are STRAIGHT alpha; the IR premultiplies
// at emit.

#include "ui/components/common.h"
#include "ui/style/style_patch.h"

#include <cstdint>

namespace silencer::tokens {

// ---- Surface backgrounds ----
constexpr ::ui::Color kSurfaceMenu = {0, 0, 0, 255};       // #000000
constexpr ::ui::Color kSurfaceGame = {0, 0, 0, 255};       // #000000
constexpr ::ui::Color kSurfaceOverlay = {6, 16, 8, 238};   // near-black green glass
constexpr ::ui::Color kSurfacePanel = {6, 16, 8, 235};     // near-black green glass
constexpr ::ui::Color kSurfaceHeroPanel = {6, 16, 8, 238};
constexpr ::ui::Color kSurfaceHudBand = {4, 12, 6, 238};

// ---- Borders ----
constexpr ::ui::Color kBorderPanel = {46, 125, 69, 255};   // #2E7D45 green-dim
constexpr ::ui::Color kBorderHeroPanel = {46, 139, 46, 255}; // #2E8B2E chrome-border
constexpr ::ui::Color kBorderHudBand = {46, 125, 69, 255};

// ---- Accent / semantic action colors (AppButton variant fills) ----
// Each variant owns a base + hover (lighter) + pressed (darker) triple so its
// patch supplies its own interaction states.
constexpr ::ui::Color kAccent = {92, 208, 92, 255};         // #5CD05C green-bright
constexpr ::ui::Color kAccentBorder = {60, 255, 60, 255};   // #3CFF3C focus stroke
constexpr ::ui::Color kAccentHover = {136, 232, 136, 255};  // lighter on hover
constexpr ::ui::Color kAccentHoverBorder = {60, 255, 60, 255};
constexpr ::ui::Color kAccentPressed = {79, 184, 103, 255}; // darker on press
constexpr ::ui::Color kAccentPressedBorder = {92, 208, 92, 255};
// Select-map / list-row selection bar. origin draws the selected row as a solid
// filled rectangle (renderer.cpp SELECTBOX: DrawFilledRectangle(..., 180)) — the
// lobby palette-2 index 180 = dark red (72,16,0), opaque, no border. NOT an
// outline box; the fill IS the selection feedback.
constexpr ::ui::Color kRowSelectFill = {72, 16, 0, 255};    // legacy palette-2 idx 180
constexpr ::ui::Color kDanger = {221, 80, 72, 255};         // #DD5048
constexpr ::ui::Color kDangerBorder = {240, 120, 112, 255}; // brighter danger edge
constexpr ::ui::Color kDangerHover = {236, 108, 100, 255};  // lighter on hover
constexpr ::ui::Color kDangerHoverBorder = {248, 150, 142, 255};
constexpr ::ui::Color kDangerPressed = {190, 58, 52, 255};  // darker on press
constexpr ::ui::Color kDangerPressedBorder = {220, 96, 88, 255};

// ---- Text (green-phosphor family — measured from the v00058 goldens) ----
// The glyph atlas is a white coverage mask, so these token colors ARE the
// rendered text color (tinted at draw time).
constexpr ::ui::Color kTextTitle = {24, 124, 20, 255};        // standard green
constexpr ::ui::Color kTextHeroTitle = {48, 168, 44, 255};    // brand/hero (brighter)
constexpr ::ui::Color kTextSubtitle = {84, 156, 104, 255};    // description prose (lighter)
constexpr ::ui::Color kTextDialogTitle = {24, 124, 20, 255};
constexpr ::ui::Color kTextBody = {24, 124, 20, 255};
constexpr ::ui::Color kTextBodyMuted = {16, 96, 8, 255};      // dim (footer/OR/inactive)
constexpr ::ui::Color kTextWeaponName = {24, 124, 20, 255};
constexpr ::ui::Color kTextWeaponDetail = {24, 124, 20, 255};
// Lobby presence group headers ("In Lobby"/"Pregame"): origin draws them at
// brightness 160 (chat_panel PushEntry 128+32) — primary of that ramp.
constexpr ::ui::Color kTextPresenceHeader = {92, 148, 92, 255};
// Staging roster level badges ("L:0"): origin Tiny text LegacyPalette(170) —
// primary of that ramp (orange).
constexpr ::ui::Color kTextRosterLevel = {136, 84, 48, 255};
// Tech grid: non-interactable rows at origin brightness 64; the slots-left
// readout at LegacyPalette(129, 144, ramp) (grey-white).
constexpr ::ui::Color kTextTechDim = {8, 56, 8, 255};
constexpr ::ui::Color kTextTechSlots = {184, 184, 184, 255};
constexpr ::ui::Color kTextWeaponDetailOff = {10, 72, 8, 255}; // disabled
constexpr ::ui::Color kTextHud = {61, 232, 61, 255};          // #3DE83D hud-green

constexpr ::ui::Color kTextBrand = {152, 28, 28, 255};     // "Silencer" wordmark (red)
constexpr ::ui::Color kTextVersion = {140, 64, 8, 255};    // build version (amber)
constexpr ::ui::Color kTextAgentName = {40, 96, 200, 255}; // agent names (cornflower blue)
constexpr ::ui::Color kTextProse = {198, 198, 198, 255};   // white prose (agency detail/description)

// ---- Vector fallbacks (drawn only while a chrome sprite is not yet baked;
// never on the golden path) ----
constexpr ::ui::Color kControlFallbackFill = {6, 16, 8, 255};   // oval/chrome button body
constexpr ::ui::Color kDialogFallbackFill = {12, 4, 4, 255};    // connect dialog panel
constexpr ::ui::Color kEmblemFallbackFill = {40, 70, 150, 255}; // agent-card emblem slot

// ---- In-game HUD exact-color text keys (variant faces baked on palette
// page 0 — origin LegacyPalette(color, brightness)).
// The KEY is an arbitrary unique id; the baked pixels are origin's.
constexpr ::ui::Color kTextHudDefault = {61, 233, 61, 255};  // (0, 128)
constexpr ::ui::Color kTextHudBright = {62, 234, 62, 255};   // (0, 136) chat/hack/trace
constexpr ::ui::Color kTextHudDim32 = {63, 235, 63, 255};    // (0, 32) inv letters
constexpr ::ui::Color kTextHudDim64 = {64, 236, 64, 255};    // (0, 64) buy rows dim
constexpr ::ui::Color kTextHudAmmo = {65, 237, 65, 255};     // (0, 128, alphaed) ammo
constexpr ::ui::Color kTextHudCredits = {66, 238, 66, 255};  // (202, 128)
constexpr ::ui::Color kTextHudHealth = {67, 239, 67, 255};   // (161, 128)
// Generic key for pulse-driven HUD text (center-message reveal, buy-row
// selection): LegacyPalette(color_idx, brightness) baked under {idx, b, 7}.
constexpr ::ui::Color hud_text_key(uint8_t color_idx, uint8_t brightness) {
  return {color_idx, brightness, 7, 255};
}

// ---- In-game HUD LCD palette (overlay over live world) ----
// SEPARATE from the menu green family: these read against the live world, so
// they run brighter/saturated.
constexpr ::ui::Color kHudGreen = {61, 232, 61, 255};    // #3DE83D own data/chat/scoreboard
constexpr ::ui::Color kHudGreenDim = {30, 122, 30, 255}; // #1E7A1E chat body / inactive
constexpr ::ui::Color kHudBlue = {58, 107, 255, 255};    // #3A6BFF economy/files/credits/dots
constexpr ::ui::Color kHudRed = {224, 48, 48, 255};      // #E03030 FATIGUE/health/2ndary ammo
constexpr ::ui::Color kHudAmber = {160, 86, 30, 255};    // #A0561E radar schematic
constexpr ::ui::Color kHudBlack = {0, 0, 0, 210};        // ~85% scoreboard bar / radar viewport
constexpr ::ui::Color kHudPanelFill = {0, 8, 2, 200};    // translucent green-black HUD well
constexpr ::ui::Color kHudClear = {0, 0, 0, 0};          // chromeless-field transparent
constexpr ::ui::Color kHudListDim = {0, 0, 0, 128};      // F1 player-list dim (palette idx 0 @ 50%)
constexpr ::ui::Color kBlipAlly = {255, 255, 255, 255};
constexpr ::ui::Color kBlipEnemy = {224, 48, 48, 255};

// ---- Font faces (font_id; see render/cppx_ui/font_registry.h FaceId) ----
constexpr uint16_t kFaceBody = 0;    // bank 133, advance 6  (origin Body)
constexpr uint16_t kFaceLarge = 1;   // bank 134, advance 8  (origin Heading)
constexpr uint16_t kFaceTitle = 2;   // bank 136, advance 16 (origin Prompt)
constexpr uint16_t kFaceTiny = 3;    // bank 132, advance 4  (origin Tiny)
constexpr uint16_t kFaceHeading = 4; // bank 135, advance 11 (origin Title — the
                                     // dominant button-label/title face)
// The same bank at a different advance needs its own face — the glyph atlas
// advance is fixed at bake time.
constexpr uint16_t kFaceScreenTitle = 5; // bank 135, advance 12 (origin ScreenTitle)
constexpr uint16_t kFaceFooter = 6;      // bank 133, advance 11 (origin Footer)
constexpr uint16_t kFaceBodySm = 7;      // bank 133, advance 7  (origin BodySm)
// In-game HUD faces (origin TextSize table, text.cpp ResolveTextRenderStyle).
// HudCounter (bank 135, advance 12) is the same tracking as ScreenTitle.
constexpr uint16_t kFaceHudCounter = 5;
constexpr uint16_t kFaceTinyCounter = 8;      // bank 132, advance 6
constexpr uint16_t kFaceMessageHeading = 9;   // bank 134, advance 10
constexpr uint16_t kFaceMessageTitle = 10;    // bank 136, advance 25
constexpr uint16_t kFaceMessageSubtitle = 11; // bank 135, advance 13

// ---- Glyph cell sizes (logical points) + line heights ----
// Text renders from the legacy bitmap glyph banks (origin/main parity), NOT TTF.
// font_size is the target cell height in logical points = the origin bank
// lineHeight * 1.5 EXACTLY (the 720-tall logical canvas is origin's 480 * 1.5);
// the glyph executor scales the native art by font_size/lineHeight. Fractional
// sizes are required — rounding 28.5 down to 28 renders every glyph ~2% narrow.
constexpr float kFontTitle = 34.5f;  // bank 136 hero/big prompt (23 * 1.5)
constexpr float kLineTitle = 34.5f;
constexpr float kFontHeading = 28.5f; // bank 135 titles + button labels (19 * 1.5)
constexpr float kLineHeading = 28.5f;
constexpr float kFontLarge = 22.5f;  // bank 134 sub-headings (15 * 1.5)
constexpr float kLineLarge = 22.5f;
constexpr float kFontBodyEm = 16.5f; // bank 133 body/log/detail (11 * 1.5)
constexpr float kLineBody = 16.5f;
constexpr float kFontTiny = 10.5f;   // bank 132 tiny (7 * 1.5)
constexpr float kLineTiny = 10.5f;

// ---- Border widths ----
constexpr float kBorderWidth = 1.0f;
constexpr float kBorderWidthSelected = 2.0f;

// ---- Patch builders (sparse overrides) ----

// Solid-fill surface (no border).
inline ::ui::StylePatch fill_patch(::ui::Color background) {
  return ::ui::patch().background(background);
}

// Solid-fill surface with a uniform border (width feeds layout via
// LayoutStyle.border_width; the color is carried here on all four sides).
inline ::ui::StylePatch panel_patch(::ui::Color background, ::ui::Color border,
                                    float border_width = kBorderWidth,
                                    float corner_radius = 0.0f) {
  return ::ui::patch()
      .background(background)
      .corner_radius(corner_radius)
      .border(::ui::Border{
          {border_width, border_width, border_width, border_width},
          {border, border, border, border}});
}

// Sprite-backed surface paint. EXPLICITLY clears fill/gradient/border/rounding
// so the role's opaque control paint can't slab behind the sprite's transparent
// corners. EVERY sprite-backed variant MUST paint through this helper, never the
// gradient-painting solid() path. texture_id 0 yields a transparent patch.
inline ::ui::StylePatch image_patch(::ui::BackgroundImage image) {
  return ::ui::patch()
      .image(image)
      .background(::ui::Color{0, 0, 0, 0}) // no opaque fill under the sprite
      .gradient(::ui::Gradient{})          // defeat the role's control gradient
      .border(::ui::Border{})              // sprite carries its own edge
      .outline(::ui::Outline{})            // no vector focus ring; focus = sprite brightness
      .corner_radius(0.f);                 // the sprite shape is authored, not rounded
}
inline ::ui::StylePatch image_patch(uint32_t texture_id,
                                    ::ui::Color tint = {255, 255, 255, 255},
                                    ::ui::SideWidths nine_slice = {},
                                    bool flip_h = false) {
  ::ui::BackgroundImage img{texture_id, tint, nine_slice};
  img.flip_h = flip_h;
  return image_patch(img);
}

// Full-bleed backdrop: aspect-preserving centered-crop (CSS cover) so a 4:3
// sprite isn't stretched on 16:9.
inline ::ui::StylePatch image_patch_cover(uint32_t texture_id) {
  ::ui::BackgroundImage img{texture_id};
  img.fit = ::ui::ImageFit::Cover;
  return image_patch(img);
}

// Lobby panel strokes: golden-MEASURED flat pixels (not brightened to
// compensate for glow — origin renders these flat).
constexpr ::ui::Color kChromeStroke = {8, 84, 0, 255};    // idx216 connected frame
constexpr ::ui::Color kInsetStroke = {24, 124, 20, 255};  // idx220 inner well

// Hairline frame over a transparent interior. Width 1.34 logical = 2 device px.
inline ::ui::StylePatch frame_patch(::ui::Color stroke, float width = 1.34f) {
  return ::ui::patch()
      .background(::ui::Color{0, 0, 0, 0})
      .gradient(::ui::Gradient{})
      .corner_radius(0.0f)
      .border(::ui::Border{{width, width, width, width}, {stroke, stroke, stroke, stroke}});
}

// Side-masked hairline frame: a zero width suppresses that edge entirely, so the
// lobby stepped pane composes its connected chrome from open-sided rectangles.
inline ::ui::StylePatch frame_patch_sides(::ui::Color stroke, bool top,
                                          bool right, bool bottom, bool left,
                                          float width = 1.34f) {
  return ::ui::patch()
      .background(::ui::Color{0, 0, 0, 0})
      .gradient(::ui::Gradient{})
      .corner_radius(0.0f)
      .border(::ui::Border{{top ? width : 0.0f, right ? width : 0.0f,
                            bottom ? width : 0.0f, left ? width : 0.0f},
                           {stroke, stroke, stroke, stroke}});
}

// Text paint. line_height 0 falls back to the face's natural skip;
// reveal_boost/step drive the in-game typewriter brightness "pop" (0 = off).
inline ::ui::StylePatch text_patch(::ui::Color color, float font_size,
                                   uint16_t font_id = kFaceBody,
                                   float line_height = 0.f,
                                   uint8_t reveal_boost = 0,
                                   uint8_t reveal_step = 0) {
  return ::ui::patch().text(::ui::TextVisual{.color = color,
                                             .font_id = font_id,
                                             .font_size = font_size,
                                             .line_height = line_height,
                                             .reveal_boost = reveal_boost,
                                             .reveal_step = reveal_step});
}

// Chromeless paint: suppress the role's chrome AND the builder-injected
// focus-ring outline (chromeless gates it; clearing .outline() alone re-injects).
inline ::ui::StylePatch chromeless_clear_patch() {
  return ::ui::patch()
      .chromeless(true)
      .background(::ui::Color{0, 0, 0, 0})
      .gradient(::ui::Gradient{})
      .corner_radius(0.0f)
      .border(::ui::Border{})
      .outline(::ui::Outline{});
}

// Chromeless input field over a sprite's baked well. Same paint across
// rest/hover/focus.
inline ::ui::StyleStatePatch chromeless_field_style(::ui::Color caret,
                                                    ::ui::Color text_color) {
  ::ui::StylePatch p = chromeless_clear_patch();
  p.text = ::ui::opt(::ui::TextVisual{.color = text_color,
                                      .font_id = kFaceBody,
                                      .font_size = kFontBodyEm,
                                      .line_height = kLineBody});
  p.caret = ::ui::opt(::ui::Caret{caret, 1.5f});
  ::ui::StyleStatePatch s{};
  s.base = p;
  s.hover = p;
  s.focus_visible = p;
  return s;
}

} // namespace silencer::tokens
