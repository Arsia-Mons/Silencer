# Task Notes: Fix tech/team/ready button width scaling regression in game UI

Created: 2026-05-17T16:54:14.910880
Completed: 2026-05-17T16:57:06.225487

## Summary
Fixed tech panel item alignment by reserving each legacy checkbox column width in the Clay grid, preventing empty remote-peer columns from collapsing and shifting the local tech-name list left. Verified with a rebuilt client and a real headless lobby/create-game/Choose Tech screenshot run.

## Files Changed

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
