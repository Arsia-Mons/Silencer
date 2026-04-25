# 11 — Animation System

## Overview

All animations in zSILENCER are sprite-based. Each animation is identified by
a **sprite bank** (`res_bank`, 0–255) and a **frame index** (`res_index`,
0–255). Banks are loaded from the game's sprite data files; each bank contains
a sequence of frames of varying dimensions.

Animations advance via a **frame counter** (`state_i`) that increments by 1
every tick (24 ticks per second = ~41.67 ms per tick). The mapping from
`state_i` to `res_index` may be 1:1 (one frame per tick), divided (e.g.,
`state_i / 2` for half-speed), modular (looping), or conditional.

Every sprite frame carries per-frame metadata:

| Property | Description |
|----------|-------------|
| `width` | Frame width in pixels |
| `height` | Frame height in pixels |
| `offset_x` | Horizontal anchor offset from entity position |
| `offset_y` | Vertical anchor offset from entity position |

The entity's bounding box for rendering and collision is derived from the
current frame's dimensions and offsets.

## Sprite Rendering

| Property | Default | Description |
|----------|---------|-------------|
| `res_bank` | 0 | Current sprite bank |
| `res_index` | 0 | Current frame within bank |
| `mirrored` | false | Flip sprite horizontally |
| `draw` | true | Whether to render |
| `render_pass` | 0 | Draw order (1 = back, 2 = mid, 3 = front) |
| `draw_checkered` | false | Render with 50% transparency checkerboard |
| `draw_alpha` | false | Render with alpha blending |
| `effect_color` | 0 | Palette color tint (0 = none) |
| `effect_brightness` | 128 | Brightness modifier (128 = neutral) |

### Render Interpolation (Nudge)

Between ticks, the renderer interpolates entity positions using **nudge**
values calculated from the difference between the current and previous
position. This provides smooth visual motion independent of the tick rate.

---

## Player Animations

The player character uses the most animation banks of any entity. All player
animations are horizontally mirrorable via the `mirrored` flag.

### Movement & Idle

| Bank | Frames | State | Speed | Loop | Description |
|------|--------|-------|-------|------|-------------|
| 9 | 0–11 | STANDING | ÷3 | Yes (wraps at frame 12) | Idle breathing cycle |
| 10 | 0–1 | STANDING | 1:1 | No | Idle start (first 2 ticks, transitions into bank 9) |
| 66 | 0–5 | RUNNING (start) | 1:1 | No | Run startup from standing |
| 11 | 0–14 | RUNNING (full) | 1:1 | Yes (wraps 6→20→6) | Full run cycle. Foot sounds at frames 4 and 11 |
| 67 | 0–3 | RUNNING (stop) | 1:1 | No | Deceleration to standing. Foot sound at frame 3 |
| 12 | 0–1 | JUMPING / FALLING (early) | ÷2 | No | Jump launch / early fall (2 frames) |
| 13 | 0–6 | FALLING / JETPACK | ÷4 | No (clamps at 6) | Mid-air descent. Jetpack uses frames 1–6 |
| 15 | 0–15 | CLIMBINGLEDGE | 1:1 | No | Ledge climb-up. Clamps at frame 15 |
| 16 | 0–20 | LADDER | 1:1 | Yes (wraps at 20) | Ladder climbing. Sounds at frames 4 and 15 |
| 17 | 0–4 | CROUCHING / UNCROUCHING | 1:1 | No | Crouch transition (4 ticks). Reverse for uncrouch |
| 18 | 0–10 | CROUCHED | ÷3 | Yes (wraps at 11) | Crouched idle breathing |
| 88 | 0–7 | ROLLING | 1:1 | No | Crouch roll (8 frames). Sound at frame 0 |

### Running Phase Detail

The RUNNING state uses a multi-phase animation system driven by `state_i`:

