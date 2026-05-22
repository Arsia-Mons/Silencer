# Sound Cue System

Sound cues replace raw `.wav` file lookups with a small node graph that
decides *which* file to play, at *what* volume and pitch, and with *what*
delay — at runtime, every time the sound is triggered.  The graph is
authored in Sound Studio (admin web → Sound Studio → Cues tab) and stored
as JSON in `shared/assets/gas/sound-cues/`.

---

## Concepts

### Cue

A cue is a named, acyclic node graph.  Its id is a snake_case slug
(`footstep_concrete`, `blaster_fire`, etc.).  Every cue has exactly one
**Output** node.  When the game triggers a cue it evaluates the graph from
Output → leaves and gets back a single `{chunk, volume, pitch, delaySec}`
result.

### Slot reference

Any GAS sound field can hold either a raw filename or a cue reference:

| Value | Meaning |
|---|---|
| `"futstonl.wav"` | plays this WAV directly from the soundbank |
| `"cue:footstep_concrete"` | evaluates the named cue graph |

`ResolveSound(slot, res)` handles both forms transparently.

---

## Node types

### Output
Every graph ends here.  Connect any node's output to `in`.  The result
propagates straight through — Output has no parameters of its own.

### WavePlayer
Leaf node.  Picks a single WAV file.

| Field | Default | Description |
|---|---|---|
| `file` | — | Filename relative to the sounds dir (e.g. `"rico1.wav"`) |
| `weight` | `1` | Relative probability when connected to a Random node |

### Random
Picks one of its inputs each time it is evaluated.  Inputs are weighted by
the `weight` field on each upstream WavePlayer (or by the number of
connections if weights are equal).  Consecutive triggers will not repeat the
same input twice in a row (last-pick exclusion).

Connect any number of WavePlayers (or other nodes) to its input ports.

### Sequence
Cycles through its inputs in order, advancing by one index each trigger.
Set `shuffle: true` to randomise the order within each full pass.

### Modulator
Applies a random volume scalar and pitch offset drawn from uniform ranges
each trigger.  Place it between any node and Output.

| Field | Unit | Description |
|---|---|---|
| `volumeMin` / `volumeMax` | scalar (1 = 100 %) | Volume range |
| `pitchMin` / `pitchMax` | semitones | Pitch shift range (±12 = ±1 octave) |

Most cues use `volumeMin=volumeMax=1` (no volume randomisation) and a small
pitch range like `−3` to `+3` semitones to add organic variation.

### Delay
Introduces a random pause before playback.

| Field | Unit | Description |
|---|---|---|
| `minSec` / `maxSec` | seconds | Range of the delay; set equal for a fixed delay |

### Concatenate *(Concatenator node)*
Plays its inputs in sequence within a single trigger — i.e. the sounds are
chained back-to-back.  Useful for "intro + loop" style cues.

### Volume
Scales the volume by a fixed `scalar`.  Use when you want a permanent
level adjustment that isn't randomised.

### Pitch
Shifts pitch by a fixed number of semitones.  Non-randomised alternative to
Modulator when you want a constant detune.

---

## Authoring in Sound Studio

1. Open **Sound Studio → Cues** tab.
2. Click **+ New Cue** and give it a slug id.
3. Drag nodes from the left panel onto the canvas.
4. Wire nodes by dragging from an output handle to an input handle.
5. Click a node to edit its parameters in the right panel.
6. Press **▶ Play** to audition the cue (evaluates the graph once and plays
   the result through your browser).
7. **Save** — the JSON is written to the admin API and synced to
   `shared/assets/gas/sound-cues/<id>.json` on disk.
8. **↓ ALL** — downloads a zip of every cue JSON in the library.

> **Handle naming quirk:** Random/Modulator input handles are labelled
> `i0`, `i1`, … in the underlying JSON; Sequence nodes use `in-0`,
> `in-1`, …  The editor hides this detail — just draw wires normally.

---

## Wiring sounds to cues in GAS

### Player / enemy GAS fields

Change the field value from a filename to a `cue:` reference:

```json
// before
"soundReload": "reload1.wav"

// after
"soundReload": "cue:player_reload"
```

### Weapon slots (`weapons.json`)

Same pattern:

```json
"soundFire": "cue:blaster_fire",
"soundHit1": "cue:blaster_hit"
```

### Code side — `ResolveSound`

C++ callers use `ResolveSound(slot, res)` which returns a
`SoundCueResult`:

```cpp
SoundCueResult r = ResolveSound(soundSlot, world.resources);
if(r.chunk) EmitSound(world, r.chunk, static_cast<int>(vol * r.volume));
```

