# Restore the original (origin/main) visual design inside the cppx UI engine

**Date:** 2026-06-01
**Branch:** `hv/cppx-migration-cc`
**Linear epic:** SIL — "Restore the original (origin/main) visual design inside the cppx UI engine"

## Problem

The cppx UI migration (SIL-5..83) replaced Silencer's Clay immediate-mode UI
with the retained cppx engine. That framework swap was correct and stays. But
SIL-17 also shipped a **deliberate visual redesign** ("modern slate redesign":
slate surfaces, single cool-blue accent `#60A5FA`, gradient rounded-rect
buttons, OTF text at arbitrary sizes, **zero sprite art**). The result is a
product-layer **visual regression** away from the origin/main design.

This effort restores the **original origin/main look** while keeping every cppx
convention. The engine, Yoga layout adapter, Image/nine-slice draw IR, SDL-free
boundaries, and OTF font faces are all correct and unchanged.

`~/repos/ui` is the reference for **how** to author correctly. **origin/main**
(`af4c50c5`, the Clay UI) is the **what** — the design spec. There is no 1:1
code mapping between them; the goal is **visually identical rendered output**,
not identical component functions.

## Original design target

- **Palette** (legacy `Colors.h`): Background `#000000`, Panel `#10141C`,
  PanelBorder `#565E6F`, Text `#E0E7F1`, Accent `#9FC9FF` (cool blue),
  Danger `#DD5048`.
- **Buttons**: green-outlined **oval sprites** (bank 6) — the green/bevel is
  baked pixels, not a color constant; 5 brightness phases on focus.
- **Chrome/art**: starfield+planet background (bank 6 idx0), panel chrome
  (bank 7), dialog frames (bank 40 idx2/4), animated SILENCER logo
  (bank 208 frames 29–60), agency emblems (bank 181), HUD status sprites
  (banks 94/95/96/97/102/103/187/188).
- **Fonts**: legacy bitmap-derived OTF faces (`silencer-{ui,ui-large,title,tiny}.otf`,
  from banks 133/134/136/132) at native ems.

## Strategy — **mixed**, foundation-first

Per element, the right approach is genuinely mixed (the five feasibility gates
made the split unambiguous):

- **Sprite-faithful** (vector cannot reach parity): green oval buttons, the
  logo, agency emblems, dialog frames, lobby/panel chrome, buy/tech + ready
  sprites, and the HUD status cluster (their identity *is* layered 8-bit art).
  All flow through the sanctioned **Image/nine-slice IR** — never
  `DrawCommandKind::Custom`.
- **Vector-approximation** (parity is structural): panel fills + 1px borders,
  the palette swap itself, all OTF text, the scoreboard panel. The GPU
  border-blur glow has no IR equivalent and is **dropped** (crisp 1px borders).

### Feasibility gates (verified)

1. **Assets — CONFIRMED.** Banks 6/7/40/208/181/132–136 + `PALETTE.BIN` exist,
   are baked into the macOS bundle, and load at runtime (verified by booting the
   headless binary). `bake_indexed_rgba`'s inputs match the runtime types 1:1
   (`Surface::pixels` + `Palette::GetColors()`). No new asset pipeline needed.
2. **Image/animation path — PARTIAL.** The Image/nine-slice IR + executor are
   built, **but no production sprite seam exists** — `bake_indexed_rgba` has
   **zero callers**; `upload_rgba`/`PipelineHost.textures_` are demo-only. The
   entire migrated UI is procedural paint. Building the bake→upload→`texture_id`
   provider seam is the heaviest shared dependency. Two hard blockers:
   **64-texture cap** (no atlasing / no sub-rect UV) and **no clock/time hook**.
3. **Fonts — PARTIAL.** Faces exist and derive from the banks, but the product
   layer never sets `font_id` (everything renders in Body face at arbitrary
   points). Need face-per-role + native-em sizes. Bank-135 (the dominant
   title/heading face) was never extracted to OTF → titles are approximate
   unless `silencer-135.otf` is generated (optional).
4. **Theme — PARTIAL.** Palette swap is a pure constant edit in
   `app_theme.cpp` + `tokens.h`. The green oval **requires** the sprite path
   (no color constant reproduces it).
5. **Layout — CONFIRMED.** Yoga expresses the legacy main-menu layout
   (right-anchored staggered button stack, corner-pinned footer) via
   flex + margins + absolute insets.

## Visual-regression goldens — **two tiers** (user requirement)

Goldens validate **rendered pixels, not code structure**.

- **Whole-screen goldens** — captured from origin/main; the parity target per
  screen. The current goldens encode the **wrong slate design** and must be
  fully re-baselined.
- **Component/primitive goldens** — buttons (each state: idle/focus/hover/
  pressed/disabled), panels, inputs, dialog/modal frames, the oval button,
  chrome borders, text per role/size — rendered from origin/main as goldens,
  diffed against the cppx component gallery (`ui_gallery` op + `gallery.cppx`
  already exist). Component-level diff catches subtle drift (radius, glow phase,
  padding, font weight) that a full-screen diff washes out.

## Tickets

### Foundation (gate every surface)

| # | Title | Pts | Deps |
|---|---|---|---|
| F1 | Restore legacy cool-blue palette in `app_theme.cpp` + `tokens.h` | 3 | — |
| F2 | Font face-per-role + native-em size parity | 3 | — |
| F3 | **Renderer-side sprite-bake → `texture_id` provider seam (core enabler)** | 8 | — |
| F4 | `BackgroundImage` style-patch helper + image-only variant rule | 2 | F3 |
| F5 | Green oval sprite button variant (bank 6, static 2-state) | 5 | F3,F4 |
| F6 | Chrome sprite button variant (bank 7 idx24, static 2-state) | 3 | F3,F4 |
| F7 | Sprite chrome panel + dialog-frame variants (bank 7, bank 40) | 3 | F3,F4 |
| F8 | Full-screen starfield background capability (bank 6 idx0) | 2 | F3,F4 |

