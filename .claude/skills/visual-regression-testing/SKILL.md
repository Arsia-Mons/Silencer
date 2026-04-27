---
name: visual-regression-testing
description: Use when validating that a design hydration / port / candidate renderer produces visuals matching the real Silencer client, screenshot-comparing two renderers, debugging UI rendering mismatches, deciding whether docs/design/ is faithful enough to rebuild a screen from scratch, or adding visual regression tests for a new screen
---

# Visual regression testing for Silencer

The Silencer rendering pipeline (8-bit indexed framebuffer, 11 sub-palettes, RLE+tile sprite codec, 24 Hz simulation) has many layers where divergences can hide. This skill captures the loop we use to surface those divergences fast and assign each one to the correct layer.

## Why a project-specific skill

macOS Screen Recording perms, Stage Manager, Metal-rendered SDL windows, and the indexed-then-resolved palette path all conspire against generic screenshot tools. The conventions below were developed against this codebase; they don't generalize cleanly to RGB-native renderers.

## The loop

```
1. Pick ONE screen          (not "the UI"; not "the design system")
2. Add a dump path to both  (the reference renderer and the candidate)
3. Pin the animation        (deterministic state, never wallclock)
4. Render and dump          (indexed framebuffer → PPM via active palette)
5. Visually A/B             (eyeball; do NOT gate on pixel-equal yet)
6. Categorize the diff      (composition / palette / codec / tile arithmetic)
7. Fix ONE thing            (not a batch)
8. Re-dump                  (the diff tells you whether the fix worked)
9. Repeat                   (stop at "looks the same"; pixel-equal is later)
```

## Step 1 — Scope is non-negotiable

If someone asks for "visual regression testing for Silencer" and you start with the lobby + HUD + buy menu + main menu at once, you'll lose. **Pick one screen.** Strip every other screen out of the candidate so the file count drops and iteration cycles stay short. The main menu was the bootstrapping screen because its object inventory is tiny (one bg overlay, one logo overlay, one version overlay, four buttons) and it doesn't depend on networking, lobby state, or world simulation.

When a screen is validated, *then* extend.

## Step 2 — Dump from inside the binary, not from the OS

Both binaries should grow an env-gated framebuffer dump:

| Binary | Env var | Where | Output |
| --- | --- | --- | --- |
| `clients/silencer` (real client) | `SILENCER_DUMP_PATH=<file>` | inside `Game::Present` | one PPM at the given path, then `exit(0)` |
| `shared/design/<stack>/` (hydration) | `SILENCER_DUMP_DIR=<dir>` | inside the dump-mode harness in main | one PPM per registered screen, then exit |

The dump writes the **indexed framebuffer**, resolved through the **active sub-palette** at that moment, as a binary P6 PPM:

```c
fprintf(f, "P6\n%d %d\n255\n", w, h);
for (int p = 0; p < w * h; ++p) {
    Uint8 ix = framebuffer[p];
    unsigned char rgb[3] = { palette[ix].r, palette[ix].g, palette[ix].b };
    fwrite(rgb, 1, 3, f);
}
```

PPM → PNG for sharing: `sips -s format png foo.ppm --out foo.png`.

**Do not try `screencapture -l <wid>` against an SDL3 window on macOS Sequoia.** It returns "could not create image from window" because of ScreenCaptureKit obsoletion + Spaces + Metal-layer compositing. Even when it succeeds, the activate-other-app dance is racy. The in-process dump bypasses all of this.

## Step 3 — Pin to a deterministic state, not a frame count

Render frame counters drift with machine speed; simulation tick counters drift with run-loop scheduling. Pin to a **deterministic state of the scene**:

- Main menu: dump when the bank-208 logo overlay's `res_index == 60` (the steady-state frame after the fade-in / hold animation completes).
- Buy menu: pin to a specific selection-pulse phase.
- Lobby: pin to chat scrollback at a known length.

The pinning logic in `clients/silencer/src/game.cpp::Game::Present`:

```cpp
const char * dumppath = getenv("SILENCER_DUMP_PATH");
if (dumppath && state == MAINMENU && !stateisnew) {
    bool steady = false;
    for (auto * obj : world.objectlist) {
        if (!obj || obj->type != ObjectTypes::OVERLAY) continue;
        Overlay * ov = static_cast<Overlay *>(obj);
        if (ov->res_bank == 208 && ov->res_index == 60) { steady = true; break; }
    }
    if (steady) { /* write PPM, exit(0) */ }
}
```

The hydration's harness should drive the simulation by calling `Tick()` enough times to reach the same pinned state, *without* opening a window. Don't sleep; just iterate the tick loop in-process.

## Step 4 — Active sub-palette must match

Silencer ships 11 sub-palettes in `PALETTE.BIN` and switches between them per game state (`MAINMENU`→1, `LOBBY`→2, `INGAME`→0). The dump's RGB resolution must use the **same** sub-palette the screen renders under. A hydration that loads all 11 sub-palettes but resolves through palette 0 will produce a recognizable-but-wrong main menu — buttons appear in nearly-black instead of teal. This is the highest-leverage single bug; check it first when colors look uniformly off.

See `docs/design/palette.md` for the file layout (`4 + s × (768+4)` stride — the seek formula matters).

