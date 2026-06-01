# Goal: Follow the Linear source of truth for the cppx migration

This worktree is:

`/Users/hv/repos/Silencer/.worktrees/cppx-migration-linear`

Linear is the durable context store for this migration. Do not treat this file
as the architecture spec.

## Start Here

Read the Linear project before implementation work:

- Project: `Silencer client cppx migration`
- Project URL:
  `https://linear.app/s1l3nc3r/project/silencer-client-cppx-migration-2a17db80b64d`
- Authoritative document:
  `Migration goal and architecture notes`
- Document URL:
  `https://linear.app/s1l3nc3r/document/migration-goal-and-architecture-notes-c28f034f7874`
- Parent issue:
  `SIL-5` - `Migrate clients/silencer UI to cppx`
- Active architecture issue:
  `SIL-8` - `Replace game.cpp UI state selection with ClientUi-owned ScreenStack navigation`

If this file and Linear disagree, Linear wins.

## Current Direction

The current course correction is recorded in Linear:

- Do not continue the `ctx.GoToState(GameState, std::unique_ptr<Screen>)`
  direction.
- Providers are scoped context boundaries for shared domain state with a
  lifetime. They are not root layers and not one-per-screen controllers.
- Hooks return virtual domain models with nested state and verbs.
- Do not introduce catch-all `actions` bags.
- Do not split one domain model into one hook per panel or command unless a
  real lifetime or sharing constraint requires it.
- `GameState` is engine/session lifecycle, not UI route state.
- `ClientUi` owns visible UI composition and `ScreenStack` navigation.
- `GameUiPipeline` is frame/input/render plumbing only.

## Required Local Instructions

Before editing, read:

- `AGENTS.md`
- `clients/silencer/CLAUDE.md`
- any nested `CLAUDE.md` in the touched area

Use the cppx authoring skill before structural cppx UI edits:

`/Users/hv/repos/ui/.codex/skills/cppx-authoring/SKILL.md`

Use `/Users/hv/repos/ui` as the authoritative cppx reference when Linear points
you there.

