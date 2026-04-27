# screen-lobby-game-summary — Mission Summary + Agency Upgrade modal

The post-match summary screen showing match stats and an agency
upgrade panel. Renders over a starfield background (no LOBBY chrome
visible).

Reference dump: `/tmp/real_lobby-gamesummary_dump.ppm` (640×480 P6,
captured via `SILENCER_DUMP_STATE=LOBBY_GAMESUMMARY`).

**Note:** unlike GameCreate / GameJoin / GameTech which sit inside
the LOBBY chrome, GameSummary covers most of the screen. The
captured dump shows a starfield background — this is sub-palette 0
or the engine's parallax starfield, not sub-palette 2's panel
exterior. The candidate's render path for this modal needs to draw
its own background.

## Sub-palette

`0` (in-game palette) per engine `SetPalette(0)` in INGAME state /
post-match. The captured dump's stars are red dots on dark blue —
consistent with sub-palette 0's starfield treatment.

## Object inventory (key elements)

Two main sections: **Mission Summary** stat list (left) and
**Agency Upgrade** panel (right).

### Mission Summary (left half)

| Object | Type | Notes |
| --- | --- | --- |
| "Mission Summary" title | overlay | top-left of left panel |
| Background plates | overlays (banks/indices TBD by engine reading) | bordered panel chrome on left half |
| Stat list (~11–13 rows) | overlays | each row: label + numeric value |

Stat labels (top to bottom, from canonical dump):
- Kills, Deaths, Suicides
- Secrets: Returned, Stolen, Fumbled
- Civilians killed, Guards killed, Robots killed
- Defenses destroyed, Fixed Cannons destroyed
- Files: Hacked, Returned
- Powerups picked up, Health packs used
- Cameras placed, Detonators planted, Fixed Cannons placed
- Viruses used, Poisons, Lazarus Tracts planted

All values are runtime stats and are zero in the canonical dump (no
match was actually played — `Stats` was zero-initialized by the
harness).

### Agency Upgrade (right half)

| Object | Type | Notes |
| --- | --- | --- |
| "+ 0 XP" overlay | overlay | top of right panel |
| "*NEW UPGRADE AVAILABLE*" overlay | overlay | below XP |
| 6 upgrade rows | "+1 <stat>" B156x33-style buttons | Endurance / Shield / Jetpack / Tech Slot / Hacking / Contacts |
| Done button | (B156x21 or similar) | text=`Done`, uid=0 |

Each upgrade row also has an overlay text "Current Endurance Level: N"
above the +1 button (level number is runtime).

## What's runtime / non-structural

- All stat values (zero in this dump).
- XP value (`+ 0 XP` because no match).
- Current level numbers.
- Whether the *NEW UPGRADE AVAILABLE* banner is shown.

Gate the candidate on:
- Both panel-chrome backgrounds.
- "Mission Summary" title.
- Stat label list (left column text content).
- All 6 upgrade-row labels.
- "Done" button.

## Spec gaps

- New widget shapes for the upgrade-row buttons (look like elongated
  pills, possibly a new widget variant — needs sprite-bank confirmation).
- The starfield background — likely the standard parallax starfield;
  candidate needs to know how to render it.
- `Stats` struct definition (engine-internal, but the row labels are
  hardcoded strings in `CreateGameSummaryInterface`).

## Cross-references

- [`screen-lobby.md`](screen-lobby.md) — modal is force-created from LOBBY in dump mode
- Future `screen-ingame.md` — true natural state for this modal (post-match)