The cue result's `pitch` and `delaySec` fields are currently informational
(pitch applied by SDL3_mixer, delay not yet wired to a timer).

---

## JSON schema

```jsonc
{
  "id": "my_cue",           // unique slug — matches filename
  "nodes": [
    {
      "id": "out",          // must exist; the Output node
      "type": "Output",
      "position": { "x": 870, "y": 90 },  // canvas position (editor only)
      "data": {}
    },
    {
      "id": "rng",
      "type": "Random",
      "position": { "x": 310, "y": 90 },
      "data": {}
    },
    {
      "id": "w1",
      "type": "WavePlayer",
      "position": { "x": 30, "y": 30 },
      "data": { "file": "rico1.wav", "weight": 1 }
    },
    {
      "id": "mod",
      "type": "Modulator",
      "position": { "x": 590, "y": 90 },
      "data": { "volumeMin": 1, "volumeMax": 1, "pitchMin": -3, "pitchMax": 3 }
    }
  ],
  "edges": [
    { "id": "e1", "source": "w1",  "sourceHandle": "out", "target": "rng", "targetHandle": "in-0" },
    { "id": "e2", "source": "rng", "sourceHandle": "out", "target": "mod", "targetHandle": "in" },
    { "id": "e3", "source": "mod", "sourceHandle": "out", "target": "out", "targetHandle": "in" }
  ]
}
```

---

## Runtime — `SoundCueLibrary`

`SoundCueLibrary::Get()` is a singleton loaded once during
`Resources::Load()` after all WAV files are in memory.  It reads every
`*.json` in `shared/assets/gas/sound-cues/`, parses each graph, and
resolves WavePlayer filenames to `Mix_Chunk*` immediately so there is no
per-trigger I/O.

Sequence round-robin counters and Random last-pick state are stored
per-cue in the library, so each cue maintains its own independent position
across triggers.

---

## Audio stacking prevention

UI sounds (hover, click) go through `Audio::PlayUI()` rather than the
regular `Audio::Play()`:

- A **dedicated channel (127)** is reserved exclusively for UI sounds.
- A **30 ms cooldown per chunk** drops rapid-fire duplicate triggers
  (e.g. hovering quickly over a list) without hard-stopping the current
  sound (which would cause a click/pop).

Game sounds that can stack (reload, pickup, etc.) use the `maxInstances`
parameter on `Audio::Play()`.  When the cap is reached the oldest playing
instance of that chunk is interrupted instead of spawning another copy.

---

## Existing cues

| Cue id | Sounds |
|---|---|
| `blaster_fire` | Blaster weapon fire |
| `blaster_hit` | Blaster projectile impact |
| `laser_fire` | Laser weapon fire |
| `laser_hit` | Laser projectile impact |
| `rocket_fire` | Rocket weapon fire |
| `explosion` | Explosion / rocket impact |
| `grenade_throw` | Grenade thrown |
| `grenade_land_impact` | Grenade landing |
| `plasma_explosion` | Plasma weapon explosion |
| `impact_blaster` | Hittable object struck by blaster |
| `impact_laser` | Hittable object struck by laser |
| `impact_laser_shield` | Shield struck by laser |
| `footstep_concrete` | Footstep on concrete surface |
| `footstep_metal` | Footstep on metal surface |
| `footstep_stair` | Footstep on stairs |
| `guard_alert` | Guard alert / challenge voice line |
| `guard_hurt` | Guard hurt |
| `player_hurt` | Player hurt |
| `player_type` | Player typing on keyboard |
| `player_ladder` | Player climbing ladder |
| `player_grunt` | Player exertion grunt |
| `player_land` | Player landing |
| `player_land_crouch` | Player landing while crouched |
| `player_fall` | Player falling |
| `player_roll` | Player roll action |
| `player_pickup` | Player picking up item |
| `player_repair` | Player repair action |
| `player_powerup` | Player power-up collect |
| `player_reload` | Player reload |
| `player_disguise` | Player disguise action |
| `player_jackin` | Player jack-in |
| `player_jackout` | Player jack-out |
| `player_undeploy` | Player undeploy |
| `player_jetpack` | Player jetpack |
| `player_weapon_charged` | Player weapon fully charged |
| `player_security_pass` | Player security pass use |
| `player_breath` | Player breath |
| `robot_melee` | Robot melee attack |
| `robot_move` | Robot movement |
| `robot_death` | Robot death |
| `civilian_hurt` | Civilian hurt |
| `civilian_death` | Civilian death |
| `object_destroy` | Destructible object destroyed |
| `door_open` | Door opening |
| `station_purchase` | Station purchase |
| `station_heal` | Station heal |
