# Lighting System

Covers the full lighting pipeline: how ambient darkness works, how placed
light actors function, shadow/occlusion zones, the `.sil` binary format,
and the designer workflow for authoring all of the above.

---

## Concepts

### Ambient darkness

Each map stores an `ambience` value (`Sint8`, −128..127) in its binary
header. The renderer converts it at startup:

```
ambiencelevel = 128 + (ambience × 4.5)
```

`ambiencelevel` is a `Uint8` that drives the whole-level darkness. At the
default of `ambience = 0` the map is full-brightness and placed lights
have no visible effect. Negative values darken the map and make lights
pop.

The designer exposes ambience in **Map Properties** with four presets:

| Preset    | `ambience` |
|-----------|-----------|
| Bright    | 0         |
| Medium    | −10       |
| Dark      | −20       |
| Very Dark | −28       |

New maps default to `ambience = −20`.

### Light actors (bank 222)

A **Light actor** is a regular map actor with `id = 71`. The map loader
creates an `Overlay` object with `res_bank = 222` and `res_index =
actortype`. Three frame sizes exist:

| `actortype` / frame | Visual |
|---------------------|--------|
| 0 | Small halo |
| 1 | Medium halo |
| 2 | Large halo |

During `DrawWorld()` every bank-222 overlay is collected into
`objectlights`. After `DrawForegroundLuminance()` they are drawn with
`DrawLight()`, which additively brightens each pixel using
`Palette::Light()`.

### LUM tiles

Individual tiles can carry a **LUM flag** (`Map::Tile::LUM`). These are
rendered by `DrawForegroundLuminance()` as a static ambient glow — they
always apply regardless of ambient level and are not affected by shadow
zones.

---

## Shadow / Occlusion Zones

Shadow zones are **axis-aligned rectangles in world-pixel space** that
block light from reaching pixels behind them. They are stored on the map
and tested per-pixel in `DrawLight()`.

### C++ data

```cpp
// map.h
struct ShadowZone { Sint32 x1, y1, x2, y2; };
std::vector<ShadowZone> shadowzones;
```

`shadowzones` is populated only for the primary map load (`team == 0`).
Base-map loads (team bases) silently ignore the shadow zone section.

### Per-pixel occlusion (ray-AABB slab test)

For each non-zero luminance pixel in `DrawLight()`:

1. Compute the pixel's **world position**: `world_x = screen_x − cameraOffX`
2. Build a parametric segment from the **light center** → **pixel** in world space
3. For each shadow zone, run the slab test (`tmin`/`tmax` in `[0,1]`)
4. If `tmin < 1.0 && tmax > 0.0` the segment crosses the zone → pixel stays dark (skip brightening)

The test uses `goto nextzone` to break out of the per-zone inner check
early — valid C++14 and avoids a nested-function call per pixel.

Shadow zone occlusion is applied **only** to bank-222 objectlights. LUM
tile `DrawLight()` calls are intentional point-lights for luminance auras
and do not receive zone data.

---

## `.sil` Binary Format

Each `.sil` file is a fixed **uncompressed header** followed by a
**zlib-compressed level blob**.

### Header (uncompressed, big-endian unless noted)

| Field | Size | Notes |
|-------|------|-------|
| `firstbyte` | 1 | Magic / version tag |
| `version` | 1 | |
| `maxplayers` | 1 | |
| `maxteams` | 1 | |
| `width` | 2 BE | Map width in tiles |
| `height` | 2 BE | Map height in tiles |
| `padding` | 1 | |
| `parallax` | 1 | Parallax layer index |
| `ambience` | 1 | Sint8 darkness level |
| `padding` | 2 | |
| `flags` | 4 BE | Uint32 flags |
| `description` | 128 | NUL-terminated string |
| `minimapcompressedsize` | 4 LE | |
| `minimapcompressed` | ≤ 172×62 | zlib-compressed minimap |
| `levelsize` | 4 LE | Size of compressed level blob |

### Level blob (zlib-compressed, little-endian throughout)

The decompressed bytes are laid out as sequential sections:

#### Section 1 — Tiles

`width × height × 36` bytes. Each tile cell is 9 bytes × 4 layers:

```
Per layer:
  Uint16  bg tile_id
  Uint8   bg flip flag
  Uint8   bg lum flag
  Uint16  fg tile_id
  Uint8   fg flip flag
  Uint8   fg lum flag
+ 4 bytes unknown/unused per tile
```

Layers are interleaved per tile (all 4 layers for tile (0,0), then all 4
for tile (1,0), etc.).

#### Section 2 — Actors

```
Uint32  numactors
Uint32  padding
numactors × {
  Uint32  actor_id       (entity kind — see table below)
  Uint32  x              (world-pixel x)
  Uint32  y              (world-pixel y)
  Uint32  direction      (0 = right, 1 = left)
  Sint32  type           (subtype / variant)
  Uint32  matchid
  Uint32  subplane
  Uint32  unknown
  Uint32  securityid
}
```

Selected `actor_id` values:

