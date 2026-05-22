# Sprite Manager

The Sprite Manager (`/sprites`) is the admin tool for viewing and editing the
game's sprite and tile banks.  All work is done locally in the browser —
files are read from and written to a folder on your machine; no data is sent
to a server during editing.

---

## Concepts

### Banks

The game's graphics are stored as numbered **banks**.  Each bank is an
independent binary file:

| Type | File pattern | DAT index |
|---|---|---|
| Sprites | `SPR_NNN.BIN` | `BIN_SPR.DAT` |
| Tiles | `TIL_NNN.BIN` | `BIN_TIL.DAT` |

The DAT index stores the frame-count for each bank slot (256 entries of 64
bytes each; frame count at byte `N * 64 + 2`).  A bank is considered
"non-empty" when its frame count is > 0.

### Frames

Each bank contains one or more **frames** (individual images).  Every frame
carries:

| Field | Description |
|---|---|
| `width` / `height` | Pixel dimensions |
| `offsetX` / `offsetY` | Anchor offset (where the game pins the sprite to its position) |
| `compSize` | Compressed size in bytes |
| `mode` | Codec mode (linear RLE vs. tile/block RLE) |

### Palettes

All graphics share a single **`PALETTE.BIN`** file with 11 sub-palettes.
Each sub-palette contains 256 RGB entries using 6-bit channels.  You can
switch the active sub-palette (0–10) when previewing or importing frames.

### Codecs

| Codec | Used for | Description |
|---|---|---|
| Linear RLE | Sprites (`SPR_*.BIN`) | Run-length encoding over a linear pixel stream |
| Tile/block RLE | Tiles (`TIL_*.BIN`) | RLE over 64 × 64 pixel blocks; shared stream per bank |

---

## Opening a folder

1. Click **[ OPEN FOLDER ]** in the header.
2. Select the `shared/assets/` directory (or any directory containing
   `PALETTE.BIN`, `BIN_SPR.DAT`, `BIN_TIL.DAT`, and the `SPR_` / `TIL_`
   BIN files).
3. The tool reads the DAT index and palette immediately; individual banks are
   decoded on demand.

Click **[ CLOSE FOLDER ]** to clear all state and return to the empty-state
panel.

---

## Tabs

| Tab | Shows | Files used |
|---|---|---|
| **sprites** | Sprite banks (`SPR_NNN.BIN`) | `BIN_SPR.DAT` |
| **tiles** | Tile banks (`TIL_NNN.BIN`) | `BIN_TIL.DAT` |

Switch tabs with the `sprites` / `tiles` buttons at the top.  The active tab
is preserved in the URL (`?tab=sprites` or `?tab=tiles`).

---

## Layout

```
┌─────────────────┬──────────────────────────┬───────────────────┐
│  Bank list      │  Frame thumbnails         │  Frame detail     │
│  (left panel)   │  + toolbar (centre)       │  (right panel)    │
└─────────────────┴──────────────────────────┴───────────────────┘
```

---

## Bank list (left panel)

Lists all non-empty banks in the selected tab, plus banks you have created in
this session.

| Control | Action |
|---|---|
| Click a bank | Select it and load its frames |
| `↑` / `↓` arrow keys | Navigate banks |
| **+ BANK** | Create a new empty bank; prompts for a bank index number |
| **– BANK** | Delete the selected bank (requires confirmation) |

---

## Frame thumbnails (centre panel)

Shows all frames in the selected bank as a grid of thumbnails.

### Toolbar

| Button | Action |
|---|---|
| **↑ .BIN** | Load a `.BIN` file from disk, replacing the current bank |
| **↓ .BIN** | Download the current bank as a `.BIN` file |
| **↓ BIN_SPR.DAT** / **↓ BIN_TIL.DAT** | Download the full DAT index |
| **↓ ZIP** | Download a ZIP of all modified banks plus the updated DAT |
| **Export sheet PNG** | Export all frames as a single sprite sheet PNG |
| Sub-palette selector | Switch preview palette (0–10) |
| **Import PNG frame** | Import a single PNG as a new frame |
| **Import sheet** | Import a sprite sheet or tile sheet PNG |
| **Resize all** *(sprites only)* | Resize every frame in the bank to a new width × height |

### Frame interactions

| Action | How |
|---|---|
| Select frame | Click a thumbnail |
| Reorder frames | Drag a thumbnail to a new position |

---

## Frame detail (right panel)

Shows the selected frame enlarged with its metadata.

