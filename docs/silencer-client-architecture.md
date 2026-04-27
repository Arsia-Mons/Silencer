# Silencer Client — Source Architecture

Reference for `clients/silencer/src/` (currently a flat ~180-file
directory). Captures the actual class hierarchy and functional
groupings as an on-ramp for locating code. Class edges and counts
are validated against the libclang AST of `src/*.h`.

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
subclass of `Object` — **41 leaf classes, no deeper subclassing
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

## `Object` subclass contract

Virtual methods on `Object` (`object.h:14`) that subclasses
override — this is what a new entity has to implement:

| Method | When called |
|--------|-------------|
| `Tick(World&)` | once per simulation frame |
| `Serialize(write, data, old)` | network replication (delta vs `old`) |
| `OnDestroy(World&)` | when the object is removed |
| `HandleHit(World&, x, y, projectile)` | when hit by a projectile |
| `HandleInput(Input&)` | controllable objects only |
| `HandleDisconnect(World&, peerid)` | when a peer drops |

Per-instance `is*` flags (`issprite`, `isphysical`, `ishittable`,
`isbipedal`, `isprojectile`, `iscontrollable`) gate which mixin
behaviors actually run, since every `Object` carries all five
mixins by inheritance regardless of whether the subclass uses
them.

## Subsystems (non-`Object` classes)

These are the engine pieces that run the entities above. All are
standalone classes except `SDL3GPUBackend : RenderDevice` (the
only non-`Object` inheritance edge in the codebase).

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

