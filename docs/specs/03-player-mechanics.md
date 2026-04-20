# 03 — Player Mechanics

## Player Defaults

| Property | Default | Description |
|----------|---------|-------------|
| `max_health` | 100 | Base maximum health |
| `max_shield` | 100 | Base maximum shield |
| `max_fuel` | 80 | Base jetpack fuel |
| `max_files` | 2,800 | Maximum downloadable files a player can carry |
| `height` | 50 | Collision box height (pixels) |
| `credits` | 0 | Starting credits (game create may override) |

## Input Model

Each tick the player produces an **Input** structure containing:

| Input | Description |
|-------|-------------|
| `move_up` | Move up / climb ladder up |
| `move_down` | Move down / climb ladder down |
| `move_left` | Move left |
| `move_right` | Move right |
| `look_up_left` | Aim diagonally up-left |
| `look_up_right` | Aim diagonally up-right |
| `look_down_left` | Aim diagonally down-left |
| `look_down_right` | Aim diagonally down-right |
| `jump` | Jump |
| `jetpack` | Activate jetpack |
| `fire` | Fire current weapon |
| `activate` | Interact (hack terminals, enter doors, etc.) |
| `use` | Use current inventory item |
| `disguise` | Toggle disguise |
| `next_inv` | Cycle to next inventory slot |
| `next_cam` | Cycle to next camera |
| `prev_cam` | Cycle to previous camera |
| `detonate` | Trigger remote detonators |
| `weapon[0–3]` | Select weapon slot directly |
| `next_weapon` | Cycle to next available weapon |

## Player States

The player operates as a finite state machine. Each state has a frame counter
(`state_i`) that increments each tick.

| State | Description |
|-------|-------------|
| **DEPLOYING** | Spawn beam-in animation. Invisible for 60 ticks, then visible for 8 ticks. Transitions to STANDING. |
| **UNDEPLOYING** | Beam-out animation on disconnect or game end. |
| **STANDING** | Idle on ground. Can transition to RUNNING, JUMPING, CROUCHING, STANDINGSHOOT, HACKING, WALKIN, etc. |
| **RUNNING** | Horizontal movement on ground. |
| **WALKIN** | Walking into a base door. |
| **WALKOUT** | Walking out of a base door. |
| **FALLING** | Airborne without jetpack. Gravity applies. |
| **LADDER** | Climbing a ladder. No gravity. |
| **CROUCHING** | Transition to crouched position (4 ticks). |
| **UNCROUCHING** | Transition from crouched to standing (4 ticks). |
| **CROUCHED** | Fully crouched. Reduced hitbox. |
| **CROUCHEDSHOOT** | Firing while crouched. |
| **CROUCHEDTHROWING** | Throwing grenade while crouched. |
| **ROLLING** | Crouched roll in mirrored direction. |
| **JUMPING** | Initial jump impulse applied, then transitions to FALLING. |
| **CLIMBINGLEDGE** | Climbing up a ledge. |
| **JETPACK** | Jetpack active. Fuel consumed each tick. |
| **HACKING** | Hacking a terminal. Player is stationary. |
| **STANDINGSHOOT** | Firing while standing. |
| **FALLINGSHOOT** | Firing while airborne (non-jetpack). |
| **LADDERSHOOT** | Firing while on a ladder. |
| **JETPACKSHOOT** | Firing while using jetpack. |
| **DYING** | Death animation. Drops all items. |
| **DEAD** | Lying dead. After 48 ticks or on activate, respawn. |
| **RESPAWNING** | Respawn beam-in at base. |
| **THROWING** | Throwing a grenade while standing. |
| **RESURRECTING** | Lazarus agency self-resurrect from DEAD state. |

## Movement

### Ground Movement

- Running speed is controlled by `xv`. The player accelerates at **3 units/tick**
  when a direction key is held and decelerates at **2 units/tick** when released.
- On slopes (STAIRSUP / STAIRSDOWN), the engine follows the surface by mapping
  `x` to `y` via the platform's slope function.

### Jumping

- Jump applies an impulse of **−17** to `yv` (upward), modified by
  `jump_impulse` (agency bonus; Noxis gets −3 extra).
- Jumping from a ladder with no horizontal input applies a stronger impulse of
  **−29**.
- Jumping from a ladder with the `activate` key held applies a weaker impulse
  of **−8** (for precise movement).

### Jetpack

- When `jetpack` input is active and fuel > 0, the player enters JETPACK
  state.