| Field | Description |
|---|---|
| **Width / Height** | Read-only; set at import time |
| **Offset X / Offset Y** | Editable anchor offsets — change where the game pins the sprite |
| **Delete frame** | Remove the frame from the bank |

---

## Importing frames

### Single PNG import

Click **Import PNG frame**.  The browser's file picker opens; select a PNG.
The image is quantized to the active sub-palette and appended as a new frame.

### Sprite sheet / tile sheet import

Click **Import sheet**.  A modal opens:

1. Select the PNG sheet file.
2. Set the grid cell size (width × height per frame).
3. Optionally set a start offset and frame count.
4. Preview the detected frames overlaid on the sheet.
5. Click **Confirm** to append all frames; **Cancel** to dismiss.

Pixels are quantized to the current sub-palette during import.

---

## Exporting

### Single bank BIN

**↓ .BIN** downloads just the current bank's binary file.  Use this to test
a single bank in-game without regenerating everything.

### Full DAT index

**↓ BIN_SPR.DAT** / **↓ BIN_TIL.DAT** downloads the updated DAT index
reflecting any banks you have added, deleted, or modified.

### ZIP of all changes

**↓ ZIP** bundles every modified bank BIN plus the updated DAT into a
single ZIP file — the complete set of files to drop into `shared/assets/`.

### Sprite sheet PNG

**Export sheet PNG** renders all frames of the current bank side-by-side into
a PNG, using the active sub-palette.  Useful for previewing or sharing.

---

## Resizing a bank (sprites only)

Click **Resize all** and enter the target width and height.  All frames in
the bank are scaled to the new size.  Anchor offsets are preserved.

---

## Online vs. offline capability

The Sprite Manager is fully **offline** — no network connection is required
for any editing operation.  The API proxy routes (`/api/sprites/...`) are
used only by other tools (Map Designer, Actor Editor) to render in-game
sprites; they are not used by the Sprite Manager itself.

| Capability | Offline (folder open) | Online only |
|---|---|---|
| View banks and frames | ✅ | — |
| Edit anchor offsets | ✅ | — |
| Import PNG frames | ✅ | — |
| Import sprite sheets | ✅ | — |
| Reorder / delete frames | ✅ | — |
| Download BIN / DAT / ZIP | ✅ | — |
| Export sheet PNG | ✅ | — |
| Resize all frames | ✅ | — |
| Serve sprites to other tools | — | ✅ (admin-api) |

---

## File format reference

### `BIN_SPR.DAT` / `BIN_TIL.DAT`

```
256 entries × 64 bytes each
Byte N*64+2 = frame count for bank N
```

A bank slot with frame count 0 is treated as empty and omitted from the list.

### `SPR_NNN.BIN` (sprite bank)

```
Header block: numFrames × 344 bytes
  Per frame (344 bytes):
    width       2 bytes
    height      2 bytes
    offsetX     2 bytes  (signed)
    offsetY     2 bytes  (signed)
    compSize    4 bytes
    mode        1 byte
    raw bytes   remaining header bytes (preserved on encode)
Pixel data: RLE-compressed frame data, frames contiguous
```

### `TIL_NNN.BIN` (tile bank)

```
Per-frame header: 12 bytes each
  width   2 bytes
  height  2 bytes
  ...     8 bytes (reserved / raw)
Pixel data: single shared RLE stream for the entire bank (block/tile RLE)
```

### `PALETTE.BIN`

```
11 sub-palettes × 256 colours × 3 bytes (RGB, 6-bit channels)
Total: 8 448 bytes
```

---

## API routes (read-only, used by other tools)

These endpoints are served by `services/admin-api` and proxied through the
Next.js app.  They are used by the Map Designer and Actor Editor to render
sprites — the Sprite Manager itself does not call them.

| Method | Path | Returns |
|---|---|---|
| `GET` | `/api/sprites` | `[{ bank, frames }]` — list of non-empty banks |
| `GET` | `/api/sprites/:bank/frames` | `[{ frame, width, height, offsetX, offsetY }]` |
| `GET` | `/api/sprites/:bank/:frame` | PNG image of the frame |
| `GET` | `/api/sprites/:bank?frame=N` | PNG image of frame N (default 0) |

All responses require an `Authorization` header with a valid admin JWT.

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| `↑` / `↓` | Navigate bank list |

---

## Deployment workflow

1. Open the folder, make your edits.
2. Click **↓ ZIP** to download all modified files.
3. Extract the ZIP into `shared/assets/` in the repo.
4. Rebuild / restart the game server so the new banks are served.