### Foundation follow-ups (unblock animation + HUD)

| # | Title | Pts | Deps |
|---|---|---|---|
| F9 | IR source-rect / UV field for partial-fill + atlasing | 5 | F3 |
| F10 | Component clock/time hook (`use_clock`) for animation | 5 | F3 |
| F11 | *(optional)* Generate `silencer-135.otf` for title/heading parity | 3 | F2 |

### Surfaces

| # | Title | Pts | Deps |
|---|---|---|---|
| S1 | Main Menu restore (starfield + static logo + oval buttons + layout + footer) | 5 | F1,F2,F5,F8 |
| S2 | Lobby Connect restore (bank-7 panel + chrome buttons + bare inputs + log) | 3 | F1,F2,F7,F6 |
| S3 | Modals (Message + Password) — bank-40 dialogs + OK chrome + masking | 5 | F1,F2,F7,F6 |
| S4 | Update Screen restore (bank-40 dialog + state-conditional tree) | 3 | F1,F2,F7,F6 |
| S5 | Options cluster skin (screen/audio/display) — starfield + ovals + toggles | 5 | F1,F2,F5,F7,F8 |
| S6 | Character Create — vector pass (palette/font + two-column detail + AgencyDef) | 8 | F1,F2 |
| S7 | Character Create — sprite layer (chrome + oval rows + alias dialog + emblems) | 8 | S6,F5,F7,F8 |
| S8 | Mission Summary — vector pass (two-column + full stats + labels) | 5 | F1,F2,F5,F8 |
| S9 | Lobby — vector-first parity (panels + title bar + chat split + roster + card) | 8 | F1,F2,F5 |
| S10 | Lobby — sprite layer (chrome bg/panels + crest + ready/tech sprites + tree) | 8 | S9,F7,F5 |
| S11 | In-Game HUD restore (status cluster + buy/tech + team strip + scoreboard) | 8 | F1,F2,F3,F9,F10 |
| S12 | *(follow-up)* Animated logo + 5-phase button brightness + caret blink | 5 | F10,F9,S1,F5 |
| S13 | *(decision-gated)* Options Controls feature-parity + audio volume IA revert | 8 | S5,F7 |

### Verification

| # | Title | Pts | Deps |
|---|---|---|---|
| V1 | Re-baseline visual-regression goldens (both tiers) + confirm vs origin/main | 5 | all surfaces |

## Resolved (2026-06-01 — full-refactor directive)

**origin/main is the SOLE visual authority. No current cppx visual quality is
authoritative. V1 = complete refactor to origin/main parity on every surface — no
fallback-as-final, no kept migrated divergences.**

1. **Scope** — full parity, including dropped *functionality* (lobby tech tree, full
   roster, ~70-line mission stats, full Options IA) — not just the skin.
2. **Main-menu labels** — restore legacy *Connect To Lobby / Tutorial / Options / Exit*.
3. **Options IA** — restore the full legacy IA (scrollable keybind table + AND/OR +
   preset oval; remove the invented volume slider). **SIL-108 is no longer gated.**
4. **Animation** — required for final parity; sequenced behind the clock/atlas
   foundations and delivered by S12/SIL-107. Static/2-state are interim stopgaps.
5. **bank-135 OTF** (F11/SIL-95) — **regenerate** for glyph-exact title/heading parity.
6. **Fixed-dialog sizing** — match origin/main visually (pick native-pixel vs
   viewport-proportional by whichever reproduces the legacy look).
7. **Scrolling** — real scroll viewports are in scope (new **SIL-111**); "clipped,
   no-scroll" is not a final state.

## Per-ticket workflow (agents: follow this for every SIL-84 ticket)

When a ticket's work is done and verified, do NOT mark it **Done** and do NOT
block waiting for approval. Instead:

1. Capture the control-socket verification screenshot (headless boot → `screenshot`).
2. Upload that screenshot to the Linear ticket as an attachment, and Discord-DM
   the same image to the user (proactive progress).
3. Set the ticket to status **In Progress** + the **"In Review"** label (the
   team has no "In Review" workflow status; the label is the proxy).
4. Immediately move on to the next ticket — the user reviews asynchronously and
   comments approval on the ticket; on approval it flips to **Done** (and the
   label is dropped). Approval is non-blocking: keep grinding the backlog.

Commits land directly on `hv/cppx-migration-cc` (the long-lived migration
branch), one commit per ticket, message ending with the `SIL-NN` id.

## Risks / deferred

- **Texture-cap (64)**: a cross-surface texture budget must be computed before
  S11/S12 (folded into F3's acceptance). Atlasing (F9) is the relief valve.
- **No aspect-cover backgrounds**: v1 stretches (legacy also stretched); true
  cover needs F9.
- **Scoreboard named roster** is empty at the model layer — wiring the
  `ingameusers`/lobby seam is a prerequisite inside S11.
- **No scroll viewports** — only `ClipPush/Pop`; v1 ships fixed-height clipped
  regions (lobby chat, game-select, mission stats).
- **Password masking** is unbuilt in the cppx Input — net-new work folded into S3.
- **Team-color emblem remap** — a remapped-palette bake path (S11) is unspecified.
- **Per-glyph LegacyPalette color ramps** (chat/tech/HUD) — token-approximate.
- **GPU border-blur glow** — dropped (no IR equivalent).
