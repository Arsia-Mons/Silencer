# screen-lobby-game-tech — Choose Tech modal

The tech selection screen, shown after clicking Choose Tech on the
game waiting lobby. Has tech checkbox grid + tech-name + tech-desc
text + Back To Teams button.

Reference dump: `/tmp/real_lobby-gametech_dump.ppm` (640×480 P6,
sub-palette 2, captured via `SILENCER_DUMP_STATE=LOBBY_GAMETECH`).

**Faux-state caveat:** the dump-mode harness creates GameTech
*alongside* a synthetic Team object so the "Disconnected from game"
overlay is suppressed. However, the **tech checkbox grid is empty**
in the captured reference because populating it requires
`world.buyableitems` (loaded at actual-game-start time from item
config) — which the dump-mode harness does not stage. The Ralph
therefore gates only on:
- The 3 button column (replaces GameJoin's Choose Tech with
  Back To Teams; Change Team and Ready are still present from the
  underlying GameJoinInterface).
- The tech-name + tech-desc overlay slots **with empty content**.

A future refinement could populate `world.buyableitems` synthetically
to capture a real tech grid; for now the structural Back To Teams
substitution is the gate.

## Sub-palette

`2`.

## Object inventory

Replaces the right-side panel area, similar bbox to GameJoin.

| z | Object | Type | x | y | Notes |
| - | --- | --- | --- | --- | --- |
| 0 | Back To Teams button | B156x21 | 242 | 68  | text=`Back To Teams`, uid= |
| 1 | Tech checkbox grid   | BCHECKBOX (multiple) | 410..452 | 125+i*13 | 3x N grid (4 columns: 3 dim placeholder + 1 active w/ name); items derived from `world.buyableitems` filtered by `techslots > 0` and `agency` |
| 2 | Tech name labels (column 4) | overlay (font 133, w=6) | 467 | 127+i*13 | One per techslot item, format `<name> (<slots>)` |
| 3 | Selected tech name | overlay (font 134, w=8) | (centered around x=401+116) | 350 | uid=60 |
| 4 | Tech description (8 lines) | overlay (font 133, w=6, color=129) | (centered) | (~365 + i*9) | 8 description lines |

The tech grid layout: 4 columns × N rows, where columns 0..2 are
dimmed placeholders (`effectbrightness=64`, `draw=false`) and column
3 is the active selection column with the tech name overlay.

## What's runtime

- The exact tech list depends on the player's agency (NOXIS / LAZARUS
  / etc) — `world.buyableitems` filtered by agencyspecific.
- The selected tech's name + 8 description lines change as the user
  hovers items.

For the canonical dump, the candidate just needs to render the
checkbox-grid scaffold with placeholder overlays — exact tech-name
text content is non-structural.

## What's behind / over

- **Behind:** LOBBY chrome.
- **Over (non-structural):** "Disconnected from game" modal — see
  GameJoin spec.

## Spec gaps

- `widget-button.md` — BCHECKBOX variant (small checkbox button used
  in tech grid).

## Cross-references

- [`screen-lobby.md`](screen-lobby.md), [`screen-lobby-game-join.md`](screen-lobby-game-join.md) — tech is reached via Choose Tech on the game-join screen
