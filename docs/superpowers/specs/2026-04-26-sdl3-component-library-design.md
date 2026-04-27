# SDL3 design-system component library extraction — design

**Date:** 2026-04-26
**Scope:** Refactor the `shared/design/sdl3/` hydration tool into a
small reusable component library (pure-render functions on a
framebuffer) plus a tiny `DumpRunner`/`ScreenSpec` registry. Eliminate
the duplication the Devin reviewer flagged on PR #56 and produce a
component layer that `clients/silencer/` can later call from inside
its existing widget classes.

## Motivation

`shared/design/sdl3/src/main.cpp` is **2,210 lines, 11 `RunDump*`
functions**, with ~500 lines duplicated across the four LOBBY-modal
variants. The SDL init / palette / sprite-load / PPM-write
scaffolding is also duplicated 11×.

Devin's PR review (PR #56) flagged the LOBBY duplication as the only
substantive cleanup item. Beyond that flag, this work positions the
hydration tool as the seed for a real component library that
`clients/silencer/` can depend on as it modernizes its UI code.

The widget code in `clients/silencer/` is old — imperative, all
state/render/input baked into one class per widget. Long-term we want
to be able to lift the *rendering* portion out and share it with the
hydration tool, so both consumers paint pixels through the same
named primitives. This refactor produces those primitives.

## Non-goals

- **No declarative tree / scene graph.** Components are plain
  functions that take a `*View` data struct and a framebuffer.
  A future declarative pivot is allowed but not part of this work.
- **No state, input, focus, or animation logic in components.**
  `state_i` ticking, hover/focus/pressed state machines, hit-testing,
  tab order, mouse routing, modal stacking — all stay in
  `clients/silencer/`. Components take steady-state values from the
  caller.
- **No `clients/silencer/` widget migration.** This refactor does not
  touch `clients/silencer/`. A future change can have e.g.
  `Button::Draw()` call `silencer::RenderButton(...)`, but that's a
  separate piece of work informed by what we learn here.
- **No new top-level directory.** Code lives inside
  `shared/design/sdl3/src/`. Lifting to `shared/ui/sdl3/` is a rename
  for the day a second consumer appears.
- **No CMake target split.** Headers and `.cpp`s build as part of
  the existing `silencer_design` executable. A static-lib split is
  trivial later.
- **Out of scope: the other Devin review items.** The
  `SeedDemoUser` race in `services/lobby/` and the static-locals
  observation in `clients/silencer/src/game.cpp`'s dump harness are
  unrelated and deferred.

## Architecture

```
shared/design/sdl3/
├── CMakeLists.txt
├── src/
│   ├── palette.{h,cpp}         (unchanged)
│   ├── sprite.{h,cpp}          (unchanged — Framebuffer + BlitSprite)
│   ├── font.{h,cpp}            (unchanged — DrawText)
│   │
│   ├── components/             NEW
│   │   ├── panel.{h,cpp}
│   │   ├── header.{h,cpp}
│   │   ├── button.{h,cpp}
│   │   ├── modal.{h,cpp}
│   │   ├── character.{h,cpp}
│   │   ├── gameselect.{h,cpp}
│   │   └── chat.{h,cpp}
│   │
│   ├── screens/                NEW (one per screen)
│   │   ├── main_menu.cpp
│   │   ├── options.cpp
│   │   ├── options_audio.cpp
│   │   ├── options_display.cpp
│   │   ├── options_controls.cpp
│   │   ├── lobby_connect.cpp
│   │   ├── lobby.cpp
│   │   ├── lobby_modal_create.cpp
│   │   ├── lobby_modal_join.cpp
│   │   ├── lobby_modal_tech.cpp
│   │   ├── lobby_modal_summary.cpp
│   │   ├── updating.cpp
│   │   ├── demo_data.{h,cpp}   (shared seed data: character, chat, games)
│   │
│   ├── dump_runner.{h,cpp}     NEW
│   └── main.cpp                shrinks to ~80 lines
```

The exact list of `screens/*.cpp` files mirrors today's `RunDump*`
function set 1:1. The `components/*.{h,cpp}` list is the right
inventory for *this* set of screens — implementers should feel free to
fold/split (e.g. add `components/textinput.h` if a screen needs it,
collapse `chat.h` into `gameselect.h` if they share too much) as the
shape becomes obvious during the work.

