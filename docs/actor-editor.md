# Actor Editor

The Actor Editor lets you define game actors — their sprite animations,
hurtboxes, state machine, and base stats.  It lives at `/actors` in the
admin web app and stores each actor as a JSON file on the server.

Actors referenced here are the C++ sprite-based entities (players, NPCs,
enemies, objects).  Their definitions live in `shared/assets/actordefs/`
and are edited per-actor through this tool.

---

## Actor list (`/actors`)

The list page shows every actor ID on the server in alphabetical order.

### Create an actor

Type a name in the input field (lowercase letters, numbers, and hyphens
only) and press **Create**.  A blank actor def is created on the server and
the editor opens immediately.

### Delete an actor

Click **Delete** next to any actor in the list.  A confirmation prompt
appears before the delete is sent.

### Sprite browser

The **Sprites** link in the header opens the sprite browser (`/sprites`),
where you can browse all sprite banks and frame indices — useful for
finding bank/frame numbers before editing animations.

---

## Editor tabs

Each actor has four tabs: **Animation**, **Hitbox**, **Props**, and
**State Machine**.  The URL reflects the active tab
(`?tab=animation|hitbox|props`).

Changes in any tab mark the actor as unsaved.  Click **Save** in the
top bar to write the changes to the server, or **Download** to export the
JSON locally.  The JSON path is `shared/assets/actordefs/{id}.json`.

---

## Animation tab

Defines the actor's sprite sequences — the named animation clips that the
game engine plays in different states.

### Layout

```
┌──────────────────┬────────────────────────────┬────────────────────┐
│  Sequence list   │    Frame list + timeline    │  Sprite preview    │
│  (left sidebar)  │    (centre)                 │  (right)           │
└──────────────────┴────────────────────────────┴────────────────────┘
```

### Sequences

A sequence is a named animation clip.  Each actor has any number of
sequences stored in `def.sequences`.

| Control | Description |
|---|---|
| **+ Add sequence** | Create a new named sequence |
| **Delete** | Remove the selected sequence |
| Loop checkbox | When checked, the sequence loops back to frame 0 on completion |

Click a sequence name in the sidebar to select it and view its frames.

### Frames

Each sequence contains an ordered list of frames.  A frame represents one
sprite image held for a set number of game ticks.

| Field | Type | Description |
|---|---|---|
| **Bank** | int | Sprite bank number (matches `SPR_*.BIN`) |
| **Index** | int | Frame index within the bank |
| **Duration** | int (ticks) | How many game ticks (at 60 fps) this frame is displayed |
| **Sound** | string | Optional sound filename or `cue:name` to play when this frame is reached |
| **Sound volume** | int (0–128) | Volume for the frame sound |

Frame controls:

| Control | Description |
|---|---|
| **+ Add frame** | Append a new frame to the sequence |
| **Delete** | Remove the selected frame |
| **↑ / ↓** | Reorder frames |
| **▶** (sound) | Preview the frame's sound in the browser |

### Timeline

Below the frame list a horizontal bar visualises each frame's relative
duration.  Wider bars = longer duration.

### Preview

The right panel renders the selected sequence as a live animation at game
speed.

| Control | Description |
|---|---|
| **1× / 2× / 3× / 4×** | Preview scale |
| Speed slider | Playback speed multiplier (0.1 – 2×) |
| Reset speed | Return to 1× speed |

---

## Hitbox tab

Defines the hurtbox for each animation frame — the rectangle the game uses
for damage detection.

### Coordinate system

Coordinates are relative to the actor's anchor point:

- `x = 0` is the horizontal centre of the actor.
- `y = 0` is the actor's foot (bottom of the sprite).
- Negative `y` values extend upward.

### Editing a hurtbox

1. Select a sequence from the dropdown.
2. Select a frame by index.
3. The sprite for that frame is rendered on the canvas.
4. **Click and drag** on the canvas to draw a hurtbox rectangle.
5. Release to save. The numeric fields update automatically.

You can also edit the four values directly in the numeric inputs:

| Field | Description |
|---|---|
| `x1` | Left edge |
| `y1` | Top edge |
| `x2` | Right edge |
| `y2` | Bottom edge |

### Auto-fit

- **Auto-fit this frame** — detects the opaque pixel bounds of the current
  sprite frame and sets the hurtbox to match.
- **Auto-fit all frames** — runs auto-fit on every frame in the selected
  sequence in one pass.

### Data model

Hurtboxes are stored per-frame inside the sequence:

```json
"sequences": {
  "idle": {
    "loop": true,
    "frames": [
      {
        "bank": 3, "index": 0, "duration": 8,
        "hurtbox": [-8, -32, 8, 0]
      }
    ]
  }
}
```

---

## Props tab

Defines the actor's base numeric stats stored in `def.props`.

