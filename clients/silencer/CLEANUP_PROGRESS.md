# cppx-migration cleanup progress

Janitorial cleanup of debris left by the cppx UI migration (#267,
`f3708c29`). Scope: `clients/silencer/src/` only. Zero behavior change.
Working on branch `hv/cleanup` (dedicated `cleanup` worktree).

## Baseline survey (iteration 1, 2026-06-17)

The codebase is **already very clean** — the migration was tidy. Targeted
noise greps surface almost nothing actionable:

- `git grep '// *(TODO|FIXME|cppx|Clay|was |HACK)'` → 9 hits, **all
  legitimate** (cppx-as-architecture-name, a real SDL3_mixer limitation
  TODO in `audio.cpp`, a design-rationale comment in `tokens.h`). None are
  scaffolding chatter. Nothing to remove.
- Trailing whitespace: 0. `(void)x;` placeholders: 0. `#if 0` blocks: 0.
  Debug `printf`/`cout`/`fprintf(stderr)` in UI dirs: 0.
- Triple+ blank lines: 3 (all in `lobby_components.cppx` lines 303/424/459).
  Double blanks elsewhere are that file's consistent style, not debris.

So this loop is near-no-op. Cleaning only genuine, verified-safe debris.

## Checklist

Status legend: `todo` / `done` / `skip`

| Area | Status | Note |
|------|--------|------|
| Mechanical sweep (whitespace/blanks/#if0/void-x) | done | only 3 triple-blanks found, in lobby_components.cppx — fixed (batch 1) |
| Descriptor-vs-JSX anti-pattern hunt | done | NOT present — all components return JSX; 0 imperative `Foo(FooProps{})`, 0 vector<UiElement> push_back, 0 descriptor structs in render path |
| client/ui/** (138 files) | done | Explore sweep: no actionable debris (one "ported verbatim" parity note left intentionally) |
| game/ui/** (7 files) | done | Explore sweep: clean |
| render/cppx_ui/** (27 files) | done | Explore sweep: clean |
| ui/style/** (9 files) | done | Explore sweep: clean |

## Per-file notes (append one line per file cleaned)

- `screens/lobby/components/lobby_components.cppx` (batch 1): collapsed 3 triple-blank-line runs to the file's double-blank style. No code/comment change.

## Deliberately left (documented exceptions)

- `screens/lobby/components/lobby_components.cppx:423-454` — a block of detached
  descriptive comments that document `GameCreatePanel`/`CreateRightCell`/
  `LobbyTitleBar`/`AgentCard`/tech panels (defined 300-900 lines below, none of
  which carry their own comments). These are REAL origin-parity documentation,
  just misplaced. Deleting loses info; relocating is a structural edit beyond
  delete-only cleanup. **Follow-up candidate for human review** (move-to-function),
  not debris to delete.
- `screens/character_create_data.h:4` — `// ported verbatim from origin/main ...`
  parity-source attribution; useful for SIL-84 visual-restore work. Left.
- `audio/audio.cpp:415`, `components/tokens.h:41` — legitimate rationale/limitation
  notes, not scaffolding. Left.
