# src/actors

All in-game entities: the base object model, player types, NPC types, and the behavior tree AI system.

## Subdirectories

| Dir | Contents |
|---|---|
| `core/` | Base classes and type registry. |
| `player/` | Human-controlled and bot player objects. |
| `npc/` | All NPC types (guard, civilian, magistrate, robot, vanta). |
| `bt/` | Behavior tree runtime + debug overlay. |

---

## `core/`

### `object.h` — `Object`
The universal base class. Inherits `Sprite`, `Physical`, `Hittable`, `Bipedal`, `Projectile`.

```cpp
class Object : public Sprite, public Physical, public Hittable, public Bipedal, public Projectile {
    virtual void Tick(World&);
    virtual void Serialize(bool write, Serializer&, Serializer* old = 0);
    virtual void OnDestroy(World&);
    virtual void HandleHit(World&, Uint8 x, Uint8 y, Object& projectile);
    virtual void HandleInput(Input&);
    virtual void HandleDisconnect(World&, Uint8 peerid);
    bool RequiresAuthority();
    int EmitSound(World&, Mix_Chunk*, Uint8 volume = 128, bool loop = false);
    int EmitGlobalSound(World&, Mix_Chunk*, Uint8 volume = 128);
};
```

Fields worth noting:
- `collectible` / `collected` — pickup items; emit `ITEM_COLLECTED` on player overlap.
- `is_moving` + move fields — scripted linear movement driven by `MOVE_ACTOR` trigger actions.
- `requiresauthority` — if true, only the authority peer runs `Tick`.

### `actordef.h` — GAS-driven animation
```
FrameHurtbox  → axis-aligned hurtbox offset per frame
FrameDef      → bank, index, duration (ticks), optional sound
AnimSequence  → ordered frames[], loop flag; Resolve(state_i), GetFrameSound(state_i)
ActorDef      → id + map<state_name, AnimSequence>; loaded from shared/assets/gas/*.json
```

### `objecttypes.h`
Integer type constants (e.g. `OBJECT_TYPE_GUARD`, `OBJECT_TYPE_PLAYER`). Used in `Object::type` and `ObjectFactory`.

### `bipedal.h`
Locomotion mixin: walking, ladder, jump state shared by `Object` subclasses.

---

## `player/`

| File | Class | Description |
|---|---|---|
| `player.h/cpp` | `Player` | Human-controlled `Object`. Reads `Input` from peer input queue. |
| `playerai.h/cpp` | `PlayerAI` | Bot player. Drives its own `Input` each tick. |

---

## `npc/`

All NPCs inherit `Object` and carry a `const BehaviorTree* bt_` + `BTContext btctx_`.

| File | Class | Notes |
|---|---|---|
| `guard.h/cpp` | `Guard` | Armed patrol guard; BT-driven; respawns. |
| `civilian.h/cpp` | `Civilian` | Unarmed; can be captured (`AddTract`). |
| `magistrate.h/cpp` | `Magistrate` | Boss NPC; dormant until `activationTicks` or `secretTriggerN`; spawns actors on death (`deathSpawnEntries`). |
| `robot.h/cpp` | `Robot` | Melee + ranged robot; can be virus-implanted (`ImplantVirus`). |
| `vanta.h/cpp` | `Vanta` | Boss NPC; same activation/death-spawn structure as Magistrate. |
| `bodypart.h/cpp` | `BodyPart` | Detachable body part spawned on actor death. |
| `npc_math.h` | (free functions) | Shared NPC math helpers (line-of-sight, pathfinding utilities). |

Boss NPC fields (`Magistrate`, `Vanta`) are map-designer-editable via the level editor:
- `activationTicks` — world ticks after game start (default 7200 = 2 min @ 60 fps).
- `secretTriggerN` — also activate when N secrets beamed (0 = disabled).
- `deathSpawnEntries` — up to 4 packed bytes: high nibble = actor type, low nibble = count.
- `deathSpawnRadius` — spread radius in pixels.

---

## `bt/` — Behavior Tree

```cpp
class BehaviorTree;   // immutable, shared across all instances of an NPC type
struct BTContext;     // per-NPC mutable state (node cursor, blackboard)
```

- Trees are parsed from GAS data at startup and cached.
- Each NPC calls `bt_->Tick(btctx_, world, *this)` inside its own `Tick`.
- `btdebug.h/cpp` — overlay rendering for live BT state (dev builds only).

---

## Rules

- All new entity types must extend `Object` and call `RequiresAuthority()` correctly.
- NPC `Tick` must guard authority-only logic with `RequiresAuthority()` — state changes that affect other peers must only run on the authority.
- `ActorDef` animation sequences are data-driven from GAS; hardcode only states too complex for the frame-table model.
- Never include NPC headers from `core/` headers — dependency flows core → npc, not the other way.
