# Silencer design system — main-menu subset

This is a focused redo of the design spec, scoped to **only** what the
real client renders on the main menu. The previous monolithic
`docs/design-system.md` (now archived as `docs/design-system.md.archive`)
tried to cover every screen at once and we discovered, while
hydrating it, that several foundational claims were wrong or
incomplete. Rather than fix the whole doc in-place, we are
rebuilding it one screen at a time, starting here.

## Goal

Produce a per-component spec faithful enough that an
implementation built **only from these docs + the binary assets in
`shared/assets/`** can render a main menu pixel-close to the real
client (`clients/silencer`).

When this subset is solid, we'll add the next screen (likely Lobby)
by extending these component docs and adding a new screen-composition
doc.

## Scope

Only the components used by the real client's
`Game::CreateMainMenuInterface` (`clients/silencer/src/game.cpp:2266`):

| Doc | Covers |
| --- | ------ |
| [palette.md](palette.md) | `PALETTE.BIN` format and the 11 sub-palettes — including which one is active during MAINMENU |
| [sprite-banks.md](sprite-banks.md) | `BIN_SPR.DAT` index, per-bank `SPR_NNN.BIN` headers, RLE codec, anchor/blit |
| [font.md](font.md) | Font sprite banks 132–136, advance widths, `DrawText` |
| [tick.md](tick.md) | 24 Hz simulation tick, `state_i` |
| [widget-overlay.md](widget-overlay.md) | Sprite-mode and text-mode `Overlay` (used for bg, logo, version) |
| [widget-button.md](widget-button.md) | `Button` widget — `B196x33` only (the menu uses no other variant) |
| [widget-interface.md](widget-interface.md) | `Interface` container, focus, mouse/keyboard dispatch |
| [screen-main-menu.md](screen-main-menu.md) | Composition: how the above wire together |

## Out of scope (for now)

- Other button variants (`B112x33`, `B220x33`, `B236x27`, `B52x21`,
  `B156x21`, `BCHECKBOX`) — covered later.
- Other widgets: TextInput, TextBox, SelectBox, ScrollBar, Toggle,
  Modal, Loading bar, HUD bars, Minimap, Buy menu, Chat overlay, etc.
- Effect transforms beyond what the menu uses (`EffectBrightness`
  only; no `EffectColor`, no `EffectRampColor`, no team color, etc.).
- Tile banks (`BIN_TIL.DAT`).
- In-game palette ranges, parallax sky swapping.

## Hydrations

Three reference renderings live alongside this spec:

| Dir | Status |
| --- | ------ |
| `shared/design/sdl3/` | C++17 + SDL3, indexed framebuffer through `palettes_[1]` (menu palette). Primary QA target. |
| `shared/design/html/` | Static HTML/CSS approximation (palette swatches + placeholders for sprite art). |
| `shared/design/raylib/` | raylib + C99. |

Each hydration's `CLAUDE.md` documents what's faithful vs. faked.

## Authoritative sources

The implementation lives in `clients/silencer/src/`. When this spec
disagrees with the source, the source wins — file an issue and fix
the spec. Specific files cited per doc.
