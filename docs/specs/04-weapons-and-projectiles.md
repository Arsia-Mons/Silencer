# 04 — Weapons & Projectiles

## Damage Model

Every projectile carries two damage values:

| Field | Meaning |
|-------|---------|
| `health_damage` | Damage dealt directly to health |
| `shield_damage` | Damage dealt to the shield |
| `bypass_shield` | If `true`, health_damage applies even when shield is up |

See [03 — Player Mechanics § Health & Shield](03-player-mechanics.md) for the
full damage resolution algorithm.

## Weapon Slots & Fire Delay

Each weapon has a **fire delay** — the minimum number of ticks between shots.
After firing, a cooldown counter (`weapon_fire_cool`) is set to
`fire_delay + 3` and decrements each tick. The player can fire again when the
counter reaches 0.

| Slot | Weapon | Fire Delay (ticks) | Ammo Per Purchase | Max Ammo |
|------|--------|--------------------|-------------------|----------|
| 0 | Blaster | 7 | ∞ | ∞ |
| 1 | Laser | 11 | 5 | 30 |
| 2 | Rocket | 21 | 3 | 30 |
| 3 | Flamer | 2 | 15 | 75 |

## Projectile Types

### Blaster

| Property | Value |
|----------|-------|
| Health damage | 40 |
| Shield damage | 4 |
| Velocity | 25 (default) |
| Move amount | 10 |
| Bypass shield | No |
| Stops at object | Yes |

The default sidearm. Infinite ammo, moderate fire rate, high health damage but
almost no shield damage.

### Laser

| Property | Value |
|----------|-------|
| Health damage | 10 |
| Shield damage | 60 |
| Velocity | 30 |
| Move amount | 12 |
| Emit offset | 24 |
| Bypass shield | No |
| Stops at object | Yes |

The anti-shield weapon. Two hits remove a standard 100-point shield, but
minimal health damage to unshielded targets.

### Rocket

| Property | Value |
|----------|-------|
| Health damage | 75 |
| Shield damage | 25 |
| Velocity | 35 |
| Move amount | 15 |
| Bypass shield | No |
| Stops at object | Yes |
| Splash radius | 30 pixels |

On impact, the rocket explodes and deals its full damage to all hittable
entities within a 30-pixel radius of the detonation point (excluding the
direct-hit target, which takes the initial hit, and friendly security objects).

### Flamer

| Property | Value |
|----------|-------|
| Health damage | 2 |
| Shield damage | 1 |
| Velocity | 7 |
| Move amount | 6 |
| Emit offset | −7 |
| Radius | 10 |
| Bypass shield | **Yes** |
| Stops at object | No |

Continuous stream of fire. Bypasses shields entirely. Each hit does small
damage but the very high fire rate (delay = 2 ticks) makes it devastating at
close range.

### Plasma

| Property | Value |
|----------|-------|
| Health damage | 4 |
| Shield damage | 5 |
| Velocity | 5 |
| Move amount | 4 |
| Radius | 5 |
| Bypass shield | No |
| Stops at object | No |

Fired by robots. Slow-moving, passes through targets, area effect.
Has a `large` variant with increased visual size.

### Flare

| Property | Value |
|----------|-------|
| Health damage | 1 |
| Shield damage | 1 |
| Velocity | 5 |
| Move amount | 1 |
| Bypass shield | No |
| Stops at object | No |
| Poisonous variant | Yes (poison flare) |

Stationary area-denial projectile. Deals continuous trickle damage to entities
within its area. The poison variant also applies poison to victims.

### Wall Projectile

| Property | Value |
|----------|-------|
| Health damage | 10 |
| Shield damage | 60 |
| Velocity | 35 |
| Move amount | 6 |
| Bypass shield | No |
| Stops at object | Yes |

Fired by wall defense turrets and fixed cannons. Same damage profile as the
laser.

## Projectile Movement

Each tick, a projectile moves by `(xv, yv)`. The `move_amount` controls how
many pixels the collision check steps through per sub-tick. Projectiles test
against:

1. **Map platforms** (RECTANGLE, STAIRSUP, STAIRSDOWN) — the projectile is
   destroyed on contact with solid geometry.
2. **Hittable entities** — if `stop_at_object_collision` is true, the
   projectile is destroyed after hitting one entity. Otherwise it passes
   through (flamer, plasma, flare).

Projectiles that spawn inside a solid platform (e.g., firing while pressed
against a wall) are cancelled — ammo is refunded and no projectile is created.

## Firing Directions

Projectiles can be fired in **8 directions** based on the player's state and
aim inputs:

| Direction | Description |
|-----------|-------------|
| Right / Left | Horizontal (mirrored determines left/right) |
| Up | Straight up |
| Down | Straight down (air only) |
| Up-right / Up-left | 45° diagonal upward |
| Down-right / Down-left | 45° diagonal downward |

Diagonal velocities use the factor `0.70710678118655` (cos 45° / sin 45°) to
maintain consistent speed.

## Grenade Sub-Types

Grenades are thrown items (not projectile-slot weapons). They use physics
(gravity, bounce) and detonate on a timer or condition.

| Sub-Type | Effect |
|----------|--------|
| EMP | Electromagnetic pulse; disables shields in blast radius |
| SHAPED | Focused upward explosion |
| PLASMA | High-damage area explosion |
| NEUTRON | Kills all entities in the entire map region; only defense is being inside a base |
| FLARE | Deployable area-denial torch |
| POISONFLARE | Flare that also poisons victims |

## Detonator

A remote-controlled device placed on the ground.

- **Plasma Detonator** — player places it, then detonates remotely via the
  `detonate` input. Explodes upward.
- **Camera** — same placement mechanic, but functions as a remote viewing
  device instead of an explosive. Can be detonated to self-destruct quietly.
