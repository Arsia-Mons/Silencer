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

## Autonomous loop protocol

You are a long-running implementation agent. Each iteration:

1. Read the Linear architecture doc (once per session is fine), then pick the next
   **unblocked** Linear issue (lowest SIL-number whose blocked-by deps are all Done).
   Foundation order that needs no open decisions: **SIL-7 → SIL-8 → SIL-9 → SIL-10**.
2. Set it **In Progress** (keep exactly one high-coupling ownership slice — SIL-13/14/15 —
   in progress at a time). Comment your starting context + files.
3. Implement the slice against the contract in the Linear doc. `/Users/hv/repos/ui` wins on
   any disagreement. No backwards-compat shims. Rename stale `zSILENCER`/`SDL2` when you
   touch a line. Combat overengineering.
4. **Verify** before claiming done: `clients/silencer/build.sh` + the issue's checks +
   `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh`. Paste evidence into the issue.
5. Commit to branch `hv/cppx-migration-cc` (push to PR #267), set the issue **Done**, repeat.

**All migration decisions are pre-resolved** — see SIL-6's decisions register (source of
truth for the loop) and the architecture doc. Proceed without stopping for them: UI fonts
are `shared/fonts/silencer-{ui,ui-large,title,tiny}.otf` (generated from the legacy banks →
exact identity, loaded via SDL_ttf); HUD is in scope using the `tiny` face; residuals are
locked. Stop and ask only on a genuinely novel blocker not covered there (e.g. an
unforeseen upstream/SDL constraint). Don't spin.

## Verify (per slice)

```sh
clients/silencer/build.sh                              # never raw cmake/ninja
tests/cli-agent/e2e/60_ui_architecture_boundaries.sh   # extend, never weaken
# + cppx format/transpile/drift checks once the tooling slice (SIL-7) lands
```
For cross-service / lobby-visible behavior, verify a real path through the full stack.
