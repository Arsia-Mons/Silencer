# Map Designer

The Map Designer is a browser-based level editor for Silencer maps.  It lives
at `/designer` in the admin web app and reads/writes `.SIL` map files.

---

## Getting started

### Load game assets

Before opening or creating a map you need the game asset files so the editor
can render tiles and sprites.  Click **Open assets folder** and select the
`shared/assets/` directory from the repo (or a game install).  The editor
looks for:

| File | Contents |
|---|---|
| `PALETTE.BIN` | 256-colour game palette |
| `BIN_TIL.DAT` | Tile bank index |
| `TIL_*.BIN` | Individual tile banks |
| `BIN_SPR.DAT` *(optional)* | Sprite bank index |
| `SPR_*.BIN` *(optional)* | Individual sprite banks (actor rendering) |

Tiles render immediately; actors render as coloured placeholders if the sprite
banks are absent.

### Open / create a map

- **Open .SIL** — load an existing map file from disk.
- **New map** — create a blank map; prompts for width × height in tiles.
- **Server maps** — browse maps already published to the server; click one to
  download and open it.

---

## Interface layout

```
┌─────────────────────────────────────────────────┐
│  Toolbar                                        │
├──────────────┬──────────────────────┬───────────┤
│              │                      │  Right    │
│  (minimap)   │      Canvas          │  panel    │
│              │                      │  (tabs)   │
├──────────────┴──────────────────────┴───────────┤
│  Status bar                                     │
└─────────────────────────────────────────────────┘
```

- **Toolbar** — active tool, visibility toggles, grid, save/publish buttons.
- **Canvas** — the editable map view; pan with Space+drag or middle-mouse,
  zoom with scroll wheel.
- **Right panel** — four tabs: Tiles, Actors, Links, Triggers.
- **Minimap** — bottom-right corner; shows BG layer 0 + actors + viewport
  rectangle.  Click anywhere on the minimap to re-center the canvas there.
- **Status bar** — current tile/world coordinates, zoom level, map size,
  copy/paste state, and object counts.

---

## Tools

Select a tool from the toolbar or press its keyboard shortcut.

| Tool | Key | Description |
|---|---|---|
| **Select** | `S` | Click or drag to select a rectangular region of tiles for copy/paste |
| **Tile BG** | `B` | Paint background tiles |
| **Tile FG** | `F` | Paint foreground tiles |
| **Erase Tile** | `E` | Erase tiles; choose BG or FG and which layer |
| **Flood Fill** | `I` | Flood-fill a contiguous region with the selected tile |
| **Platform Rect** | `P` | Draw a rectangular solid platform |
| **Stairs Up** | — | Draw a staircase-up platform |
| **Stairs Down** | — | Draw a staircase-down platform |
| **Ladder** | — | Draw a ladder platform |
| **Track** | — | Draw a track (robot/object rail) |
| **Outside Room** | — | Mark a region as outside |
| **Specific Room** | — | Mark a region as a specific room |
| **Erase Platform** | — | Click a platform to delete it |
| **Actor** | `A` | Place an actor at the clicked position |
| **Shadow Zone** | — | Draw a rectangular shadow-casting zone |
| **Trigger Zone** | — | Draw a rectangular trigger zone (referenced by triggers) |
| **Nav Link** | — | Click source actor then target actor to create an AI navigation link |

### Tile layers

Maps have 8 tile layers: **BG0–BG3** (background) and **FG0–FG3**
(foreground).  Press `1`–`4` to set the active layer while a tile tool is
selected.

Each tile cell stores:
- **Tile ID** — which tile from the active bank
- **Flip X** — horizontal mirror
- **LUM** (luminous) — tile emits light; toggle via right-click → *Luminous*

### Tile picker (Tiles tab)

Browse tile banks with **◀ / ▶** or type a bank number directly.  Click a
tile to select it.  A search box filters by `bank:tile` index or bank number.
The currently selected bank and tile index are shown below the picker.

---

## Platform types

Platforms define the collision geometry of the map.

| Type | Description |
|---|---|
| **Rectangle** | Solid wall or floor |
| **Stairs Up** | Climbable slope going up to the right |
| **Stairs Down** | Climbable slope going down to the right |
| **Ladder** | Vertical climb surface |
| **Track** | Rail for tracked enemies or objects |
| **Outside Room** | Marks an exterior area (affects audio ambience) |
| **Specific Room** | Marks a named interior region |

Platforms can be moved and resized after placement by clicking and dragging
their handles on the canvas.

---

## Actors

Place actors with the **Actor** tool (`A`).  Choose the actor type from the
actor picker in the toolbar before placing.

### Actor properties (right-click → edit)

Common fields available on every actor:

| Field | Description |
|---|---|
| Type | Actor type ID |
| Direction | Facing direction (left / right) |
| Match ID | Links actors together for triggers and spawn logic |
| Security ID | Security clearance level required to interact |
| Destructible | Can be destroyed in-game |
| Collectible | Can be picked up |
| Health | Starting health value |

### Special actor: Light (type 71)

| Field | Description |
|---|---|
| Size | Radius — small (80 px), medium (140 px), large (200 px) |
| Shape | Circle or Spot (directional cone) |
| Direction | Facing angle for spot lights |
| Animation | Static, Flicker, Pulse |
| Pulse speed | Animation rate |
| Dynamic shadows | If off, shadows are baked into the light mask at save time |
| Color tint | RGB tint applied to the light |

### Special actors: Boss NPCs (types 72 / 73)

| Field | Description |
|---|---|
| Facing | Initial direction |
| Radius | Detection / patrol radius |
| Activation delay | Seconds before the boss activates |
| Death spawns | List of actors to spawn on death (type + match ID) |
| Secrets to activate | Number of secrets the player must find first |
| Security spawn condition | Security level that triggers a spawn |

### Actor list panel (Actors tab)

Lists every actor on the map with a search/filter box.  Click an actor in
the list to centre the canvas on it.  Right-click to open the actor
properties menu.

---

## Navigation links (Nav Links tab)

Nav links tell the AI pathfinder how actors can move between points that are
not connected by normal collision (jumps, falls, jetpack traversals).

**To create a link:** select the **Nav Link** tool, choose the link type
(Jump / Fall / Jetpack), click the source actor, then click the target actor.

**To delete a link:** select it in the Nav Links panel and press `Delete`.

### Link types

| Type | Description |
|---|---|
| Jump | AI can jump from source to reach target |
| Fall | AI drops from source and lands at target |
| Jetpack | AI uses jetpack; optional `launch X` and `target X` waypoints can be set to guide the trajectory |

The Nav Links panel lists all links.  Click a link to centre the canvas on
the midpoint between its two actors.

---

## Triggers (Triggers tab)

Triggers react to in-game events and fire actions.  Each trigger has:

- **Enabled** — active from the start of the mission.
- **One-shot** — fires once then disables itself.
- **Condition logic** — `ALL_OF` (all conditions must be true) or `ANY_OF`
  (at least one must be true).
- One or more **conditions** and one or more **actions**.

### Conditions

| Condition | Description |
|---|---|
| `TEAM_CHECK` | True when a specific team meets a criterion |
| `OBJECTIVE_STATE` | True when a named objective is in a given state |
| `PLAYER_COUNT` | True when the number of players meets a threshold |
| `HEALTH_THRESHOLD` | True when an actor's health crosses a value |
| `COUNT_REACHED` | True when a counter reaches a target value |
| `FLAG_SET` | True when a named flag has been set |

### Actions

| Action | Description |
|---|---|
| `OPEN_DOOR` | Opens a door actor by match ID |
| `LOCK_DOOR` | Locks a door so it cannot be opened |
| `UNLOCK_DOOR` | Unlocks a door |
| `PLAY_SOUND` | Plays a named sound or cue at a world position |
| `SHOW_OBJECTIVE` | Makes an objective visible on the HUD |
| `PAN_CAMERA` | Pans all players' cameras to a world position |
| `SPAWN_ACTOR` | Spawns an actor by type and match ID |
| `END_MISSION` | Ends the current mission |
| `DESTROY_ACTOR` | Removes an actor by match ID |
| `MOVE_ACTOR` | Teleports an actor to a target position |
| `APPLY_DAMAGE_IN_ZONE` | Deals damage to all actors inside a zone |
| `ENABLE_TRIGGER` | Re-enables a disabled trigger |
| `DISABLE_TRIGGER` | Disables a trigger so it cannot fire |
| `COMPLETE_OBJECTIVE` | Marks an objective as complete |
| `LOCK_INPUT` | Freezes player input |
| `UNLOCK_INPUT` | Restores player input |
| `SET_FLAG` | Sets a named boolean flag |

### Objectives

Each trigger can define objectives — mission goals shown on the HUD.  Each
objective has an `id`, a `text` description, and a `required` flag (required
objectives must be completed to win).

### Zones

Zones are named rectangular regions referenced by trigger conditions and
actions.  Define them in the Triggers tab or draw them on the canvas with the
**Trigger Zone** tool.  Each zone has an `id`, `label`, and `x1/y1/x2/y2`
bounds.

---

## Map properties

Open the **Map Properties** panel from the toolbar.

