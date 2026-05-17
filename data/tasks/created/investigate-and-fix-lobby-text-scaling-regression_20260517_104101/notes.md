# Task Notes: Investigate and fix lobby text scaling regression

Created: 2026-05-17T10:41:01.788237
Completed: 2026-05-17T10:46:21.275342

## Summary
Fixed the lobby/menu text shrink jump by splitting gameplay vs menu UI scale heuristics, keeping gameplay on strict legacy fit while menus round to the nearest legacy scale. Added ui_scale/ui_width/ui_height to the control-socket state output and added a focused resize regression script. Verified with a wrapper build, PASS on tests/cli-agent/e2e/52_menu_ui_scale_resize.sh, and PASS on tests/cli-agent/e2e/15_lobbyconnect_scaled_text_focus.sh.

## Files Changed

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
