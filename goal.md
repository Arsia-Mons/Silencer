# Goal: Migrate `clients/silencer` to cppx with Linear-guided execution

You are a long-running implementation agent working in:

`/Users/hv/repos/Silencer/.worktrees/cppx-migration-linear`

Your objective is to complete the refactor of `clients/silencer` so the
Silencer client UI uses the cppx UI framework from `/Users/hv/repos/ui`.
The `/Users/hv/repos/ui` checkout is the golden standard and is
authoritative for architecture, runtime behavior, component style, screen
stack design, cppx authoring, hooks, providers, and tooling expectations.

Treat the current Silencer client UI as deprecated implementation detail. Use
it to understand product behavior and parity requirements, not as the
architecture to preserve.

## Required first steps

1. Read repo instructions before editing:
   - `AGENTS.md`
   - `clients/silencer/CLAUDE.md`
   - any nested `CLAUDE.md` in the area being touched
2. Use the cppx authoring skill before structural UI edits.
   - First check the user-requested location:
     `/Users/hv/repos/gmux/.codex/skills/cppx-authoring/SKILL.md`
   - If that path is absent, use the authoritative cppx repo copy:
     `/Users/hv/repos/ui/.codex/skills/cppx-authoring/SKILL.md`
   - Record which skill path was used in the Linear project or active issue.
3. Read the authoritative cppx references in `/Users/hv/repos/ui`:
   - `src/CLAUDE.md`
   - `src/client/ui/CLAUDE.md`
   - `src/ui/runtime/react.h`
   - `src/ui/runtime/element.h`
   - `src/client/ui/app_shell/navigation/screen_stack.{h,cpp}`
   - representative screen modules, especially the loadout screen and its
     local provider/hook/component structure
   - `tools/cppx_transpile.py` and `tools/cppx_format.py` when syntax,
     lowering, or deterministic formatting behavior is unclear
4. Establish Linear tracking through MCP before implementation work. Do not
   rely on chat-only task lists for this migration.

## Linear operating model

Use Linear MCP to guide the entire effort. Discover the workspace shape first,
then create or update durable tracking objects.

Recommended MCP flow:

1. Use `mcp__linear.list_teams`, `mcp__linear.list_projects`, and
   `mcp__linear.list_issue_statuses` to find the right team, existing project,
   labels, and workflow states.
2. Create or reuse a Linear project named:
   `Silencer client cppx migration`
3. Add a project document named:
   `Migration goal and architecture notes`
   Include the high-level objective, authoritative cppx references, current
   branch/worktree, verification commands, and the rule that `/Users/hv/repos/ui`
   wins when Silencer's deprecated UI disagrees with it.
4. Create a parent Linear issue named:
   `Migrate clients/silencer UI to cppx`
5. Create child issues for implementation slices. Keep slices small enough that
   each can be reviewed, built, and verified independently.
6. Keep exactly one implementation child issue in progress unless there is a
   real blocker. Move issues through Linear states as work starts and finishes.
7. Use `mcp__linear.save_comment` on the active issue at meaningful boundaries:
   - starting context and files being touched
   - architectural decisions and why they match `/Users/hv/repos/ui`
   - verification commands and results
   - blockers, risks, or follow-up issue links
8. Link GitHub issues and PRs from Linear. Linear does not replace the repo's
   required GitHub issue, branch, PR, and squash-merge workflow.

Suggested Linear child issues:

1. Audit Silencer UI against authoritative cppx architecture.
2. Port or vendor the cppx runtime, transpiler, format tooling, generated-file
   flow, and CMake integration needed by `clients/silencer`.
3. Replace the `game.cpp` UI screen state machine with a `ClientUi` owner and
   cppx-style `ScreenStack`.
4. Introduce the retained runtime, root providers, deferred UI mutation queue,
   focus/input glue, and frame wrapper needed by cppx screens.
5. Migrate generic UI primitives and semantic app components to JSX-style
   `.cppx`/`.hx` sources.
6. Ensure every sprite-based/resizable button uses nine-slice rendering through
   private implementation details, never public sprite-bank or fixed-sprite
   codes.
7. Migrate menu, auth, lobby, loadout, options, controls, modal, HUD, and
   overlay flows screen by screen.
8. Move lobby-specific reads and writes behind a narrow `use_lobby()` /
   `UseLobby()` hook and provider rather than expanding `ScreenContext`.
9. Delete deprecated UI paths, obsolete state-machine code, dead shims, and
   generated artifacts that should not be source.
10. Add or tighten deterministic cppx lint, format, transpile, generated-output,
    build, and architecture-boundary checks.
