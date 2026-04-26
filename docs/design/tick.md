# Tick rate and `state_i`

**Source:** `clients/silencer/src/game.cpp::Loop` (line 268).

## 24 Hz simulation tick

The simulation advances on a **42 ms** wall-clock interval
(`wait = 42` in `Game::Loop`). Render is uncapped. On a tick:

1. Network drained
2. Local input sampled, sent to peers
3. `Game::Tick()` runs (state transitions, fade timer)
4. `World::Tick()` runs (calls `Tick` on every live `Object`,
   including `Button`, `Overlay`, `Interface`)
5. Renderer recomputes per-frame state (palette mixes, etc.)

`24 Hz` is the canonical rate; the rest of the spec (and the legacy
spec doc) measures every animation in *ticks*.

## `state_i` — per-object tick counter

Every `Object` carries a `state_i` field bumped each tick by its
own `Tick()` method. Different components use it differently:

| Component | What `state_i` drives |
| --------- | --------------------- |
| `Button`  | 4-tick ramp during `ACTIVATING` / `DEACTIVATING` (hover in/out). `effectbrightness = 128 + state_i * 2`, then sprite frame `base_index + state_i`. See [widget-button.md](widget-button.md). |
| `Overlay` (bank 208 logo) | Animation frame index. Fade-in `(state_i / 2) + 29` from idx 29 → 60 over 60 ticks (~2.5 s), holds at idx 60 for 60 ticks, fades back, loops. See [widget-overlay.md](widget-overlay.md). |
| `Overlay` (other banks) | Per-bank custom animation; the menu only uses the bank-208 path |

For the main menu, the only timing-dependent visuals are the logo
animation and (if the user is hovering) one button's hover ramp.

## Implication for hydrations

A "render one frame" QA capture (like the dump modes added to
`shared/design/sdl3` and `clients/silencer`) must:

1. Run `Init` once.
2. Call `Tick()` until the menu's pinned scene state is reached —
   for the main menu, that's the bank-208 logo overlay reaching
   `res_index = 60` (the steady-state hold frame). This requires
   **at least 120 simulation ticks**: the fade-in increments
   `res_index = state_i / 2 + 29` from idx 29 to idx 60 over the
   first 60 ticks, after which idx stays at 60 for the next 60.
3. Call `Draw` once.
4. Snapshot the framebuffer.

Pinning to the *scene state* (not the tick count) is what makes the
hydration's dump and the real client's dump comparable, because
the real client's render loop schedules ticks against wallclock
(42 ms `wait`), not at a known cadence. See
[`.claude/skills/visual-regression-testing/SKILL.md`](../../.claude/skills/visual-regression-testing/SKILL.md)
for the full pattern.

Hydrations that draw without ticking will catch the logo at
`state_i = 0` (idx 29 — first fade-in frame, dim and partial),
which looks like a bug but isn't.
