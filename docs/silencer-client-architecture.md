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

## `Object` lifecycle (gotchas)

`World` owns every `Object` and four parallel indices: `objectlist`,
`tobjectlist` (collidables only), `objectsbytype[type]`, and
`objectidlookup[id]`. All four are kept in sync **only** by the
`World` lifecycle methods — never push/erase them directly.

**Creation** — `World::CreateObject(type, id=0)` (`world.cpp:2429`).
Goes through the `ObjectTypes` factory. Returns `nullptr` if:
- `objectlist.size() == maxobjects` (32000 cap)
- `world.replaying` is true (replay rewinds via snapshot load, not
  lifecycle calls)
- caller is `REPLICA` and the type's `RequiresAuthority()` is true

ID bit 15 distinguishes AUTHORITY-allocated IDs (low) from
REPLICA-allocated IDs (`| 0x8000`) — both peers can mint IDs for
locally-spawned objects without colliding.

**Destruction is two-phase.** `MarkDestroyObject(id)`
(`world.cpp:2475`) sets `wasdestroyed = true` and queues the id;
`DestroyMarkedObjects()` drains the queue and calls
`DestroyObject(id)` which calls `OnDestroy(world)`, removes from
all four indices, and `delete`s. The drain runs at the end of
`TickObjects` (AUTHORITY) and at the top of `Tick` (REPLICA).

> Never call `DestroyObject` directly from inside a `Tick`,
> `HandleHit`, or any iteration over `objectlist` — it mutates the
> lists you're walking. Always `MarkDestroyObject`. The
> `wasdestroyed` flag is checked in `TickObjects` so a
> just-marked object is skipped for the rest of the frame.

`OnDestroy` also runs from `DestroyAllObjects` during `~World` —
make sure your override is safe with a half-torn-down `World`.

## `World` lifecycle (gotchas)

| Phase | What happens |
|-------|--------------|
| `World(mode)` | opens UDP socket, loads buyable items. **No map yet.** Mode is `AUTHORITY` or `REPLICA`. |
| `Connect` / `Listen` | networking begins; map data flows in for replicas. |
| `Tick()` | **AUTHORITY**: `SendSnapshots` → `TickObjects` (calls every `Object::Tick`) → process input queue. **REPLICA**: `ProcessSnapshotQueue` → `DestroyMarkedObjects` only. **Replicas never call `Object::Tick` from `World::Tick`** — state arrives as snapshot deltas. (`ClientSidePredict` is the one exception, used between snapshots for the local player.) |
| `HandleDisconnect(peerid)` | calls `Object::HandleDisconnect` on every object the peer controlled, then removes the peer from teams. |
| `SwitchToLocalAuthorityMode` | promotes a `REPLICA` to `AUTHORITY` when the authority peer drops mid-game (`world.cpp:1183`). `objectlist` survives intact — only the peer relationship changes. Code that branches on `IsAuthority()` must be safe across this transition at any tick boundary. |
| `~World` | `Disconnect` → close socket → `DestroyAllObjects` (runs `OnDestroy` on each then `delete`s) → free peers + snapshots. |

Implications:
- **Gameplay logic in `Tick` only runs on `AUTHORITY`.** Visual /
  audio side-effects that should fire on every peer belong in
  `Serialize` (state-driven) or in render code, not `Tick`.
- **`IsAuthority()` is not stable for the lifetime of a `World`** —
  `SwitchToLocalAuthorityMode` can flip it mid-game.
- **Always null-check `CreateObject`'s return value.** The cap and
  the AUTHORITY check fail silently.

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

