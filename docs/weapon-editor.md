# Weapon Editor

The Weapon Editor (`/weapons`) is the admin tool for viewing and editing every
weapon definition — damage, projectile physics, sprite banks, sounds, and
which agencies are allowed to carry each weapon.

All editing is **local** (GAS folder on your machine).  No data is sent to a
server; changes are saved by downloading the updated `weapons.json`.

---

## Opening a folder

1. Click **Open GAS folder** (shown on the empty-state landing screen).
2. Select the `shared/assets/gas/` directory.
3. The tool reads `weapons.json` and `agencies.json` from the folder.
4. The weapon list populates on the left.

Click **✕** (top-right of the header) to close the folder and clear all state.

The folder is shared with the GAS Editor — opening a folder in either tool
makes it available in the other.

---

## Layout

```
┌──────────────────┬─────────────────────────────────────────────┐
│  Weapon list     │  Detail panels                              │
│  (left)          │  Ballistics · Grenade/Rocket/Plasma         │
│                  │  Sprite Banks · Sounds · Agency Loadout     │
│                  │  Ballistics Preview                         │
└──────────────────┴─────────────────────────────────────────────┘
```

---

## Weapon list (left panel)

Lists every weapon defined in `weapons.json`.

- Click a weapon to select it and load its details on the right.
- The selected weapon is highlighted.

---

## Detail panels (right)

### Ballistics

Core projectile parameters shared by all weapon types.

| Field | Description |
|---|---|
| `projectileType` | Physics mode: `physics`, `flamer`, `rocket`, `plasma`, `grenade` |
| `healthDamage` | HP damage per hit |
| `shieldDamage` | Shield damage per hit |
| `healthDamageLarge` | HP damage for large/splash variant |
| `shieldDamageLarge` | Shield damage for large/splash variant |
| `fireDelay` | Ticks between shots |
| `velocity` | Initial projectile speed |
| `moveAmount` | Per-tick movement distance |
| `radius` | Collision radius (pixels) |
| `projectileLife` | Ticks before expiry |
| `emitOffset` | Barrel offset for spawn point |
| `exhaustPlumes` | Exhaust plume count |
| `trailPlumes` | Trail plume count |
| `primaryCount` | Number of simultaneous primary projectiles |
| `secondaryCount` | Number of secondary projectiles |
| `primaryVectors` | Per-projectile launch vectors `[{xv, yv}]` |
| `secondaryVectors` | Secondary launch vectors `[{xv, yv}]` |
| `bounceDamping` | Speed reduction per bounce (fraction) |
| `ammoCapacity` | Magazine size |
| `reloadTicks` | Reload duration in ticks |
| `snapshotInterval` | Network snapshot frequency |
| `poisonRate` | Poison stacks per hit |
| `poisonMax` | Max poison stacks inflicted |
| `poisonCheckInterval` | Ticks between poison damage ticks |

### Grenade / throw params *(grenades)*

| Field | Description |
|---|---|
| `throwSpeedStanding` | Throw speed when standing |
| `throwSpeedMoving` | Throw speed while moving |
| `throwSpeedRunning` | Throw speed while running |
| `throwXvDown` / `throwYvDown` | Throw vector aimed downward |
| `throwXvDownDiag` / `throwYvDownDiag` | Diagonal-down vector |
| `throwXvUp` / `throwYvUp` | Throw vector aimed upward |
| `throwXvUpDiag` / `throwYvUpDiag` | Diagonal-up vector |
| `throwXvCrouch` / `throwYvCrouch` | Throw vector while crouching |
| `explosionTick` | Tick of primary explosion |
| `secondaryTick` | Tick of secondary effect |
| `destroyTick` | Tick when projectile is removed |
| `neutronDestroyTick` | Neutron grenade destroy tick |
| `neutronTraceTime` | Neutron trace duration |
| `flareDuration` | Flare effect duration |
| `flareSpawnInterval` | Ticks between flare spawns |

### Rocket params *(rockets)*

| Field | Description |
|---|---|
| `rocketSlowInitial` | Speed fraction during slow launch phase |
| `rocketHoverTick` | Tick when rocket enters hover phase |
| `rocketSlowHover` | Speed fraction during hover |
| `rocketExplosionPlumeSpeed` | Explosion plume speed |
| `detonatorLaunchYv` | Upward velocity on detonation |
| `audioFadePropulsionMs` | Propulsion sound fade duration (ms) |

### Plasma params *(plasma)*

| Field | Description |
|---|---|
| `plasmaGravity` | Downward pull per tick |
| `plasmaLifeNormal` | Lifetime of normal plasma bolt |
| `plasmaLifeLarge` | Lifetime of large plasma bolt |

### Sprite Banks

Shows the directional sprite bank assignments and the hit-overlay bank.

| Element | Description |
|---|---|
| 8 direction tiles | Sprite bank index for each of the 8 fire directions (E, NE, N, NW, W, SW, S, SE). Click a tile to open the bank picker. |
| Hit overlay tile | Sprite bank used for the impact VFX overlay. Click to open the picker. |

**Bank picker modal**

- Lists all non-empty sprite banks fetched from `/api/sprites`.
- Each entry shows the first frame of the bank as a thumbnail.
- Click a bank to assign it to the selected slot.
- Click the background or **✕** to close without changing.

### Sounds

All sound slots for the weapon.  Each field is a dropdown populated from
`/api/sounds` (all uploaded sound files) plus a **▶ / ■** button to
preview or stop playback in the browser.

