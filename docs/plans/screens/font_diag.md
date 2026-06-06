# Font diagnosis — bank-135 Heading face (silencer-135.otf) renders BLANK in cppx UI

**Status:** Root cause found and empirically confirmed. One-line fix in the
generator (+ optional immediate strip of the committed OTF). Recommend applying
the fix and re-pointing `ScreenTitle` to `kFaceHeading`.

## TL;DR

`silencer-135.otf` is the **only** Silencer face that carries an OpenType
`SVG ` table. The in-game text path uses Homebrew **SDL_ttf 3.2.2 + FreeType
2.14.3, and that FreeType is built with `FT_CONFIG_OPTION_SVG` enabled**. When a
glyph has an OT-SVG document, FreeType routes it through the OT-SVG driver and
will only produce a bitmap if the application has registered an SVG renderer
hook. SDL_ttf registers none, so `FT_Load_Glyph` yields the glyph in
`FT_GLYPH_FORMAT_SVG` with **no raster** — and FreeType does **not** auto-fall
back to the perfectly-good `glyf` outline. Result: `TTF_RenderText_Blended`
blits nothing → blank glyphs → invisible Screen/Dialog titles.

The other four faces (`ui`, `ui-large`, `title`, `tiny`) have **no `SVG `
table**, so they always take the `glyf` path and render fine. That is the *only*
structural difference between 135 and the working faces.

## Evidence

Font internals (`fontTools`), bank 135 vs the working Title face:

| Property                | silencer-135.otf            | silencer-title.otf | silencer-ui-large.otf |
| ----------------------- | --------------------------- | ------------------ | --------------------- |
| File size               | **762 KB**                  | 182 KB             | 58 KB                 |
| Tables                  | `... SVG glyf loca ...`      | `glyf loca` (no SVG) | `glyf loca` (no SVG)  |
| Has `SVG ` table        | **YES**                     | NO                 | NO                    |
| cmap entries            | 155 (A/B/S/T/1 all mapped)  | 155                | 155                   |
| glyf `A`                | 63 contours, bbox 64,128–640,1024 (real ink) | 115 contours, real ink | 36 contours, real ink |
| unitsPerEm / native     | 1088 (17px×64)              | 1536 (24px×64)     | 832 (13px×64)         |

So the font is **not** empty/zero-width: the cmap is correct, the `glyf` table
has real outlines with sensible bboxes, advances are non-zero (512), and the OTF
*was* regenerated (it landed in the SIL-95 commit `63de1a3b`, 762 KB binary
added). None of the original hypotheses (empty glyphs from extract.py, wrong
cmap, missing glyf, OTF not regenerated, TTF_OpenFont size) hold.

Rasterization proof — render `"START"` through FreeType:

- **PIL/Pillow FreeType (SVG OFF)**: silencer-135.otf inks 293 px @17px, 1172 px
  @34px — renders identically to title/ui-large. Falls back to `glyf` because
  PIL's bundled FreeType has no `FT_CONFIG_OPTION_SVG`.
- **In-game = Homebrew FreeType 2.14.3 (`FT_CONFIG_OPTION_SVG` defined,
  confirmed at `ftoption.h:566`)** → SVG driver active, no hook → blank.

Strip-and-retest: deleting the `SVG ` table from the committed OTF drops it to
**99 KB** and it still inks **293 px @17px** through FreeType — proving the
`glyf` table alone is correct and the SVG table is the sole liability.

Build wiring confirming the SVG-capable FreeType is what runs in-game:

- `build/CMakeCache.txt`: `SDL3_ttf_DIR=/opt/homebrew/lib/cmake/SDL3_ttf`
- `/opt/homebrew/include/SDL3_ttf/SDL_ttf.h`: version **3.2.2**
- `brew list`: `freetype 2.14.3`, `sdl3_ttf 3.2.2`
- `/opt/homebrew/Cellar/freetype/2.14.3/.../ftoption.h:566`:
  `#define FT_CONFIG_OPTION_SVG`
- `font_registry.cpp:128` / `draw_executor.cpp:101`: plain
  `TTF_RenderText_Blended`, no SVG hook registered anywhere in `src/render/`.

## Why the `SVG ` table is on 135 (and a latent landmine for ALL faces)

`extract.py` emits an `SVG ` table **unconditionally** for every bank
(`build_font()` → `fb.font["SVG "] = svg_table`, lines ~335–337). The four
working faces in git simply predate that (or were generated with a different
fonttools) and were "intentionally left untouched" in SIL-95 — that's why they
have no SVG table. **The moment anyone reruns `uv run extract.py` to regenerate
all faces, every face gains an `SVG ` table and ALL in-game text goes blank.**
This is the true root cause and must be fixed in the generator, not just on 135.

The `SVG ` table exists to give the web/CSS consumer per-pixel AA via
`fill-opacity` (the `currentColor` recolor trick). But `shared/fonts/index.css`
registers only tiny/ui/ui-large/title — **silencer-135.otf is not even exposed
to the web**, so its SVG table serves no consumer and is pure cost.

## Concrete fix (recommended)

**Make SDL_ttf the first-class target: don't ship an `SVG ` table in fonts the
game loads.** The `glyf` table already preserves the original AA via the
opacity threshold in `make_outline_glyph` (pixels with opacity < 0.5 are
dropped), so in-game parity is unchanged.

Two parts:

