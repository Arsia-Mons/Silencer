# 09 — Map & Environment

## Map Structure

Each map is a binary file containing tile data, actor placements, and platform
geometry.

### Map Header

| Field | Type | Description |
|-------|------|-------------|
| `first_byte` | u8 | File signature byte |
| `version` | u8 | Map format version |
| `max_players` | u8 | Maximum players the map supports |
| `max_teams` | u8 | Maximum teams the map supports |
| `width` | u16 | Map width in tiles |
| `height` | u16 | Map height in tiles |
| `parallax` | u8 | Parallax background mode |
| `ambience` | s8 | Ambient lighting level |
| `flags` | u32 | Map feature flags |
| `description` | char[128] | Human-readable map name/description |
| `minimap_compressed_size` | u32 | Size of compressed minimap data |
| `minimap_compressed` | u8[…] | Compressed minimap bitmap (172×62) |
| `level_size` | u32 | Size of level data |

### Tile Data

The map is a grid of `width × height` cells. Each cell has 4 tile layers
(parallax layers). Each layer contains:

| Field | Type | Description |
|-------|------|-------------|
| `fg` | u16 | Foreground tile index |
| `bg` | u16 | Background tile index |
| `fg_flags` | int | Foreground flags (FLIPPED, LUM) |
| `bg_flags` | int | Background flags (FLIPPED, LUM) |

Tile flags:
- **FLIPPED** (bit 0) — horizontally mirrored.
- **LUM** (bit 1) — luminosity modifier.

Each tile occupies **64×64 pixels** in world space.

## Platforms

Platforms are the collision geometry of the map. They define where entities
can walk, climb, and collide.

### Platform Properties

| Property | Type | Description |
|----------|------|-------------|
| `type` | u8 | Collision type bitmask |
| `id` | u16 | Unique platform identifier |
| `x1, y1` | int | Top-left corner |
| `x2, y2` | int | Bottom-right corner |

### Platform Types

| Type | Description |
|------|-------------|
| RECTANGLE | Solid axis-aligned surface |
| STAIRSUP | Slope ascending left to right |
| STAIRSDOWN | Slope descending left to right |
| LADDER | Vertical climbable surface |
| TRACK | Movement track |
| OUTSIDEROOM | Marks outdoor area (rain renders here) |
| SPECIFICROOM | Associates geometry with a named room |

### Adjacent Platforms

Platforms that share an edge are linked as **adjacent**. This allows bipedal
entities to walk seamlessly between connected surfaces, including transitioning
between flat and sloped segments.

### Platform Sets

Connected sequences of adjacent platforms form **PlatformSets**. These are used
by AI pathfinding to determine reachable areas.

## Actors (Map-Placed Entities)

The map file contains a list of **actors** — pre-placed entities with
positions and properties.

| Actor Type ID | Entity | Notes |
|---------------|--------|-------|
| 4 | Player start location | Random spawn selection |
| 5 | Guard | Sub-type: 0 = patrol, 1 = stationary |
| 6 | Robot | Sub-type: 0 = patrol, 1 = guard |
| 48 | Small terminal | Hackable computer |
| 49 | Big terminal | Used for secret beaming |
| 50 | Civilian | Neutral NPC |
| 54 | Heal machine | In-base health restorer |
| 55 | Credit machine | In-base file converter |
| 56 | Inventory station | In-base shop access |
| 57 | Tech station | In-base tech management |
| 58 | Secret return | In-base secret delivery point |
| 61 | Warper | Teleportation pad (paired) |
| 63 | Power-up | Sub-types: see Power-Ups |
| 64 | Vent | Activatable air vent |
| 65 | Base exit | Base exit zone |
| 67 | Surveillance monitor | In-base camera viewer |

Actors associated with a team base have a vertical offset applied based on the
base's Y position in the expanded map.

## Bases

Each team's base is a separate region of the map, typically stacked vertically
below the main play area in an **expanded map** section. The base contains:

- **BaseDoor** — the entrance/exit warp door (can be relocated by the player).
- **HealMachine** — restores health and shield (240-tick cooldown).
- **CreditMachine** — converts files to credits.
- **SecretReturn** — accepts delivered secrets.
- **InventoryStation** — provides access to the buy menu.
- **TechStation** — allows tech repair and is a virus target. Has 240 health
  and 240 shield. Can be destroyed; destroyed tech randomly disables one of
  the team's available tech items.
- **WallDefense** — in-base laser turrets.
- **SurveillanceMonitor** — displays camera feeds.

### Base Discovery

Enemy bases must be **discovered** before a player can enter. A BaseDoor
tracks `discovered_by[team]` and `entered_by[team]` arrays. Players can only
enter a base door that has been discovered by their team.

## Power-Ups

Power-ups are special pickups placed on the map that grant temporary abilities.

| Sub-Type | Name | Effect |
|----------|------|--------|
| 0 | Super Shield | Greatly enhances shield |
| 1 | Neutron Bomb | Free neutron bomb pickup |
| 2 | Jetpack | Extended jetpack fuel/time |
| 3 | Invisible | Temporary invisibility |
| 4 | Hacking | Double hacking speed for a duration |
| 5 | Radar | Reveals all players on the minimap |
| 6 | Depositor | Allows depositing files without returning to base |

Power-ups respawn after **60 seconds** (`powerup_respawn_time = 60`). They
have a countdown animation before becoming collectible.

## Warpers

Warpers are paired teleportation pads. Each warper has an `actor_match` ID
linking it to its partner. When a player steps on a warper, they are teleported
to the matched warper's location.

A warper has a cooldown (`countdown`) after each use.

## Vents

Vents are activatable environmental objects. A player can press `activate`
near a vent to trigger it. Vents serve as movement shortcuts, typically
launching the player to another location.

## Rain

Rain renders in areas marked with the **OUTSIDEROOM** platform type but only
where there is no solid platform (STAIRSUP, STAIRSDOWN, RECTANGLE) overhead.
Rain puddle locations are pre-calculated based on platform layout.

## Surveillance Cameras

The map pre-places surveillance camera locations. These are positions that
in-base surveillance monitors display as feed options.
