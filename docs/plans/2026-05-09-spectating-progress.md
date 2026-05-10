# Spectating — Progress

Tracker for the multi-phase spectating feature. Design:
[2026-05-09-spectating.md](2026-05-09-spectating.md).

## Phases

- [x] **Phase 1 — Game-creation: spectatable flag.** Shipped —
  [PR #148](https://github.com/Arsia-Mons/Silencer/pull/148).
- [ ] **Phase 2 — Server browser: spectatable affordance.** Not started.
- [ ] **Phase 3 — Joining as spectator (any time).** Not started.
- [ ] **Phase 4 — Spectator controls.** Not started.

## Handoff prompt

> Phase 1 (the spectatable flag at game creation) just shipped on
> branch `hv/spectator` / PR #148 — wire format, create-game UI,
> config persistence. The PR is in draft awaiting test-plan
> verification.
>
> Next up is Phase 2 (server-browser affordance for spectatable
> games). Before touching code, walk through the Phase 2 open
> questions in the design doc with the user — visual treatment
> (icon/badge/text), whether to show spectator count, treatment of
> full-but-spectatable rows, and click action model. Phase 3 has the
> bulk of the open questions and is the real engineering chunk;
> Phase 2 is mostly UX choices on the existing game-list panel
> (`clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp`).
