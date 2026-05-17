# Task Notes: Plan fix for overflowing game options menu

Created: 2026-05-17T15:50:32.846546
Completed: 2026-05-17T15:52:22.837971

## Summary
Explored the lobby game-create UI. The overflow comes from the fixed-height `LobbyRightUpperBox` clipping `BuildGameCreateUpperTree`, which currently fits all six option rows with no viewport/scroll model. Existing repo patterns suggest a caller-owned scroll state plus a registered scroll area (like options controls) is the safest path; Clay-only clipping would restore wheel scrolling but would not handle `UiActionKind::Scroll` from control-socket/automation.

## Files Changed

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
