# 01 — World & Physics

## Tick-Based Simulation

The game world advances in discrete **ticks**. The canonical tick rate is
**24 ticks per second**. All movement, cooldowns, and timers are expressed in
ticks unless stated otherwise. Rendering may interpolate between ticks, but
game logic is deterministic per tick.

## Coordinate System

- The coordinate space is a 2-D pixel grid.
- **Origin** is the top-left corner of the map.
- **+X** is to the right; **+Y** is downward.
- Entity positions (`x`, `y`) are signed 16-bit integers representing the
  center-bottom point of the entity's collision box (feet for bipedal
  entities).

## Gravity

| Constant | Value |
|----------|-------|
| `gravity` | 3 (added to `yv` each tick for falling entities) |
| `max_y_velocity` | 45 (terminal velocity clamp) |

Every physical entity that is not supported by a platform has `gravity` added
to its vertical velocity (`yv`) each tick. `yv` is then clamped to
`max_y_velocity`.

## Velocity

- Horizontal velocity (`xv`) and vertical velocity (`yv`) are **signed 8-bit**
  integers.
- Each tick, an entity moves by `(xv, yv)` pixels, subject to collision
  resolution.

## Collision Detection

### Platform Collision (Map Geometry)

The map contains axis-aligned or sloped rectangular collision volumes called
**Platforms** (see [09 — Map & Environment](09-map-and-environment.md)). Each
platform has a **type** bitmask:

| Bit | Type | Description |
|-----|------|-------------|
| 0 | RECTANGLE | Solid horizontal/vertical surface |
| 1 | STAIRSUP | Sloped surface ascending left-to-right |
| 2 | STAIRSDOWN | Sloped surface descending left-to-right |
| 3 | LADDER | Climbable vertical surface |
| 4 | TRACK | Movement track (unused in gameplay) |
| 5 | OUTSIDEROOM | Marks area as outdoors (used for rain effects) |
| 6 | SPECIFICROOM | Associates area with a specific room |

Collision queries take an axis-aligned bounding box (AABB) and a type mask.
Two primary queries exist:

1. **TestAABB** — returns the first platform whose AABB overlaps the query box
   and whose type matches the mask.
2. **TestIncr** — incrementally moves an AABB by `(xv, yv)` one pixel at a
   time, stopping at the first platform collision. Returns the actual
   displacement achieved.

### Entity-Entity Collision

Entities are tested against each other using AABB overlap. The world provides a
`TestAABB` query that filters by object type, ignoring a specified owner or
team. This is used for:

- Projectile-vs-hittable collision
- Player-vs-pickup collision
- Player-vs-interactive-object collision (terminals, machines, etc.)

### Bipedal Ground-Following

Bipedal entities (players, guards, civilians) track a `currentplatformid`.
While on the ground, movement follows the surface of the current platform,
including slopes. When the entity walks past the edge of a platform, it checks
for adjacent platforms; if none exist, it transitions to a **falling** state.

### Minimum Wall Distance

Bipedal NPCs reverse direction when they are within **35 pixels** of a
platform edge (`min_wall_distance = 35`).

## Authority Model

The game uses a client-server authority model:

- The **authority** (server) owns the canonical game state and creates
  authoritative objects.
- **Clients** send input packets and receive state snapshots.
- Some objects (e.g., UI elements) are created locally and do not require
  authority.
- Objects flagged `requires_authority = true` can only be created by the
  server.

## Snapshots & Serialization

Game state is synchronized via **snapshots**. Each object implements a
`Serialize` method that reads/writes its fields to a binary stream. The server
sends periodic snapshots to each client; the client applies them and replays
any un-acknowledged local input.

| Constant | Value |
|----------|-------|
| `max_objects` | 32,000 |
| `max_peers` | 25 |
| `max_old_snapshots` | 36 |
| `peer_timeout` | 10,000 ms |
| `max_teams` | 6 |
