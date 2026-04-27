# Silencer Client — Source Architecture

Reference for `clients/silencer/src/` (currently a flat ~180-file
directory). Captures the actual class hierarchy and functional
groupings; intended as the starting point for a folder
reorganization and as the on-ramp for new contributors trying to
locate code.

## Class hierarchy

The shape is **wide and two levels deep**, not a chain.

`Object` (`object.h:14`) uses **mixin multiple inheritance** of
five bases:

```cpp
class Object : public Sprite, public Physical, public Hittable,
               public Bipedal, public Projectile
```

Each base contributes one slice of state/behavior:

| Mixin | Role |
|-------|------|
| `Sprite` | bank/index/frame, position, rendering data |
| `Physical` | velocity, mass, collision response |
| `Hittable` | health, damage, hit reactions |
| `Bipedal` | walking, jumping, platform attachment |
| `Projectile` | projectile motion / lifetime fields |

Every gameplay or UI thing in the world is then a single-level
subclass of `Object` — **42 leaf classes, no deeper subclassing
anywhere**. Grouped by role:

### Actors (4)
`Player`, `Civilian`, `Guard`, `Robot`

### Projectiles & ordnance (11)
`BlasterProjectile`, `LaserProjectile`, `RocketProjectile`,
`FlamerProjectile`, `PlasmaProjectile`, `FlareProjectile`,
`WallProjectile`, `Grenade`, `Shrapnel`, `BodyPart`, `Detonator`

### Stations / interactables (14)
`HealMachine`, `CreditMachine`, `InventoryStation`, `TechStation`,
`Terminal`, `WallDefense`, `FixedCannon`, `Vent`, `BaseDoor`,
`BaseExit`, `Warper`, `SecretReturn`, `SurveillanceMonitor`,
`TeamBillboard`

### UI widgets (8)
`Interface`, `Button`, `TextBox`, `TextInput`, `SelectBox`,
`ScrollBar`, `Toggle`, `Overlay`

UI widgets being `Object` subclasses is a quirk worth knowing —
it means UI shares serialization, sprite, and physical state with
gameplay entities even though conceptually they're separate.

### Misc gameplay state (4)
`PickUp`, `Plume`, `State`, `Team`

## Subsystems (non-`Object` classes)

These are the engine pieces that run the entities above. Each is
a standalone class — no inheritance into the `Object` tree.

| Group | Files |
|-------|-------|
| Sim core | `world.{h,cpp}`, `map.{h,cpp}`, `platform.{h,cpp}`, `platformset.{h,cpp}`, `camera.{h,cpp}`, `replay.{h,cpp}`, `objecttypes.{h,cpp}`, `serializer.{h,cpp}` |
| Networking | `peer.{h,cpp}`, `lobby.{h,cpp}`, `lobbygame.{h,cpp}`, `controlserver.{h,cpp}`, `controldispatch.{h,cpp}`, `lagsimulator.{h,cpp}`, `dedicatedserver.{h,cpp}` |
| Rendering | `renderer.{h,cpp}`, `renderdevice.h`, `sdl3gpubackend.{h,cpp}`, `surface.{h,cpp}`, `palette.{h,cpp}`, `minimap.{h,cpp}` |
| Audio / input / config | `audio.{h,cpp}`, `input.{h,cpp}`, `config.{h,cpp}`, `resources.{h,cpp}` |
| AI / data-driven NPCs | `behaviortree.{h,cpp}`, `playerai.{h,cpp}`, `actordef.{h,cpp}` |
| User / shop / stats | `user.{h,cpp}`, `stats.{h,cpp}`, `buyableitem.{h,cpp}` |
| Updater | `updater.{h,cpp}`, `updaterdownload.{h,cpp}`, `updaterzip.{h,cpp}`, `updaterstage2.{h,cpp}`, `updatersha256.{h,cpp}` |
| Platform shims | `os.{h,cpp}`, `cocoawrapper.{h,mm}`, `SDLMain.{h,m}` |
| Utility | `sha1.{h,cpp}`, `mapfetch.{h,cpp}`, `shared.h` |
| Top-level | `game.{h,cpp}`, `main.cpp` |
| Third-party (vendored) | `zlib/` |

## Implications for restructuring

Observations that should inform any folder reorganization:

1. **Entities vs subsystems is the cleanest split.** Every
   `Object` subclass lives in the world (or pretends to);
   everything else runs them.
2. **The five mixins belong with `Object`.** `Sprite`,
   `Physical`, `Hittable`, `Bipedal`, and `Projectile` exist only
   as parts of `Object` — they should live next to it, not
   scattered across rendering / physics / etc.
3. **The four `Object`-subclass clusters** (actors, projectiles,
   stations, ui) are the obvious folder candidates inside an
   `entities/` (or similar) parent.
4. **UI widgets crossing the gameplay/UI boundary via `Object`
   inheritance** is real coupling — folder layout will not hide
   it. Worth deciding whether that's a refactor target or a fixed
   constraint.
5. **Updater is self-contained** (5 files, no cross-deps into
   gameplay) — easy folder candidate.
6. **Platform shims** (`os`, `cocoawrapper`, `SDLMain`) are a
   small, clear group.

## How this was generated

Class edges grepped from `clients/silencer/src/*.h`:

```bash
grep -EHn 'class [A-Za-z_]+ ?: ?(public|private|protected) ?[A-Za-z_]+' \
  clients/silencer/src/*.h
```

Standalone classes (no inheritance) found by inverting that grep
on `^class [A-Za-z_]+`. Counts and groupings reflect the
repository state on 2026-04-27.
