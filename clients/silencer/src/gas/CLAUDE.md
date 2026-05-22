# clients/silencer/src/gas — C++ GAS loader

Two files: `gasloader.h` (all `*Def` structs + `GASLoader` singleton) and
`gasloader.cpp` (JSON parsing + error emission). The loader reads
`shared/assets/gas/*.json` at startup and populates typed C++ structs that
the rest of the engine reads instead of hardcoded values.

## Struct catalogue

| Struct | JSON file | What it owns |
|---|---|---|
| `AgencyDef` | `agencies.json` | upgrade caps, default upgrades, weapon allow-list |
| `PlayerDef` | `player.json` | health, shield, fuel, move speeds, jump impulses |
| `GameEngineDef` | `gameengine.json` | tick rate, audio fades, network/physics constants |
| `WeaponDef` | `weapons.json` | ballistics, sprites (8-dir), sounds, ammo, spread |
| `ItemDef` | `items.json` | pickup/tech-slot items, spawn ammo, price |
| `EnemyDef` | `enemies.json` | guard/civilian/robot stats, patrol, sounds, timers |
| `AbilityDef` | `abilities.json` | active ability stats |
| `EffectDef` | `effects.json` | VFX particle parameters |
| `LightDef` | `lights.json` | point-light radius, colour |
| `GameObjectDef` | `gameobjects.json` | doors, vents, terminals, barrels, etc. |
| `WorldDef` | `world.json` | map-wide physics constants |

## GASLoader singleton

```cpp
GASLoader& loader = GASLoader::Get();
loader.Load(gasDir);          // called once at startup
loader.Reload(gasDir);        // called by control-socket "gas reload"

// Access loaded data:
loader.agencies    // std::vector<AgencyDef>
loader.player      // PlayerDef
loader.weapons     // std::vector<WeaponDef>
// ... etc.

// Check errors:
loader.lastLoadErrors  // std::vector<GASLoadError> — empty = clean
```

## Error shape

```cpp
struct GASLoadError {
    std::string file;          // e.g. "weapons.json"
    std::string instancePath;  // RFC 6901 JSON Pointer e.g. "/weapons/3/fireDelay"
    std::string code;          // OPEN_FAILED | PARSE_ERROR | SCHEMA_ERROR | FIELD_ERROR
    std::string message;
};
```

This shape is identical to the TypeScript `GASError` in
`shared/gas-validation/` — errors round-trip over the control socket
unchanged so the admin UI and `silencer-cli` can consume them.

## Adding a field

1. Add the field to the relevant `*Def` struct in `gasloader.h` with a
   sensible default (matches the old hardcoded value).
2. Parse it in `gasloader.cpp` in the matching `Load*` function.
3. Mirror the field in `shared/gas-validation/schemas.ts` — same name,
   matching type, description from the C++ comment.
4. Add it to the seed JSON in `shared/assets/gas/`.
5. Wire it into the engine callsite.

Never change balance values in step 4 — seed JSON must match old
hardcoded defaults exactly until a designer deliberately changes them.

## Build / run

```bash
# From clients/silencer/
bash build.sh        # macOS/Linux
build.ps1            # Windows
```

Do not invoke cmake directly. Use the wrapper scripts.
