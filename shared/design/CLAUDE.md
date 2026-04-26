# shared/design — design-system hydrations

Reference renderings of [`docs/design/`](../../docs/design/) — currently
scoped to the main menu. Used to verify the spec is faithful and
complete.

| Dir | Stack | How to QA |
| --- | ----- | --------- |
| [`sdl3/`](sdl3/CLAUDE.md) | SDL3 + C++17 (`brew install sdl3`) | `cmake -B build && cmake --build build && SILENCER_DUMP_DIR=/tmp/d ./build/silencer_design ../../assets` then `sips -s format png /tmp/d/screen_00.ppm --out /tmp/d/screen_00.png` |

Visual-equivalence target is the real client's framebuffer dump:

```
SILENCER_DUMP_PATH=/tmp/real.ppm \
  /path/to/Silencer.app/Contents/MacOS/Silencer
```

The two PPMs should be near-identical when both renderers wait for
the bank-208 logo to reach `res_index = 60` (steady state).

## History

`html/` and `raylib/` hydrations of the previous monolithic spec
(`docs/design-system.md.archive`) were removed when we rebuilt the
spec around the main-menu subset. They can be re-introduced one at
a time as the per-component docs in `docs/design/` expand. Until
then, `sdl3/` is the only reference rendering.