| `actor_id` | Entity |
|-----------|--------|
| 0 | Agent guard (blaster) |
| 2 | Agent guard (blaster variant) |
| 3 | Trooper guard (rocket) |
| 6 | Robot |
| 36 | Player start location |
| 37 | Surveillance camera |
| 47 | Doodad overlay (type selects sprite bank 49-58) |
| 64 | Vent |
| 65 | Base exit |
| 66 | Tech station |
| 67 | Wall defense |
| 68 | Team billboard + surveillance monitor |
| 69 | Computer doodad (bank 171) |
| 71 | **Light actor** → bank 222, frame = `type` |

#### Section 3 — Platforms

```
Uint32  numplatforms
Uint32  padding
numplatforms × {
  Sint32  x1
  Sint32  y1
  Sint32  x2
  Sint32  y2
  Uint32  type1
  Uint32  type2
}
```

Platform types from `(type1, type2)`:

| type1 | type2 | Platform type |
|-------|-------|--------------|
| 0 | 0 | Rectangle (solid floor/wall) |
| 1 | 0 | Ladder |
| 0 | 1 | Stairs up |
| 0 | 2 | Stairs down |
| 2 | 0 | Track |
| 3 | 0 | Outside room |
| 3 | 1 | Specific room |

#### Section 4 — Shadow Zones *(optional, backward-compatible)*

This section is **only appended when at least one shadow zone exists**.
Old clients stop reading after platforms (`return true`) and never see it.

```
Uint32  numzones
Uint32  padding
numzones × {
  Sint32  x1   (world-pixel, left edge)
  Sint32  y1   (world-pixel, top edge)
  Sint32  x2   (world-pixel, right edge)
  Sint32  y2   (world-pixel, bottom edge)
}
```

All values are little-endian. Normalisation (ensuring x1 < x2, y1 < y2)
is done at runtime in `DrawLight()`, not at parse time.

---

## Camera Coordinate System

Screen pixel `(screen_x, screen_y)` maps to world pixel:

```
world_x = screen_x − camera.GetXOffset()
world_y = screen_y − camera.GetYOffset()
```

where `GetXOffset() = screenWidth/2 − camX`. `DrawLight()` receives
`cameraOffX = camera.GetXOffset()` and uses it to convert each rendered
pixel back to world space for the ray-AABB test.

---

## Designer Workflow

The level designer is at `web/admin/app/designer/`.

### Map Properties panel

- **Ambience slider** (−128..127) with quick preset buttons
- Formula preview matches C++ exactly: `ambiencelevel = 128 + ambience × 4.5`

### Placing a Light actor

1. Select **Light** from the Actors section of the Toolbar
2. Click on the canvas to place. The actor panel lets you set `type` (0/1/2 = small/medium/large)
3. The canvas preview renders the bank-222 sprite and a radial glow when the lighting layer is on

### Shadow / occlusion zone tool (🌑)

The **SHADOW_ZONE** tool is in the "Other tools" section of the Toolbar.

- **Draw**: select the tool, then click-drag to define an axis-aligned rectangle
- **Preview**: while dragging, a red semi-transparent rectangle with a dashed border is shown; "SZ" label appears at zoom > 0.25×
- **Delete**: right-click an existing zone while the SHADOW_ZONE tool is active
- Zones are stored in `SilMapData.shadowZones: MapShadowZone[]`
- `resizeMap` automatically filters zones that fall outside the new bounds

### Save / Publish

Both `saveMap` (local `.sil` download) and `publishMap` (upload to the
map API) serialize shadow zones into the optional trailing section.
Maps with no zones serialize identically to old clients' format.

---

## Key Source Files

| File | Role |
|------|------|
| `clients/silencer/src/world/map.h` | `Map::ShadowZone` struct, `shadowzones` vector, `Map::Header` |
| `clients/silencer/src/world/map.cpp` | `LoadFile()` — header/tiles/actors/platforms/shadow-zone parsing; `Unload()` |
| `clients/silencer/src/render/renderer.h` | `DrawLight()` declaration; `GetAmbienceLevel()` |
| `clients/silencer/src/render/renderer.cpp` | `DrawLight()` ray-AABB occlusion; `DrawWorld()` objectlights loop |
| `web/admin/lib/types.ts` | `MapShadowZone`, `SilMapData`, `MapActor`, `MapPlatform` |
| `web/admin/app/designer/useSilMap.ts` | `parseShadowZones()`, serialize, `addShadowZone`, `removeShadowZone` |
| `web/admin/app/designer/Toolbar.tsx` | SHADOW_ZONE tool entry |
| `web/admin/app/designer/MapCanvas.tsx` | Zone rendering, drag-to-draw, right-click delete |
| `web/admin/app/designer/MapPropertiesPanel.tsx` | Ambience slider and presets |
| `web/admin/app/designer/page.tsx` | Wires `addShadowZone`/`removeShadowZone` into MapCanvas |
| `shared/assets/gas/lights.json` | Data-driven light catalogue (`LightDef` — name, frame, radius) |
