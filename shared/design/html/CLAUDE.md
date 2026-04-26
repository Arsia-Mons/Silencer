# shared/design/html — Silencer Design System (HTML hydration)

A static HTML/CSS demo of every component and screen in
[`docs/design-system.md`](../../../docs/design-system.md). No build step,
no dependencies — open `index.html` directly in any modern browser.

## How to view

```sh
open shared/design/html/index.html
```

`index.html` links to one page per component and per screen. Each page
renders inside a fixed `640 × 480` `.canvas` container (the game's logical
resolution); use the **Scale: 1× / 2×** toggle in the top-right corner to
switch to 2× zoom.

## Layout

| Path | Contents |
| ---- | -------- |
| `index.html` | Landing page with links to every component + screen. |
| `styles.css` | Shared design tokens — palette CSS variables, font sizes, button/textinput/etc. base styles. |
| `silencer.js` | Vanilla-JS interactivity: scale toggle, button hover state machine, toggle radio groups, Tab/Shift-Tab focus cycling, Enter/Escape routing. |
| `components/*.html` | One page per reusable widget and per screen-composition (Button, Toggle, TextInput, …, Buy Menu, HUD Bars, Minimap, …). |
| `screens/*.html` | One page per top-level screen from §Appendix B (Main Menu, Lobby Connect, Lobby, Game Create, Game Summary, Options × 4, In-Game HUD, Loading). |

## What is faked

- **Bitmap glyphs.** The original glyphs live in `shared/assets/bin_spr/SPR_132.BIN`–`SPR_136.BIN` as
  RLE-compressed 8-bit indexed sprites. We can't decode those in pure HTML, so
  every text element renders in the [VT323](https://fonts.google.com/specimen/VT323)
  Google web font as a near-pixel monospace substitute. The character advances
  (4/6/7/8/9/10/11/12/13/15/25 px depending on context) are approximated by
  CSS `font-size`, not enforced per-glyph.
- **Sprite art.** Agency icons (bank 181), HUD frame art (bank 94/95),
  buy-menu chrome (bank 102), button backplates (banks 6/7), modal background
  (bank 40), the horizontal-stretch chat panel (bank 188), animated overlays
  (banks 54/57/58/171/208/222), hit-flash/shield/hacking effect overlays
  (banks 153/177/178), and team HUD indicator dots (bank 103) are all rendered
  as solid-color rectangles labeled `SPR <bank>#<idx>` in the same palette
  ramp as the original tinting.
- **State counters.** The 23.8 Hz `Renderer::state_i` clock isn't simulated.
  The caret blink uses the browser's native `caret-color` cadence; the buy-menu
  selection pulse uses a CSS keyframes animation tuned to the documented 16-tick
  (~672 ms) period. Per-character announcement brightness pulses are not
  animated — characters render at their final brightness.
- **Sounds.** `whoom.wav` triggers are documented but no audio is played.

## What is faithful

- **Palette colors** are the exact hex values from §Color System, exposed as
  CSS custom properties (`--c-fc0000`, `--sem-title-text`, etc.).
- **Coordinates and dimensions** are in 640 × 480 logical pixels, matching the
  values in the spec (e.g., the chat overlay rect is exactly `(400, 280) 231 × 30`;
  the loading bar is exactly `500 × 20` centered on `(320, 240)`).
- **Button variants** match the documented `(W × H, font bank, text advance,
  text Y-offset)` table — including the brightness-only animation of `B156x21`
  and the no-animation `BCHECKBOX`.
- **Button state machine.** Hover and Tab focus both trigger
  `INACTIVE → ACTIVATING → ACTIVE`; the CSS `filter: brightness(1.0625)` matches
  the documented endpoint (128 → 136). The 4-tick (~168 ms) ramp is
  approximated by a `transition: filter 168ms`.
- **Tab order, Enter, Escape** routing within a `.canvas` mirrors the
  Interface focus manager. `[data-escape-button]` marks the button bound to
  Escape (the documented `buttonescape`).
- **Toggle radio groups** (`data-toggle-set="..."`) deselect siblings on
  click — matching the `set > 0` mutual-exclusion logic.
- **Modal dialogs** show both variants — with OK button (centered text at
  y=200, button at `(242, 230)`) and the no-button async-status form (text at
  y=218).
- **Layout coordinates** for the Lobby (Header, Character panel, Chat panel,
  Game List), the In-Game HUD (HUD bars, minimap, team HUD, chat overlay,
  status messages, top message, ammo, credits, inventory slots), and Buy Menu
  match the absolute coordinates in §Layout & Spacing and the per-component
  sections.

## What's missing / out of scope

- Sprite-bank pixel content — see "What is faked" above.
- The Tech Menu screen variant (uses the same SelectBox shape as the Buy Menu;
  not separately rendered).
- The Update Interface, Password Dialog, and Map Preview modals (composed from
  the same primitives as Modal Dialog).
- Replay scaling of animation timings.
- The dynamic parallax sky-color swap (palette indices 226–255).

If you need pixel-exact sprite content, point a port at the actual
`shared/assets/` binaries and decode with the format documented in
[`docs/design-system.md` → Asset Formats](../../../docs/design-system.md#asset-formats).

## Verifying

The pages are pure static HTML — no server needed. Just `open
shared/design/html/index.html` and click around. Every page links back to
the index in the top-left.
