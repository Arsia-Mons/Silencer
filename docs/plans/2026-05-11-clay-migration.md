# Clay-idiomatic UI rewrite

**Status:** In progress (Ralph loop)
**Date:** 2026-05-11
**Supersedes:** [2026-05-10-ui-v2-declarative-layout.md](2026-05-10-ui-v2-declarative-layout.md), [2026-05-10-ui-v2-progress.md](2026-05-10-ui-v2-progress.md)

## Why this exists

The prior plan introduced a `Node` IR — a retained-mode tree screens
populate, then a custom walker translates each `Node` into manual
`Clay__OpenElement` / `Clay__ConfigureOpenElement` / `Clay__CloseElement`
calls. The `CLAY()` macro is never used. `Clay_Hovered` / `Clay_OnHover` /
`Clay_PointerOver` are never used (we re-hit-test cached rects ourselves
in `dispatch.cpp`). All element IDs are `Clay_GetElementIdWithIndex("ui-v2-id", n++)`
— unstable across sibling reorder. There is no use of `userData` /
`customData` for app draw data. Result: ~600 LOC of `layout.cpp` + `node.h`
+ `render.cpp` + `dispatch.cpp` doing work Clay does for free, while
losing capabilities Clay provides.

That was wrong. We are throwing it out. Prior decisions are **not**
preserved — including the "pure function returning a `Node` tree"
authoring contract.

**North stars:**

- [docs/research/clay-ui-patterns.md](../research/clay-ui-patterns.md)
  — canonical Clay patterns, with citations to Nic Barker's docs and
  the official examples. Read this first.
- This document — the migration plan and progress tracker.

## Mandate

1. **Do it the Clay way.** Composition = small functions emitting
   `CLAY(){…}` blocks. Styling = style functions returning
   `Clay_ElementDeclaration` or file-scope const declarations.
   Interaction = `Clay_Hovered()` / `Clay_OnHover()` / `Clay_PointerOver()`.
   IDs = `CLAY_ID(stable_string)` for anything queried, animated, or
   floating; `CLAY_AUTO_ID()` (source-location-hashed) for leaf chrome.
   App draw data = `Clay_ElementDeclaration.userData` /
   `CLAY_RENDER_COMMAND_TYPE_CUSTOM` with `.custom.customData`.
2. **Simplicity is correctness.** Total LOC in `clients/silencer/src/ui/`
   must decrease substantially. Baseline (2026-05-11): **8902** lines.
   Target: **under 5500** lines (≥38% reduction). Anything bigger than
   that is wrong shape.
3. **No in-between architectures.** The Node IR exists *only* as
   scaffolding during the migration. Final state has zero references
   to `ui::v2::Node`, zero direct calls to `Clay__OpenElement` /
   `Clay__ConfigureOpenElement` / `Clay__CloseElement`, no `dispatch.cpp`,
   no per-node `RenderNode` switch.
4. **Build must stay green every iteration.** `cmake --build build`
   must succeed before each commit.

## What good looks like

A screen is a `void` function that emits `CLAY(){…}` blocks directly,
reading state from `ctx` and calling `Clay_Hovered()` inline:

```cpp
// screens/main_menu.cpp (sketch, post-migration)

static Clay_ElementDeclaration MenuButtonStyle(bool hovered) {
    return {
        .layout = { .padding = CLAY_PADDING_ALL(8), .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) } },
        .backgroundColor = hovered ? kColorAccent : kColorIdle,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    };
}

static void MenuButton(ButtonId id, Clay_String label, const Context& ctx) {
    CLAY({ .id = CLAY_ID(id.key), ...MenuButtonStyle(Clay_Hovered()) }) {
        Clay_OnHover(OnButtonHover, (intptr_t)&ctx.dispatch);
        CLAY_TEXT(label, CLAY_TEXT_CONFIG({ .fontId = kFontMenu, .fontSize = 14 }));
    }
}

void RenderMainMenu(const Context& ctx) {
    CLAY({ .id = CLAY_ID("MainMenuRoot"),
           .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                       .childGap = 12 } }) {
        MenuButton({"tutorial"}, CLAY_STRING("Tutorial"), ctx);
        MenuButton({"connect"},  CLAY_STRING("Connect To Lobby"), ctx);
        MenuButton({"options"},  CLAY_STRING("Options"), ctx);
        MenuButton({"exit"},     CLAY_STRING("Exit"), ctx);
    }
}
```

Sprite chrome rides on `userData` (or `customData` for full sprites),
and is consumed in the render-command loop — not in a per-Node switch.

Per-frame in the Runtime:

```cpp
void MainMenuRuntime::Render(...) {
    Clay_SetPointerState(/* mouse pos */, /* mouse_down */);
    Clay_UpdateScrollContainers(/* enabled */, /* scrollDelta */, dt);
    Clay_BeginLayout();
    RenderMainMenu(ctx);
    Clay_RenderCommandArray commands = Clay_EndLayout();
    DrawRenderCommands(commands, renderer);
}
```

Click dispatch happens during the layout pass via `Clay_OnHover(fn,
userData)` callbacks (which fire when pointer is over the element and
state changes), or via `Clay_PointerOver(id)` queries against known
interactive IDs in a small input router. Either way, no separate
walker, no cached `rect_*` fields, no `Node::kind` switch.

## What we delete

Citations are file paths under `clients/silencer/src/ui/`:

