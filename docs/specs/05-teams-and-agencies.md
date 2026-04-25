# 05 — Teams & Agencies

## Agencies

There are **5 playable agencies**, each with a unique identity, default stat
bonuses, and one exclusive item.

| Index | Name | Specialty | Default Bonus | Exclusive Item |
|-------|------|-----------|---------------|----------------|
| 0 | **Noxis** | Endurance | +3 Endurance | Health Pack |
| 1 | **Lazarus** | Resurrection | (none stat) | Lazarus Tract, self-resurrect |
| 2 | **Caliber** | Contacts | +3 Contacts | Security Pass |
| 3 | **Static** | Hacking | +3 Hacking | Virus |
| 4 | **Black Rose** | Shield | +2 Shield | Poison Flare |

### Agency-Specific Abilities

- **Noxis** — jump impulse bonus of **−3** (jumps higher). Higher base
  endurance means more health.
- **Lazarus** — can self-resurrect once per life from the DEAD state
  (RESURRECTING state). Has the Lazarus Tract item to convert civilians.
- **Caliber** — starts with higher contacts bonus, earning more credits per
  file return. Security Pass makes guards and robots ignore the player.
- **Static** — higher base hacking speed. Has the Virus item to subvert enemy
  tech stations, robots, and fixed cannons.
- **Black Rose** — higher base shield. Poison Flare is an enhanced flare that
  poisons targets.

## Team Structure

Each **Team** object represents one team in a match.

| Property | Type | Description |
|----------|------|-------------|
| `agency` | u8 | Which agency (0–4) |
| `number` | u8 | Team slot (0–5) |
| `color` | u8 | Suit color override (0 = auto from number) |
| `num_peers` | u8 | Current player count |
| `peers[4]` | u8[4] | Peer IDs of team members (max 4 per team) |
| `secrets` | u8 | Number of secrets delivered |
| `secret_delivered` | u16 | ID of player who just delivered a secret (transient) |
| `secret_progress` | u8 | Insider-info accumulated progress (0–180) |
| `base_door_id` | u16 | Object ID of the team's base door |
| `beaming_terminal_id` | u16 | Terminal currently beaming a secret for this team |
| `disabled_tech` | u32 | Bitmask of technologies disabled by enemy viruses |
| `player_with_secret` | u16 | ID of the player currently carrying a secret |

### Maximum Team Size

A team can have at most **4 players**. A match supports up to **6 teams**.

## Team Colors

Each team slot has a default color derived from its number:

| Team # | Base Color | Shade |
|--------|-----------|-------|
| 0 | 10 (green) | 7 |
| 1 | 14 (blue) | 8 |
| 2 | 13 (pink) | 8 |
| 3 | 9 (red) | 8 |
| 4 | 15 (white) | 11 |
| 5 | 12 (orange) | 10 |

Black Rose agency overrides the base color to 8 with shade `8 + (number − 4)`.

Colors are encoded as a single byte: `(shade << 4) | base_color`.

## Tech Tree

Each team has access to a set of **technologies**. Technologies are represented
as bits in a 32-bit mask.

- Each peer (player) contributes their personal `tech_choices` mask to the
  team's available tech via bitwise OR.
- A team's available tech = OR of all members' `tech_choices`.
- Items in the buy menu require specific tech bits to be available.
- Enemy **virus** attacks can disable tech bits (`disabled_tech` mask). A
  disabled tech prevents purchasing the associated item.
- Tech can be **repaired** at a tech station for a cost.

### Tech Bit Assignments

| Bit | Item | Tech Slots |
|-----|------|------------|
| 0 | Laser | 1 |
| 1 | Rocket | 1 |
| 2 | Flamer Ammo | 1 |
| 3 | Health Pack | 1 |
| 4 | — | — |
| 5 | Shaped Bomb | 1 |
| 6 | Plasma Bomb | 2 |
| 7 | Neutron Bomb | 8 |
| 8 | Plasma Detonator | 2 |
| 9 | Fixed Cannon | 1 |
| 10 | Flare | 1 |
| 11 | Base Door | 1 |
| 12 | Base Defense | 1 |
| 13 | Insider Info | 1 |
| 14 | Lazarus Tract | 1 |
| 15 | — | — |
| 16 | Poison Flare | 1 |
| 17 | Security Pass | 1 |
| 18 | Camera | 1 |
| 19 | Virus | 1 |

Each player's `tech_choices` is a bitmask indicating which tech bits they
bring to their team. The number of tech slots consumed by each item is listed
above.

### Tech Slots Per Player

Each user has a **tech slots** stat (default 3, max 8). This limits the total
tech slot weight of items they can enable in their `tech_choices`.

## Finding a Team

When a peer joins a game, the server assigns them to a team of the requested
agency. If no team of that agency has room, a new team is created (up to 6).
