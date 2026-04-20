# 08 — NPCs & Security

The game world is populated with government security forces and neutral
civilians. These are authoritative (server-controlled) entities.

## Guards

Guards are bipedal government security agents that patrol platforms and shoot
at threats.

### Stats

| Property | Value |
|----------|-------|
| Max Health | 25 |
| Max Shield | 15 |
| Speed | 5 |
| Cooldown time | 48 ticks |
| Respawn time | 30 seconds |

### Behavior States

| State | Description |
|-------|-------------|
| NEW | Just spawned; finding ground |
| STANDING | Idle at a position |
| CROUCHING | Transitioning to crouch |
| CROUCHED | Fully crouched |
| SHOOTCROUCHED | Firing while crouched |
| UNCROUCHING | Standing up from crouch |
| LOOKING | Scanning for targets |
| WALKING | Patrolling along platform |
| SHOOTSTANDING | Firing while standing |
| SHOOTUP | Firing upward |
| SHOOTDOWN | Firing downward |
| SHOOTUPANGLE | Firing diagonally upward |
| SHOOTDOWNANGLE | Firing diagonally downward |
| SHOOTLADDERUP | Firing up while on ladder |
| SHOOTLADDERDOWN | Firing down while on ladder |
| LADDER | Climbing a ladder |
| HIT | Recoiling from damage |
| DYING | Death animation |
| DYINGEXPLODE | Explosive death |
| DEAD | Dead; waiting for respawn timer |

### Targeting Rules

Guards will target an entity if:

- It is a **Player** who is NOT disguised, NOT invisible, and does NOT have a
  Security Pass — OR is the player the guard is already chasing.
- It is a **Robot** that has been virus-implanted (`virus_planter ≠ 0`).
- It is a **FixedCannon** (player-deployed turrets are always hostile to
  government forces).

### Patrol Behavior

Guards can be configured as:
- **Patrol** — walks back and forth along their platform, reversing at edges.
- **Stationary** — stays in place, only attacking if a target enters line of
  sight.

### Weapons

Guards carry a weapon type:

| Weapon ID | Type | Ammo Dropped on Death |
|-----------|------|----------------------|
| 0 | Blaster | (none) |
| 1 | Laser | 5 Laser ammo |
| 2 | Rocket | 3 Rocket ammo |

### Respawn

After `respawn_seconds` (30s), a dead guard respawns at its original position
with full health and shield.

## Robots

Robots are heavily-armored bipedal security units with melee attacks.

### Stats

| Property | Value |
|----------|-------|
| Max Health | 200 |
| Max Shield | 400 |
| Speed | (walking animation-driven) |
| Respawn time | 45 seconds |

### Behavior States

| State | Description |
|-------|-------------|
| NEW | Just spawned |
| SLEEPING | Dormant; activates when a target is near |
| ASLEEP | Fully dormant |
| AWAKENING | Waking up animation |
| WALKING | Patrol or approach target |
| SHOOTING | Firing plasma projectile |
| DYING | Death animation |
| DEAD | Dead; waiting for respawn |

### Targeting

Robots target:
- **Players** who are not disguised, not invisible, and don't have a Security
  Pass (unless virus-implanted, in which case they target based on their new
  allegiance).
- **FixedCannons** belonging to a different team than the virus planter.
- **Guards** (if the robot has been virus-implanted, it turns against
  government security).

### Melee Attack

Robots deal damage through physical contact rather than ranged projectiles.
They also fire plasma projectiles at range.

### Virus Implant

A player can use a **Virus** item on a robot to take control. A
virus-implanted robot:
- Has `virus_planter` set to the implanting team's ID.
- Attacks guards and enemy-team entities instead of the implanting team.
- Government guards will start targeting it.

### Patrol Behavior

Like guards, robots can be set to patrol or stationary mode.

## Civilians

Civilians are neutral NPCs that walk along platforms.

### Stats

| Property | Default |
|----------|---------|
| Speed | 4 |
| Suit color | `(7 << 4) + 11` |
| Hittable | Yes |

### Behavior States

| State | Description |
|-------|-------------|
| NEW | Spawning; finding ground |
| STANDING | Idle |
| WALKING | Walking along platform at `speed` |
| RUNNING | Fleeing at `5 + speed` (triggered by seeing combat) |
| DYINGFORWARD | Death animation (falling forward) |
| DYINGBACKWARD | Death animation (falling backward) |
| DYINGEXPLODE | Explosive death |
| DEAD | Dead; waits then respawns |

### Sight & Fleeing

Civilians periodically scan for nearby threats (every 5–10 ticks). If they see
combat or a threatening entity, they switch to RUNNING and flee in the opposite
direction. They reverse direction when reaching a platform edge.

### Lazarus Tract Conversion

A Lazarus agency player can use a **Lazarus Tract** inventory item near a
civilian to convert it. A converted civilian has `tract_team_id` set to the
player's team.

## Fixed Cannons

Player-deployed auto-turrets.

| Property | Value |
|----------|-------|
| Health | 40 |
| Shield | 16 |

### States

| State | Description |
|-------|-------------|
| NEW | Just placed; deploying animation |
| UP | Aiming upward, scanning |
| DOWN | Aiming downward, scanning |
| SHOOTING_UP | Firing upward |
| SHOOTING_DOWN | Firing downward |
| MOVING_UP | Transitioning aim upward |
| MOVING_DOWN | Transitioning aim downward |
| DYING | Destruction animation |

Fixed cannons fire wall-type projectiles and belong to the placing player's
team. They can be virus-implanted to switch allegiance.

## Wall Defenses

In-base wall-mounted turrets purchased via the buy menu.

### States

| State | Description |
|-------|-------------|
| DEAD | Destroyed or not yet activated |
| ACTIVATING | Deploying animation |
| WAITING | Scanning for targets |
| SHOOTING | Firing at an intruder |

Wall defenses fire wall projectiles at players detected inside the base who
are not on the owning team. Multiple purchases of Base Defense increase the
wall defense structure's durability.

## Security Classification

An entity is classified as "government security" if:
- It is a **Guard** (always).
- It is a **Robot** with `virus_planter == 0` (not virus-implanted).
- It is a **WallDefense** with `team_id == 0` (government-owned, not
  player-purchased).

Government security entities do not damage each other (e.g., rocket guards
won't splash-damage nearby government robots).