| Property | Description |
|---|---|
| Description | Map name shown in the lobby |
| Background / Parallax | Choose the parallax background layer (BG0–BG4 thumbnails) |
| Ambience | Background ambient sound track |
| Max players | Maximum player count |
| Max teams | Maximum number of teams |
| Supported game modes | Bitmask of allowed game modes (see below) |

### Game modes

| Mode | Description |
|---|---|
| Data Retrieval | Steal data and extract |
| Deathmatch | Free-for-all kills |
| Team Deathmatch | Team kills |
| Survival | Last team standing |
| Extraction | Escort a VIP to the exit |
| Assassination | Protect / eliminate the target |
| Sabotage | Destroy the objective |
| Manhunt | Find and eliminate a hidden target |
| Control Points | Hold zones for score |
| Escort | Protect a moving target |

---

## Visibility toggles

The toolbar exposes layer and overlay toggles to reduce visual clutter while
editing.

| Toggle | Description |
|---|---|
| BG 0–3 | Show/hide individual background tile layers |
| FG 0–3 | Show/hide individual foreground tile layers |
| Platforms | Show/hide collision geometry |
| Actors | Show/hide actor sprites |
| Parallax | Show/hide the parallax background |
| Grid | Toggle tile grid overlay |
| Lighting | Toggle baked light preview |
| Links | Show/hide nav link arrows |

Grid size cycles through **8 / 16 / 32 / 64** pixels.

---

## Light baking

Static lights (actors with type 71 and *dynamic shadows* off) are baked
into per-light masks at save/publish time.  The bake:

1. Collects occluders from solid Rectangle platforms and all shadow zones.
2. For each static light, casts rays from the light centre and builds a
   masked circle or spot cone at the light's radius.
3. Embeds the masks into the `.SIL` file so the game loads them with zero
   runtime cost.

Dynamic lights (type 71 with *dynamic shadows* on) are calculated in-engine
every frame and are not baked.

Enable the **Lighting** toggle to preview baked light masks in the editor.

---

## Copy / paste

1. Switch to the **Select** tool (`S`).
2. Click and drag a rectangular region to select tiles.
3. `Ctrl+C` to copy.
4. `Ctrl+V` to enter paste mode — a floating preview follows the cursor.
5. Click to stamp the paste at the cursor position (can stamp repeatedly).
6. `Esc` to exit paste mode.

The paste buffer copies both the tile IDs and their flip/LUM attributes, on
all layers within the selected rectangle.

---

## Saving and publishing

| Action | Description |
|---|---|
| **Save** | Writes the `.SIL` file to disk using the File System Access API (or downloads it if the API is unavailable) |
| **Publish** | Uploads the map to the server; prompts for filename, author name, and optional API key |
| **Server maps** | Lists maps on the server; open, delete, or re-download any of them |

Publish runs light baking automatically before uploading.

---

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `B` | Tile BG tool |
| `F` | Tile FG tool |
| `E` | Erase tile tool |
| `I` | Flood fill tool |
| `P` | Platform rect tool |
| `A` | Actor tool |
| `S` | Select tool |
| `1` – `4` | Set active tile layer (0–3) |
| `G` | Toggle grid |
| `L` | Toggle lighting preview |
| `?` | Show / hide keyboard shortcuts overlay |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` / `Ctrl+Shift+Z` | Redo |
| `Ctrl+C` | Copy tile selection |
| `Ctrl+V` | Paste (enter paste mode) |
| `Esc` | Cancel paste |
| `Delete` | Delete selected nav link |
| `Space+drag` | Pan canvas |
| Scroll wheel | Zoom |
| Middle-mouse drag | Pan canvas |

---

## .SIL file format

`.SIL` is a binary format.  The editor reads and writes it entirely in the
browser via `useSilMap.ts`.  Key sections:

| Offset range | Contents |
|---|---|
| 0 | First byte / magic |
| 1 | Version |
| Fields 2–11 | Max players, max teams, width, height, parallax index, ambience, flags, description, supported modes |
| Tile data | 8 layers × (width × height) cells; each cell = 36 bytes storing tile ID, flip flag, and luminance |
| Actors | Variable-length list of actor records |
| Platforms | Variable-length list of platform records |
| Shadow zones | Rectangular occlusion zones |
| Light masks | Baked per-light pixel masks (static lights only) |
| Nav links | AI traversal edges (from/to actor index, type, waypoint X values) |
| Triggers | Nodes, conditions, actions, objectives, zone definitions |

---

## Undo / redo

The editor keeps up to **50 snapshots** of the full map state.  Every
discrete edit (tile paint, actor move, platform draw, property change, etc.)
pushes a snapshot.  Painting is batched — the entire paint stroke counts as
one undo step (`beginPaint` / `commitPaint`).

`Ctrl+Z` steps back; `Ctrl+Y` or `Ctrl+Shift+Z` steps forward.
