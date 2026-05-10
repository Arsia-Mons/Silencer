# Spectating — Progress

Tracker for the multi-phase spectating feature. Design:
[2026-05-09-spectating.md](2026-05-09-spectating.md).

All four phases land in a single umbrella branch / PR:
`hv/spectator` /
[PR #148](https://github.com/Arsia-Mons/Silencer/pull/148). The PR
merges when the whole feature is in — phases below check off as
they're complete on branch.

## Phases

- [x] **Phase 1 — Game-creation: spectatable flag.** Done on branch.
  Toggle wired through wire format, create-game UI, and config
  persistence.
- [ ] **Phase 2 — Server browser: spectatable affordance.** Not started.
- [ ] **Phase 3 — Joining as spectator (any time).** Not started.
- [ ] **Phase 4 — Spectator controls.** Not started.

## Handoff prompt

> Branch `hv/spectator` is the umbrella for the entire spectating
> feature; PR #148 lands when all four phases are in. Phase 1 (the
> spectatable flag at game creation) is done on branch — wire format,
> create-game UI, config persistence.
>
> Next up is Phase 2 (server-browser affordance for spectatable
> games). Before touching code, walk through the Phase 2 open
> questions in the design doc with the user — visual treatment
> (icon/badge/text), whether to show spectator count, treatment of
> full-but-spectatable rows, and click action model. Phase 3 has the
> bulk of the open questions and is the real engineering chunk;
> Phase 2 is mostly UX choices on the existing game-list panel
> (`clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp`).
