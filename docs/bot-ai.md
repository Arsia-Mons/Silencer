# Bot AI System

The Silencer bot AI is a tick-based state machine that simulates a full player — navigation, hacking, combat, and retreat — all expressed through the same `Input` struct that a human presses.

## Entry point

`PlayerAI::Tick(World& world)` is called once per simulation tick for every bot player. It mirrors what `HandleInput()` does for humans: it saves `player.oldinput = player.input` before zeroing `player.input`, so rising-edge checks (`keyX && !oldinput.keyX`) work correctly.

Bots are spawned in **TESTGAME mode** (`-testgame` flag), which creates ten bot players in-process. The _Test Game_ button on the main menu enables this.

## State machine

States are declared in the private enum inside `PlayerAI`:

| State | Goal |
|---|---|
| `IDLE` | Transient — immediately transitions to `HACK` |
| `HACK` | Navigate to nearest available terminal and hack it |
| `EXITBASE` | Navigate to the base exit and leave |
| `GETSECRET` | _(reserved)_ |
| `GOTOBASE` | Navigate to the enemy base door and enter |
| `RETURNSECRET` | Navigate to the secret return point and deliver |
| `RETREAT` | Health critical — return to base, use HealMachine |
| `KILLSECRET` | Enemy carries the secret — chase them down |

State transitions happen in `Tick()` before navigation runs:

- Respawn → `EXITBASE`
- Death → tap activate every other tick (respawn prompt)
- Carrying secret + in own base → `RETURNSECRET`
- Carrying secret + outside base → `GOTOBASE`
- Health ≤ retreat threshold + not carrying secret → `RETREAT`
- Healed (≥ 75% max health) while in base → `EXITBASE`
- Enemy carrying secret detected → `KILLSECRET`
- Secret no longer held by enemy → back to `HACK`

## Navigation

### Platform graph

The map is divided into **PlatformSets** — groups of platforms at the same elevation reachable by walking. `CreatePathToPlatformSet` runs a BFS over the set graph to produce an ordered path. Each hop in the path is a **link** — a connection between two adjacent sets.

### Link types

| Type | Condition |
|---|---|
| `LINK_LADDER` | A ladder connects the two sets |
| `LINK_FALL` | Target is below, X ranges overlap |
| `LINK_JUMP` | Height diff ≤ 50px, horizontal gap ≤ 160px, no wall between |
| `LINK_JETPACK` | Height diff 50–600px, horizontal gap ≤ 250px |

`FindAnyLink` tries LADDER first (always geometry-based), then checks baked designer navlinks if any exist in the map. If the map has **no** navlinks, it falls back to runtime geometry heuristics for FALL/JUMP/JETPACK.

### Jetpack link execution

Jetpack routes have two phases controlled by `linkEdgeX`, `linkTargetX`, and `linkDir`:

1. **Ground phase** — bot walks toward `linkEdgeX` (the launch point). Once within `EDGE_DEAD = 32px` (half a tile), it holds jetpack. The stuck timer resets here so fuel-refill waits do not trigger a replan.
2. **Air phase** — once off the ground (`!OnGround()`), the bot keeps holding jetpack and moves horizontally toward `linkTargetX` (or the destination platform range) until `player.y ≤ min target platform y1`.

Key details:
- `linkDir` always encodes the **air** direction (launch point → destination), never the walk direction.
- `linkEdgeX` is `Sint32` — maps with X coordinates > 32 767 previously caused silent overflow when it was `Sint16`.
- `player.fuellow` suppresses `keyjetpack` during fuel refill but does not increment the stuck timer.

### Baked navlinks (designer-controlled)

The `.sil` binary format stores navlinks with 20 bytes each:

```
fromIdx  u32
toIdx    u32
type     u8 (0=JUMP, 1=FALL, 2=JETPACK)
pad      3 bytes
sourceX  s32  (launch X; INT32_MIN = use platform edge)
targetX  s32  (landing X; INT32_MIN = use destination center)
```

When any navlinks are present in the map, the runtime heuristics are bypassed entirely for JUMP/FALL/JETPACK; only baked links are used. Old 16-byte links (without `sourceX`) are auto-detected by stride and remain compatible.

### Stuck detection and blacklisting

`linkStuckTicks` counts consecutive ticks on the same link:

- Ground phase: resets whenever the bot is at the edge or airborne.
- Air phase: counts up; blacklists the link after 180 ticks mid-air or 120 ticks on the ground.

A blacklisted link (`BadLink`) has a TTL of 600 ticks. During that window, `FindLink` immediately returns false for that from→to pair so BFS routes around it.

## Combat

`ApplyCombat` runs every tick in all states except `HACK` and `RETREAT`.