### Boundaries

**Components** are pure functions:
```cpp
void Render*(Framebuffer&, const SpriteSet&, [const Palette&,
             int active_sub,] const *View&);
```
`*View` structs are plain data (POD-ish, no SDL types). Component
headers depend only on `<cstdint>`/`<vector>`/`<string>` and the
existing `palette.h` / `sprite.h` / `font.h`. **No SDL3 runtime in
component interfaces** — that's the property that lets a future
`clients/silencer/src/button.cpp::Draw()` call
`silencer::RenderButton(fb, sprites, font, ToButtonView(*this))`
without dragging the dump-mode lifecycle into the game.

**Screens** are thin compose-the-frame functions, one per file:
```cpp
void Compose<Screen>(Framebuffer&, const SpriteSet&, const Palette&,
                     const Font&);
```

**`DumpRunner`** owns the scaffolding shared by all 11 screens:
```cpp
struct ScreenSpec {
  const char* name;             // CLI selector + output filename suffix
  std::vector<int> banks;       // sprite banks to load
  int active_sub_palette;       // for PPM resolve at end
  int background_index;         // initial fb fill
  void (*compose)(Framebuffer&, const SpriteSet&, const Palette&,
                  const Font&);
};

int RunDump(const ScreenSpec&, const std::string& assets_dir,
            const std::string& dump_dir);
```

`RunDump` does: `SDL_Init(0)` → `Palette::Load` →
`SpriteSet::Load(spec.banks)` → `fb.fill(spec.background_index)` →
`spec.compose(...)` → `WritePPM(dump_dir/spec.name.ppm)` →
`SDL_Quit()`.

**`main.cpp`** becomes a `static const ScreenSpec kScreens[]` registry
and an argv dispatcher. Defaulting to "run all" preserves today's
behavior; an optional `<screen_name>` arg lets the Ralph loops target
one screen.

### Component shapes (illustrative — implementers refine)

```cpp
// panel.h
struct PanelView { int x, y, w, h; int bank, idx; };
void RenderPanel(Framebuffer&, const SpriteSet&, const PanelView&);

// header.h
struct HeaderView { std::string title, version; bool show_back_button; };
void RenderHeader(Framebuffer&, const SpriteSet&, const Palette&,
                  int active_sub, const HeaderView&);

// button.h
enum class ButtonVariant { B156x21, B98x21, BCircle, /* 7 variants */ };
struct ButtonView {
  std::string label; int x, y;
  ButtonVariant variant;
  int brightness;        // INACTIVE = 128 (caller picks; no state machine here)
};
void RenderButton(Framebuffer&, const SpriteSet&, const Palette&,
                  int active_sub, const ButtonView&);

// character.h        — left panel (CharacterInterface composition)
// gameselect.h       — right panel (GameSelectInterface)
// chat.h             — bottom-left (ChatInterface)
// modal.h            — Create/Join/Tech/Summary variants for the LOBBY family
```

The spec-of-record for what each composition must produce visually is
`docs/design-system.md` (2,233 lines, authoritative). Components must
not change *visual output* — only *who calls what to produce it*.

### What a screen looks like after the refactor

```cpp
// screens/lobby_modal_create.cpp
void ComposeLobbyModalCreate(Framebuffer& fb, const SpriteSet& sprites,
                             const Palette& pal, const Font& font) {
  RenderPanel(fb, sprites, {.x=0, .y=0, .w=640, .h=480, .bank=7, .idx=1});
  RenderHeader(fb, sprites, pal, 2,
               {.title="Silencer", .version="00029",
                .show_back_button=true});
  RenderCharacterInterface(fb, sprites, pal, 2, demo::Character());
  RenderChatPane(fb, sprites, pal, 2, demo::Chat());
  RenderModalCreate(fb, sprites, pal, 2, demo::CreateModal());  // ← only this differs vs join/tech/summary
}
```

The four LOBBY-modal screens become structurally identical except for
the last line. Devin's complaint disappears.

### Final size estimate

- `main.cpp`: 2,210 → ~80 lines (registry + argv dispatch).
- `screens/*.cpp`: ~30–80 lines each × 11 files.
- `components/*.{h,cpp}`: ~50–150 lines each × 7 components.
- `dump_runner.{h,cpp}`: ~60 lines.