| `state_i` Range | Phase | Bank | `res_index` | Notes |
|-----------------|-------|------|-------------|-------|
| 0–5 | Startup | 66 | `state_i` | Acceleration. If already at speed ≥ 4, skips to frame 1 |
| 6–20 | Full run | 11 | `state_i − 6` | Loops: at frame 20, resets to 6. Foot sounds at indices 4 and 11 |
| 21–24 | Deceleration | 67 | `state_i − 21` | Stopping. Foot sound at index 3. Transitions to STANDING at 24 |

### Running Speed

| Condition | Max Horizontal Speed (`xv`) |
|-----------|-----------------------------|
| Normal | ±14 |
| Disguised | ±11 |
| Carrying secret | ±11 |
| Disguised + secret | ±8 |

Acceleration: **+3 per tick** when pressing a direction. Deceleration:
**×0.5 per tick** when no direction pressed.

### Jumping & Falling Phase Detail

| `state_i` Range | Bank | `res_index` | Description |
|-----------------|------|-------------|-------------|
| 0–3 (÷2) | 12 | 0–1 | Initial jump / early fall |
| 4+ (÷4) | 13 | 1–6 | Mid-fall. Clamps at `state_i = 24` (frame 6) |

Air control:
- `falling_nudge` accumulates ±1 per tick (max ±8) from directional input.
- `xv += falling_nudge / 2` each tick.

### Jetpack

| Bank | Frames | Speed | Description |
|------|--------|-------|-------------|
| 13 | 1–6 | ÷4 | Jetpack hover/rise. `res_index = (state_i / 4) + 1`, clamped at 6 |

Jetpack physics:
- `yv -= 1` every 2 ticks.
- `xv` adjusted by ±1 every 2 ticks from directional input.
- Max vertical speed: −9 (up). Max horizontal speed: ±14.
- Fuel: −1 per tick. Plume particles emitted each tick (3 per tick).

### Ladder Climbing Speed

| Condition | Climb Speed (`yv`) |
|-----------|--------------------|
| Normal | ±6 |
| Disguised | ±5 |
| Carrying secret | ±5 |
| Disguised + secret | ±4 |

### Shooting Animations

All shooting states follow the same pattern: frames 0–4 are the
wind-up (arm raising), frame 4 is the hold/aim pose, and frames 5–8 are the
release/recoil. Weapon fires at `state_i = 5`.

**Standing Shoot (8 directions)**

| Bank | Direction | Mirrored |
|------|-----------|----------|
| 21 | Right / Left | Left = mirrored |
| 22 | Up | No |
| 23 | Down | No |
| 24 | Up-right / Up-left | Up-left = mirrored |
| 25 | Down-right / Down-left | Down-left = mirrored |

**In-Air Shoot (8 directions)**

| Bank | Direction | Mirrored |
|------|-----------|----------|
| 31 | Right / Left | Left = mirrored |
| 32 | Up | No |
| 33 | Down | No |
| 34 | Up-right / Up-left | Up-left = mirrored |
| 35 | Down-right / Down-left | Down-left = mirrored |

**Ladder Shoot (8 directions)**

| Bank | Direction | Mirrored |
|------|-----------|----------|
| 26 | Right / Left | Left = mirrored |
| 27 | Up | No |
| 28 | Down | No |
| 29 | Up-right / Up-left | Up-left = mirrored |
| 30 | Down-right / Down-left | Down-left = mirrored |

**Crouched Shoot**

| Bank | Direction | Mirrored |
|------|-----------|----------|
| 36 | Right / Left | Left = mirrored |

### Shoot Frame Timing

| `state_i` | `res_index` | Phase |
|-----------|-------------|-------|
| 0–4 | 0–4 | Wind-up (approaches aim at double speed — `state_i++` extra) |
| 5 | 4 or 5 | **Fire frame** — projectile created. Index 4 for most weapons; Flamer uses index 5 |
| 6–(5+delay) | 4 | Hold pose (weapon-dependent delay) |
| 5+delay → 8+delay | loops to 5 | Auto-fire loop if fire held |
| Release: 200–204 | 4→0 | Retract animation (4 frames) |

