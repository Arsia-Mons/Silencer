# Goal: migrate `clients/silencer` UI to the retained cppx engine

Replace Silencer's **Clay** immediate-mode UI with the **golden retained cppx engine**
from `/Users/hv/repos/ui` (authoritative). Screens become pure `.cppx` view functions
composing hooks + semantic components, reconciled into a retained tree, laid out by Yoga,
drawn through an RGBA `DrawCommandList`, with all mutation queued and drained after render.
The golden repo uses **zero Clay** — this is a full engine replacement, not JSX-on-Clay.

## Source of truth = Linear (not this file)

This file is a pointer + starter prompt. The living context lives in Linear:

- **Project:** Silencer client cppx migration —
  https://linear.app/silencer-cc/project/silencer-client-cppx-migration-f106f12cf8a7
- **Architecture & contract (read first):** "Migration goal and architecture notes" —
  https://linear.app/silencer-cc/document/migration-goal-and-architecture-notes-3b8352cbedb6
- **Parent issue:** SIL-5 · **Slices:** SIL-6 … SIL-24 (dependency graph wired via blocked-by).

Track all task context, decisions, and progress in Linear. Do not grow this file.

## Authoritative references

- Golden repo `/Users/hv/repos/ui` wins on any disagreement with the deprecated Silencer UI.
- cppx authoring skill: `/Users/hv/repos/ui/.codex/skills/cppx-authoring/SKILL.md`
  (the `/Users/hv/repos/gmux/...` path in older notes does not exist).
- Read before structural edits: `ui/src/ui/runtime/{react,element,tree}.h`,
  `ui/src/client/ui/app_shell/navigation/screen_stack.{h,cpp}`, the loadout screen.

## Where the work lands

- Worktree: `/Users/hv/repos/Silencer/.worktrees/cppx-migration-cc`
- Branch `hv/cppx-migration-cc` → single long-lived draft **PR #267** (Arsia-Mons/Silencer).
  All slices land here; squash-merge at the end. Linear children are the task units —
  no per-slice PRs.

## How to work

1. Read the Linear architecture doc, then pick the next **unblocked** issue (start at **SIL-6**).
2. Keep exactly one high-coupling ownership slice (SIL-13/14/15) in progress at a time.
3. Move the Linear issue through states; comment at boundaries (context, decisions, verification).
4. No backwards-compat shims. Rename stale `zSILENCER`/`SDL2` when you touch a line. Combat overengineering.

## Verify (per slice)

```sh
clients/silencer/build.sh                              # never raw cmake/ninja
tests/cli-agent/e2e/60_ui_architecture_boundaries.sh   # extend, never weaken
# + cppx format/transpile/drift checks once the tooling slice (SIL-7) lands
```
For cross-service / lobby-visible behavior, verify a real path through the full stack.