1. **Generator (`shared/fonts/tools/extract.py`)** — stop emitting OT-SVG for
   the in-game faces. Minimal change: gate SVG emission off (or only on for a
   `--web` flavor). Concretely, drop the SVG build for the banks the game loads:

   ```python
   # build_font(cfg): only attach SVG for web-only flavors.
   if cfg.get("svg", False):
       svg_table = table_S_V_G_()
       svg_table.docList = svg_docs
       fb.font["SVG "] = svg_table
   ```

   and set `"svg": False` (or omit) on every BANK entry the client uses. Since
   no `shared/fonts` consumer currently needs SVG-135 and the other four ship
   without SVG today, the simplest correct change is to **remove the
   `fb.font["SVG "]` assignment entirely** (delete lines ~335–337 and the
   `svg_docs`/`make_svg` plumbing) — the web AA story is already glyf-only for
   the four registered faces, and the docstring's `currentColor` recolor still
   works via CSS `color:` on the glyf outlines (no per-pixel AA, but that
   already matches what the registered web fonts ship today).

   > Combat-overengineering note: do NOT add a dual web/game build mode unless a
   > web SVG consumer is actually re-introduced. Today there is none.

2. **Immediate unblock without a full regen** — strip the table from the
   committed binary so the current branch renders now:

   ```bash
   cd shared/fonts
   uv --project tools run python - <<'PY'
   from fontTools.ttLib import TTFont
   f = TTFont("silencer-135.otf"); del f["SVG "]; f.save("silencer-135.otf")
   PY
   ```

   (Drops 762 KB → ~99 KB; glyf-only; renders identically through FreeType.)

3. **Re-point `ScreenTitle`** (`clients/silencer/src/client/ui/components/text/
   screen_title.cppx`): the `Screen` and `Dialog` variants currently force
   `kFaceTitle`/`kFontTitle`/`kLineTitle` with a "renders BLANK" workaround
   comment. Restore them to the Heading face:

   ```cpp
   case ScreenTitleVariant::Screen:
     color = tokens::kTextTitle;
     face  = tokens::kFaceHeading;  // 4 → silencer-135.otf
     font  = tokens::kFontHeading;  // 17
     line  = tokens::kLineHeading;  // 19
     height = 30.0f;  // re-check vs golden; 135 is shorter than 136
     break;
   case ScreenTitleVariant::Dialog:
     color = tokens::kTextTitle;
     face  = tokens::kFaceHeading;
     font  = tokens::kFontHeading;
     line  = tokens::kLineHeading;
     height = 24.0f;
     break;
   ```

   and delete the BLANK-glyph workaround comment block. Hero/Popup stay on the
   136 Title face by design.

### Should we just keep the Title face instead?

**No — fix it.** The Title face (bank 136, native 24px) is a *different,
larger* glyph set than the legacy heading face (bank 135, native 17px); SIL-95's
whole point was glyph parity for Screen/Dialog headings, and the legacy
`Renderer::DrawText` used bank 135 for those. Keeping 136 is a visible parity
miss (oversized headings, wrong letterforms). The real fix is one line in the
generator plus a binary strip — cheap and correct — so there is no reason to
settle for the Title-face workaround.

## Verify recipe

1. Apply fix part 2 (strip) + part 3 (re-point), build:
   `clients/silencer/build.sh` (default `win-ninja` / on mac the wrapper's mac
   preset).
2. Confirm the OTF loads + measures (existing guard already covers this):
   `clients/silencer/tests/ttf_font_smoke_tests.cpp` asserts
   `silencer-135.otf` opens and measures @17px — extend it to assert
   `TTF_RenderText_Blended` of `"A"` yields a surface with **non-zero ink**
   (sum of alpha > 0), which would have caught this regression.
3. Headless visual capture (reach any screen with a title — e.g. Lobby/Options):
   ```bash
   . clients/silencer/tests/cli-agent/e2e/lib.sh
   PORT=$(pick_port); PID=$(start_silencer "$PORT"); wait_alive "$PORT"
   bun clients/cli/index.ts --port $PORT goto <state-with-a-title>
   bun clients/cli/index.ts --port $PORT screenshot /tmp/title.png
   stop_silencer "$PID" "$PORT"
   ```
   Confirm the ScreenTitle text is visible (green) and in the smaller 135
   letterforms, not the 136 Title face.
4. Re-bless the affected visual-regression goldens (titles change face/size).

## Risks / unknowns

- **Other-platform FreeType may differ.** Some FreeType builds *without*
  `FT_CONFIG_OPTION_SVG` would ignore the SVG table and render 135 fine — so the
  bug is FreeType-build-dependent and will look "fixed" on a machine whose
  FreeType lacks SVG. The generator fix removes the dependency entirely (always
  glyf), which is the robust answer regardless of platform FreeType flags.
- **Latent all-faces breakage:** anyone running `uv run extract.py` today
  regenerates all five faces *with* SVG tables and blanks the entire UI. Fixing
  the generator (part 1) is what actually prevents recurrence; the binary strip
  (part 2) only patches 135.
- **Web AA loss:** removing SVG drops per-pixel `fill-opacity` AA for any future
  web consumer. None exists today (135 isn't in `index.css`; the four registered
  faces already ship glyf-only). If a web SVG flavor is ever needed, add a
  `--web` build that emits SVG — do not put SVG back on the game faces.
- **Heading metrics vs golden:** bank 135 is shorter than 136; the `height`
  values in `ScreenTitle` (30/24) were tuned for the Title face and should be
  re-checked against the golden once 135 renders. Likely needs a small height
  adjustment, not a structural change.
- **Glyf AA fidelity:** the glyf outline drops sub-50%-opacity edge pixels
  (threshold in `make_outline_glyph`), so in-game 135 is crisper/less-AA than
  the SVG would have been — but this exactly matches how the other four faces
  already render in-game, so it is consistent, not a regression.