11. Run final end-to-end verification and document remaining intentional
    follow-ups in Linear before marking the project complete.

## Architecture rules

- `/Users/hv/repos/ui` is authoritative. When the current Silencer client
  conflicts with cppx, move toward cppx.
- Write authored UI as JSX-style `.cppx` with matching `.hx` or `.h` headers as
  appropriate. Do not write generated-code-shaped source in `.cppx`.
- Screens are components. Avoid splitting a screen into a screen class plus a
  view model when the component can compose hooks and semantic components
  directly.
- Public UI APIs are props, children, hooks, and providers. Do not expose
  renderer, retained runtime, frame, context-bag, mutation-sink, or screen-stack
  plumbing through component contracts.
- Keep each context private to its provider implementation. Export providers
  from `providers/` and consumer hooks from `hooks/`.
- Use capability hooks rather than broad god hooks. Shared concerns can bubble
  up through providers/hooks; feature-local concerns should stay colocated with
  the feature.
- Do not expand `ScreenContext` for lobby behavior. Add a narrow lobby provider
  and `use_lobby()` / `UseLobby()` boundary near the UI screen tree, then route
  lobby reads/writes through that hook.
- Do not mutate state directly during retained UI build. Queue deferred
  mutations and drain them at the same point in the frame contract used by the
  authoritative cppx implementation.
- `Game` may collect input and request high-level transitions, but it must not
  own or traverse the UI screen stack. `ClientUi` owns visible UI composition
  and navigation.
- `game.cpp` must no longer contain a huge state machine for choosing which UI
  screen to render.
- Keep renderer code focused on world/pixel drawing primitives and compositor
  support. UI screen/HUD composition belongs in client UI code.
- No backwards-compatibility shims during the refactor unless the user asks for
  one explicitly. Update callers to the new design and delete old paths.
- Rename stale `zSILENCER` and `SDL2` references opportunistically when touching
  nearby code or docs.
- Combat overengineering. Add abstractions only where the cppx architecture or
  repeated real usage requires them.

## Button and sprite rendering requirements

- All sprite-based buttons that need to resize must use nine-slice rendering.
- Public button/component APIs should expose semantic `variant` and `size`
  concepts, not sprite bank names, fixed sprite dimensions, palette indexes, or
  one-off per-screen presets.
- The nine-slice payload/compositor path is an implementation detail owned by
  the primitive or renderer layer.
- Add focused tests or fixtures for nine-slice geometry and generated draw
  commands when practical.

## Tooling requirements

If Silencer lacks deterministic cppx tooling, add it as part of the migration.
The repo should have clear, repeatable commands for:

- formatting authored `.cppx` and `.hx`
- transpiling `.cppx` into generated C++ outputs
- checking generated output drift
- running cppx parser/transpiler fixtures
- building `clients/silencer` through the wrapper scripts
- running UI architecture boundary checks

Prefer adapting the proven tooling patterns from `/Users/hv/repos/ui` instead
of inventing new ones. Generated outputs should be wired through CMake in the
same spirit as the authoritative repo.

## Verification expectations

For each implementation slice, run the narrowest meaningful verification before
updating Linear. For UI ownership or architecture changes, include:

```sh
tests/cli-agent/e2e/60_ui_architecture_boundaries.sh
```

For client builds, use the wrapper from `clients/silencer/CLAUDE.md`; do not
invoke CMake directly:

```sh
clients/silencer/build.sh
```

When editing cppx sources, use the cppx formatter and transpiler checks from
the active repo/tooling. If those commands do not exist yet in Silencer, create
the deterministic path and track that work in Linear.

For cross-service or lobby-visible behavior, verify with a real path through
the relevant stack before claiming completion.

## Completion criteria

The migration is complete only when:

- `clients/silencer` uses the cppx retained UI framework and JSX-style authored
  components for client UI screens/components.
- The authoritative screen-stack model from `/Users/hv/repos/ui` is in place.
- `game.cpp` no longer owns a large UI screen-selection state machine.
- Lobby UI state and mutations flow through a dedicated lobby hook/provider
  boundary, not an expanded `ScreenContext`.
- Sprite-based resizable buttons are nine-sliced.
- Deprecated Silencer UI implementation paths and temporary migration shims are
  deleted.
- Deterministic cppx format/transpile/build/check tooling exists and is
  documented enough for future agents to run it.
- Relevant builds, tests, architecture checks, and end-to-end flows pass.
- Linear project, parent issue, and child issues are updated with final
  verification evidence and any explicit follow-up work.
