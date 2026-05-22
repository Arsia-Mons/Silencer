# Behavior Trees

Silencer uses a behavior tree system to drive NPC AI.  Trees are authored in
the admin web app, stored as JSON in `shared/assets/behaviortrees/`, and
evaluated at runtime by the C++ `BehaviorTree` engine each game tick.

---

## Concepts

### What a behavior tree does

A behavior tree is a hierarchical decision graph.  On every tick the engine
walks from the root downwards, evaluating nodes until one returns a result.
Each node returns one of three values:

| Result | Meaning |
|---|---|
| **Success** | The node completed its goal |
| **Failure** | The node could not complete its goal |
| **Running** | The node is still working (multi-tick action) |

Results propagate upward through composites which use them to decide which
branch to visit next.

### Blackboard

The blackboard is a key-value store shared across the whole tree evaluation
for one actor.  Leaves write values (positions, flags, counters) and
Conditions read them to make decisions.  Keys are declared in the tree's
blackboard schema — each key has a name and type (`bool`, `int`, `float`,
`string`).

**Live-written keys** — the game engine writes these automatically every tick:

| Key | Type | Description |
|---|---|---|
| `health_pct` | float | Actor's current health as a 0–1 fraction |
| `dist_to_target` | float | Distance in pixels to the current target |
| `has_target` | bool | Whether the actor has a current target |
| `on_ladder` | bool | Actor is on a ladder |
| `at_ladder` | bool | Actor is adjacent to a ladder |
| `on_ground` | bool | Actor is standing on the ground |
| `state_name` | string | Current state machine state name |

---

## Node types

### Composites

Composites have one or more children and use their results to decide what to
return.

#### Selector
Tries each child in order.  Returns the result of the **first child that
succeeds or is running**.  Returns `Failure` only if every child fails.

> Use when you have multiple strategies and want to try the best one first,
> falling back to cheaper alternatives.

#### Sequence *(memory)*
Tries each child in order.  Returns `Failure` on the **first child that
fails**.  Returns `Success` only when every child succeeds.  If a child
returns `Running`, the sequence remembers its position and resumes from there
on the next tick.

> Use to chain steps that must all succeed — e.g. "find target, move to
> target, attack target".

#### Parallel

| Prop | Type | Default | Description |
|---|---|---|---|
| `threshold` | int | all children | Number of children that must succeed for the Parallel to succeed |

Ticks **all** children every tick regardless of their results.  Succeeds when
`successes >= threshold`.  Otherwise returns `Running`.

> Use when you need multiple things to happen simultaneously — e.g. patrol
> while scanning for threats.

#### RandomSelector

| Prop | Type | Default | Description |
|---|---|---|---|
| `weights` | number[] | uniform | Per-child probability weights (one per child; leave blank for equal chance) |

Picks one child at random (weighted if weights are set) and returns its
result.  If it fails, picks another until one succeeds or all have been tried.

> Use to add variety — random patrol destinations, randomised attack
> choices, etc.

---

### Decorators

Decorators wrap a single child and modify its result or behaviour.

#### Inverter
Returns `Success` when the child returns `Failure` and vice-versa.
`Running` is passed through unchanged.

#### Cooldown

| Prop | Type | Description |
|---|---|---|
| `duration` | float (seconds) | After the child succeeds, block it for this long |

While the cooldown is active, returns `Failure` without ticking the child.
After the timer expires the child is allowed to run again.

> Use to rate-limit actions — e.g. "attack, but no more than once every 2 s".

#### Repeat

| Prop | Type | Default | Description |
|---|---|---|---|
| `count` | int | — | How many times to run the child; `0` = infinite |

Keeps ticking the child until it has succeeded `count` times (or forever when
`count = 0`).  Returns `Running` while repeating, `Success` when done.

#### Timeout

| Prop | Type | Description |
|---|---|---|
| `duration` | float (seconds) | Maximum time to allow the child to run |

Ticks the child normally.  If the child has not succeeded within `duration`
seconds, returns `Failure` and stops the child.

#### ForceSuccess
Ticks the child and always returns `Success` regardless of the child's result.

> Use to make an optional step never block a sequence.

---

### Leaves

Leaf nodes perform concrete game actions or evaluate conditions.

#### Wait

| Prop | Type | Description |
|---|---|---|
| `duration` | float (seconds) | How long to wait |

Returns `Running` for `duration` seconds, then returns `Success`.

#### Condition