| Sound slot | When it plays |
|---|---|
| `soundFire` | Each shot fired |
| `soundHit1` | Primary impact |
| `soundHit2` | Secondary impact (alternate) |
| `soundLoop` | Continuous loop while firing (e.g. flamer) |
| `soundExplosion` | Explosion on detonation |
| `soundLand` | Projectile lands without exploding |
| `soundThrow` | Grenade throw |
| `soundWarn` | Warning sound (e.g. rocket lock-on) |

All slots accept either a plain filename (`"flamebg2.wav"`) or a sound cue
reference (`"cue:blaster_fire"`).  See
[sound-cue-system.md](sound-cue-system.md) for the cue reference.

### Agency Loadout

A checkbox row for every agency defined in `agencies.json`.  Check an agency
to allow its players to carry this weapon; uncheck to restrict it.

This writes to the `weapons[]` array on each `AgencyDef`, not to the weapon
definition itself.

### Ballistics Preview

A read-only visual summary of the selected weapon's key ballistic parameters —
useful for at-a-glance balance comparison without scrolling through the form.

---

## Saving changes

When any field is edited the **Download JSON** button becomes active
(indicated by the `dirty` state).  Click it to download the updated
`weapons.json` to your machine, then copy it into `shared/assets/gas/`.

> **Tip:** After saving `weapons.json` you may also need to save
> `agencies.json` if you changed agency loadouts — both files are
> written independently.

---

## API calls

The Weapon Editor makes these network calls (all require a valid admin JWT in
`localStorage` as `zs_token`):

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/sprites` | Fetch sprite bank list for the bank picker modal |
| `GET` | `/api/sprites/:bank` | Load thumbnail image for each bank tile |
| `GET` | `/api/sounds` | Populate sound dropdowns with available filenames |
| `GET` | `/api/sounds/:name/play` | Stream audio for in-browser preview |

No write endpoints are called; saving is local (file download).

---

## `weapons.json` schema

Each entry in the `weapons` array:

```jsonc
{
  "id": "blaster",               // string — unique weapon identifier
  "projectileType": "physics",   // string — physics mode
  "healthDamage": 40,            // int
  "shieldDamage": 4,             // int
  "healthDamageLarge": 0,        // int (optional)
  "shieldDamageLarge": 0,        // int (optional)
  "fireDelay": 7,                // int — ticks between shots
  "velocity": 0,                 // int (optional)
  "moveAmount": 10,              // int — px/tick
  "radius": 0,                   // int — collision radius
  "projectileLife": 6,           // int — ticks before expiry
  "emitOffset": 0,               // int — barrel offset
  "exhaustPlumes": 0,            // int
  "trailPlumes": 0,              // int
  "rocketExplosionPlumeSpeed": 0,// int (rockets)
  "primaryCount": 1,             // int
  "secondaryCount": 0,           // int
  "primaryVectors": [],          // [{xv, yv}]
  "secondaryVectors": [],        // [{xv, yv}]
  "bounceDamping": 0,            // number
  "ammoCapacity": 0,             // int
  "reloadTicks": 0,              // int
  "snapshotInterval": 6,         // int
  "poisonRate": 0,               // int
  "poisonMax": 0,                // int
  "poisonCheckInterval": 0,      // int
  "spriteBanks": [160,161,162,163,164,163,162,161], // int[8] — one per direction
  "hitOverlayBank": 222,         // int
  "soundFire": "cue:blaster_fire",
  "soundHit1": "cue:blaster_hit",
  "soundHit2": "",
  "soundLoop": "",
  "soundExplosion": "",
  "soundLand": "",
  "soundThrow": "",
  "soundWarn": "",
  "audioFadePropulsionMs": 0,    // int (rockets/flamer)
  // Grenade-only:
  "throwSpeedStanding": 0, "throwSpeedMoving": 0, "throwSpeedRunning": 0,
  "throwXvDown": 0, "throwYvDown": 0,
  "throwXvDownDiag": 0, "throwYvDownDiag": 0,
  "throwXvUp": 0, "throwYvUp": 0,
  "throwXvUpDiag": 0, "throwYvUpDiag": 0,
  "throwXvCrouch": 0, "throwYvCrouch": 0,
  "explosionTick": 0, "secondaryTick": 0, "destroyTick": 0,
  "neutronDestroyTick": 0, "neutronTraceTime": 0,
  "flareDuration": 0, "flareSpawnInterval": 0,
  // Rocket-only:
  "rocketSlowInitial": 0.2, "rocketHoverTick": 100, "rocketSlowHover": 0.3,
  "detonatorLaunchYv": 0,
  // Plasma-only:
  "plasmaGravity": 0, "plasmaLifeNormal": 0, "plasmaLifeLarge": 0
}
```

### `projectileType` values

| Value | Description |
|---|---|
| `physics` | Standard ballistic (blaster, laser) |
| `flamer` | Continuous flame stream |
| `rocket` | Guided/unguided rocket with hover phase |
| `plasma` | Gravity-affected plasma bolt |
| `grenade` | Thrown explosive with timed detonation |

---

## Deployment workflow

1. Open the GAS folder, edit weapon fields, adjust sprite banks and sounds.
2. Click **Download JSON** → saves `weapons.json`.
3. If agency loadouts changed, save `agencies.json` via the GAS Editor.
4. Copy the downloaded files into `shared/assets/gas/`.
5. Rebuild / restart the game server.
