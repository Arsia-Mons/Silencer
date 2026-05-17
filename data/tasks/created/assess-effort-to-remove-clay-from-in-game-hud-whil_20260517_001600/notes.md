# Task Notes: Assess effort to remove Clay from in-game HUD while keeping menu migration

Created: 2026-05-17T00:16:00.132110
Completed: 2026-05-17T00:19:34.476566

## Summary
Assessed gameplay HUD rollback scope. Current in-game HUD is integrated into the shared ClientUi/Clay frame via game.cpp, split across multiple Clay HUD builders, fed by HudView, and interactive overlays are handled by InGameUiController plus control-socket tests. Conclusion: menus can stay Clay, but a full in-game HUD rollback is a moderate-to-large targeted effort rather than a one-commit revert.

## Files Changed

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
