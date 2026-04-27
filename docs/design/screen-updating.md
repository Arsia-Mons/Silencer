# screen-updating — UPDATING state (auto-update progress)

The auto-update progress UI shown when the client detects a
version mismatch and downloads an update.

Reference dump: `/tmp/real_updating_dump.ppm` (640×480 P6,
captured via `SILENCER_DUMP_STATE=UPDATING`).

## Sub-palette

`(captured dump appears black except for the green-bordered box —
likely sub-palette 1 or 2 depending on transition state. Empirical
check: 12 RGB bytes at idx 0 of palette — the candidate should
match the reference by trial.)`

## Object inventory

Minimal — the captured reference shows:

| z | Object | Notes |
| - | --- | --- |
| 0 | Empty bordered box (~318x60, centered around y=215) | The progress / message area, empty in the canonical dump because the engine never actually started a download |
| 1 | Cancel button (B156x21-style) | text=`Cancel`, right-aligned within the box |

In normal flow `CreateUpdateInterface` likely creates a TextBox for
log lines and a progress indicator alongside the Cancel button — but
the captured reference is minimal because dump mode entered UPDATING
without a real update to fetch.

## What's runtime / non-structural

- TextBox content (download log; empty in this dump).
- Progress percentage / bar (not visible in this dump — may not
  render until download starts).

## What this Ralph gates

The candidate dump should show:
- A black/dark background.
- A bordered box with a Cancel button inside.

**Faux-state caveat:** the captured reference is *intentionally
minimal*. UPDATING normally entered from a version-mismatch detected
during LOBBYCONNECT auth, with a real download progress bar /
manifest URL / log lines streaming in. The dump-mode harness force-
enters UPDATING without any of that, so only the empty bordered
box + Cancel button render. Populating UPDATING fully would require
either pointing the lobby at a real `update.json` manifest (the
lobby logs warn it's missing) and triggering a fake-version mismatch,
or extending the harness to inject mock download state. For now the
structural skeleton is the gate.

## Spec gaps

- Full `CreateUpdateInterface` object inventory (engine line 3677);
  the captured dump is too minimal to enumerate.
- Whether the box border is bank 7 idx X or some other panel sprite.

## Cross-references

- [`screen-lobby-connect.md`](screen-lobby-connect.md) — sibling state in startup flow (UPDATING is entered from version mismatch during LOBBYCONNECT)