- Fuel decreases by **1 each tick** while the jetpack is active.
- When fuel reaches 0, the `fuel_low` flag is set and the jetpack cannot be
  reactivated until the player lands.
- Jetpack horizontal speed is capped at **±14**.
- Jetpack vertical speed is capped at **−9** (upward).
- Base fuel is 80; agency upgrades add **+10 per jetpack level**.
- A jetpack power-up grants bonus fuel time.

### Ladder

- A player grabs a ladder when pressing up or down while overlapping a LADDER
  platform and the horizontal center is close enough.
- On a ladder, gravity does not apply. The player moves up/down at
  **1 unit/tick**.

## Health & Shield

- **Shield** absorbs damage first. Each projectile has separate `shield_damage`
  and `health_damage` values.
- If the target has a shield and the projectile does **not** bypass shields:
  - Shield takes `shield_damage`.
  - If shield is depleted by the hit, overflow damage is calculated:
    `overflow = (abs(shield − shield_damage) / shield_damage) × health_damage`
    and applied to health.
- If the target has no shield or the projectile **bypasses** shields:
  - Health takes `health_damage` directly.
- When health reaches 0, the entity enters its death state.

## Poison

- Poison is applied by certain items (poison, poison flare).
- Poison amount increases by the item's value, capped at **9**
  (`max_poisoned`).
- Poison can only be applied to players on a different team.
- Poison is cleared on death or by using a heal machine.

## Disguise

- Players can activate a disguise (`disguise` input). While disguised
  (`disguised >= 100`), the player appears as a civilian to other players and
  to guards.
- Firing a weapon, using items, or taking certain actions automatically
  **removes** the disguise.

## Invisibility

- Granted by the invisibility power-up. While active, the player is invisible
  to guards, robots, and other players.
- Has a time limit tracked by `invisible_bonus_time`.

## Inventory

The player has **4 inventory slots**. Each slot holds one item type and a
quantity count.

| Inventory Item | Description |
|----------------|-------------|
| NONE | Empty slot |
| HEALTHPACK | Restores health to maximum when used |
| LAZARUSTRACT | Converts a nearby civilian to the player's team |
| SECURITYPASS | Guards ignore the player in the field |
| VIRUS | Infects a nearby robot or cannon, or disables an enemy tech station |
| POISON | Poisons a nearby enemy player |
| NEUTRONBOMB | Kills all entities in the region (except those inside a base) |
| EMPBOMB | EMP grenade |
| SHAPEDBOMB | Directional upward explosion |
| PLASMABOMB | High-damage area grenade |
| PLASMADET | Remote-detonated explosive |
| FIXEDCANNON | Deployable auto-turret |
| FLARE | Stationary area-denial torch |
| POISONFLARE | Flare that also poisons victims |
| BASEDOOR | Relocates the team's base entrance |
| CAMERA | Remote viewing device |

Every player starts with one **BASEDOOR** in their inventory.

## Weapons

Players can switch between 4 weapon slots:

| Slot | Weapon | Ammo | Max Ammo |
|------|--------|------|----------|
| 0 | Blaster | Infinite | — |
| 1 | Laser | `laser_ammo` | 30 |
| 2 | Rocket | `rocket_ammo` | 30 |
| 3 | Flamer | `flamer_ammo` | 75 |

Switching to a weapon slot that has 0 ammo falls back to the Blaster (slot 0).

See [04 — Weapons & Projectiles](04-weapons-and-projectiles.md) for detailed
weapon stats.

## Base Interaction

- Players enter a base by pressing `activate` near a discovered BaseDoor.
- Inside their own base, players can interact with:
  - **HealMachine** — restore health and shield to maximum (cooldown: 240
    ticks ≈ 10 seconds at 24 ticks/sec).
  - **CreditMachine** — convert carried files to credits. Credits awarded =
    `files × (1 + credits_bonus)`.
  - **SecretReturn** — deliver a carried secret to score.
  - **InventoryStation** — open the buy menu.
  - **TechStation** — repair or virus enemy tech.
- Players can only buy items when near an InventoryStation inside their own
  base.

## Respawning

1. On death, the player enters DYING → DEAD.
2. From DEAD, after 48 ticks or on `activate` press:
   - **Lazarus agency** players who have not yet used their resurrection
     ability enter RESURRECTING (self-revive on the spot).
   - Otherwise, the player warps to their base (RESPAWNING) with full
     health/shield.
   - If no base exists, the player warps to a random spawn location.