| Anti-pattern | Where | Why wrong |
|---|---|---|
| `Node` IR with `NodeKind` enum, `rect_*` cache, chainable `.at()/.onClick()/.withKey()` | `node.h` | Retained-mode layer on immediate-mode lib |
| `Clay__OpenElement` / `Clay__ConfigureOpenElement` / `Clay__CloseElement` called directly | `layout.cpp:197-206` | Bypasses `CLAY()` macro |
| `Clay_GetElementIdWithIndex("ui-v2-id", n++)` for every element | `layout.cpp:82-91` | Unstable across sibling reorder |
| `BeforeLayout` declared but never called | `layout.cpp:211` | `Clay_Hovered` reads stale; scroll can't react |
| Custom hit-tester `ButtonHit` over cached rects | `dispatch.cpp:12-43`, `render.cpp:152` | `Clay_Hovered`/`Clay_PointerOver` exist |
| `VStack`/`HStack`/`Center`/`Padding`/`Spacer` as distinct `NodeKind`s | `node.h:26-30` | `layoutDirection` + `GROW` already provide these |
| Button styling reconstructed inside `BuildDecl` switch each frame | `layout.cpp:144-149` | Should be a style function returning `Clay_ElementDeclaration` |
| `.gap()`/`.padding()` accept only uniform `Uint16` | `node.h:81-82` | Clay supports per-side padding |
| `using namespace ui::v2;` namespace itself | many | The `v2` segment is migration-relic; drop it |

## Architecture (post-migration)

- `runtime.h` — unchanged. `Runtime::Render(...)` per frame drives the
  Clay lifecycle.
- `screens/<name>.cpp` — defines `void Render<Name>(const Context&)`
  emitting `CLAY(){…}` blocks, plus `<Name>Runtime` that owns app
  state and calls `Render<Name>` inside `Clay_BeginLayout` / `Clay_EndLayout`.
- `modals/<name>.cpp` — same shape as screens.
- `render_commands.cpp` — single consumer of `Clay_RenderCommandArray`.
  Switches on `commandType`, reads `userData`/`customData` for sprite
  chrome, palette, font selection. **Replaces** `render.cpp`.
- `context.h` — unchanged shape; `Context` is still the bag of inputs
  screens read. `dispatch` field becomes a callback target for
  `Clay_OnHover` userdata.
- **Deleted:** `node.h`, `layout.cpp`, `layout.h`, `dispatch.cpp`,
  `dispatch.h`, `render.cpp` (replaced by `render_commands.cpp`),
  `button_chrome.h` if its data moves to `userData`.

## Phases (operational backlog: `.ralph/prd.json`)

This section describes the work in prose. The **operational state**
lives in `.ralph/prd.json` — the Ralph loop flips one `passes: true`
flag per iteration. To see live progress, run:
`jq -r '.items | sort_by(.priority)[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' .ralph/prd.json`.

**P0 — Foundation.** Lifecycle wiring (`Clay_SetPointerState` +
`Clay_UpdateScrollContainers` before `Clay_BeginLayout`); a
`theme.h` of file-scope Clay constants; a `render_commands.cpp` that
consumes `Clay_RenderCommandArray` and replaces the per-Node renderer
as the single draw path.

**P1 — Stable IDs.** Replace the shared-label-+-index scheme with
keys derived from semantic identity. `CLAY_ID(key)` for queried /
animated / floating / scrollable elements; `CLAY_AUTO_ID()` for
inert leaf chrome.

**P2 — Native Clay interaction.** Delete `dispatch.cpp` + `ButtonHit`
+ `rect_*` caches. Hover styling reads `Clay_Hovered()` inline; click
dispatch via `Clay_PointerOver(id)` or `Clay_OnHover` callbacks.

**P3 — Screens emit `CLAY(){…}` directly.** Each screen rewrites
`Node Build<Name>(const Context&)` → `void Render<Name>(const Context&)`.
One screen per iteration. Order in `prd.json`: simple → complex,
ending at `lobby_shell` (may split into sub-items) and `storybook`.

**P4 — Demolition.** Delete `node.h`, `layout.cpp/.h`, `dispatch.cpp/.h`,
the per-Node `render.cpp`. Drop the `ui::v2` namespace. Update
`clients/silencer/src/ui/CLAUDE.md`.

**P5 — Verify shrinkage.** LOC under 5500; the grep predicates below
all hold; build green.

## Verification protocol (every iteration)

Before committing, the loop must:

1. `cmake --build build` — green. If red, the iteration is incomplete.
2. For any screen touched in this iteration, run
   `./build/silencer --preview-screen <name>` headlessly (PPM output)
   and confirm it produces non-empty output. (Pixel parity with the
   prior path is *not* a hard requirement — the migration may
   intentionally change layout details where the prior path was
   wrong.)
3. Commit with a message of the form `refactor(silencer/ui): <P#> <one-line summary>`.
4. Tick completed checkboxes in this doc.

## Done criteria → completion promise

When **all** of the following hold, and **only then**, output the
completion promise:

- Every checkbox in P0–P5 is ticked.
- `cmake --build build` is green on a clean build.
- Total `clients/silencer/src/ui/` LOC < 5500.
- All P5 grep predicates pass.

Completion promise: `<promise>CLAY-MIGRATION-COMPLETE</promise>`

Do not output the promise until every condition is genuinely true.
