# 10 — Progression & Stats

## User Accounts

Each player has a persistent account tracked by the lobby server. Accounts
store per-agency progression data.

### Account Data

| Field | Type | Description |
|-------|------|-------------|
| `account_id` | u32 | Unique account identifier |
| `name` | string | Display name (max 63 characters) |
| `password` | hash | SHA-1 hashed password |
| `agency[5]` | struct | Per-agency stats and upgrades |

## Per-Agency Progression

Each account tracks independent progression for all 5 agencies.

| Field | Type | Default | Max | Description |
|-------|------|---------|-----|-------------|
| `wins` | u16 | 0 | — | Games won with this agency |
| `losses` | u16 | 0 | — | Games lost with this agency |
| `xp_to_next_level` | u16 | 0 | — | XP remaining until next level |
| `level` | u8 | 0 | 99 | Current level |
| `endurance` | u8 | 0* | 5* | Bonus max health (+20 per point) |
| `shield` | u8 | 0* | 5* | Bonus max shield (+20 per point) |
| `jetpack` | u8 | 0 | 5 | Bonus jetpack fuel (+10 per point) |
| `tech_slots` | u8 | 3 | 8 | Maximum equippable tech weight |
| `hacking` | u8 | 0* | 5* | Hacking speed bonus (+0.10 per point) |
| `contacts` | u8 | 0* | 5* | Credits bonus (+0.10 per point) |

\* Some agencies have default bonuses that raise the starting value and may
raise the maximum:

| Agency | Stat | Default Bonus | Effective Max |
|--------|------|---------------|---------------|
| Noxis | Endurance | +3 | 8 |
| Caliber | Contacts | +3 | 8 |
| Static | Hacking | +3 | 8 |
| Black Rose | Shield | +2 | 7 |
| Lazarus | (none) | — | — |

## Stat Effects In-Game

When a player spawns, their abilities are loaded from their account:

| Stat | In-Game Effect |
|------|----------------|
| Endurance | `max_health += endurance × 20` |
| Shield | `max_shield += shield × 20` |
| Jetpack | `max_fuel += jetpack × 10` |
| Hacking | `hacking_bonus = hacking × 0.10` |
| Contacts | `credits_bonus = contacts × 0.10` |
| Tech Slots | Determines total tech weight the player can equip |

**Noxis special:** players on Noxis also receive a jump impulse bonus of
`−3` (they jump higher).

## Upgrade Points

Players earn upgrade points by leveling up. Each level grants points that can
be spent on any stat.

Total possible upgrade points for an agency =
`max_contacts + max_endurance + max_hacking + max_jetpack + max_shield + max_tech_slots − default_bonuses`

## Experience & Leveling

Experience is earned at the end of each game based on in-match statistics.
`CalculateExperience()` (`ui/stats.cpp`) tallies weighted values from the stat
sheet and returns an XP amount (minimum 0). The lobby server applies the gain
via `UpdateStats()` (`services/lobby/store.go`), which advances the player's
level as many times as the XP warrants.

### XP Awards per Match

| Action | XP |
|--------|-----|
| Player kill | +35 |
| Death | −10 |
| Secret returned to own base | +125 |
| Secret stolen (enemy secret returned) | +200 |
| Secret picked up | +25 |
| Secret dropped (died while carrying) | −10 |
| Guard killed | +5 |
| Robot killed | +8 |
| Fixed cannon destroyed | +10 |
| Wall defense destroyed | +10 |
| Civilian killed | −2 |
| Blaster hits (per 3) | +1 |
| Laser hits (per 3) | +2 |
| Rocket hits (per 3) | +3 |
| Files hacked (per 50) | +1 |
| Files returned (per 50) | +1 |

Total XP for a match is clamped to a minimum of 0.

### Level Thresholds

`XPToNextLevel` stores XP accumulated within the **current level** bucket
(range: 0 to threshold − 1). The threshold to advance from level N to N+1 is:

```
threshold(N) = 100 × (N + 1)
```

This means each level costs 100 more XP than the previous one. Max level is 99.

**Cumulative XP formula** (total XP needed to reach level N from level 0):

```
total_xp(N) = 50 × N × (N + 1)
```

**Level breakdown (selected milestones):**

| Level | XP for this level | Cumulative XP to reach |
|-------|-------------------|------------------------|
| 0 | 100 | 0 |
| 1 | 200 | 100 |
| 2 | 300 | 300 |
| 3 | 400 | 600 |
| 4 | 500 | 1,000 |
| 5 | 600 | 1,500 |
| 10 | 1,100 | 5,500 |
| 15 | 1,600 | 12,000 |
| 20 | 2,100 | 21,000 |
| 25 | 2,600 | 32,500 |
| 30 | 3,100 | 46,500 |
| 40 | 4,100 | 82,000 |
| 50 | 5,100 | 127,500 |
| 60 | 6,100 | 183,000 |
| 70 | 7,100 | 248,500 |
| 75 | 7,600 | 285,000 |
| 80 | 8,100 | 324,000 |
| 90 | 9,100 | 409,500 |
| 95 | 9,600 | 456,000 |
| 99 | 10,000 | 495,000 |

The UI displays **XP remaining** to next level:

```
xp_remaining = 100 × (level + 1) − XPToNextLevel
```

## Match Statistics

At the end of each match, the following statistics are recorded per player:

### Combat Stats

| Stat | Description |
|------|-------------|
| `weapon_fires[4]` | Shots fired per weapon slot |
| `weapon_hits[4]` | Shots that hit a target per weapon slot |
| `player_kills_weapon[4]` | Player kills per weapon slot |
| `kills` | Total player kills |
| `deaths` | Total deaths |
| `suicides` | Self-inflicted deaths |
| `poisons` | Successful poison applications |

### NPC Stats

| Stat | Description |
|------|-------------|
| `civilians_killed` | Civilians killed |
| `guards_killed` | Guards killed |
| `robots_killed` | Robots killed |
| `defense_killed` | Wall defenses destroyed |

### Objective Stats

| Stat | Description |
|------|-------------|
| `secrets_picked_up` | Secrets collected |
| `secrets_returned` | Secrets delivered to base |
| `secrets_stolen` | Enemy secrets returned |
| `secrets_dropped` | Secrets lost (death while carrying) |
| `files_hacked` | Files downloaded from terminals |
| `files_returned` | Files converted at credit machine |
| `powerups_picked_up` | Power-ups collected |

### Item Usage Stats

| Stat | Description |
|------|-------------|
| `tracts_planted` | Lazarus Tracts used on civilians |
| `grenades_thrown` | Total grenades thrown |
| `neutrons_thrown` | Neutron bombs deployed |
| `emps_thrown` | EMP bombs thrown |
| `shaped_thrown` | Shaped bombs thrown |
| `plasmas_thrown` | Plasma bombs thrown |
| `flares_thrown` | Flares placed |
| `poison_flares_thrown` | Poison flares placed |
| `health_packs_used` | Health packs consumed |
| `fixed_cannons_placed` | Fixed cannons deployed |
| `fixed_cannons_destroyed` | Fixed cannons lost |
| `dets_planted` | Detonators placed |
| `cameras_planted` | Cameras placed |
| `viruses_used` | Viruses deployed |

## Lobby Storage

User accounts and stats are persisted in `lobby.json` (flat-file, atomic
writes). The lobby server maintains this data and serves it to clients on
connection.
