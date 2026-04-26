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
`shared/assets/`** can render the covered screens pixel-close to the
real client (`clients/silencer`).

Each new screen extends the component docs only as much as it
needs. Screens that don't introduce new widgets just add a
composition doc plus a row in the [QA dump](#qa-dump) table.

## Scope

The components needed by the real client's
`Game::CreateMainMenuInterface` (`clients/silencer/src/game.cpp:2266`)
and `Game::CreateOptionsInterface` (`game.cpp:2352`) — the same
substrate, two different compositions:

| Doc | Covers |
| --- | ------ |
| [palette.md](palette.md) | `PALETTE.BIN` format and the 11 sub-palettes — including which one is active during MAINMENU |
| [sprite-banks.md](sprite-banks.md) | `BIN_SPR.DAT` index, per-bank `SPR_NNN.BIN` headers, RLE codec, anchor/blit |
| [font.md](font.md) | Font sprite banks 132–136, advance widths, `DrawText` |
| [tick.md](tick.md) | 24 Hz simulation tick, `state_i` |
| [widget-overlay.md](widget-overlay.md) | Sprite-mode and text-mode `Overlay` (used for bg, logo, version) |
| [widget-button.md](widget-button.md) | `Button` widget — `B196x33`, `B112x33`, `B220x33`, `B52x21`, `BNONE` variants |
| [widget-textbox.md](widget-textbox.md) | `TextBox` — multi-line text display (chat / status log shape) |
| [widget-textinput.md](widget-textinput.md) | `TextInput` — single-line editor with blinking caret, optional password mode |
| [widget-scrollbar.md](widget-scrollbar.md) | `ScrollBar` — state holder for scroll position, optionally renders track + thumb |
| [widget-interface.md](widget-interface.md) | `Interface` container, focus, mouse/keyboard dispatch, scroll wiring |
| [screen-main-menu.md](screen-main-menu.md) | Composition: main menu (logo, buttons, version) |
| [screen-options.md](screen-options.md) | Composition: options sub-screen (four B196x33 buttons over the main-menu plate; inherits sub-palette 1 from MAINMENU without setting it) |
| [screen-options-controls.md](screen-options-controls.md) | Composition: configure-controls (six rows of keybinding triplets, scrollbar state, Save/Cancel) |
| [screen-options-display.md](screen-options-display.md) | Composition: display-options (Fullscreen + Smooth Scaling toggles, Save/Cancel) |
| [screen-options-audio.md](screen-options-audio.md) | Composition: audio-options (single Music toggle, Save/Cancel) |
| [screen-lobby-connect.md](screen-lobby-connect.md) | Composition: lobby login (TextBox status log, two TextInputs, Login/Cancel B52x21 buttons; first sub-palette-2 screen) |

## QA dump

The real client (`clients/silencer/src/game.cpp::Game::Present`)
exposes a framebuffer dump path gated by env vars:

| Env | Values | Effect |
| --- | ------ | ------ |
| `SILENCER_DUMP_PATH` | absolute file path | When set, write a 640×480 binary P6 PPM to this path once the target screen has reached steady state, then `exit(0)` |
| `SILENCER_DUMP_STATE` | `MAINMENU` (default), `OPTIONS`, `OPTIONSCONTROLS`, `OPTIONSDISPLAY`, `OPTIONSAUDIO`, `LOBBYCONNECT` | Selects which screen to dump. The binary navigates by chaining clicks: MAINMENU.{Options=uid 2, ConnectToLobby=uid 1}; OPTIONS.{Controls=uid 1, Display=uid 2, Audio=uid 3}. LOBBYCONNECT also waits for `Lobby::state == IDLE` before dumping so the TextBox content is deterministic. |

Hydrations should accept `SILENCER_DUMP_DIR` (writes one PPM per
registered screen) for parity. Visual A/B between the real and
hydration PPMs is the validation gate; see
[`.claude/skills/visual-regression-testing/SKILL.md`](../../.claude/skills/visual-regression-testing/SKILL.md).

## Out of scope (for now)

- Other button variants (`B236x27`, `B156x21`, `BCHECKBOX`) —
  covered when a screen needs them.
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
