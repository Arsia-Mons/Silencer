# shared/assets/gas — GAS JSON data files

Runtime game data consumed by:
- **C++ client** — `GASLoader::Load()` in `clients/silencer/src/gas/gasloader.cpp`
  reads these at startup; hot-reloaded via the `gas reload` control-socket op.
- **Admin web** — `/gas` page validates and edits these files in-browser.
- **`silencer-cli gas validate`** — runs `validateDirectory()` from
  `shared/gas-validation/` against this folder.

## Files

| File | C++ struct | Description |
|---|---|---|
| `agencies.json` | `AgencyDef` | Agency definitions, upgrade caps, weapon allow-lists |
| `player.json` | `PlayerDef` | Player stats, movement speeds, jump impulses |
| `gameengine.json` | `GameEngineDef` | Tick rate, audio fades, network/physics constants |
| `weapons.json` | `WeaponDef` | Weapon ballistics, sprites, sounds, ammo |
| `items.json` | `ItemDef` | Pickup and tech-slot item definitions |
| `enemies.json` | `EnemyDef` | Guard, civilian, robot stats and timers |
| `abilities.json` | `AbilityDef` | Active ability parameters |
| `effects.json` | `EffectDef` | VFX particle effect parameters |
| `lights.json` | `LightDef` | Point-light definitions |
| `gameobjects.json` | `GameObjectDef` | Doors, vents, terminals, barrels, etc. |
| `world.json` | `WorldDef` | Map-wide physics constants |

## Rules

- All values in the seed files are 1:1 copies of the original C++ hardcoded
  values. **Do not change balance during a migration phase** — only wire
  the field, keep the number identical to what was compiled in.
- Schema is enforced by `shared/gas-validation/schemas.ts`. If you add a
  field, update the schema and the C++ struct in the same PR.
- Validate before committing: `silencer-cli gas validate <path-to-this-dir>`
  or use the `/gas` page in the admin dashboard.