## Step 5 — Visually A/B, don't gate on pixel-equal

Two PPMs at 640×480 will differ in subpixel ways for benign reasons (animation timing edge cases, alpha-blit modes, font kerning rounding). Pixel-equal turns those nits into blockers and buries real bugs.

Eyeball both PPMs side-by-side. The signal you want is **categorical mismatch**, not subpixel mismatch. Pixel-equal is a goal for the *final* validation, not the iteration loop.

For sharing screenshots when SSH'd from a phone:

```
cd /tmp && python3 -m http.server 8765 --bind 0.0.0.0 &
# user opens http://<lan-or-tailscale-ip>:8765/foo.png in their phone browser
```

## Step 6 — Categorize divergences by layer

| Symptom in the diff | Layer | Where to look |
| --- | --- | --- |
| Object missing entirely | **Composition** (the screen) | The screen's `CreateXxxInterface` call list — the spec is missing an object, or the candidate skipped one |
| Colors uniformly wrong (same shapes, wrong tints) | **Palette** | Active sub-palette index; PALETTE.BIN seek formula |
| Shape garbled (rough outline OK, internals scrambled) | **Sprite codec** | RLE decoder; per-sprite header `+4` filler; `comp_size` lying for tile-mode sprites |
| Scrambled inside a single glyph or letter | **Tile arithmetic** | 64×64 tile traversal order; partial-edge tile width math |
| Object in wrong position by hundreds of pixels | **Sprite anchor** | `offset_x`/`offset_y` in sprite header; `(x - offset_x)` blit math |
| Same chars but different (mid-fade) | **Animation timing** | Pinning state isn't deterministic — see step 3 |

Each row maps to one file. Don't waste a turn investigating multiple at once.

## Step 7 — One change per iteration

The diff after each fix tells you whether the fix worked. If you change three things at once and the screenshot still looks wrong, you don't know which of the three was wrong, or whether two of them broke each other. Tedious but fastest.

## Falsifiability — the part that's easy to skip

If the same author writes the spec doc *and* the candidate renderer with the engine source open, the candidate "passes" the spec by accident. Both sides will share the same blind spots.

Real validation: spawn a subagent with no engine access, give it `docs/design/` + `shared/assets/` + SDL3 only, and ask it to build a working candidate. If its dump matches the reference, the spec is faithful. If not, the gaps the subagent flagged are the spec's real gaps. The two phases (spec authoring, spec validation) must be separated by author and context.

This is why `clients/silencer/` is off-limits to spec-only builds.

## Anti-patterns

- **`screencapture -l <window-id>`** on macOS Sequoia. It fails. Skip directly to the in-process dump.
- **`activate_application` then screencapture.** macOS 14+ blocks programmatic activation across-app for unsigned/unprivileged callers. The `activate()` call returns success and does nothing.
- **Dumping after N render frames.** Render rate is machine-dependent; the candidate and reference will pin to different animation states.
- **Resolving framebuffer to RGBA inside the engine and dumping that.** Hides palette layer divergences. Always dump indexed → resolve via the same palette path the live render uses.
- **"Let me just check `clients/silencer/` real quick"** during a spec-only build. The spec gap you'd close that way is exactly the gap you're trying to surface. Stop, document, guess.
- **Pixel-equal as an early gate.** It's the *last* gate, not the first.
- **Whole-design-system iteration.** One screen at a time.

## Minimum file inventory for a new hydration

A spec-only hydration needs surprisingly little to render the main menu:

```
shared/design/<stack>/
  CMakeLists.txt
  src/
    main.cpp           dump-mode harness + tick loop
    palette.{h,cpp}    PALETTE.BIN loader, SetActive, IndexedToRgb
    sprite.{h,cpp}     BIN_SPR.DAT index, SPR_NNN.BIN load, RLE codec, blit
    font.{h,cpp}       glyph blit using sprite banks 132–136
    widgets/
      widget.h         shared base
      overlay.{h,cpp}  sprite + text Overlay (incl. bank-208 fade animation)
      button.{h,cpp}   B196x33 only (other variants until needed)
      interface.{h,cpp}  Tab focus, Enter/Esc shortcuts
      primitives.{h,cpp} Clear (rest optional)
    screens/
      screen.h
      screen_main_menu.cpp   composition only
```

## When to stop iterating on a screen

- The two PPMs are visually equivalent to a casual reader. Tiny version-string differences and one-pixel kerning differences are acceptable.
- The remaining diffs (if any) have been categorized as "animation phase" or "transient text content" and are demonstrably non-structural.
- The spec docs have been updated to reflect every fix made during the loop. *This is a checklist item, not an aspiration.*

## Reference dumps from this project

- Real client main menu: `/tmp/real_dump.png` (see also `/tmp/real_dump.ppm`)
- Hydration target: same shape, mostly-equivalent palette indices, identical logo frame at idx 60
- The single accepted divergence is the version string in the bottom-left (compile-time constant differing between client and hydration)

## See also

- `docs/design/README.md` — entry point for the spec subset
- `docs/design/palette.md` — palette stride and active-palette state machine
- `docs/design/sprite-banks.md` — RLE codec and the +4 filler
- `clients/silencer/CLAUDE.md` — engine source-of-truth (do **not** read during spec-only builds)