Total LOC roughly comparable to today (we're paying header overhead),
but no file over ~200 lines and zero duplication.

## Testing — the regression contract

**The existing PPM dumps are the contract.** After the refactor:

1. Snapshot every reference PPM produced by the pre-refactor binary
   at the PR #56 head (commit `9ff82e9`) — `screen_*.ppm` for all 11
   screens.
2. Do the refactor.
3. Re-build, re-dump every screen.
4. **Byte-compare every output PPM against its pre-refactor snapshot
   with `cmp`.** Zero diff is required. A single byte change is a
   regression to root-cause (likely a missing brightness/sub-palette
   thread-through or a forgotten bank in the spec).

The Ralph loops in `.ralph/lobby-extras/`, `.ralph/lobby-connect/`,
etc. already produce reference PPMs — re-use those as the snapshot.
No new unit tests are added; the PPMs *are* the test, and they cover
every visual output path the components produce.

Optional CI hook (follow-up, not blocking): a CMake `add_test`
that runs the full dump set and `cmp`s against committed reference
PPMs.

## Migration path to `clients/silencer/` (future)

Not part of this work, but the design protects this future:

- `clients/silencer/` already runs SDL3 (commit `872ec3b` on `main`,
  Phases 2+3, RenderDevice + GPU backend). No version-bridge layer
  needed.
- `clients/silencer/src/button.cpp::Draw()` could later wrap
  `silencer::RenderButton(fb, sprites, font, ToButtonView(*this))`,
  shedding its inline draw code.
- The component headers are explicitly free of SDL3 runtime types —
  only `Framebuffer` / `SpriteSet` / `Palette` / `Font` cross the
  boundary, none of which contain SDL handles in their public
  interface.
- A future declarative layer (scene-graph / data-driven UI) would
  walk a tree and bottom out in calls to these same primitives. This
  refactor is the prerequisite for that pivot, not a competitor to it.

## Hand-off to a remote agent

A remote agent picking this up should:

1. **Branch from the PR head** at `origin/hv/design-ralph` (commit
   `9ff82e9`). The hydration tool already exists there in working
   shape; this refactor restructures it.
2. **Snapshot reference PPMs** before changing any code:
   ```
   cd shared/design/sdl3 && cmake -B build && cmake --build build
   mkdir -p /tmp/sdl3_dump_baseline
   SILENCER_DUMP_DIR=/tmp/sdl3_dump_baseline \
     ./build/silencer_design <repo>/shared/assets
   # copy /tmp/sdl3_dump_baseline somewhere safe
   ```
3. **Implement in this order:**
   1. `dump_runner.{h,cpp}` + `ScreenSpec` shape + a stub `main.cpp`
      that runs one screen via `RunDump` to prove the scaffolding
      works (byte-compare that one PPM).
   2. Extract `components/panel.h`, `components/header.h`,
      `components/button.h` first — used by every screen. After
      each, byte-compare every PPM.
   3. Extract `components/character.h`, `chat.h`, `gameselect.h`,
      `modal.h` next — LOBBY-family. Byte-compare after each.
   4. Move screen bodies into `screens/*.cpp` last, one at a time,
      byte-comparing after each.
4. **Byte-compare PPMs after every component or screen extraction.**
   Caught early, regressions are one-line fixes; caught late, they
   require bisecting.
5. **Final size check:** `wc -l shared/design/sdl3/src/main.cpp` ≤
   100. If `main.cpp` is still over 200 lines, something didn't move.

## Decisions captured

- **Approach A** (pure-render component layer) over B (full widget
  library with state) and C (stack-agnostic data layer). A is
  shippable in days, doesn't preclude B or C, and produces the
  primitives both would build on.
- **Cut at level (b)** (Component + DumpRunner) over (a) (components
  only) and (c) (component + screen-skeleton). Level (b) eliminates
  the worst duplication (scaffolding + LOBBY) without baking
  speculative screen-shape opinions.
- **No declarative pivot now.** Best validated against
  `clients/silencer/` once the primitives are real, not against the
  hydration tool which has no input/state to surface declarative's
  hard problems.
