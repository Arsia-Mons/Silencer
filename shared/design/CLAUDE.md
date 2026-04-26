# shared/design — design-system hydrations

Three independent renderings of [`docs/design-system.md`](../../docs/design-system.md),
each built purely from the spec + `shared/assets/` (no reference to
`clients/silencer/`). Used to verify the spec is faithful and complete.

| Dir | Stack | How to QA |
| --- | ----- | --------- |
| [`html/`](html/CLAUDE.md) | Static HTML + CSS + a touch of vanilla JS | `open shared/design/html/index.html` |
| [`sdl3/`](sdl3/CLAUDE.md) | SDL3 + C++17 (`brew install sdl3`) | `cmake -B build && cmake --build build && ./build/silencer_design ../../assets` |
| [`raylib/`](raylib/CLAUDE.md) | raylib 5.x + C99 (`brew install raylib`) | `cmake -B build && cmake --build build && ./build/silencer_design ../../assets` |

SDL3 and raylib parse `PALETTE.BIN` and the sprite banks (RLE codec from
§Asset Formats). HTML fakes glyphs with the VT323 web font and substitutes
sprite art with palette-colored placeholders. Per-hydration CLAUDE.md
documents what's faithful vs. faked.