| Prop | Type | Description |
|---|---|---|
| `key` | string | Blackboard key to read |
| `op` | string | Comparison operator: `==` `!=` `>` `<` `>=` `<=` |
| `value` | any | Value to compare against (type matches the key's declared type) |

Returns `Success` when the comparison is true, `Failure` otherwise.  No
side-effects.

#### Leaf

Executes a named game action.  The `action` field selects which action to run;
additional props are action-specific.

**Generic actions** (available on all actors):

| Action | Props | Description |
|---|---|---|
| `SetBlackboard` | `key`, `value` | Write a value to the blackboard |
| `RandomChance` | `chance` (0–1 float) | Succeeds with the given probability |
| `PlayAnim` | `bank` (int), `frames` (int), `loop` (bool) | Play an animation |
| `EmitSound` | `sound` (string), `volume` (int), `global` (bool) | Play a sound or cue |
| `EmitSpawnSound` | `sound` (string) | Play a spawn sound |
| `EmitDeathSound` | `sound` (string) | Play a death sound |
| `SetFacing` | `dir` (`flip` \| `left` \| `right`) | Set actor facing |
| `SetSpeed` | `speed` (number) | Set movement speed |
| `ApplyVelocity` | `xv`, `yv` (numbers) | Apply an impulse |
| `CheckGround` | `key` (string, default `on_ground`) | Write whether actor is on ground to blackboard |
| `Raycast` | `range` (number), `result_key` (string, default `ray_hit`) | Cast a ray and write hit result to blackboard |

**NPC-specific actions** are available depending on the tree's actor family
(`guard`, `civilian`, `robot`, `magistrate`, `vanta`).  These are injected
by the C++ actor classes when they register their `BTContext.actions`.

#### SubTree

| Prop | Type | Description |
|---|---|---|
| `tree_id` | string | Filename stem of the tree to call (e.g. `guard_patrol`) |

Evaluates another tree in-place, sharing the same blackboard and context.
Recursion is guarded — a tree cannot call itself directly or indirectly.

> Use to decompose large trees into reusable modules.

---

## Editor

The Behavior Tree editor lives at `/behavior-trees` in the admin web app.

### Opening trees

The editor works against a local folder of `.json` files — it never writes
directly to the server.

1. Click **Open folder** on the list page and pick the
   `shared/assets/behaviortrees/` directory.
2. All `.json` files in the folder are loaded into memory.
3. Click any tree in the list to open it in the editor.

### List page actions

- **Open folder** — load a directory of tree JSON files.
- **+ New Tree** — prompts for a name (letters, numbers, hyphens only), creates
  a blank tree with a root Selector node, saves to the in-memory store, and
  downloads the JSON to disk.
- **DEL** — deletes a tree from the in-memory store (confirm required).
- **Close folder** — clears the store.

### Editor layout

```
┌──────────────────────────────────────────────────────────┐
│  Toolbar (undo/redo/validate/duplicate/delete/layout)    │
├────────────┬──────────────────────────────┬──────────────┤
│  Node      │                              │  Inspector / │
│  palette   │         Canvas               │  Properties  │
│            │                              │              │
│  Blackboard│                              │  Blackboard  │
│  keys      │                              │  panel       │
└────────────┴──────────────────────────────┴──────────────┘
```

### Canvas interactions

| Interaction | Result |
|---|---|
| Drag palette item onto canvas | Create node at drop position |
| Click palette item | Create node at a random position |
| Click node | Select node; show properties in inspector |
| Click empty canvas | Deselect |
| Drag node | Move node |
| Drag child horizontally inside a composite | Re-order children |
| Drag handle → handle | Connect parent → child |
| Click edge × | Delete connection |
| Scroll wheel | Zoom |
| Click+drag empty canvas | Pan |

### Toolbar actions

| Button | Description |
|---|---|
| **Auto Layout** | Automatically arrange nodes into a clean tree layout |
| **Undo** | Step back one edit (`Ctrl+Z`) |
| **Redo** | Step forward one edit (`Ctrl+Y` / `Ctrl+Shift+Z`) |
| **Validate** | Check the tree for errors (disconnected nodes, missing required props, etc.) |
| **Duplicate** | Clone the selected node (same type/label/props, no children, offset +40 px) |
| **Delete Node** | Remove the selected node |
| **+ Key** | Add a blackboard key |
| **Save / Download** | Download the tree JSON to disk |

### Node inspector

Click any node to open its properties in the inspector panel.

**All nodes:**
- **Label** — display name (cosmetic only).
- **Set as root** — mark this node as the tree root.
- **Children** — composites show their child list with ↑ ↓ reordering buttons.

**Type-specific props** are shown below the common fields — see the node
reference above for the full list per type.

The inspector also exposes a generic **extra props** editor for any
non-standard fields (key-value pairs).

### Blackboard panel

Lists all declared blackboard keys (name + type + default value).  Click
**+ Key** in the toolbar to add one.  Click a key row to edit or delete it.

The panel can be docked alongside the inspector or floated as a separate
overlay.

### Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+Shift+Z` | Redo |

---

## Live simulation

When a game instance is running locally with the debug websocket enabled, the
editor can connect to it for real-time tree inspection.

1. Connect — the editor automatically tries `ws://localhost:9339`.
2. A **connected** indicator appears in the toolbar when the link is live.
3. Select an actor from the actor dropdown.
4. The canvas colours each node by its last evaluated result:

| Colour | Result |
|---|---|
| Green | Success |
| Amber | Running |
| Red | Failure |

5. The live blackboard panel shows the current value of every key for the
   selected actor, updating each tick.

---

## JSON format

Trees are stored as plain JSON files.  The runtime ignores `positions` (editor
layout only).

```jsonc
{
  "version": 1,
  "id": "guard_patrol",           // matches filename stem
  "rootId": "sel_root",           // id of the root node
  "blackboard": [
    { "key": "target_x", "type": "float", "default": 0 },
    { "key": "has_target", "type": "bool", "default": false }
  ],
  "nodes": {
    "sel_root": {
      "type": "Selector",
      "label": "Root",
      "children": ["seq_attack", "leaf_patrol"],
      "props": {}
    },
    "seq_attack": {
      "type": "Sequence",
      "label": "Attack sequence",
      "children": ["cond_has_target", "leaf_move", "leaf_shoot"],
      "props": {}
    },
    "cond_has_target": {
      "type": "Condition",
      "label": "Has target?",
      "children": [],
      "props": { "key": "has_target", "op": "==", "value": true }
    },
    "leaf_shoot": {
      "type": "Leaf",
      "label": "Shoot",
      "children": [],
      "props": { "action": "SpawnProjectile", "direction": 0 }
    },
    "cd_shoot": {
      "type": "Cooldown",
      "label": "Shoot cooldown",
      "children": ["leaf_shoot"],
      "props": { "duration": 1.5 }
    }
  },
  "positions": {
    "sel_root":      { "x": 400, "y": 200 },
    "seq_attack":    { "x": 200, "y": 350 }
  }
}
```

### Node `props` keys by type

| Type | Key | Type | Notes |
|---|---|---|---|
| `Parallel` | `threshold` | int | Min successes needed; defaults to child count |
| `RandomSelector` | `weights` | number[] | One per child; omit for uniform |
| `Cooldown` | `duration` | float | Seconds |
| `Repeat` | `count` | int | 0 = infinite |
| `Timeout` | `duration` | float | Seconds |
| `Wait` | `duration` | float | Seconds |
| `Condition` | `key`, `op`, `value` | — | Blackboard comparison |
| `Leaf` | `action` | string | Action name + action-specific keys |
| `SubTree` | `tree_id` | string | Filename stem of sub-tree |

---

## C++ runtime

### Loading

```cpp
BehaviorTreeLibrary lib;
lib.Load("shared/assets/behaviortrees/");  // reads all *.json

// or fetch from admin API:
FetchBehaviorTrees(apiBaseUrl, lib);
```

### Ticking

```cpp
BTContext ctx;
ctx.dt = deltaSeconds;
ctx.blackboard["has_target"] = true;

// Register leaf action handlers:
ctx.actions["Patrol"] = [](BTContext& c) -> BTResult { ... };

BehaviorTree* tree = lib.Get("guard_patrol");
BTResult result = tree->tick(ctx);
// ctx.nodeResults now maps node id → 0/1/2 (Success/Failure/Running)
```

### `BTContext` fields

| Field | Type | Description |
|---|---|---|
| `dt` | float | Delta time in seconds for this tick |
| `blackboard` | `unordered_map<string, json>` | Shared key-value state |
| `state` | `unordered_map<string, json>` | Internal timer/counter storage (sequence index, cooldown timers, etc.) |
| `actions` | `unordered_map<string, BTLeafFn>` | Registered leaf action handlers |
| `props` | pointer | Current node's props (set by the engine before calling a leaf handler) |
| `userData` | void* | Actor pointer passed through to action handlers |
| `nodeResults` | `unordered_map<string, int>` | Per-node result after tick (for live editor overlay) |
| `logFn` | function | Optional log callback for debugging |

### Helper methods on `BTContext`

```cpp
json  ctx.bb(key, defaultValue);   // read blackboard with fallback
void  ctx.bbSet(key, value);       // write blackboard
```

---

## File locations

| Path | Contents |
|---|---|
| `shared/assets/behaviortrees/*.json` | Tree definition files |
| `clients/silencer/src/actors/bt/behaviortree.h` | C++ runtime header |
| `clients/silencer/src/actors/bt/behaviortree.cpp` | C++ runtime implementation |
| `web/admin/app/behavior-trees/` | Editor UI |