The fire delay per weapon (ticks between auto-fire shots):

| Weapon | Fire Delay |
|--------|-----------|
| Blaster | 7 |
| Laser | 11 |
| Rocket | 21 |
| Flamer | 2 |

### Throwing

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 113 | 0–16 | 1:1 | THROWING | Standing grenade throw |
| 114 | 0–16 | 1:1 | CROUCHEDTHROWING | Crouched grenade throw |

### Hacking

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 173 | 0–16 | 1:1 | HACKING | Player hacking a terminal |

Hacking frame detail:
- Frames 0–14: approach and jack-in animation.
- Frame 14: jack-in sound plays.
- Frames 15–16: looping hack cycle (data extraction happens at frame 16,
  resets to 15 on successful hack tick). If the terminal becomes unhackable
  (another player takes it, or it's fully drained), the retract phase begins
  immediately. When `state_i` reaches 16 and the terminal's `juice` is
  depleted, the hack ends and retract begins.
- On release: frames 17–32 reverse out (retract animation plays in reverse).
  `res_index = (17 - state_i) + 16` during retract.

### Deploy / Respawn / Death

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 68 | 0–8 | 1:1 | DEPLOYING | Beam-in teleport effect. Invisible for 60 ticks, then frames 0–8 |
| 68 | 8→0 | 1:1 | UNDEPLOYING | Beam-out (reverse of deploy) |
| 198 | 0–27 | ÷2 | RESPAWNING | Base respawn beam-in. 56 ticks total |
| 199 | 0–27 | ÷2 | RESURRECTING | Lazarus self-resurrect. 54 ticks total. Sound at tick 2 |
| 20 | 0–15 | 1:1 | DYING | Death fall animation (15 frames). Gravity applies |
| 190 | 0–5 | 1:1 | (overlay) | Detonator trigger overlay. Plays on top of standing idle |

### Base Door Entry / Exit

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 19 | 0–15 | 1:1 | WALKIN | Walking into base door (15 frames). Sound at frame 1 |
| 19 | 15→0 | 1:1 | WALKOUT | Walking out of base door (reverse, 15 frames). Sound at frame 0 |

### Disguised Player Animations

When disguised, the player uses civilian animation banks:

| Bank | Replaces | Description |
|------|----------|-------------|
| 121 | 9 (standing) | Civilian idle |
| 122 | — (civilian walk) | Disguised slow patrol walk |
| 123 | 11 (running) | Civilian run cycle. Also used for disguised falling (frame 0 only) |
| 124 | 16 (ladder) | Civilian ladder climb (0–19 frames, loops at 19) |
| 127 | 173 (hacking) | Civilian hacking animation (0–18 frames) |

---

## Guard Animations

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 59 | 0 | — | STANDING | Single idle frame |
| 60 | 0–18 | mod 19 | WALKING | Walk cycle. Speed = 5 px/tick. Loops continuously |
| 61 | 0–9 | 1:1 | SHOOTSTANDING | Shoot horizontal. Fire at frame 7. Reverses 9→0 after hold |
| 62 | 0–19 | 1:1 | LADDER | Ladder climb cycle. Loops at 20 |
| 63 | 0–3 | 1:1 | HIT | Recoil animation (4 frames) |
| 64 | 0–9 | 1:1 | DYING | Death fall (10 frames) |
| 69 | 0–5 | ÷4 | LOOKING | Head scan (6 unique frames, 24 ticks total) |
| 154 | 0–9 | 1:1 | SHOOTUP | Shoot upward. Fire at frame 7 |
| 155 | 0–8 | 1:1 | SHOOTDOWN | Shoot downward. Fire at frame 6 |
| 156 | 0–8 | 1:1 | SHOOTUPANGLE | Shoot diagonal up. Fire at frame 6 |
| 157 | 0–8 | 1:1 | SHOOTDOWNANGLE | Shoot diagonal down. Fire at frame 6 |
| 158 | 0–9 | 1:1 | CROUCHING / CROUCHED | Crouch transition (9 frames). Frame 9 = fully crouched |
| 159 | 0–8 | 1:1 | SHOOTCROUCHED | Shoot while crouched. Fire at frame 6. Reverses after hold |
| 196 | 0–8 | 1:1 | SHOOTLADDERUP | Shoot upward from ladder. Fire at frame 6 |
| 197 | 0–8 | 1:1 | SHOOTLADDERDOWN | Shoot downward from ladder. Fire at frame 6 |

### Guard Shoot Frame Timing

All guard shoot animations follow a similar pattern:

| Phase | `state_i` | `res_index` | Description |
|-------|-----------|-------------|-------------|
| Wind-up | 0 → peak | 0 → peak | Raise weapon toward direction |
| Fire | 6 or 7 | — | Projectile created |
| Hold | peak → peak+3 | peak (held) | Pause after firing (gap of ~3 frames) |
| Retract | peak+4 → peak×2 | peak → 0 | Reverse animation back to neutral |
| Transition | — | — | Returns to STANDING (from standing/walking shoots) or CROUCHED (from crouched shoot) |

Cooldown between shots: **48 ticks** (2 seconds).

### Guard Walking

- Walk cycle: bank 60, 19 frames, loops via `state_i % 19`.
- Speed: 5 px/tick.
- Duration: walks for **240 ticks** (10 seconds) then transitions to LOOKING.
- Reverses at platform edge (within 35 px of end).

---

## Robot Animations

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 45 | 0–19 | mod 20 | WALKING | Walk cycle. Speed = 4 px/tick. Foot sounds at frames 1 and 10 |
| 46 | 0–17 | 1:1 | SHOOTING | Shoot forward. Projectile at frame 11. Forward then reverse |
| 47 | 0–14 | 1:1 | ASLEEP / AWAKENING / SLEEPING | Dormant→awake transition. Frame 0 = asleep |
| 48 | 0–15 | ÷2 | DYING | Death explosion sequence. Clamps at frame 15 |

### Robot Walk Detail

- Bank 45, frames 0–19, loops via `state_i % 20`.
- Speed: 4 px/tick.
- Melee check every 40 ticks (overlapping AABB).
- Non-patrol robots return to SLEEPING after 100 ticks of walking.
- Walk cycle loops until direction change or target spotted.

### Robot Shoot Detail

| Phase | `state_i` | `res_index` | Description |
|-------|-----------|-------------|-------------|
| Forward | 0–17 | 0–17 | Full animation forward |
| Fire | 11 | — | Rocket projectile created |
| Reverse | 18–35 | 17→0 | Animation plays in reverse |

Total shoot duration: **36 ticks** (1.5 seconds).

### Robot Death

- Bank 48, `res_index = state_i / 2`, clamped at 15.
- Duration: **96 ticks** (4 seconds).
- Plume explosions every 2 ticks from tick 5 onward.
- At tick 8: main explosion sound.
- At tick 96: death burst — 6 plasma projectiles scatter outward.

---

## Civilian Animations

| Bank | Frames | Speed | State | Description |
|------|--------|-------|-------|-------------|
| 121 | 0–9 | 1:1 | STANDING | Idle cycle. Loops at 10 |
| 122 | 0–19 | 1:1 | WALKING | Walk cycle. Speed = 4 px/tick. Sounds at frames 5 and 15. Loops at 20 |
| 123 | 0–14 | mod 15 | RUNNING | Flee cycle. Speed = 9 px/tick. Sounds at frames 6 and 14. Loops at 15 |
| 125 | 0–13 | 1:1 | DYINGBACKWARD | Death backward (14 frames) |
| 126 | 0–13 | 1:1 | DYINGFORWARD | Death forward (14 frames) |

### Civilian Walk Detail

- Speed: `speed` field (default 4 px/tick).
- Looks for threats every 5 ticks during WALKING.
- On seeing a projectile within detection range, switches to RUNNING for 150
  ticks (6.25 seconds), then returns to WALKING.
- Running speed: `5 + speed` (default 9 px/tick).
- Running checks for threats every 10 ticks.
- Dead civilians respawn (warp-in) after **100 ticks** (≈4.2 seconds).

---

## Shared Shoot Pattern

Guards and players share a common animation language for shooting:

1. **Wind-up** — frames 0→N animate the character raising the weapon toward
   the target direction.
2. **Fire frame** — at a specific `state_i`, the projectile is created.
3. **Hold** — the character holds the aimed pose for a weapon-dependent number
   of ticks (controlled by fire delay).
4. **Retract** — frames animate back from N→0 as the weapon is lowered.

For players, the retract phase uses a trick: `state_i` jumps to 200, and
`res_index = 204 − state_i` counts down from 4 to 0 over 4 ticks.

## Hit Flash

When an entity takes damage, a `state_hit` value is set:

| Hit Type | `state_hit` Value | Visual Effect |
|----------|-------------------|---------------|
| Health damaged | `1 + (0 × 32)` | Red flash |
| Shield depleted + overflow | `1 + (1 × 32)` | Red flash |
| Shield absorbed | `1 + (2 × 32)` | Blue/shield flash |

The hit flash lasts `state_hit % 32` ticks (decrementing each tick down to 0,
so a maximum of ~31 ticks ≈ 1.3 seconds), overlaying the entity's normal
animation with a tinted version.

## Warp Effect

Many spawn/respawn transitions use a **warp flash** (`state_warp`):
- Set to 12 on respawn/teleport.
- Counts down 1 per tick.
- During warp, the sprite is rendered with a bright white/blue tint.

---

## Fixed Cannon Animations

| Bank | Frames | State | Description |
|------|--------|-------|-------------|
| 90 | — | All | Fixed cannon sprite set |

States cycle through UP, DOWN, SHOOTING_UP, SHOOTING_DOWN, MOVING_UP,
MOVING_DOWN. The cannon scans for targets by looking in both directions and
fires wall-type projectiles.

## Wall Defense Animations

Wall defenses cycle through DEAD → ACTIVATING → WAITING → SHOOTING states.
They use a dedicated sprite bank for each state. When SHOOTING, they fire wall
projectiles at detected intruders.

## Detonator / Camera

| Bank | Frames | Speed | Description |
|------|--------|-------|-------------|
| 182 | 0–3 | ÷4 | Idle blink cycle. Loops via `(state_i / 4) % 4` |

On detonation, `state_i` advances past 16, the device launches upward
(`yv = −15`), then explodes at frame 22.

## Plume / Explosion Effects

Plumes are short-lived visual effects with multiple type variants:

| Type | Description |
|------|-------------|
| 0–1 | Smoke puffs (jetpack, impacts) |
| 4 | Explosion fire |
| 8 | Breath mist (cold weather) |

Each type has its own animation cycle and lifetime.

## Body Parts (Gibs)

On explosive death, entities spawn **BodyPart** objects — sprite fragments
that inherit velocity from the death impact, apply gravity, and bounce off
platforms. They despawn after a set lifetime.

---

## Summary: Frame Timing Quick Reference

One tick = 1/24 second ≈ 41.67 ms.

| Notation | Meaning | Real Time Per Frame |
|----------|---------|---------------------|
| 1:1 | 1 frame per tick | ~42 ms |
| ÷2 | 1 frame per 2 ticks | ~83 ms |
| ÷3 | 1 frame per 3 ticks | ~125 ms |
| ÷4 | 1 frame per 4 ticks | ~167 ms |
| mod N | Loop every N ticks | varies |
