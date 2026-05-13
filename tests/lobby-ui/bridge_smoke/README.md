# Clay → Surface bridge smoke test (P3)

Drives the `clay_bridge_smoke` JSON-lines control op to render a tiny
fixed Clay tree (one `RECTANGLE`, one `BORDER`, one `TEXT`, one `IMAGE`)
into a 640×480 paletted Surface, dumps it to PNG, and pixdiffs against
the committed `reference.png`.

## Run

```
bash tests/lobby-ui/bridge_smoke/run.sh
```

Pass bar: pixdiff < 1.0%. The committed
reference reproduces byte-for-byte across boots; current threshold has
zero slack on purpose. If a future change bumps the diff above 0%,
either the bridge regressed or determinism slipped — investigate before
re-baselining.

## Re-baseline

If the test scene legitimately changes (new primitive added to
`clay_smoke.cpp`, new bank/index for the IMAGE, etc.), regenerate:

```
REGEN=1 bash tests/lobby-ui/bridge_smoke/run.sh
```

That overwrites `reference.png` from the live binary. Commit the new
PNG alongside the source change in the same patch.

## Why this scene?

- `RECTANGLE` — solid palette-index fill (idx 200).
- `BORDER`   — 4-edge stroke, no background, palette idx 220.
- `TEXT`     — bank 135 (Title), cell width 11, palette idx 152
  (the lobby-title effect color used in P11+).
- `IMAGE`    — bank 7 idx 9, the scrollbar track; small (~8×48), known
  to exist in the default resource bundle, no animation.

Together they cover every render-command type the lobby will use except
`SCISSOR_START/END` (P11+ panel scrolling) and `CUSTOM` (P5+ button
chrome / scrollbar).

## Determinism

The bridge resets `Clay_SetPointerState`, `Clay_UpdateScrollContainers`,
and `Clay_ResetMeasureTextCache` before every `Clay_BeginLayout`, and
the smoke op uses the static base palette (`renderer.palette.GetColors()`)
instead of `Game::GetPaletteColors()` to avoid the main-menu fade-in
palette drift. Without those, consecutive shots within a boot differ by
~1.2% and across-boot diffs are non-zero. See clay_bridge.cpp's
`EnsureInitialized` for the resets and clay_smoke.cpp's `RunSmoke` for
the palette choice.