### Target acquisition

`ScanForTarget` does an AABB scan within `aiCombatRange` px for enemy players who are alive, visible, not disguised, and not in their base. Secret holders win regardless of distance. Results are locked for `aiTargetLockTicks` ticks before re-scanning.

Line-of-sight is verified with `map.TestLine` before engaging. Enemies within 80px are skipped (let navigation handle separation).

### Reaction delay

When a new enemy is spotted, a reaction timer delays the first shot by `aiReactionTicks` ticks (scaled by difficulty, plus random variance). During the delay the bot faces the target but does not fire.

### Burst fire

Rather than holding fire every tick, bots fire in bursts:

- Hold `keyfire` for `aiShootBurstTicks` ticks (scaled by difficulty)
- Pause for `aiShootPauseTicks` ticks (scaled by difficulty, plus variance)
- Repeat

### Jetpack dodge

In combat, MEDIUM/HARD bots randomly fire their jetpack to dodge. Interval is `aiJetpackCombatInterval`; if the bot took damage this tick the interval is halved. A cooldown of 30–60 ticks prevents continuous dodging.

### Evasion

MEDIUM/HARD bots randomly jump when taking damage (`aiEvadeInterval` interval).

## Difficulty scaling

All combat parameters in `shared/assets/gas/player.json` scale by difficulty:

| Parameter | EASY | MEDIUM | HARD |
|---|---|---|---|
| Reaction ticks | `×2` | `×1` | `÷2` |
| Burst ticks | `÷2` | `×1` | `×1.5` |
| Pause ticks | `×1.5` | `×1` | `÷2` |
| Jetpack dodge | disabled | enabled | enabled |
| Evasion | disabled | enabled | enabled |

## GAS parameters (`player.json`)

All AI tuning is done in `shared/assets/gas/player.json` — no recompile needed.

| Key | Default | Purpose |
|---|---|---|
| `aiCombatRange` | 300 | Max pixel distance to scan for enemies |
| `aiTargetLockTicks` | 20 | Ticks before target re-scan |
| `aiReactionTicks` | 12 | Base delay before first shot |
| `aiShootBurstTicks` | 8 | Base burst duration (ticks) |
| `aiShootPauseTicks` | 18 | Base pause between bursts (ticks) |
| `aiJetpackCombatInterval` | 25 | 1-in-N chance per tick to dodge |
| `aiEvadeInterval` | 15 | 1-in-N chance per tick to jump-evade |
| `aiRetreatHealthPct` | 30 | Health % threshold to trigger retreat |
| `aiDisguiseInterval` | 50 | 1-in-N chance per tick to use disguise |
| `aiArrivalThreshold` | 8 | px tolerance for "arrived at target" |
| `aiLadderJumpUpInterval` | 12 | 1-in-N chance per tick to jump up ladder |
| `aiLadderJumpDownInterval` | 5 | 1-in-N chance per tick to jump off ladder down |

## Designer workflow — jetpack navlinks

Use the **Level Designer** (`/designer`) to draw jetpack routes between platforms.

1. Select the **NAV_LINK** tool and choose type **JETPACK**.
2. Click on the **from-platform** at the intended launch X position — a circle marker appears (`sourceX`).
3. Click on the **to-platform** at the intended landing X position — a diamond marker appears (`targetX`). An arrow connects the two waypoints.
4. To edit a placed link: select it in the NavLink panel. The _launch X_ and _target X_ number inputs appear. The × button on each clears the value back to auto (platform edge / platform center).
5. Save or export the map; links are written in 20-byte format with `sourceX` and `targetX`.

When `sourceX` is set, the bot walks precisely to that X before launching. When `targetX` is set, the bot steers horizontally toward that X during flight. Either can be left unset (auto).

## File index

| File | Role |
|---|---|
| `clients/silencer/src/actors/playerai.h` | State variables, public API |
| `clients/silencer/src/actors/playerai.cpp` | Full state machine, pathfinding, combat |
| `clients/silencer/src/world/map.h` | `NavLink` struct (`fromIdx`, `toIdx`, `type`, `sourceX`, `targetX`) |
| `clients/silencer/src/world/map.cpp` | Binary navlink parser (stride auto-detect) |
| `web/admin/lib/types.ts` | Shared `NavLink` TypeScript interface |
| `web/admin/app/designer/useSilMap.ts` | `.sil` parse/save (20-byte navlinks) |
| `web/admin/app/designer/MapCanvas.tsx` | Click-to-place navlink tool, waypoint markers |
| `web/admin/app/designer/NavLinkPanel.tsx` | sourceX/targetX editing UI |
| `shared/assets/gas/player.json` | All AI tuning parameters |