| Field | Type | Description |
|---|---|---|
| **HP** | int (≥ 1) | Starting health points |
| **Shield** | int (≥ 0) | Starting shield points |
| **Speed** | number (≥ 0) | Base movement speed |
| **Spawn weight** | int (0–100) | Relative probability of this actor being chosen when the game spawns a random actor of this type |
| **Faction** | string | Faction identifier — determines ally/enemy relationships |

A summary at the bottom of the tab shows the actor ID, total sequence
count, and total frame count.

---

## State Machine tab

Defines the finite-state machine that drives the actor's animation
playback.  The state machine maps sequences to states and defines the
conditions under which the actor transitions between them.

### Canvas

The tab renders a ReactFlow node graph.  Each node represents one
sequence/state; edges represent transitions.

| Interaction | Result |
|---|---|
| Drag node | Reposition state |
| Drag handle → handle | Create a transition between two states |
| Click edge | Select transition; edit condition or delete |
| Click **Auto Layout** | Arrange nodes automatically (dagre) |
| Minimap | Navigate large state machines |

### States

Each state node corresponds to a sequence name in `def.sequences`.  The
node shows:

- Sequence label
- Loop status
- **Initial** marker if this is the starting state
- Sprite preview of frame 0
- Frame count

To set the initial state, right-click a node or use the node inspector.

### Transitions

An edge from state A → state B fires when the **condition** becomes true
while the actor is in state A.

| Field | Description |
|---|---|
| Source | The state the actor is currently in |
| Target | The state to transition into |
| Condition | The trigger condition (see list below) |

#### Available conditions

| Condition | Description |
|---|---|
| `sequence_complete` | The current sequence has finished playing |
| `player_in_range` | A player is within detection range |
| `player_visible` | A player is in line of sight |
| `player_lost` | The tracked player has left detection range |
| `hp_low` | Actor health is below threshold |
| `hp_zero` | Actor health has reached zero |
| `velocity_nonzero` | Actor is moving |
| `velocity_zero` | Actor is stationary |
| `grounded` | Actor is standing on the ground |
| `alerted` | Actor has been alerted by sound or sight |
| `attack_cooldown` | Attack cooldown has expired |
| `spawn` | Actor has just spawned (fires once on entry) |

### Data model

```json
"stateMachine": {
  "initial": "idle",
  "transitions": [
    { "id": "t1", "from": "idle",   "to": "run",    "condition": "velocity_nonzero" },
    { "id": "t2", "from": "run",    "to": "idle",   "condition": "velocity_zero" },
    { "id": "t3", "from": "idle",   "to": "attack", "condition": "player_in_range" },
    { "id": "t4", "from": "attack", "to": "idle",   "condition": "sequence_complete" }
  ],
  "positions": {
    "idle":   { "x": 100, "y": 200 },
    "run":    { "x": 400, "y": 200 },
    "attack": { "x": 250, "y": 400 }
  }
}
```

---

## Actor def JSON schema

The full actor definition stored on disk:

```jsonc
{
  "props": {
    "hp": 100,
    "shield": 0,
    "speed": 2.5,
    "spawnWeight": 10,
    "faction": "guard"
  },
  "sequences": {
    "idle": {
      "loop": true,
      "frames": [
        {
          "bank": 3,
          "index": 0,
          "duration": 8,
          "hurtbox": [-8, -32, 8, 0],
          "sound": "cue:guard_alert",
          "soundVolume": 100
        }
      ]
    }
  },
  "stateMachine": {
    "initial": "idle",
    "transitions": [
      { "id": "t1", "from": "idle", "to": "walk", "condition": "velocity_nonzero" }
    ],
    "positions": {}
  }
}
```

---

## API endpoints

| Method | Path | Description |
|---|---|---|
| `GET` | `/actors` | List all actor IDs |
| `GET` | `/actors/:id` | Fetch one actor def |
| `PUT` | `/actors/:id` | Save actor def |
| `DELETE` | `/actors/:id` | Delete actor def |
| `GET` | `/api/sprites/:bank/:frame` | Fetch a sprite frame image |
| `GET` | `/sounds` | List available sound files (for frame sound picker) |
| `GET` | `/sound-cues` | List available sound cues |
| `GET` | `/sound-cues/:id` | Fetch a specific sound cue |

---

## Related GAS files

Actor defs define sprite and state data.  Gameplay stats for built-in
actor types are in the following GAS files (edited via the GAS editor, not
the Actor Editor):

| File | Contents |
|---|---|
| `shared/assets/gas/enemies.json` | Enemy HP, speed, weapon, chase ranges, sound slots, AI timings |
| `shared/assets/gas/player.json` | Player base health/shield/fuel, movement timings, upgrade multipliers, sound slots |
| `shared/assets/gas/gameobjects.json` | Placeable objects — cooldowns, detection ranges, sound slots |
