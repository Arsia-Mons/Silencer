# 02 — Entities & Objects

## Object Hierarchy

Every in-game entity is an **Object**. Objects compose multiple trait mixins:

```
Object
 ├── Sprite      — rendering: position, draw flag, mirror, render pass
 ├── Physical    — physics: xv, yv, collidable
 ├── Hittable    — combat: health, shield, hit state
 ├── Bipedal     — ground following: current platform, walk helpers
 └── Projectile  — projectile data: owner, damage, velocity, radius
```

Each trait is optional. An object declares which traits are active via boolean
flags:

| Flag | Meaning |
|------|---------|
| `is_sprite` | Has a visual representation (default `true`) |
| `is_physical` | Participates in physics / collision |
| `is_hittable` | Can receive damage |
| `is_bipedal` | Follows ground surfaces |
| `is_projectile` | Carries projectile damage data |
| `is_controllable` | Receives player input |

## Object Lifecycle

1. **Creation** — `CreateObject(type, id?)` allocates the object and assigns a
   unique 16-bit ID. Authoritative objects use IDs 1–32,000; local-only
   objects use IDs with the high bit set (0x8000+).
2. **Tick** — each tick, every object's `Tick()` is called to advance its
   state machine.
3. **Serialization** — objects serialize/deserialize their state for network
   snapshots.
4. **Destruction** — objects are marked for deferred destruction
   (`MarkDestroyObject`) and removed after the current tick completes.

## Entity Types

The following entity types exist. Types are identified by a sequential enum
starting at 0.

### UI / Interface (local-only, not authoritative)

| Type | Name | Purpose |
|------|------|---------|
| OVERLAY | Overlay | Text/sprite overlay |
| INTERFACE | Interface | UI container |
| BUTTON | Button | Clickable button |
| TOGGLE | Toggle | On/off toggle |
| SELECTBOX | SelectBox | Scrollable list |
| SCROLLBAR | ScrollBar | Scroll control |
| TEXTBOX | TextBox | Text display |
| TEXTINPUT | TextInput | Editable text field |
| STATE | State | Game state indicator |

### Gameplay Entities (authoritative)

| Type | Name | Purpose |
|------|------|---------|
| TEAM | Team | Team/agency container; tracks secrets, tech, peers |
| PLAYER | Player | Player-controlled agent |
| CIVILIAN | Civilian | Neutral NPC that walks platforms |
| GUARD | Guard | Government security — bipedal gunman |
| ROBOT | Robot | Government security — armored melee unit |
| TERMINAL | Terminal | Hackable computer; yields files and secret intel |
| VENT | Vent | Activatable air vent (movement shortcut) |
| HEALMACHINE | HealMachine | In-base health/shield restorer |
| CREDITMACHINE | CreditMachine | In-base file-to-credits converter |
| SECRETRETURN | SecretReturn | In-base secret delivery point |
| SURVEILLANCEMONITOR | SurveillanceMonitor | In-base camera viewer |
| TECHSTATION | TechStation | In-base tech repair/virus target |
| INVENTORYSTATION | InventoryStation | In-base shop access point |
| TEAMBILLBOARD | TeamBillboard | Displays team info |

### Projectiles

| Type | Name | Details |
|------|------|---------|
| BLASTERPROJECTILE | Blaster shot | Default weapon projectile |
| LASERPROJECTILE | Laser shot | High shield damage |
| ROCKETPROJECTILE | Rocket | Splash damage on impact |
| FLAMERPROJECTILE | Flamer shot | Bypasses shields |
| PLASMAPROJECTILE | Plasma shot | Area damage, passes through targets |
| FLAREPROJECTILE | Flare | Stationary area denial |
| WALLPROJECTILE | Wall turret shot | Fired by wall defenses |

### Deployables & Items

| Type | Name | Purpose |
|------|------|---------|
| BASEDOOR | BaseDoor | Team base entrance/exit |
| PICKUP | PickUp | Dropped or spawned collectible |
| WARPER | Warper | Paired teleportation point |
| DETONATOR | Detonator | Remote explosive or camera |
| FIXEDCANNON | FixedCannon | Deployable auto-turret |
| GRENADE | Grenade | Thrown explosive (multiple sub-types) |
| WALLDEFENSE | WallDefense | In-base wall-mounted turret |
| BASEEXIT | BaseExit | Base exit trigger zone |

### Effects (visual only)

| Type | Name | Purpose |
|------|------|---------|
| PLUME | Plume | Explosion/smoke effect |
| SHRAPNEL | Shrapnel | Hit-impact particles |
| BODYPART | BodyPart | Death giblets |

## Common Object Properties

Every object carries:

| Property | Type | Description |
|----------|------|-------------|
| `type` | u8 | Entity type enum value |
| `id` | u16 | Unique instance identifier |
| `x`, `y` | s16 | World position (pixels) |
| `requires_authority` | bool | Only the server can create this object |
| `requires_map_loaded` | bool | Object depends on map data |
| `snapshot_interval` | int | Ticks between network snapshots (−1 = never) |

## Sprite Properties

| Property | Type | Description |
|----------|------|-------------|
| `res_bank` | u8 | Sprite sheet index |
| `res_index` | u8 | Frame index within sheet |
| `draw` | bool | Whether to render |
| `mirrored` | bool | Flip horizontally |
| `render_pass` | u8 | Draw order layer (1 = back, 2 = mid, 3 = front) |
| `draw_checkered` | bool | Render with checkered transparency |
