# cppx-migration cleanup progress

Janitorial cleanup of debris left by the cppx UI migration (#267,
`f3708c29`). Scope: `clients/silencer/src/` only. Zero behavior change.
Working on branch `hv/cleanup` (dedicated `cleanup` worktree).

## Status: DONE (2026-06-17)

All three primary targets complete. Stop-condition sweep
(`git grep -nE 'SIL-[0-9]+|// *(TODO|FIXME|cppx|Clay|was |HACK)'`) surfaces
nothing actionable — every remaining hit is a documented exception (below).

### Target 1 — SIL-#### ticket-ref removal (the big one): DONE

All ~271 `SIL-####` breadcrumbs removed from code comments under `src/`.
Comment-only, zero behavior change (verified: of 20 changed lines that carry
code, every one is `code; // comment` where only the trailing comment changed;
zero pure-code lines touched). Build clean (`win-ninja`),
`60_ui_architecture_boundaries` e2e PASS. Net -54 comment lines.

Done in one batch via 7 parallel comment-only agents partitioning all 88
in-scope files (commit `b18f6d66`):
- game/ui/game_ui_pipeline.{cpp,h} + lobby_ui_model + session_phase (60 refs)
- render core: sdl3gpubackend.{cpp,h}, renderdevice.h, renderer.h (26 refs)
- render/cppx_ui/** (19 files, net -6 lines)
- client/ui app-shell + theme + components + hooks + providers (~55 refs)
- client/ui/screens/** (incl lobby_components detached-block relocation)
- game core + net/controldispatch + world (30 refs, wire/sim comments-only)
- ui/** misc (focus, interaction_hooks, draw_command_builder, cppx_smoke, …)

### Target 2 — migration-narration trimming: DONE

Trimmed opportunistically in the same pass: `was-Clay` / `ported-verbatim` /
`legacy-path-removed` asides, parity essays restating the next line, the
"vector interim / replaced in SIL-89" aside in tokens.h, resolved-TODO
pointers. Preserved genuine non-obvious "why" rationale (origin-parity color
source-of-truth, dirty-skip caveat, canonical-cell notes, etc.).

### Target 3 — orphaned/detached comment block: DONE

`screens/lobby/components/lobby_components.cppx:423-454` cleared: 8 descriptive
comments relocated to sit directly above the functions they document
(GameBrowser, ConnectingPanel, StagingPanel, TechListCell, LobbyTitleBar,
AgentCard, LobbyChatPanel, MapPreviewOverlay, …); redundant narration the
target functions already document internally was deleted.

## Earlier mechanical sweep (iteration 1)

- Whitespace/blanks/#if0/void-x/descriptor-anti-pattern: none found (codebase
  was mechanically clean). Only fix: 3 triple-blank runs collapsed in
  lobby_components.cppx (batch 1, commit `93251af8`).

## Deliberately left (documented exceptions)

- `render/cppx_ui/ui_demo.cpp:97` — `static const char *kTitle =
  "SIL-11 cppx UI bridge";` is a STRING LITERAL (on-screen demo title), not a
  comment. Editing it is a visual/behavior change → out of scope.
- `audio/audio.cpp:415` — real SDL3_mixer 3.x limitation TODO (post-mix ffmpeg
  callback unavailable). Genuine, not migration debris.
- `client/ui/components/tokens.h:41` — `origin/main's accent edge was a baked
  sprite, not a fill ramp; these stops are the green-phosphor family.` Color
  source-of-truth rationale (matched only by the word "was").
- All `cppx` mentions in comments — `cppx` is the NAME of the live UI engine /
  architecture, not migration chatter. Kept everywhere.
- SIL refs under `**/CLAUDE.md` and `docs/` — intentional cross-references to
  live work (e.g. SIL-84 design-parity restore, SIL-240 GPU path); explicitly
  out of scope per CLEANUP_LOOP.md.
- `net/` pre-migration refs (`(issue #23)`, `PR #152`) — predate the cppx
  migration, not part of this cleanup's scope.
- `src/ui/runtime/{react,element,tree}.*` — vendored upstream golden code from
  `~/repos/ui`; never touched.

## Checklist

| Area | Status | Note |
|------|--------|------|
| Mechanical sweep (whitespace/blanks/#if0/void-x) | done | only 3 triple-blanks, fixed batch 1 |
| Descriptor-vs-JSX anti-pattern hunt | done | NOT present |
| Target 1: SIL-#### ticket-ref removal | done | ~271 refs, commit b18f6d66 |
| Target 2: migration-narration trimming | done | folded into the same pass |
| Target 3: detached comment block relocation | done | lobby_components.cppx |
