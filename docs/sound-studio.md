# Sound Studio

Sound Studio is the audio management tool in the admin web app.
It lives at `/sound-studio` and has four tabs: **Sounds**, **Music**,
**Ambient**, and **Cues**.

All playback runs entirely in the browser via WebAudio — no server call is
needed to audition sounds.  Editing features (upload, delete, rename,
repack) require the admin API to be reachable.

---

## Opening a local folder

Sound Studio works offline against a local checkout.  Click **Open
shared/assets/** to pick the `shared/assets/` directory from the repo.
The browser reads `sound.bin` and the GAS files (actor defs, cue JSONs)
directly and makes them available for playback and editing.

When a folder is open, the status bar shows counts for sounds in the bin
and staged uploads.

---

## Sounds tab

The main library view.  Every sound in `sound.bin` is listed with its
size, duration, peak level (decoded from ADPCM), waveform, and all call
sites known from the game source.

### Playback

Click any row to select it and press **▶** (or click the play button in
the row) to decode and play it.  Playback stops automatically when the
sound ends or when another sound starts.  Clicking the row while it is
playing pauses.

Loop sounds (marked **loop**) are shown with their known fadeout
duration from the game source.

**Sound set playback** — sounds that belong to a set of interchangeable
random variants (e.g. `rico1.wav` / `rico2.wav`) show a **▶ set** button
that picks a random member to audition — useful to hear how the group
sounds as a whole.

### Waveform and level

Selecting a row and playing it draws a miniature waveform in the detail
panel on the right.  The peak and RMS levels are displayed.  A **HDR**
badge appears in the list if the decoded peak exceeds −0.3 dBFS while
the known call-site volume is 128 (full scale) — this flags sounds that
are likely to clip in-game.

The **normalize** checkbox (toolbar) enables peak normalization during
repack, bringing all sounds to −0.3 dBFS headroom.

### A/B compare

Right-click (or use the detail panel) to pin a sound to **slot A** or
**slot B**.  Then:

- **▶ A** / **▶ B** — play each slot independently.
- **A→B** — plays A to completion then immediately plays B — useful for
  comparing a replacement with the original back-to-back.

### Filters

| Filter | Shows |
|---|---|
| all | Everything |
| cpp | Sounds referenced by name in C++ source |
| actordef | Sounds referenced in GAS actor-def JSON |
| loop | Sounds that are looped in-game |
| ambient | Background ambient sounds |
| attenuated | Sounds whose known call-site volume is < 128 |
| ui | Sounds in the UI category |
| orphaned | Sounds in the bin with no known reference (safe to delete) |
| missing | Sounds referenced in C++/GAS but absent from the bin |
| headroom | Decoded sounds near full scale at vol=128 call sites |

Active filter is highlighted.  `missing (N)` and `headroom (N)` show
counts when there are violations.

### Sort and grouping

Click column headers (**name**, **size**, **duration**, **level**,
**refs**) to sort ascending/descending.  Without sorting, sounds are
grouped by category (Player → NPC → Weapon → World → UI → Ambient) with
a collapsed **uncategorised** bucket at the bottom.

### Upload new sounds

**Drag and drop** WAV files anywhere onto the page, or click **+ Upload
WAV** in the toolbar.  Uploaded files are staged on the server (shown
with a `[staged]` badge) and do not enter the bin until you repack.

### Delete and rename

Select one or more rows:

- **✕ stage delete (N)** — marks selected sounds for deletion on the
  next repack.  The row dims with a `[del]` badge.
- Rename — click the row's **✎** button to rename it.  The new name is
  queued with a `[→newname]` badge.

**Bulk orphan delete** — when the `orphaned` filter is active, a
**stage all orphans for deletion** button appears.  It stages every
orphaned sound in one click.

### Repack

When there are staged changes (uploads, deletions, renames), the **⚡
Repack** button turns green.  Clicking it:

1. Sends all staged changes to the server.
2. Rebuilds `sound.bin` server-side.
3. Downloads the new `sound.bin` to your machine.
4. Re-open `shared/assets/` to load the updated bin.

The **Pending changes** panel (accessible from the toolbar) lists added,
modified, deleted, and renamed files before committing.

---

## Music tab

Lists the music files available on the server (`/sounds/music`).  Click
any track to stream and play it.  Music plays exclusively — starting a
new track stops the previous one.

---

## Ambient tab

A live mixer for the three background ambient channels the game blends
at runtime:

| Channel | File | Role |
|---|---|---|
| BG_BASE | `wndloopb.wav` | Indoor base hum (always present) |
| BG_AMBIENT | `cphum11.wav` | Computer-room hum (indoor areas) |
| BG_OUTSIDE | `wndloop1.wav` | Outdoor wind (exterior areas) |

Controls:

- **▶ Start / ■ Stop** — starts or stops all three channels looping
  simultaneously, matching in-game synchronisation.
- **Indoor / Outdoor** — switches the gain preset between the two modes
  the game uses.
- **Ratio** slider — cross-fades between indoor and outdoor weight in
  real time so you can hear the transition.
- **Mute** buttons — silence individual channels for isolation.

This tab lets you audition replacement recordings against each other in
the same mix context as the game, before repacking.

---

## Cues tab

The node-based sound cue editor.  See [sound-cue-system.md](sound-cue-system.md)
for the full cue reference.  Quick summary of the UI:

**Sidebar** — lists all cues.  Click a cue to open it in the editor.
The **↓ ALL** button downloads a zip of every cue JSON.

**Canvas** — drag nodes from the left panel, wire them by dragging from
an output handle to an input handle.  Pan with middle-mouse or space+drag.
Zoom with scroll wheel.

**Node inspector** — click any node to edit its parameters in the right
panel (file picker for WavePlayer, range sliders for Modulator, etc.).

**▶ Play** — evaluates the graph once using the current node settings and
plays the result through WebAudio.  Decodes ADPCM on the fly from the
local folder if one is open.

**Save** — writes the cue JSON to the admin API; it is persisted on disk
in `shared/assets/gas/sound-cues/<id>.json`.

**New Cue** — creates a blank graph with just an Output node.  Name it
with a snake_case slug that matches what you will reference in GAS
(`cue:my_cue_name`).

**Delete Cue** — removes the cue from the server and from the local
sidebar (does not touch the JSON on disk until the next repack/sync).

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| `Space` | Play / pause selected sound (Sounds tab) |
| `↑` / `↓` | Move selection up / down |
| `Shift+click` | Range-select multiple rows |

---

## Offline vs. online mode

| Feature | Offline (no API) | Online (API reachable) |
|---|---|---|
| Playback | ✅ (local folder) | ✅ (streams from server) |
| Waveform / level | ✅ | ✅ |
| A/B compare | ✅ | ✅ |
| Ambient mixer | ✅ (local folder) | ✅ |
| Cue editing + play | ✅ (local folder) | ✅ |
| Upload / stage | ✗ | ✅ |
| Delete / rename | ✗ | ✅ |
| Repack | ✗ | ✅ |
| Music tab | ✗ | ✅ |
